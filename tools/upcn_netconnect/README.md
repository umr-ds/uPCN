
µPCN TCP-ZMQ-Proxy
==================

This program is used to connect to µPCN when running it on a microcontroller
connected via a TCP connection.
It decodes received data packets and forwards them via a ZeroMQ publisher (PUB)
socket.
On the other hand, it accepts transmissions via a ZeroMQ reply (REP) socket
and forwards the data encapsulated to the board.

To run the program manually you have to specify at least the IP address of the device, the port of upcn on the device,  an address for the PUB socket and one for the REP socket.
You also might want to specify a baud-rate. The default is 9600 baud.

    upcn_netconnect <ip device> <port device> <pub_sock> <rep_sock>
