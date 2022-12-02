/**
 * @file client.c
 * @author Burak Köroğlu (e2448637@ceng.metu.edu.tr)
 * @brief Client implementation
 * 
 */

#include "conn.h"
#include "log.h"

/** Mutex and conditions for sender, receiver and input thread synchronization */
pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t cond = PTHREAD_COND_INITIALIZER;

/** Mutex and conditions for sender, receiver and input thread synchronization */
pthread_mutex_t timeout_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t timeout_cond = PTHREAD_COND_INITIALIZER;

/** Packet queue, packets that will be sent. Detailed explanation of queueing is in conn.h */
struct packet_queue queue;

/** Global variables for socket, connection control and termination */
int sockfd = -1;
int terminate = 0;
char connection_exists = 0;

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

	pthread_mutex_lock(&timeout_mutex);
	int res = pthread_cond_timedwait(&timeout_cond, &timeout_mutex, &ts);
	if (res == ETIMEDOUT)
		log_print(LOG, "Timed out, sending packages again");
	else if (res)
		log_print(ERROR, "Timed wait error");
	pthread_mutex_unlock(&timeout_mutex);
}

/**
 * @brief Thread function for sending packets.
 * 
 * @param args 
 * @return void* 
 */
void *send_packets(void *args)
{
	log_print(LOG, "Send thread created, waiting for turn");

	struct addrinfo *target = *((struct addrinfo **)args);

	/** Run until the program termination */
	while (terminate < 2) {
		while (queue.size) {
			int i = 0;
			/** Get the lock to prevent the synchronization issues with the receiver thread acknowledge */
			pthread_mutex_lock(&mutex);
			/** Iterate over the queue until the end or window size */
			for (struct packet_t *head = queue.head;
			     head && i < WINDOW_SIZE; head = head->next, i++) {
				queue.last_sent = head->data.id;
				log_print(LOG, "Sending the packet %d in queue",
					  head->data.id);
				/** Send packets to server. If there is an error, exit with error message */
				int bytes_sent = 0;
				if ((bytes_sent =
					     sendto(sockfd, &head->data,
						    sizeof(struct packet_data),
						    0, target->ai_addr,
						    target->ai_addrlen)) == -1)
					log_print(ERROR, "Cannot send packet");
				log_print(LOG, "Sent %d bytes to server",
					  bytes_sent);
			}
			pthread_mutex_unlock(&mutex);

			/** Wait 100ms for ack, if no ack then resend remaining packets */
			wait_or_signal(100);
		}

		/** If the queue becomes empty, wait packets to be added to the queue */
		pthread_mutex_lock(&mutex);
		pthread_cond_wait(&cond, &mutex);
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
	char init = 0;

	char *line = 0;
	size_t line_len = 0;
	/** Run until the program termination */
	while (terminate < 2) {
		int num_read = getline(&line, &line_len, stdin);
		/** Count the number of empty lines */
		if (!num_read || !strcmp(line, "\n")) {
			terminate++;
		} else {
			terminate = 0;
			struct packet_data data;
			memcpy(data.char_seq, line, line_len);
			add_packet(&queue, &data);
			log_print(LOG, "Adding %s to data", line);
			if (connection_exists &&
			    (queue.size == 1 || (!init && queue.size >= 1))) {
				/** Send packets arrived signal to the send_packets thread */
				pthread_mutex_lock(&mutex);
				pthread_cond_signal(&cond);
				pthread_mutex_unlock(&mutex);
				init = 1;
			}
		}
	}

	terminate = 2;
	free(line);
	pthread_exit(EXIT_SUCCESS);
}

int main(int argc, char *argv[])
{
	/** Get arguments */
	char *server_ip = 0;
	char *server_port = 0;
	if (argc != 3) {
		log_print(ERROR, "Wrong argument count");
	} else {
		server_ip = argv[1];
		server_port = argv[2];
	}

	/** Socket init-configuration start */

	struct addrinfo hints;
	struct addrinfo *res;

	memset(&hints, 0, sizeof(hints));
	hints.ai_family = AF_INET;
	hints.ai_socktype = SOCK_DGRAM;

	if (getaddrinfo(server_ip, server_port, &hints, &res) == -1)
		log_print(ERROR, "Cannot get %s:%s info", server_ip,
			  server_port);

	int yes = 1;
	if ((sockfd = socket(res->ai_family, res->ai_socktype,
			     res->ai_protocol)) == -1)
		log_print(ERROR, "Cannot initialize socket");
	log_print(LOG, "Socket at %s:%s initialized", server_ip, server_port);

	if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int)) ==
	    -1)
		log_print(ERROR, "Cannot configure socket");
	log_print(LOG, "Socket configured");

	/** Socket init-configuration end */

	/** Initialization packet. This packet is added to the queue */
	struct packet_data init;
	init.init_conn = 1;
	init.is_ack = 0;
	init.id = 0;
	add_packet(&queue, &init);

	/** Create the input and send threads, main thread will listen for packets */
	int err = 0;
	pthread_t line_read_thread, send_thread;
	if ((err = pthread_create(&send_thread, 0, &send_packets, &res)))
		log_print(ERROR, "Cannot create thread, error no %s",
			  strerror(err));
	if ((err = pthread_create(&line_read_thread, 0, &read_input, 0)))
		log_print(ERROR, "Cannot create thread, error no %s",
			  strerror(err));

	unsigned int exp_seq_num = 1;
	struct packet_data packet;
	/** Run until termination */
	while (terminate < 2) {
		/** Listen for packets */
		struct sockaddr_storage server_addr;
		socklen_t server_addr_len = sizeof(server_addr);

		int bytes_transmitted = 0;
		if ((bytes_transmitted = recvfrom(
			     sockfd, &packet, sizeof(struct packet_data), 0,
			     (struct sockaddr *)&server_addr,
			     &server_addr_len)) == -1)
			log_print(ERROR, "Cannot read from socket");
		log_print(LOG, "%d bytes received", bytes_transmitted);

		/** Mark the connection as established with the response from server */
		if (packet.init_conn) {
			log_print(LOG, "Connection established with server");
			connection_exists = 1;
		}

		/** Ack function explanation in conn.c */
		if (packet.is_ack) {
			log_print(LOG, "Received ACK for packet %d", packet.id);
			int res = acknowledge_packet(&queue, packet.id - 1,
						     &mutex);
			if (res != -1) {
				/** Signal the next batch of packets in the queue to be sent */
				pthread_mutex_lock(&timeout_mutex);
				pthread_cond_signal(&timeout_cond);
				pthread_mutex_unlock(&timeout_mutex);
			}
			/** Since we just got an ack, continue */
			continue;
		}

		/** If the packet is not an ack and has the expected sequence number, print it */
		if (exp_seq_num == packet.id) {
			if (!packet.init_conn) {
				printf("%s", packet.char_seq);
			}
			exp_seq_num++;
		}

		/** Send cumulative ack for the packet */
		if (exp_seq_num >= packet.id) {
			struct packet_data ack;
			ack.is_ack = 1;
			ack.id = exp_seq_num; /** Cumulative ack */
			ack.init_conn = packet.init_conn;
			int ack_bytes = 0;
			if ((ack_bytes = sendto(sockfd, &ack,
						sizeof(struct packet_data), 0,
						(struct sockaddr *)&server_addr,
						server_addr_len)) == -1)
				log_print(ERROR, "Cannot send packet");
			log_print(LOG, "Sent ACK for packet %d", ack.id);
		} else {
			log_print(LOG, "Expected id %d, got %d", exp_seq_num,
				  packet.id);
		}
	}

	return EXIT_SUCCESS;
}
