#ifndef __GBN__
#define __GBN__

#include "conn.h"

/*
* Implementation of Go-Back-N protocol.
*/

void sender(uint16_t src_port, uint16_t dest_port, void* data);
void* receiver(uint16_t src_port, uint16_t dest_port);

#endif // !__GBN__
