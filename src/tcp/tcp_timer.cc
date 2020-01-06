#include <tcp_timer.h>

#include <thread>

#include <tcp.h>

void TcpTimeout() {
  while (TcpRunning()) {
    for (int tcb_id = 0; tcb_id < kMaxConnection; ++tcb_id) {
      TcbPtr tcb = tcb_table[tcb_id];
      if (!tcb)
        continue;

      /* Time wait timeout */
      if (tcb->timer_flags & kTimeWaitTimer) {
        if (--tcb->timers[kTimeWaitTimer] == 0) {
          tcb->state = kClosed;
          std::cout << "[TCP] time wait timed out " << tcb_id << " is now closed" << std::endl;
	  tcb->waiting.notify_one();
        }
	continue;
      }

      /* Restransmit timeout */
      if (tcb->timer_flags & kRetransmitTimer) {
        if (--tcb->timers[kRetransmitTimer] == 0) {
          if (tcb->send_buffer.MaxRetryReached()) {
            tcb->send_buffer.PopUnacked();
            tcb->waiting.notify_one();
          } else {
            tcb->send_buffer.ResendUnacked();
	    tcb->timers[kRetransmitTimer] = kRetransmitTimeout;
	  }
        }
      }
    }

    std::this_thread::sleep_for(std::chrono::milliseconds(kTimeoutInterval));
  }
}
