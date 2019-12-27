#include "air.h"

#include "tcp/tcp.h"
#include "ip/ip.h"
#include "overlay/overlay.h"
#include "common/common.h"

int Init() {
  srand(time(nullptr));

  if (OverlayInit() < 0) {
    std::cerr << "[AIR] overlay init failed" << std::endl;
    return -1;
  }

  if (IpInit() < 0) {
    std::cerr << "[AIR] network layer init failed" << std::endl;
    return -1;
  }
 
  if (TcpInit() < 0) {
    std::cerr << "[AIR] transport layer init failed" << std::endl;
    return -1;
  }
  
  return 0;
}

int Stop() {
  if (TcpStop() < 0) {
    std::cerr << "[AIR] tranposrt layer stop failed" << std::endl;
    return -1;
  }

  if (IpStop() < 0) {
    std::cerr << "[AIR] network layer stop failed" << std::endl;
    return -1;
  }

  if (OverlayStop() < 0) {
    std::cerr << "[AIR] overlay layer stop failed" << std::endl;
    return -1;
  }
  
  return 0;
}
