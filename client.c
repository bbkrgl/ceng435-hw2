#include "conn.h"
#include "log.h"

pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t cond = PTHREAD_COND_INITIALIZER;
struct packet_queue queue;

int sockfd = -1;
int terminate = 0;
char connection_exists = 0;

void *send_packets(void *args)
{
	log_print(LOG, "Send thread created, waiting for turn");

	struct addrinfo *target = *((struct addrinfo **)args);

	while (terminate < 2) {
		while (queue.size) {
			int i = 0;
			for (struct packet_t *head = queue.head;
			     head && i < WINDOW_SIZE; head = head->next, i++) {
				if (head->acknowledged)
					continue;
				log_print(LOG, "Sending the packet %d in queue",
					  head->data.id);
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
			usleep(100000);
		}

		LOCK(pthread_cond_wait(&cond, &mutex))
	}

	pthread_exit(EXIT_SUCCESS);
}

void *read_input(void *args)
{
	char init = 0;

	char *line = 0;
	size_t line_len = 0;
	while (terminate < 2) {
		int num_read = getline(&line, &line_len, stdin);
		if (!num_read || !strcmp(line, "\n")) {
			terminate++;
		} else {
			terminate = 0;
			struct packet_data data;
			memcpy(data.char_seq, line, line_len);
			add_packet(&queue, &data);
			if (connection_exists &&
			    (queue.size == 1 || (!init && queue.size >= 1))) {
				LOCK(pthread_cond_signal(&cond))
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
	char *server_ip = 0;
	char *server_port = 0;
	if (argc != 3) {
		log_print(ERROR, "Wrong argument count");
	} else {
		server_ip = argv[1];
		server_port = argv[2];
	}

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

	struct packet_data init;
	init.init_conn = 1;
	init.is_ack = 0;
	init.id = 0;
	add_packet(&queue, &init);

	int err = 0;
	pthread_t line_read_thread, send_thread;
	if ((err = pthread_create(&send_thread, 0, &send_packets, &res)))
		log_print(ERROR, "Cannot create thread, error no %s",
			  strerror(err));
	if ((err = pthread_create(&line_read_thread, 0, &read_input, 0)))
		log_print(ERROR, "Cannot create thread, error no %s",
			  strerror(err));

	unsigned int exp_seq_num = 0;
	struct packet_data packet;
	while (terminate < 2) {
		struct sockaddr_storage server_addr;
		socklen_t server_addr_len = sizeof(server_addr);

		int bytes_transmitted = 0;
		if ((bytes_transmitted = recvfrom(
			     sockfd, &packet, sizeof(struct packet_data), 0,
			     (struct sockaddr *)&server_addr,
			     &server_addr_len)) == -1)
			log_print(ERROR, "Cannot read from socket");
		log_print(LOG, "%d bytes received", bytes_transmitted);

		if (packet.init_conn) {
			log_print(LOG, "Connection established with server");
			connection_exists = 1;
		}

		if (packet.is_ack) {
			log_print(LOG, "Received ACK for packet %d", packet.id);
			acknowledge_packet(&queue, packet.id);
			continue;
		}

		if (exp_seq_num == packet.id)
			printf("%s", packet.char_seq);
		else
			log_print(LOG, "Expected id %d, got %d",
				  exp_seq_num, packet.id);

		// TODO: Filter packets that have been acked before
		struct packet_data ack;
		ack.is_ack = 1;
		ack.id = packet.id;
		int ack_bytes = 0;
		if ((ack_bytes = sendto(sockfd, &ack,
					sizeof(struct packet_data), 0,
					(struct sockaddr *)&server_addr,
					server_addr_len) == -1))
			log_print(ERROR, "Cannot send packet");
		log_print(LOG, "Sent ACK for packet %d", ack.id);
	}

	return EXIT_SUCCESS;
}
