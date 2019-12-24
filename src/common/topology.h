#ifndef AIR_TOPOLOGY_H_
#define AIR_TOPOLOGY_H_

typedef ID unsigned long;
typedef Cost uint32_t;

class Topology {
 private:
  std::unordered_map<ID, std::unordered_map<ID, Cost>> costs_;

 public:

};
