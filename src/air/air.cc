#include "air.h"

int Init() {
  OverlayInit();
  IpInit();
  TcpInit();
}

int Stop() {
  TcpStop();
  IpStop();
  OverlayStop();
}
