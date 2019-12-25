#include "pkt.h"

int SendPacket(PktPtr pkt, int conn) {
  const char *pkt_start = "!&";
  const char *pkt_end = "!#";

  if (send(connection, pkt_start, 2, 0) < 0) {
    perror("[PKT] SendPacket failed to send pkt_start");
    return -1;
  }

  if (send(connection, pkt->get(), pkt->length, 0) < 0) {
    perror("[PKT] SendPacket failed to send packet");
    return -1;
  }

  if (send(connection, pkt_end, 2, 0) < 0) {
    perror("[PKT] SendPacket failed to send pkt_end");
    return -1;
  }

  return 0;
}

PktPtr RecvPacket(int conn) {
  char buf[65535];
  memset(buf, 0, 65535);

  uint32_t idx = 0;

  int state = kWaitFirstStart;
  char c;
  int recved = 0;
  while ((recved = recv(conn, &c, 1, 0)) > 0) {
    switch (state) {
      case kWaitFirstStart:   // Wait !
        if (c == '!')
          state = kWaitSecondStart;
        break;
      case kWaitSecondStart:  // Wait &
        if (c == '&') {
          state = kWaitFirstEnd;
        } else {
          state = kWaitFirstStart;
        }
        break;
      case kWaitFirstEnd:     // Wait !
        buf[idx++] = c;
        if (c == '!') {
          state = kWaitSecondEnd;
        }
        break;
      case kWaitSecondEnd:    // Wait #
        buf[idx++] = c;
        // "!#" is received, the segment receiving is complete
        if (c == '#') {
          PktPtr pkt = std::make_shared<Packet>();
 
          uint16_t pkt_len = idx - 2;
          memcpy(pkt.get(), buf, pkt_len);
          pkt->length = pkt_len;

          return pkt;
        } else if (c == '!') {
          // Previous '!' is not end control character
          // This one might be, so still wait for '#'
        } else {
          // Previous '!' is definitely not end control character
          // Go back to previous state to wait for first end control character
          state = kWaitFirstEnd;
        }
        break;
      default:
        break;
    }
  }

  return nullptr;
}
