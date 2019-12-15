#ifndef AIR_COMMON_H_
#define AIR_COMMON_H_

#include <iostream>
#include <assert.h>

#define DEBUG_ENABLE

#ifdef DEBUG_ENABLE
  #define SDEBUG std::cerr << "SERVER: "
  #define CDEBUG std::cerr << "CLIENT: "
#else
  #define SDEBUG 0 && std::cout
  #define CDEBUG 0 && std::cout
#endif

#endif
