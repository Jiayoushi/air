#include "topology.h"

#include <string>
#include <unordered_map>

int Topology::Init() {
  if (ReadTopology("topology.dat") < 0) {
    std::cerr << "Read topology from topology.dat failed";
    return -1;
  }

  return 0;
}

int Topology::ReadTopology(const std::string &filename) {
  ifstream file(filename);
  if (!file) {
    std::cerr << "Failed to open " << filename << std::endl;
    return -1;
  }

  do {
    std::string from_node;
    std::string to_node;
    uint32_t cost;

    file >> from_node;
    file >> to_node;
    file >> cost; 
    
    costs_[from_node][to_node] = cost;
  } while (true);

  return 0;
}

Cost Topology::GetCost(ID from_id, ID to_id) {
  auto p = id_to_cost.find(from_id);
  if (p == id_to_cst.end())
    return 0;

  return *p; 
}
