/**
 * @file conn.h
 * @author Burak Köroğlu (e2448637@ceng.metu.edu.tr)
 * @brief Connection header including helper structs and function declarations.
 * 
 */

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
#include <fcntl.h>

/** Set window size and char buffer size */
#define WINDOW_SIZE 16
#define BUFFER_SIZE 9

/**
 * @struct packet_data
 * 
 * @brief Packet structure. Has fields for marking ack and init. id is the sequence number.
 * 
 */
struct packet_data {
	char char_seq[BUFFER_SIZE];
	unsigned int id;
	char is_ack;
	char init_conn;
	char terminate_conn;
} __attribute__((
	packed)); /** Disable alignment to match the 16 byte requirement */

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
struct packet_t *find_packet(struct packet_queue *queue, int id);
struct packet_t *add_packet(struct packet_queue *queue,
			    struct packet_data *data);
int acknowledge_packet(struct packet_queue *queue, int id,
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
