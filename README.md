<p align="center"><h2>GROUP MEMBERSHIP LIST USING GOSSIP PROTOCOL</h2></p>


This machine programming assignment is a part of Coursera's course, Cloud Computing Concepts, Part 1, by University of Illinois at Urbana-Champaign.

This program is about implementing a gossip-style distributed membership protocol. Since it is infeasible to run a thousand cluster nodes (peers) over a real network, the program has been implemented with the use of an emulated network layer (EmulNet). The membership protocol implementation will sit above EmulNet in a peer-to-peer (P2P) layer, but below an App layer. Think of this like a three-layer protocol stack with Application, P2P, and EmulNet as the three layers (from top to bottom).


**The implemented gossip protocol satifies:**

1. Completeness all the time: every non-faulty process will detect every node join, failure, and leave.

2. Accuracy of failure detection when there are no message losses and message delays are small. When there are message losses, completeness is satisfied and accuracy will be high. It achieves all of these even under simultaneous multiple failures.


**Testing**
1. To compile the code, run **make**.

2. To execute the program, from the program directory run: **./Application testcases/<test_name>.conf**. The conf files contain information about the parameters used by the application.


**Emulated Network: EmulNet**\
\
ENinit is called once by each node (peer) to initialize its own address (myaddr). ENsend and ENrecv are called by a peer respectively to send and receive waiting messages. ENrecv enqueues a received message using a function specified through a pointer enqueue(). ENcleanup is called at the end of the simulator run to clean up the EmulNet implementation.


**Application**\
\
This layer drives the simulation. FilesApplication.{cpp,h} contain the code for this. The main() function runs in synchronous periods (globaltime variable). 

During each period, some peers may be started up, and some may be caused to crash. Most importantly, for each peer that is alive, the function nodeLoop() is called. nodeLoop() is implemented in the P2P layer (MP1Node.{cpp,h}) and basically receives all messages that were sent for this peer in the last period, as well as checks whether the application has any new waiting requests.


**P2P Layer**\
\
Files MP1Node.{cpp,h} contain code for this. This is the layer responsible for implementing the gossip membership protocol. JOINREQ messages are received by the introducer. The introducer is the first peer to join the system. The introducer replies to a JOINREQ with a JOINREP message. JOINREP messages specify the cluster member list.


**Parameters**\
\
Params.{cpp,h} contains the setparams() function that initializes several parameters at the simulator start, including the number of peers in the system(EN_GPSZ),and the global time variable globaltime, etc.
