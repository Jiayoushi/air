#include "air.h"


int Init() {
  TcpInit();
  IpInit();
  SrtServerInit();
}

int Stop() {
  TcpStop();
  IpStop();
  OverlayStop();
}
