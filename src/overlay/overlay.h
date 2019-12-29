#ifndef AIR_OVERLAY_H_
#define AIR_OVERLAY_H_

#include "common/pkt.h"
#include "common/common.h"

int OverlayInit();
int OverlayStop();
int OverlaySend(PktBufPtr pkt);

Ip GetLocalIp();

std::vector<std::pair<Ip, Cost>> GetDirectNeighborCost();

#endif
