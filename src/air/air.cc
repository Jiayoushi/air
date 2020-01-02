#include "air.h"

#include <thread>
#include <atomic>

#include "tcp/tcp.h"
#include "ip/ip.h"
#include "overlay/overlay.h"
#include "common/common.h"

static std::atomic<int> running;

static std::vector<std::thread> modules;

int Init() {
  srand(time(nullptr));

  running = 0;

  modules.emplace_back(OverlayMain);
  modules.emplace_back(IpMain);
  modules.emplace_back(TcpMain);

  while (running != 3)
    std::this_thread::sleep_for(std::chrono::seconds(1));

  return 0;
}

void RegisterInitSuccess() {
  ++running;
}

int Stop() {
  running = 0;

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
