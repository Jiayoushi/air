#ifndef AIR_OVERLAY_H_
#define AIR_OVERLAY_H_

#include "common/pkt.h"

int OverlaySendPacket(Ip next_host, PktPtr pkt);
PktPtr OverlayRecvPacket(int conn);
int OverlayStop();

#endif
