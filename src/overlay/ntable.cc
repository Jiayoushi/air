#include "ntable.h"

#include <fstream>

NeighborTable::NeighborTable() {}

void NeighborTable::Init() {
  if (ReadCostTable("topology.dat") < 0)
    exit(-1);
}

int NeighborTable::ReadCostTable(const std::string &filename) {
  std::ifstream file(filename);
  if (!file) {
    std::cerr << "Failed to open " << filename << std::endl;
    return -1;
  }

  std::string local_ip_str;
  file >> local_ip_str;
  local_ip_ = inet_addr(local_ip_str.c_str());

  do {
    std::string from_ip_str;
    std::string to_ip_str;
    Cost cost;

    file >> from_ip_str;

    if (file.eof())
      break;

    file >> to_ip_str;
    file >> cost;

    if (file.eof())
      break;

    Ip from_ip = inet_addr(from_ip_str.c_str());
    Ip to_ip = inet_addr(to_ip_str.c_str());

    AddCost(from_ip, to_ip, cost);
    AddCost(to_ip, from_ip, cost);
  } while (true);

  file.close();
  return 0;
}

int NeighborTable::AddConnection(Ip ip, int conn) {
  if (neighbors_.find(ip) == neighbors_.end())
    return -1;

  neighbors_[ip].conn = conn;
  return 0;
}
