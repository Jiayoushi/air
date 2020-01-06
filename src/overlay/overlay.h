#ifndef AIR_OVERLAY_H_
#define AIR_OVERLAY_H_

#include <pkt.h>
#include <atomic>
#include <common.h>

int OverlayMain();
int OverlayStop();
int OverlaySend(PktBufPtr pkt);

Ip GetLocalIp();

std::vector<std::pair<Ip, Cost>> GetAllCost();
Cost GetCost(Ip from, Ip to);

extern std::atomic<bool> ov_running;

#endif
