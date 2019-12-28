#include "air.h"

#include <thread>
#include <atomic>

#include "tcp/tcp.h"
#include "ip/ip.h"
#include "overlay/overlay.h"
#include "common/common.h"

static std::vector<std::thread> modules;

static std::atomic<int> running;

int Init() {
  srand(time(nullptr));

  running = 0;

  modules.emplace_back(OverlayInit);
  modules.emplace_back(IpInit);
  modules.emplace_back(TcpInit);

  while (running != 3)
    std::this_thread::sleep_for(std::chrono::seconds(1));

  return 0;
}

void RegisterInitSuccess() {
  ++running;
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
