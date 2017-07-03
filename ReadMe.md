Project Instructions:

The bootstrap process refers to how switches register with the controller, as well as learn about each
other. This will be implemented using socket programming, the bootstrap process must also
enable the controller process, and each of the switch processes, to learn each other’s host/port information
(hostname and UDP port number information), so communication is feasible.
1. The controller process binds to a well known port number. Each switch process is provided with its
own id, as well as the hostname and port number that the controller process runs on as command line
arguments. When a switch A joins the system, it contacts the controller with a REGISTER_REQUEST,
along with its id. The controller learns the host/port information of the switch from this message.
2. The controller responds with an REGISTER_RESPONSE message, which includes a list of neighbors
of A. Specifically, for each neighbor, information returned includes: (i) the id of the neighboring switch;
(ii) a flag indicating whether the neighbor is active or not. The neighbor is considered inactive if it has not
yet registered with the controller (by sending it a REGISTER_REQUEST message), or if the controller considers the neighboring switch to be dead; and (iii) for each active switch, the host/port information of
that switch process .
3. On receiving a REGISTER_RESPONSE, switch A immediately sends a "KEEP-ALIVE" message to
each of the active neighbors (using the host/port
information provided in the response). If node ​ B receives
a "KEEP-ALIVE" message from switch A, it marks A as an active neighbor, and learns the host/port
information for switch A. Both A and B periodically send KEEP_ALIVE messages to each other as
described later in this document.

Path computations:
1. Once all switches have registered, the controller computes paths between each source destination pair
using the widest path algorithm. Note that this is not a traditional Dijkstra algorithm. Specifically, of all
possible paths between the source and destination, the path with the highest “bottleneck capacity” is
chosen. The bottleneck capacity of a path is the minimum capacity across all links of the path.
2. Once path computation is completed, the controller sends each switch its “routing table” using a
ROUTE_UPDATE message. This table includes for every other switch, the next hop to reach every
destination.

Periodic operations and failure detection:

1. Each switch takes the following operations:
1) Every K seconds, the switch sends a KEEP_ALIVE message to each of the neighboring
switches.
2) Every K seconds, the switch sends a TOPOLOGY_UPDATE message to the controller.
The TOPOLOGY_UPDATE message includes a set of live neighbors of that switch.
2. If a switch A has not received a KEEP_ALIVE message from a neighboring switch B for M * K
seconds, then, switch A declares the link connecting it to switch B as down. Immediately, it sends a
TOPOLOGY_UPDATE message to the controller sending the controller its updated view of the list
of live neighbors.
3. Once a switch A receives a KEEP_ALIVE message from a neighboring switch B that it
previously considered unreachable, it immediately marks that neighbor alive, and sends a
TOPOLOGY_UPDATE to the controller indicating its revised list of live neighbors.
4. If the controller does not receive a TOPOLOGY_UPDATE message from a switch for
M * K seconds, then it considers that switch dead, and updates its topology. Likewise, if it
starts receiving TOPOLOGY_UPDATE messages from a switch it previously considered
dead, it updates its topology again.

5. If a controller receives a TOPOLOGY_UPDATE message from a switch that indicates a
neighbor is no longer reachable, then the controller updates its topology to reflect that link
as unusable. Likewise, if the TOPOLOGY_UPDATE indicates the neighbor is now reachable,
the controller updates the topology to reflect that fact.

Failure handling:
When a controller detects that a switch or link has failed, it performs a recomputation of paths
using the shortest-widest path algorithm described above. It sends a ROUTE_UPDATE message
to all switches as described previously

Topology File Input Format:

The first line of each file is the number of switches.
Every subsequent line is of the form:

<switchID1>: <switchID2>: ​ BW:​ Delay

This indicates that there is a link connecting switchID1 and switchID2, which has a bandwidth ​ BW, and a
delay ​ Delay.

Here is an example configuration for the topology shown

6
1 2 100 10
1 4 200 30
1 6 80 10
2 3 50 10
2 5 180 20
3 4 50 5
3 6 150 20
4 5 100 10
