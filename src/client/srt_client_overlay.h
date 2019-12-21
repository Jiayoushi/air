#ifndef AIR_CLIENT_OVERLAY_H_
#define AIR_CLIENT_OVERLAY_H_

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <assert.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <netdb.h>
#include <stdio.h>
#include <time.h>
#include <iostream>

int OverlayClientStart(const char *hostname, unsigned int hostport);

void OverlayClientStop(int conn);



#endif
