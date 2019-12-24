#include "ntable.h"

#include <#include <arpa/inet.h>

NeighborTable::NeighborTable() {
  Init();
}

void NeighborTable::Init() {
  SetLocalNetInfo();
  ReadCostTable();
}

int NeighborTable::SetLocalNetInfo() {
  local_.conn = -1;

  struct ifaddrs *ifaddr, *ifa;

  if (getifaddrs(&ifaddr) < 0) {
    perror("getifaddres failed");
    return -1;
  }

  for (struct ifaddrs *ifa = ifaddr; ifa != nullptr; 
       ifa = ifa->ifa_next) {
    if (!ifa->ifa_addr)
      continue;

    int family = ifa->ifa_addr->sa_family;

    if (family == AF_INET) {
      struct sockaddr_in *addr = (struct sockaddr_in *)ifa->ifa_addr;
      if (addr->sin_addr.s_addr != 16777343) {
        local_.ip = addr->sin_addr.s_addr;
        return 0;
      }
    }
  }

  local_.ip = 0;
  return -1;
}

void NeighborTable::ReadCostTable(const std::string &filename) {
  ifstream file(filename);
  if (!file) {
    std::cerr << "Failed to open " << filename << std::endl;
    return -1;
  }

  do {
    std::string from_ip_str;
    std::string to_ip_str;
    Cost cost;

    file >> from_ip_str;
    file >> to_ip_str;
    file >> cost;

    Ip from_ip = inet_addr(from_ip_str);
    Ip to_ip = inet_addr(to_ip_str);

    costs_[from_ip][to_ip] = cost;
    costs_[to_ip][from_ip] = cost;
  } while (true);

  file.close();
  return 0;
}

int NeighborTable::AddConnection(Ip ip, int conn) {
  if (neighbors_.find(ip) == neighbors_.end())
    return -1;

  neighbors_[ip] = conn;
  return 0;
}

Ip NeighborTable::GetIp() const {
  return local_.ip;
}
