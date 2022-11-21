#ifndef __CONN__
#define __CONN__

#include <pthread.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <stdio.h>
#include <stdint.h>
#include <unistd.h>
#include <netdb.h>
#include <sys/wait.h>
#include <sys/types.h>

struct connection_t {
	int id;
	pthread_t thread_id;
	struct sockaddr target_addr;
	socklen_t target_addr_len;
	pthread_cond_t cond;

	struct connection_t *next;
	struct connection_t *prev;
};

char connection_exist(struct connection_t *list, struct sockaddr *addr);
struct connection_t *find_connection(struct connection_t *list,
				     struct sockaddr *addr);
struct connection_t *add_connection(struct connection_t *list,
				    struct sockaddr *addr, socklen_t addr_len);
void delete_connection(struct connection_t *conn);
void free_connection_list(struct connection_t *last);

#endif // !__CONN__
