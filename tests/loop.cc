#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <cstring>
#include <fstream>
#include <string>
#include <iostream>

#include "client/srt_client.h"
#include "air/air.h"
#include "ip/ip.h"

int test();

int main(int argc, char *argv[]) {
  return test();
}

int test() {
  Init();

  SrtClientInit();

  while(1)
    sleep(10000);

  return 0;
}
