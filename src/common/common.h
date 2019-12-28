#ifndef AIR_COMMON_H_
#define AIR_COMMON_H_

#include <assert.h>
#include <iostream>
#include <cstddef>
#include <netinet/in.h>

#define DEBUG_ENABLE

#ifdef DEBUG_ENABLE
  #define SDEBUG std::cerr << "[TCP] "
  #define CDEBUG std::cerr << "[TCP] "
#else
  #define SDEBUG 0 && std::cout
  #define CDEBUG 0 && std::cout
#endif


typedef in_addr_t Ip;
typedef uint32_t Cost;


#endif
