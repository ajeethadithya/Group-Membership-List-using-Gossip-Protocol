/**********************************
 * FILE NAME: MP1Node.cpp
 *
 * DESCRIPTION: Membership protocol run by this Node.
 * 				Definition of MP1Node class functions.
 **********************************/

#include "MP1Node.h"

/*
 * Note: You can change/add any functions in MP1Node.{h,cpp}
 */

/**
 * Overloaded Constructor of the MP1Node class
 * You can add new members to the class if you think it
 * is necessary for your logic to work
 */
MP1Node::MP1Node(Member *member, Params *params, EmulNet *emul, Log *log, Address *address) {
	for( int i = 0; i < 6; i++) {
		NULLADDR[i] = 0;
	}
	this->memberNode = member;
	this->emulNet = emul;
	this->log = log;
	this->par = params;
	this->memberNode->addr = *address;
}

/**
 * Destructor of the MP1Node class
 */
MP1Node::~MP1Node() {}

/**
 * FUNCTION NAME: recvLoop
 *
 * DESCRIPTION: This function receives message from the network and pushes into the queue
 * 				This function is called by a node to receive messages currently waiting for it
 */
int MP1Node::recvLoop() {
    if ( memberNode->bFailed ) {
    	return false;
    }
    else {
    	return emulNet->ENrecv(&(memberNode->addr), enqueueWrapper, NULL, 1, &(memberNode->mp1q));
    }
}

/**
 * FUNCTION NAME: enqueueWrapper
 *
 * DESCRIPTION: Enqueue the message from Emulnet into the queue
 */
int MP1Node::enqueueWrapper(void *env, char *buff, int size) {
	Queue q;
	return q.enqueue((queue<q_elt> *)env, (void *)buff, size);
}

/**
 * FUNCTION NAME: nodeStart
 *
 * DESCRIPTION: This function bootstraps the node
 * 				All initializations routines for a member.
 * 				Called by the application layer.
 */
void MP1Node::nodeStart(char *servaddrstr, short servport) {
    Address joinaddr;
    joinaddr = getJoinAddress();

    // Self booting routines
    if( initThisNode(&joinaddr) == -1 ) {
#ifdef DEBUGLOG
        log->LOG(&memberNode->addr, "init_thisnode failed. Exit.");
#endif
        exit(1);
    }

    if( !introduceSelfToGroup(&joinaddr) ) {
        finishUpThisNode();
#ifdef DEBUGLOG
        log->LOG(&memberNode->addr, "Unable to join self to group. Exiting.");
#endif
        exit(1);
    }

    return;
}

/**
 * FUNCTION NAME: initThisNode
 *
 * DESCRIPTION: Find out who I am and start up
 */
int MP1Node::initThisNode(Address *joinaddr) {
	/*
	 * This function is partially implemented and may require changes
	 */
    try {
        int id = *(int*)(&memberNode->addr.addr);
        int port = *(short*)(&memberNode->addr.addr[4]);
        long heartbeat;

        memberNode->bFailed = false;
        memberNode->inited = true;
        memberNode->inGroup = false;
        // node is up!
        memberNode->nnb = 0;
        memberNode->heartbeat = 0;
        memberNode->pingCounter = TFAIL;
        memberNode->timeOutCounter = -1;

        heartbeat = (long)(memberNode->heartbeat);

        initMemberListTable(memberNode, id, (short)port, heartbeat);
    }

    catch(int err) {
        return -1;
    }

    return 0;
}

/**
 * FUNCTION NAME: introduceSelfToGroup
 *
 * DESCRIPTION: Join the distributed system
 */
int MP1Node::introduceSelfToGroup(Address *joinaddr) {
	MessageHdr *msg;
#ifdef DEBUGLOG
    static char s[1024];
#endif

    if ( 0 == memcmp((char *)&(memberNode->addr.addr), (char *)&(joinaddr->addr), sizeof(memberNode->addr.addr))) {
        // I am the group booter (first process to join the group). Boot up the group
#ifdef DEBUGLOG
        log->LOG(&memberNode->addr, "Starting up group...");
#endif
        memberNode->inGroup = true;
    }
    else {
        size_t msgsize = sizeof(MessageHdr) + sizeof(joinaddr->addr) + sizeof(long) + 1;
        msg = (MessageHdr *) malloc(msgsize * sizeof(char));

        // create JOINREQ message: format of data is {struct Address myaddr}
        msg->msgType = JOINREQ;
        memcpy((char *)(msg+1), &memberNode->addr.addr, sizeof(memberNode->addr.addr));
        memcpy((char *)(msg+1) + 1 + sizeof(memberNode->addr.addr), &memberNode->heartbeat, sizeof(long));

#ifdef DEBUGLOG
        sprintf(s, "Trying to join...");
        log->LOG(&memberNode->addr, s);
#endif

        // send JOINREQ message to introducer member
        emulNet->ENsend(&memberNode->addr, joinaddr, (char *)msg, msgsize);
        

        free(msg);
    }

    return 1;

}

/**
 * FUNCTION NAME: finishUpThisNode
 *
 * DESCRIPTION: Wind up this node and clean up state
 */
int MP1Node::finishUpThisNode() {
  
    free(memberNode);
}

/**
 * FUNCTION NAME: nodeLoop
 *
 * DESCRIPTION: Executed periodically at each member
 * 				Check your messages in queue and perform membership protocol duties
 */
void MP1Node::nodeLoop() {

    if (memberNode->bFailed) {
    	return;
    }
    // Check my messages
    checkMessages();

    // Wait until you're in the group...
    if( !memberNode->inGroup ) {
    	return;
    }

    // ...then jump in and share your responsibilites!
    nodeLoopOps();

    return;
}

/**
 * FUNCTION NAME: checkMessages
 *
 * DESCRIPTION: Check messages in the queue and call the respective message handler
 */
void MP1Node::checkMessages() {

    void *ptr;
    int size;
    // Pop waiting messages from memberNode's mp1q
    while ( !memberNode->mp1q.empty() ) {
    	ptr = memberNode->mp1q.front().elt;
    	size = memberNode->mp1q.front().size;
    	memberNode->mp1q.pop();
    	recvCallBack((void *)memberNode, (char *)ptr, size);
    }

    return;
}

/**
 * FUNCTION NAME: recvCallBack
 *
 * DESCRIPTION: Message handler for different message types
 */
bool MP1Node::recvCallBack(void *env, char *data, int size ) {

    MessageHdr* msg = (MessageHdr*) data;
 
    if(msg->msgType == JOINREQ)
        respondToJOINREQ(msg);

    else if(msg->msgType == JOINREP) 
        respondToJOINREP(msg);
    
    else if(msg->msgType == GOSSIPMSG)
        respondToGOSSIPMSG(msg);

}

/**
 * FUNCTION NAME: respondToJOINREQ
 *
 * DESCRIPTION: Introducer responds to the JOINREQ message
 *              with a JOINREP message
 */
void MP1Node::respondToJOINREQ(MessageHdr* msg) {

    Address toaddr;
    short newPort;
    long newHeartbeat;
    char* memberListByteStream;
    int newId, sizeOfMemberList;

    sizeOfMemberList = memberNode->memberList.size();
    memberListByteStream = pack(&sizeOfMemberList);
    
    //send JOINREP message
    memcpy(toaddr.addr, (char*)(msg + 1), sizeof(toaddr.addr));
    memcpy(&newHeartbeat, (char*)(msg + 1) + sizeof(memberNode->addr.addr) + 1, sizeof(long));

    checkNullAddress(&toaddr);
    newId = *(int*)(&toaddr.addr);
    newPort = *(short*)(&toaddr.addr[4]);

    // update memberList with new node entry
    MemberListEntry* entry = new MemberListEntry(newId, newPort, newHeartbeat, par->getcurrtime());
    memberNode->memberList.push_back(*entry);
    memberNode->nnb++;

    sendMessage(toaddr, sizeOfMemberList, memberListByteStream, "JOINREP");

    #ifdef DEBUGLOG
        log->logNodeAdd(&memberNode->addr, &toaddr);
    #endif

}

/**
 * FUNCTION NAME: respondToJOINREP
 *
 * DESCRIPTION: Respond to JOINREP message
 */
void MP1Node::respondToJOINREP(MessageHdr* msg) {

    processMSG(msg);
    memberNode->inGroup = true;

}

/**
 * FUNCTION NAME: respondToGOSSIPMSG
 *
 * DESCRIPTION: Respond to the GOSSIPMSG
 */
void MP1Node::respondToGOSSIPMSG(MessageHdr* msg) {

    processMSG(msg);
}

/**
 * FUNCTION NAME: processMSG
 *
 * DESCRIPTION: Respond to the message by
 *              unpacking and updating the membership list  
 */
void MP1Node::processMSG(MessageHdr * msg) {

    int sizeOfMemberList;
    char* memberListByteStream;
    vector<MemberListEntry> newMemberList;

    memcpy(&sizeOfMemberList, (char*)(msg + 1) + sizeof(memberNode->addr.addr), sizeof(int));
    memberListByteStream = (char*)(msg + 1) + sizeof(memberNode->addr.addr) + sizeof(int);
    
    unpack(memberListByteStream, sizeOfMemberList, &newMemberList);
    updateMemberList(&newMemberList);
}

/**
 * FUNCTION NAME: pack
 *
 * DESCRIPTION: Serialized the membership list from 
 *              vector<MembershipListEntry> to char*      
 */
char* MP1Node::pack(int* sizeOfMemberList) {

    size_t  offset = 0;
    vector <MemberListEntry>::iterator iter;
    char* memberListByteStream = (char*)malloc(*sizeOfMemberList * sizeof(MemberListEntry));

    for(iter = memberNode->memberList.begin(); iter != memberNode->memberList.end(); iter++) {
        memcpy(memberListByteStream + offset, &(*iter), sizeof(MemberListEntry));
        offset = offset + sizeof(MemberListEntry);  
    }
    
    return memberListByteStream;
}

/**
 * FUNCTION NAME: unpack
 *
 * DESCRIPTION: Deserialize the membership list from 
 *              char* to vector<MembershipListEntry>          
 */
void MP1Node::unpack(char* memberListByteStream, int sizeOfMemberList, vector<MemberListEntry>* returnNewMemberList) {

    size_t  offset = 0;
    vector<MemberListEntry> newMemberList;

    for(int i = 0; i < sizeOfMemberList; i++) {
        MemberListEntry *entry = (MemberListEntry*)(memberListByteStream + offset);
        newMemberList.push_back(*entry);
        offset = offset + sizeof(MemberListEntry);
    }

    *returnNewMemberList = newMemberList;

}

/**
 * FUNCTION NAME: updateMemberList
 *
 * DESCRIPTION: Update the node's membership list with the newly gossip'd
 *              membership list
 *              For each node in the new membership list, check if it 
 *              has exceeded TFAIL
 */
void MP1Node::updateMemberList(vector<MemberListEntry>* newMemberList) {
  
    Address addr;
    int sizeOfMemberList, sizeOfNewMemberList;
    sizeOfMemberList = memberNode->memberList.size();
    sizeOfNewMemberList = (*newMemberList).size();

    for(int i = 0; i < sizeOfNewMemberList; i++) {
        for(int j = 0; j <= sizeOfMemberList; j++) {    // <= is used when node is not there is the list

            if(par->getcurrtime() - (*newMemberList)[i].timestamp < TFAIL) {    // if the node has failed, do not update it to the membershipList

                if(j == sizeOfMemberList) { // if the entry is new

                    memberNode->memberList.push_back((*newMemberList)[i]);
                    memberNode->nnb++;
                    addr = Address(to_string((*newMemberList)[i].id) + ":" + to_string((*newMemberList)[i].port));
                    checkNullAddress(&addr);

                    #ifdef DEBUGLOG
                        log->logNodeAdd(&memberNode->addr, &addr);
                    #endif
                }

                else if(memberNode->memberList[j].id == (*newMemberList)[i].id && 
                        memberNode->memberList[j].port == (*newMemberList)[i].port) {   // if the entry is already existing

                    if((*newMemberList)[i].heartbeat > memberNode->memberList[j].heartbeat) {
                        memberNode->memberList[j].heartbeat = (*newMemberList)[i].heartbeat;
                        memberNode->memberList[j].timestamp = par->getcurrtime();
                    }
                    break;  // break, or else j == sizeOfMemberList will execute
                }
            }
        }
    }
}

/**
 * FUNCTION NAME: nodeLoopOps
 *
 * DESCRIPTION: Check if any node hasn't responded within a timeout period and then delete
 * 				the nodes
 * 				Propagate your membership list
 */
void MP1Node::nodeLoopOps() {

    int id;
    short port;
    Address addr;

    // update node info, periodically
    memberNode->heartbeat++;
    memberNode->memberList[0].heartbeat = memberNode->heartbeat;
    memberNode->memberList[0].timestamp = par->getcurrtime();
    
    // check and remove failed nodes
    for(int index = memberNode->memberList.size() - 1; index >= 0; index--)  {
        if(par->getcurrtime() - memberNode->memberList[index].timestamp > TREMOVE) {

            id = memberNode->memberList[index].id;
            port = memberNode->memberList[index].port;
            addr = Address(to_string(id) + ":" + to_string(port));
            checkNullAddress(&addr);

            #ifdef DEBUGLOG
                log->logNodeRemove(&memberNode->addr, &addr);
            #endif

            memberNode->memberList.erase(memberNode->memberList.begin() + index);

        }
    }

    multicastGossip();

    return;
}

/**
 * FUNCTION NAME: multicastGossip
 *
 * DESCRIPTION: Multicast the GOSSIPMSG to 'b' random targets
 */
void MP1Node::multicastGossip() {

    int id;
    short port;
    Address toaddr;
    char* memberListByteStream;
    int i, lower, upper, targetCount, targetIndex, sizeOfMemberList;

    sizeOfMemberList = memberNode->memberList.size();
    memberListByteStream = pack(&sizeOfMemberList);

    targetCount = (sizeOfMemberList < 2) ? sizeOfMemberList : 2;
    
    // uniform distribution random number generator
    const int min  = 1;  // make sure GOSSIPMSG is not sent to index 0 (node itself)
    const int max    = memberNode->memberList.size() - 1;
    std::random_device rand_dev;
    std::mt19937 generator(rand_dev());
    std::uniform_int_distribution<int> distr(min, max);

     if(sizeOfMemberList > 1) {  // GOSSIPMSG is only sent if there is atleast one other node in the memberList

        for(i = 0; i < targetCount; i++) {
            
            targetIndex = distr(generator); // node index to which gossip should be sent

            // get the address of receiver
            id = memberNode->memberList[targetIndex].id;
            port = memberNode->memberList[targetIndex].port;
            toaddr = Address(to_string(id) + ":" + to_string(port));
            checkNullAddress(&toaddr);

            sendMessage(toaddr, sizeOfMemberList, memberListByteStream, "GOSSIPMSG");
            i++;
        }
    }
}

/**
 * FUNCTION NAME: sendMessage
 *
 * DESCRIPTION: Send message of type JOINREP or GOSSIPMSG to address toaddr
 */
void MP1Node::sendMessage(Address toaddr, int sizeOfMemberList, char* memberListByteStream, char* msgtype) {

    MessageHdr* sendMsg;
 
    size_t msgsize =    sizeof(MessageHdr)
                        + sizeof(memberNode->addr.addr) // memberNode addr
                        + sizeof(int)   // sizeOfMemberList
                        + (sizeOfMemberList * sizeof(MemberListEntry)); // memberList
    
    sendMsg = (MessageHdr *) malloc(msgsize * sizeof(char));

    sendMsg->msgType = (msgtype == "JOINREP") ? JOINREP : GOSSIPMSG;    // set msgType

    memcpy((char*)(sendMsg + 1), &memberNode->addr.addr, sizeof(memberNode->addr.addr));
    memcpy((char*)(sendMsg + 1) + sizeof(memberNode->addr.addr), &sizeOfMemberList, sizeof(int));
    memcpy((char*)(sendMsg + 1) + sizeof(memberNode->addr.addr) + sizeof(int), memberListByteStream, sizeOfMemberList * sizeof(MemberListEntry));
    
    emulNet->ENsend(&memberNode->addr, &toaddr, (char*)sendMsg, msgsize);

    free(sendMsg);

}

/**
 * FUNCTION NAME: checkNullAddress
 *
 * DESCRIPTION: Function checks if the address is NULL
 */
void MP1Node::checkNullAddress(Address *addr) {

if(isNullAddress(addr)) {
        #ifdef DEBUGLOG
            log->LOG(&memberNode->addr, "NULL address. Exit.");
        #endif
        exit(1);
    }
}

/**
 * FUNCTION NAME: isNullAddress
 *
 * DESCRIPTION: Function checks if the address is NULL
 */
int MP1Node::isNullAddress(Address *addr) {
	return (memcmp(addr->addr, NULLADDR, 6) == 0 ? 1 : 0);
}

/**
 * FUNCTION NAME: getJoinAddress
 *
 * DESCRIPTION: Returns the Address of the coordinator
 */
Address MP1Node::getJoinAddress() {
    Address joinaddr;

    memset(&joinaddr, 0, sizeof(Address));
    *(int *)(&joinaddr.addr) = 1;
    *(short *)(&joinaddr.addr[4]) = 0;
    return joinaddr;
}

/**
 * FUNCTION NAME: initMemberListTable
 *
 * DESCRIPTION: Initialize the membership list
 */
void MP1Node::initMemberListTable(Member *memberNode, int id, short port, long heartbeat) {
	
    memberNode->memberList.clear();
    MemberListEntry* introducer = new MemberListEntry(id, port, heartbeat, (long)par->getcurrtime());
    memberNode->memberList.push_back(*introducer);

    #ifdef DEBUGLOG
        log->logNodeAdd(&memberNode->addr, &memberNode->addr);
    #endif

}

/**
 * FUNCTION NAME: printAddress
 *
 * DESCRIPTION: Print the Address
 */
void MP1Node::printAddress(Address *addr)
{
    printf("%d.%d.%d.%d:%d \n",  addr->addr[0],addr->addr[1],addr->addr[2],
                                                       addr->addr[3], *(short*)&addr->addr[4]) ;    
}

