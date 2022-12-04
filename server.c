/**
 * @file server.c
 * @author Burak Köroğlu (e2448637@ceng.metu.edu.tr)
 * @brief Server implementation
 * 
 */

#include "conn.h"
#include "log.h"

/** Global variables for socket and active connections */
int sockfd = -1;
int active_conn = 0;
char terminate = 0;

/** Global mutex and current connection. Connection struct explanation in conn.h */
pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
struct connection_t *curr_conn = 0;

/**
 * @brief Wait for signal or timeout
 * 
 * @param time 
 */
void wait_or_signal(long time)
{
	struct timespec ts;
	clock_gettime(CLOCK_REALTIME, &ts);

	ts.tv_sec += time / 1000;
	long ns_left = (time % 1000) * 1000000;
	long left_limit = 1000000000 - ts.tv_nsec;

	if (ns_left > left_limit) {
		ts.tv_sec++;
		ts.tv_nsec = ns_left - left_limit;
	} else {
		ts.tv_nsec += ns_left;
	}

	pthread_mutex_lock(&curr_conn->timeout_mutex);
	int res = pthread_cond_timedwait(&curr_conn->timeout_cond,
					 &curr_conn->timeout_mutex, &ts);
	if (res == ETIMEDOUT)
		log_print(LOG, "Timed out, sending packages again");
	else if (res)
		log_print(ERROR, "Timed wait error");
	pthread_mutex_unlock(&curr_conn->timeout_mutex);
}

/**
 * @brief Thread function for sending packets.
 * 
 * @param args 
 * @return void* 
 */
void *send_packets(void *args)
{
	/**  */
	struct connection_t *conn = (struct connection_t *)args;

	log_print(LOG, "Connection thread %d created, waiting for turn",
		  conn->id);

	/** Run until the program termination */
	while (1) {
		while (conn->queue.size) {
			int i = 0;
			/** Get the lock to prevent the synchronization issues with the receiver thread acknowledge */
			pthread_mutex_lock(&mutex);
			/** Iterate over the queue until the end or window size */
			for (struct packet_t *head = conn->queue.head;
			     head && i < WINDOW_SIZE; head = head->next, i++) {
				conn->queue.last_sent = head->data.id;
				log_print(
					LOG,
					"Thread %d sending the packet %d in queue",
					conn->id, head->data.id);
				/** Send packets to server. If there is an error, exit with error message */
				int bytes_sent = 0;
				if ((bytes_sent = sendto(
					     sockfd, &head->data,
					     sizeof(struct packet_data), 0,
					     &conn->target_addr,
					     conn->target_addr_len)) == -1)
					log_print(ERROR, "Cannot send packet");
				log_print(LOG, "Sent %d bytes to client %d",
					  bytes_sent, conn->id);
			}
			pthread_mutex_unlock(&mutex);

			/** Wait 100ms for ack, if no ack then resend remaining packets */
			wait_or_signal(100);
		}

		/** If the queue becomes empty, wait packets to be added to the queue */
		pthread_mutex_lock(&mutex);
		pthread_cond_wait(&conn->cond, &mutex);
		pthread_mutex_unlock(&mutex);
	}

	pthread_exit(EXIT_SUCCESS);
}

/**
 * @brief Thread for getting user input. Adds packets to the queue.
 * 
 * @param args 
 * @return void* 
 */
void *read_input(void *args)
{
	char terminate_read = 0;
	char *line = 0;
	size_t line_len = 0;
	/** Run until the program termination */
	while (terminate_read < 2 && !terminate) {
		int num_read = getline(&line, &line_len, stdin);
		/** Count the number of empty lines */
		if (!num_read || !strcmp(line, "\n")) {
			terminate_read++;
		} else {
			terminate_read = 0;

			if (!curr_conn) {
				/** If no connection, ignore */
				log_print(LOG, "No connections exists");
				continue;
			}

			line_len = strlen(line);
			for (int i = 0; i < line_len; i += BUFFER_SIZE) {
				struct packet_data data;
				memset(data.char_seq, '\0', BUFFER_SIZE);
				if (i + BUFFER_SIZE < line_len)
					strncpy(data.char_seq, line + i,
						BUFFER_SIZE);
				else
					strncpy(data.char_seq, line + i,
						line_len - i);
				add_packet(&curr_conn->queue, &data);
				log_print(LOG, "Adding %s to data", line + i);
			}
			if (curr_conn->queue.size >= 1) {
				/** Send packets arrived signal to the send_packets thread */
				pthread_mutex_lock(&mutex);
				pthread_cond_signal(&curr_conn->cond);
				pthread_mutex_unlock(&mutex);
			}
		}
		free(line);
		line = 0;
		line_len = 0;
	}

	log_print(LOG, "Starting termination");
	/** For all connections, add termination packet to all queues */
	struct connection_t *conn = curr_conn;
	while (conn) {
		struct packet_data term;
		term.terminate_conn = 1;
		free_queue(&conn->queue); /** Flush remaining elements */
		add_packet(&conn->queue, &term);

		/** Send signal again if sending thread is waiting */
		pthread_mutex_lock(&mutex);
		pthread_cond_signal(&conn->cond);
		pthread_mutex_unlock(&mutex);

		conn = conn->next; /** For connections after the current connection */
	}
	conn = curr_conn;
	while (conn) {
		struct packet_data term;
		term.terminate_conn = 1;
		free_queue(&conn->queue); /** Flush remaining elements */
		add_packet(&conn->queue, &term);

		/** Send signal again if sending thread is waiting */
		pthread_mutex_lock(&mutex);
		pthread_cond_signal(&conn->cond);
		pthread_mutex_unlock(&mutex);

		conn = conn->prev; /** For connections before the current connection */
	}

	pthread_exit(EXIT_SUCCESS);
}

int main(int argc, char *argv[])
{
	/** Get arguments */
	char *server_port = 0;
	if (argc != 2)
		log_print(ERROR, "Wrong argument count");
	else
		server_port = argv[1];

	/** Socket init-configuration start */

	struct addrinfo hints;
	struct addrinfo *res;

	memset(&hints, 0, sizeof(hints));
	hints.ai_family = AF_INET;
	hints.ai_socktype = SOCK_DGRAM;
	hints.ai_flags = AI_PASSIVE;

	if (getaddrinfo(NULL, server_port, &hints, &res) == -1)
		log_print(ERROR, "Cannot get port %s info", server_port);

	int yes = 1;
	if ((sockfd = socket(res->ai_family, res->ai_socktype,
			     res->ai_protocol)) == -1)
		log_print(ERROR, "Cannot initialize socket");
	log_print(LOG, "Socket at port %s initialized", server_port);

	if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int)) ==
	    -1)
		log_print(ERROR, "Cannot configure socket");
	log_print(LOG, "Socket configured");

	if (bind(sockfd, res->ai_addr, res->ai_addrlen) == -1) {
		close(sockfd);
		log_print(ERROR, "Cannot bind to port");
	}
	freeaddrinfo(res);
	log_print(LOG, "Ready for connections");

	/** Socket init-configuration end */

	/** Create the input thread, main thread will listen for packets */
	int err = 0;
	pthread_t line_read_thread;
	if ((err = pthread_create(&line_read_thread, 0, &read_input, 0)))
		log_print(ERROR, "Cannot create thread, error no %s",
			  strerror(err));

	/** Initialize the connection list and helper variables */
	struct connection_t *conn_list_head = 0;
	struct connection_t *last_conn = 0;
	struct packet_data packet;
	/** Run until termination */
	char first = 1;
	while (active_conn || first) {
		struct sockaddr_storage client_addr;
		socklen_t client_addr_len = sizeof(client_addr);

		/** If in termination sequence, set the socket timeout to 1s */
		if (terminate) {
			struct timeval tv;
			tv.tv_sec = 1;
			tv.tv_usec = 0;
			if (setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, &tv,
				       sizeof(tv)) == -1)
				log_print(ERROR, "Cannot set timeout");
		}

		/** Wait for packets */
		int bytes_transmitted = 0;
		if ((bytes_transmitted = recvfrom(
			     sockfd, &packet, sizeof(struct packet_data), 0,
			     (struct sockaddr *)&client_addr,
			     &client_addr_len)) == -1 &&
		    !terminate) {
			log_print(ERROR, "Cannot read from socket");
		} else if (bytes_transmitted == -1) {
			/** No packets arrived since the last 1s, assuming the ack is arrived */
			log_print(LOG, "No connections left, exiting");
			exit(0);
		}
		log_print(LOG, "%d bytes received", bytes_transmitted);

		/** If a packet is received, look up for its source in the connection list. */
		struct connection_t *conn = 0;
		if (!(conn = find_connection(conn_list_head,
					     (struct sockaddr *)&client_addr))) {
			if (!packet.init_conn) {
				/** If the source is unknown and not initiating, ignore */
				log_print(
					LOG,
					"Packet from unknown origin, ignoring");
				continue;
			} else if (terminate) {
				log_print(LOG,
					  "In termination sequence, ignoring");
				continue;
			}
			first = 0;

			/** Initialize the connection by adding a new entry to the connection list */
			last_conn = conn = add_connection(
				last_conn, (struct sockaddr *)&client_addr,
				client_addr_len);
			conn->exp_seq_num++;
			active_conn++;
			log_print(LOG,
				  "New connection added, total %d connections",
				  active_conn);

			if (!conn_list_head)
				conn_list_head = curr_conn = last_conn;

			/** Create the thread for that sends packets to this client */
			if ((err = pthread_create(&last_conn->thread_id, 0,
						  &send_packets, last_conn)))
				log_print(ERROR,
					  "Cannot create thread, error no %s",
					  strerror(err));
		}

		/** Ack function explanation in conn.c */
		if (packet.is_ack) {
			log_print(LOG, "Received ACK for packet %d", packet.id);
			if (packet.terminate_conn && conn->is_active) {
				conn->is_active = 0;
				active_conn--;
				continue;
			}

			int res = acknowledge_packet(&conn->queue,
						     packet.id - 1, &mutex);
			if (res != -1) {
				/** Signal the next batch of packets in the queue to be sent */
				pthread_mutex_lock(&conn->timeout_mutex);
				pthread_cond_signal(&conn->timeout_cond);
				pthread_mutex_unlock(&conn->timeout_mutex);
			}
			/** Since we just got an ack, continue */
			continue;
		}

		/** If the packet is not an ack and has the expected sequence number, print it */
		if (conn->exp_seq_num == packet.id) {
			if (!packet.init_conn)
				printf("%s", packet.char_seq);
			conn->exp_seq_num++;
		}

		/** Send cumulative ack for the packet */
		if (conn->exp_seq_num > packet.id) {
			struct packet_data ack;
			ack.is_ack = 1;
			ack.id = conn->exp_seq_num; /** Cumulative ack */
			ack.init_conn = packet.init_conn;
			ack.terminate_conn = packet.terminate_conn;
			if (packet.terminate_conn) {
				/** Do not exit right away if the last connection, wait for 1s timeout */
				if (active_conn != 1)
					active_conn--;
				conn->is_active = 0;
				/** Enter the termination sequence if the last connection */
				if (active_conn == 1)
					terminate = 1;
			}
			int ack_bytes = 0;
			if ((ack_bytes = sendto(sockfd, &ack,
						sizeof(struct packet_data), 0,
						&conn->target_addr,
						conn->target_addr_len)) == -1)
				log_print(ERROR, "Cannot send packet");
			log_print(LOG, "Sent ACK for packet %d", ack.id);
		} else {
			log_print(LOG, "Expected id %d, got %d",
				  conn->exp_seq_num, packet.id);
		}
	}

	log_print(LOG, "No connections left, exiting");
	exit(0);
}
