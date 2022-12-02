#include "conn.h"
#include "log.h"

int sockfd = -1;
int terminate = 0;

pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
struct connection_t *curr_conn = 0;

void *send_packets(void *args)
{
	struct connection_t *conn = (struct connection_t *)args;

	log_print(LOG, "Connection thread %d created, waiting for turn",
		  conn->id);

	while (terminate < 2) {
		while (conn->queue.size) {
			int i = 0;
			pthread_mutex_lock(&mutex);
			for (struct packet_t *head = conn->queue.head;
			     head && i < WINDOW_SIZE; head = head->next, i++) {
				conn->queue.last_sent = head->data.id;
				log_print(
					LOG,
					"Thread %d sending the packet %d in queue",
					conn->id, head->data.id);
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
			usleep(100000);
		}

		pthread_mutex_lock(&mutex);
		pthread_cond_wait(&conn->cond, &mutex);
		pthread_mutex_unlock(&mutex);
	}

	pthread_exit(EXIT_SUCCESS);
}

void *read_input(void *args)
{
	char *line = 0;
	size_t line_len = 0;
	while (terminate < 2) {
		int num_read = getline(&line, &line_len, stdin);
		if (!num_read || !strcmp(line, "\n")) {
			terminate++;
		} else {
			if (!curr_conn) {
				log_print(LOG, "No connections exists");
				continue;
			}

			struct packet_data data;
			memcpy(data.char_seq, line, line_len);
			add_packet(&curr_conn->queue, &data);
			if (curr_conn->queue.size == 1) {
				pthread_mutex_lock(&mutex);
				pthread_cond_signal(&curr_conn->cond);
				pthread_mutex_unlock(&mutex);
			}

			terminate = 0;
		}
	}

	terminate = 2;
	free(line);
	pthread_exit(EXIT_SUCCESS);
}

int main(int argc, char *argv[])
{
	char *server_port = 0;
	if (argc != 2)
		log_print(ERROR, "Wrong argument count");
	else
		server_port = argv[1];

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

	int err = 0;
	pthread_t line_read_thread;
	if ((err = pthread_create(&line_read_thread, 0, &read_input, 0)))
		log_print(ERROR, "Cannot create thread, error no %s",
			  strerror(err));

	struct connection_t *conn_list_head = 0;
	struct connection_t *last_conn = 0;
	struct packet_data packet;
	while (terminate < 2) {
		struct sockaddr_storage client_addr;
		socklen_t client_addr_len = sizeof(client_addr);

		int bytes_transmitted = 0;
		if ((bytes_transmitted = recvfrom(
			     sockfd, &packet, sizeof(struct packet_data), 0,
			     (struct sockaddr *)&client_addr,
			     &client_addr_len)) == -1)
			log_print(ERROR, "Cannot read from socket");
		log_print(LOG, "%d bytes received", bytes_transmitted);

		struct connection_t *conn = 0;
		if (!(conn = find_connection(conn_list_head,
					     (struct sockaddr *)&client_addr))) {
			if (!packet.init_conn) {
				log_print(
					LOG,
					"Packet from unknown origin, ignoring");
				continue;
			}

			last_conn = conn = add_connection(
				last_conn, (struct sockaddr *)&client_addr,
				client_addr_len);
			conn->exp_seq_num++;
			log_print(LOG, "New connection added");

			if (!conn_list_head)
				conn_list_head = curr_conn = last_conn;

			if ((err = pthread_create(&last_conn->thread_id, 0,
						  &send_packets, last_conn)))
				log_print(ERROR,
					  "Cannot create thread, error no %s",
					  strerror(err));
		}

		if (packet.is_ack) {
			log_print(LOG, "Received ACK for packet %d", packet.id);
			acknowledge_packet(&conn->queue, packet.id - 1, &mutex);
			continue;
		}

		if (conn->exp_seq_num == packet.id) {
			if (!packet.init_conn) {
				printf("Client %d sent:\n", conn->id);
				printf("%s", packet.char_seq);
			}
			conn->exp_seq_num++;
		}

		if (conn->exp_seq_num > packet.id) {
			struct packet_data ack;
			ack.is_ack = 1;
			ack.id = conn->exp_seq_num;
			ack.init_conn = packet.init_conn;
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

	close(sockfd);
	free_connection_list(last_conn);

	return EXIT_SUCCESS;
}
