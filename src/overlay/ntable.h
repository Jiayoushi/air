#ifndef AIR_NTABLE_H_
#define AIR_NTABLE_H_

#include <arpa/inet.h>

class NeighborTable {
 private:
  struct Neighbor {
    in_addr_t ip;
    int conn;
  };

 public:
  NeighborTable();

};



#endif
