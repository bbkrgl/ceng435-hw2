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

#define LOCK(...)                   \
	pthread_mutex_lock(&mutex); \
	__VA_ARGS__;                \
	pthread_mutex_unlock(&mutex);

#define WINDOW_SIZE 16
#define BUFFER_SIZE 256

struct packet_data {
	char is_ack;
	char init_conn;
	unsigned int id;
	char char_seq[BUFFER_SIZE];
};

struct packet_t {
	char acknowledged;
	struct packet_data data;
	struct packet_t *next;
	struct packet_t *prev;
};

struct packet_queue {
	int size;
	unsigned int last_sent;
	struct packet_t *head;
	struct packet_t *tail;
};

struct packet_t *find_packet(struct packet_queue *queue, int id);
struct packet_t *add_packet(struct packet_queue *queue,
			    struct packet_data *data);
void acknowledge_packet(struct packet_queue *queue, int id);
void free_queue(struct packet_queue *queue);

struct connection_t {
	int id;
	unsigned int exp_seq_num;
	pthread_t thread_id;
	struct sockaddr target_addr;
	socklen_t target_addr_len;
	pthread_cond_t cond;
	struct packet_queue queue;

	struct connection_t *next;
	struct connection_t *prev;
};

struct connection_t *find_connection(struct connection_t *list,
				     struct sockaddr *addr);
struct connection_t *add_connection(struct connection_t *list,
				    struct sockaddr *addr, socklen_t addr_len);
void delete_connection(struct connection_t *conn);
void free_connection_list(struct connection_t *last);

#endif // !__CONN__
