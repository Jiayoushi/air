#ifndef AIR_CONSTANTS_H_
#define AIR_CONSTANTS_H_

#include <chrono>

#include "common.h"
#include "seg.h"

#define kGbnWindowSize                  15
#define kUnsentCapacity                 100
#define kMaxSynRetry                    100

#define kTimeoutIntervalInMs            6000

const std::chrono::seconds kRouteUpdateIntervalInSecs(8);
const Ip     kBroadcastIpAddr         = 0;
const size_t kMaxPacketData           = 512 + sizeof(SegmentHeader);
const size_t kMaxHostNum              = 1024;

#endif
