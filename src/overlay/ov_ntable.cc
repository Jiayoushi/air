#include <ov_ntable.h>

#include <fstream>

#include <ip.h>

NeighborTable::NeighborTable() {}

void NeighborTable::Init() {
  local_ip_ = GetLocalIp();

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

    if (from_ip_str == "#")
      continue;

    if (file.eof())
      break;

    file >> to_ip_str;
    file >> cost;

    if (file.eof())
      break;

    Ip from_ip = HostnameToIp(from_ip_str.c_str());
    Ip to_ip = HostnameToIp(to_ip_str.c_str());

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

/* Make sure from_ip is inside the table, otherwise crash directly */
Cost NeighborTable::GetCost(Ip from_ip, Ip to_ip) const {
  if (from_ip == GetLocalIp() && to_ip == GetLocalIp())
    return 0;

  auto p = neighbors_.at(from_ip).costs.find(to_ip);
  if (p != neighbors_.at(from_ip).costs.end())
    return p->second;
  else
    return std::numeric_limits<uint32_t>::max();
}
