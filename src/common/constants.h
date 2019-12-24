#ifndef AIR_CONSTANTS_H_
#define AIR_CONSTANTS_H_

/* Test */
#define kPacketLossRate 				0.20
#define kPacketErrorRate                0.20

/* Send */
#define kGbnWindowSize                  5
#define kUnsentCapacity                 100
#define kMaxSynRetry                    100

const size_t kMss           = 512;
const size_t kMaxPacketData = 512;
const size_t kMaxHostNum    = 1024;

/* Timeout */
#define kTimeoutIntervalInMs            6000




#endif
