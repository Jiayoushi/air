#include <tcp.h>

void TcpInputFromIp() {
  while (1) {
    SegBufPtr seg_buf = TcpInputQueuePop();

    if (!TcpRunning())
      break;

    if (seg_buf != nullptr) {
      std::cout << std::endl;
      TcpInput(seg_buf);
    }
  }
}

int TcpInput(SegBufPtr seg_buf) {
  SegPtr seg = seg_buf->segment; 
  uint8_t flags = seg->header.flags;

  std::cout << "[TCP] [RCVD] " << seg_buf << std::endl;

  /* Checksum */
  if (!ValidChecksum(seg, seg_buf->data_size + sizeof(SegmentHeader))) {
    std::cout << "[TCP] " << "Invalid checksum" << std::endl;
    return -1;
  }

  /* Locate tcb */
  int sock_fd = TcpDemultiplex(seg_buf);
  if (sock_fd < 0) {
    std::cout << "[TCP] " << "No matching tcb" << std::endl;
    return -1;
  }
  std::shared_ptr<Tcb> tcb = tcb_table[sock_fd];
  if (tcb->state == kClosed)
    return 0;
  std::lock_guard<std::mutex> lck(tcb->lock);

  if (tcb->state == kTimeWait)
    return 0;

  /* Connection */
  switch (tcb->state) {
    case kListening: {
      if (flags & kAck)
        return 0;
      if ((flags & kSyn) == 0)
        return 0;

      std::cout << "[TCP] [Syn Rcvd]" << std::endl;
      tcb->state = kSynRcvd;

      tcb->dest_ip = seg_buf->src_ip;
      tcb->dest_port = seg->header.src_port;
      tcb->irs = seg->header.seq;
      tcb->rcv_nxt = tcb->irs + 1;
      tcb->send_buffer.Clear();
      goto OUTPUT;
    }
    /* If ACK of our SYN, connection completed */
    case kSynSent: {
      if ((flags & kAck) == 0)
        return 0;
      if (seg->header.ack != tcb->iss + 1)
        return 0;
      if ((flags & kSyn) == 0)
        return 0;

      std::cout << "[TCP] [Connected]" << std::endl;
      tcb->state = kConnected;
      tcb->irs = seg->header.seq;
      tcb->rcv_nxt = tcb->irs + 1;
      tcb->send_buffer.Clear();
      tcb->waiting.notify_one();
      goto OUTPUT;
    }
    default:
      break;
  }

  /* Handle Ack */
  if (flags & kAck) {
    switch (tcb->state) {
      case kSynRcvd: {
        if (flags & kSyn)
          break;
        if (seg->header.ack != tcb->iss + 1) {
	  std::cout << "[TCP] dropped: unexpected ack" << std::endl;
          return 0;
	}

        std::cout << "[TCP] [Connected]" << std::endl;
	tcb->send_buffer.Clear();
        tcb->state = kConnected;
        tcb->waiting.notify_one();
        return 0;
      }
      case kConnected:
      case kFinWait1:
      case kFinWait2:
      case kCloseWait:
      case kClosing:
      case kLastAck:
      case kTimeWait: {
        uint32_t last_acked = tcb->send_buffer.Ack(seg->header.ack);
        std::cout << "[TCP] [Send Buffer]"
		  << " last_acked=" << last_acked 
		  << " ,unacked_size=" << tcb->send_buffer.Unacked()
		  << " ,unsent_size=" << tcb->send_buffer.Unsent()
		  << std::endl;
        if (last_acked == 0)
	  break;

	switch (tcb->state) {
          case kFinWait1: {
            if (last_acked + 1 != tcb->snd_nxt)
	      return 0;

	    std::cout << "[TCP] [Fin Wait 2]" << std::endl;
            tcb->state = kFinWait2;
	    break;
          }
          case kClosing: {
            std::cout << "[TCP] [Time Wait]" << std::endl;
	    tcb->state = kTimeWait;
	    tcb->timer_flags |= kTimeWaitTimer;
            tcb->timers[kTimeWaitTimer] = kTimeWaitPeriod;
	    break;
	  }
          case kLastAck: {
            if (last_acked + 1 != tcb->snd_nxt)
	      return 0;

	    std::cout << "[TCP] [Closed]" << std::endl;
	    tcb->state = kClosed;
	    tcb->waiting.notify_one();
	    break;
          }
	  case kTimeWait: {
	    tcb->timer_flags |= kTimeWaitTimer;
            tcb->timers[kTimeWaitTimer] = kTimeWaitPeriod;
	    break;
          }
        }
        break;
      }
      default:
	break;
    }
  }

  /* Handle Fin */
  if (flags & kFin) {
    if (seg->header.seq != tcb->rcv_nxt)
      return 0;

    switch (tcb->state) {
      case kSynRcvd:
      case kConnected: {
	std::cout << "[TCP] [Close Wait]" << std::endl;
        tcb->rcv_nxt += 1;
        tcb->state = kCloseWait;
	break;
      }
      case kFinWait1: {
	std::cout << "[TCP] [Closing]" << std::endl;
        tcb->state = kClosing;
	break;
      }
      case kFinWait2: {
	std::cout << "[TCP] [Time Wait]" << std::endl;
        tcb->rcv_nxt += 1;
        tcb->state = kTimeWait;
        tcb->timer_flags = kTimeWaitTimer;
	tcb->timers[kTimeWaitTimer] = kTimeWaitPeriod;
	goto OUTPUT;
      }
      case kTimeWait: {
        tcb->timer_flags = kTimeWaitTimer;
	tcb->timers[kTimeWaitTimer] = kTimeWaitPeriod;
	goto OUTPUT;
      }
      default:
	break;
    }
  }

  if (seg_buf->data_size != 0) {
    if (seg->header.seq != tcb->rcv_nxt) {
      std::cout << "[TCP] segment dropped, expected sequence number"
	        << tcb->rcv_nxt
		<< " got " << seg->header.seq
		<< std::endl;
      return 0;
    }

    tcb->rcv_nxt += seg_buf->data_size;
    tcb->recv_buffer.PushBack(seg_buf);
    tcb->waiting.notify_one();
  }

OUTPUT:
  if (!PureAck(seg_buf))
    TcpOutput(tcb);

RETURN:
  return 0;
}
