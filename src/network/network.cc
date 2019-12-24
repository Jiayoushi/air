#include "network.h"

#include <thread>

int IpConnectOverlay() {
  
}









int main() {
  std::cout << "[IP] network layer starting ..." << std::endl;


  int overlay_conn = IpConnectOverlay();
  if (overlay_conn < 0) {
    std::cerr << "[IP] connect overlay failed" << std::endl;
    exit(-1);
  }

  std::thread update = std::thread(IpRouteUpdate);
  std::thread input = std::thread(IpInput);

  std::cout << "[IP] network layer started" << std::endl;

  while (1)
    std::this_thread::sleep_for(60);

  return 0;
}
