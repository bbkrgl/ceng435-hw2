/**
 * @file conn.h
 * @author Burak Köroğlu (e2448637@ceng.metu.edu.tr)
 * @brief Connection header including helper structs and function declarations.
 * 
 */

#ifndef __CONN__
#define __CONN__

#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdint.h>
#include <unistd.h>
#include <pthread.h>
#include <netdb.h>
#include <sys/socket.h>
#include <arpa/inet.h>

/** Set window size and char buffer size */
#define WINDOW_SIZE 16
#define PAYLOAD_SIZE 9

/**
 * @struct packet_data
 * 
 * @brief Packet structure. Has fields for marking ack, init and termination. seq_num is the sequence number.
 * 
 */
struct packet_data {
	/** Payload */
	char char_seq[PAYLOAD_SIZE];
	/** Set if the packet is ack */
	char is_ack;
	/** Set if the packet is initializing a connection */
	char init_conn;
	/** Set if the packet is terminating a connection */
	char terminate_conn;
	/** Sequence number
	 * In this implementation the sequence number directly shows the packet number,
	 * since the packet size does not change and would be incremented with the same number every time. */
	unsigned int seq_num;
};

/**
 * @struct packet_t
 * 
 * @brief A packet queue element
 * 
 */
struct packet_t {
	struct packet_data data;
	struct packet_t *next;
	struct packet_t *prev;
};

/**
 * @struct packet_queue
 * 
 * @brief Linked list queue for storing packets.
 * 
 * @details Outgoing packets are queued in using this structure.
 * This struct will be filled with packages that comes from the user input thread.
 * Ack deletes from the item to the end.
 * 
 */
struct packet_queue {
	/** Queue size */
	int size;
	/** Last sent sequence number. Used to determine the seq. number if the queue is empty. */
	unsigned int last_sent;
	/** Queue mutex */
	pthread_mutex_t mutex;
	/** First and last elements of the queue */
	struct packet_t *head;
	struct packet_t *tail;
};

/** These functions will be explained in conn.c */
struct packet_t *find_packet(struct packet_queue *queue, int seq_num);
struct packet_t *add_packet(struct packet_queue *queue,
			    struct packet_data *data);
int acknowledge_packet(struct packet_queue *queue, int seq_num,
		       pthread_mutex_t *mutex);
void free_queue(struct packet_queue *queue);

/**
 * @struct connection_t
 * 
 * @brief Linked list structure for connection. 
 * 
 * @details An item is created when a connection is initiated.
 * The server uses this structure to handle multiple connections.
 * 
 */
struct connection_t {
	/** Is the connection active */
	char is_active;
	/** Connection id*/
	int id;
	/** Sequence number that the connection expects */
	unsigned int exp_seq_num;
	/** Thread for the connection */
	pthread_t thread_id;
	/** Client address */
	struct sockaddr target_addr;
	socklen_t target_addr_len;
	/** Condition to signal a new item if the queue is empty */
	pthread_cond_t cond;
	/** Queue of the packets that will be sent with this connection */
	struct packet_queue queue;

	/** Timedout wait mutex and condition */
	pthread_mutex_t timeout_mutex;
	pthread_cond_t timeout_cond;

	/** Next and previous elements of the connection list */
	struct connection_t *next;
	struct connection_t *prev;
};

/** These functions will be explained in conn.c */
struct connection_t *find_connection(struct connection_t *list,
				     struct sockaddr *addr);
struct connection_t *add_connection(struct connection_t *list,
				    struct sockaddr *addr, socklen_t addr_len);
void delete_connection(struct connection_t *conn);
void free_connection_list(struct connection_t *last);

#endif // !__CONN__
