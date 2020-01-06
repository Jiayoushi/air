#ifndef AIR_TCP_TIMER_H_
#define AIR_TCP_TIMER_H_

#define kConnectingTimer      0
#define kRetransmitTimer      1
#define kDelayedAckTimer      2
#define kPersistTimer         3
#define kKeepaliveTimer       4
#define kFinWait2Timer        5
#define kTimeWaitTimer        6

#define kTimerNum             7
#define kTimeoutInterval      500      /* 500ms */
#define kTimeWaitPeriod       10       /* 10 intervals */
#define kRetransmitTimeout    10       /* 10 intervals */

#endif
