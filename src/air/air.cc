#include <air.h>

#include <thread>
#include <atomic>

#include <tcp.h>
#include <ip.h>
#include <overlay.h>
#include <common.h>

static std::vector<std::thread> modules;

int Init() {
  srand(time(nullptr));

  modules.emplace_back(OverlayMain);
  modules.emplace_back(IpMain);
  modules.emplace_back(TcpMain);

  while (!TcpInitialized()|| !IpInitialized() || !OverlayInitialized())
    std::this_thread::sleep_for(std::chrono::seconds(1));

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

  for (int i = 0; i < modules.size(); ++i)
    modules[i].join();
  
  return 0;
}
