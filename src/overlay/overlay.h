#ifndef AIR_OVERLAY_H_
#define AIR_OVERLAY_H_

#include "common/pkt.h"

int OverlayInit();
int OverlayStop();
int OverlaySend(Ip next_hop, PktBufPtr pkt);
PktPtr OverlayRecv(int conn);

#endif
