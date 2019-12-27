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

  do {
    std::string from_ip_str;
    std::string to_ip_str;
    Cost cost;

    file >> from_ip_str;
    file >> to_ip_str;
    file >> cost;

    Ip from_ip = inet_addr(from_ip_str.c_str());
    Ip to_ip = inet_addr(to_ip_str.c_str());

    costs_[from_ip][to_ip] = cost;
    costs_[to_ip][from_ip] = cost;
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
