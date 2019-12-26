#ifndef AIR_OVERLAY_H_
#define AIR_OVERLAY_H_

#include "common/pkt.h"

int OverlayInit();
int OverlayStop();
int OverlaySendPacket(Ip next_host, PktPtr pkt);
PktPtr OverlayRecvPacket(int conn);

#endif
