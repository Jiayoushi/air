#include "ntable.h"

#include <#include <arpa/inet.h>

NeighborTable::NeighborTable() {}

void NeighborTable::Init() {
  SetLocalNetInfo();
  ReadCostTable();
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
