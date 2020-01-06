#include <tcp.h>

void TcpInputFromIp() {
  while (1) {
    SegBufPtr seg_buf = TcpInputQueuePop();

    if (!TcpRunning())
      break;

    if (seg_buf != nullptr) {
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
        if (seg->header.ack != tcb->iss + 1)
          return 0;

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
        std::cout << "[TCP] [Last Acked] " << last_acked << std::endl;

	switch (tcb->state) {
          case kFinWait1: {
            if (last_acked != tcb->snd_nxt)
	      return 0;

            tcb->state = kFinWait2;
	    break;
          }
          case kClosing: {
	    // TODO
	    break;
	  }
          case kLastAck: {
            if (last_acked != tcb->snd_nxt)
	      return 0;

	    tcb->state = kClosed;
	    break;
          }
	  case kTimeWait: {
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
    switch (tcb->state) {
      case kSynRcvd:
      case kConnected: {
        tcb->state = kCloseWait;
	break;
      }
      case kFinWait1: {
        tcb->state = kClosing;
	break;
      }
      case kFinWait2: {
        tcb->state = kTimeWait;
	// TODO: turn off other timers
	tcb->timers[kTimeWaitTimer] = kTimeWaitPeriod;
        break;
      }
      case kTimeWait: {
        tcb->timers[kTimeWaitTimer] = kTimeWaitPeriod;
        break;
      }
      default:
	break;
    }
  }

OUTPUT:
  TcpOutput(tcb);
  return 0;

RETURN:
  return 0;
}
