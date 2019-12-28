#ifndef AIR_CONSTANTS_H_
#define AIR_CONSTANTS_H_

#include <chrono>

#include "common.h"

/* Test */
#define kPacketLossRate                 0
#define kPacketErrorRate                0

#define kGbnWindowSize                  5
#define kUnsentCapacity                 100
#define kMaxSynRetry                    100

#define kTimeoutIntervalInMs            6000

const std::chrono::seconds kRouteUpdateIntervalInSecs(2);
const Ip     kBroadcastIpAddr         = 0;
const size_t kMss                     = 512;
const size_t kMaxPacketData           = 512;
const size_t kMaxHostNum              = 1024;

#endif
