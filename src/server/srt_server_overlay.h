#ifndef AIR_SRT_SERVER_OVERLAY_H_
#define AIR_SRT_SERVER_OVERLAY_H_

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <assert.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <time.h>
#include <iostream>


/*       
 *      my protocol
 * ------- Overlay ------
 *      tcp/ip
 */

int OverlayServerStart(const char *hostname, unsigned int hostport);

void OverlayServerStop(int conn);

#endif
