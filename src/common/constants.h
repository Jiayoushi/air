#ifndef AIR_CONSTANTS_H_
#define AIR_CONSTANTS_H_

#define MAX_TRANSPORT_CONNECTIONS		10
#define kPort							9008
#define MAX_SEG_LEN  					1500
#define RECEIVE_BUF_SIZE 				1000000
#define kPacketLossRate 				0.05
#define GBN_WIN 						10
#define DATASEG_TIMEOUT_MS 				500
#define SYNSEG_TIMEOUT_NS 				500000000
#define FINSEG_TIMEOUT_NS 				500000000
#define SYN_MAX_RETRY 					5
#define FIN_MAX_RETRY 					5
#define CLOSEWAIT_TIME 					1

#define kFailure 						-1
#define kSuccess  						0

#define kSegmentNotLost					0
#define kSegmentLost					1

#endif