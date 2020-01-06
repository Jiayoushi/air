#ifndef AIR_COMMON_H_
#define AIR_COMMON_H_

#include <assert.h>
#include <iostream>
#include <arpa/inet.h>
#include <cstddef>
#include <netinet/in.h>
#include <limits>
#include <algorithm>

#define DEBUG_ENABLE

// TODO: all in one and add a lock
#ifdef DEBUG_ENABLE
  #define SDEBUG std::cerr << "[TCP] "
  #define CDEBUG std::cerr << "[TCP] "
#else
  #define SDEBUG 0 && std::cout
  #define CDEBUG 0 && std::cout
#endif

#define DEBUG_ROUTING

typedef in_addr_t Ip;
const Ip kInvalidIp = 0;

typedef uint32_t Cost;
const Cost kInvalidCost = std::numeric_limits<Cost>::max();

inline char *IpStr(Ip ip) {
  struct in_addr a;
  a.s_addr = ip;

  return inet_ntoa(a);
}

#endif
