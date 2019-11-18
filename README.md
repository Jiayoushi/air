# air

Structure is designed by Dartmouth DartNet

SRT provides reliable deliver of in order byte stream.
 Assumption:
  1. Unidirectional transport: data segments flow from client to sender
  2. Connection Management: the client initiates and tears down connections
  3. No flow control, or congestion control
 Functionalities:
  1. Connection management (connection setup and teardown)
  2. Checksum on segments which is used to detect corrupt data at the receiver.
  3. Go back N is implemented for reliable delivery of data at the receiver. Sequence numbers are used to detect missing data and for putting data back into order.
  4. Retransmission: the sender (client) retransmits lost data and control packets (e.g., SYN) using timeouts and for DATA reception of ACKs.
