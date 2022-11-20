#ifndef __GBN__
#define __GBN__

#include <errno.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <netdb.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <pthread.h>

/*
* Implementation of Go-Back-N protocol.
*/

void sender(uint16_t src_port, uint16_t dest_port, void* data);
void* receiver(uint16_t src_port, uint16_t dest_port);

#endif // !__GBN__
