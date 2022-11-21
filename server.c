#include "conn.h"
#include "gbn.h"
#include "log.h"

#define BUFFER_SIZE 255
char INBUFFER[BUFFER_SIZE];
char OUTBUFFER[BUFFER_SIZE];

int sockfd = -1;

pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;

void *flush_buffer(void *args)
{
	struct connection_t *conn = (struct connection_t *)args;
	log_print(LOG, "Thread %d created, waiting for turn", conn->id);

	pthread_mutex_lock(&mutex);
	pthread_cond_wait(&conn->cond, &mutex);
	pthread_mutex_unlock(&mutex);

	log_print(LOG, "Thread %d sending the message in buffer");

	pthread_exit(0);
}

int main(int argc, char *argv[])
{
	char *server_port = 0;
	if (argc != 2) {
		log_print(ERROR, "Wrong argument count");
	} else {
		server_port = argv[1];
	}

	struct addrinfo hints;
	struct addrinfo *res;

	memset(&hints, 0, sizeof(hints));
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_DGRAM;
	hints.ai_flags = AI_PASSIVE;

	if (getaddrinfo(NULL, server_port, &hints, &res) == -1)
		log_print(ERROR, "Cannot get port %s info", server_port);

	int yes = 1;
	if ((sockfd = socket(res->ai_family, res->ai_socktype,
			     res->ai_protocol)) == -1)
		log_print(ERROR, "Cannot initialize socket");
	log_print(LOG, "Socket at port %s initialized", server_port);

	if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &yes,
		       sizeof(uint32_t)) == -1)
		log_print(ERROR, "Cannot configure socket");
	log_print(LOG, "Socket configured");

	if (bind(sockfd, res->ai_addr, res->ai_addrlen) == -1) {
		close(sockfd);
		log_print(ERROR, "Cannot bind to port");
	}
	freeaddrinfo(res);

	struct connection_t *conn_list_head = 0;
	struct connection_t *last_conn = 0;
	log_print(LOG, "Socket binded and listening for connections");
	while (1) {
		memset(INBUFFER, 0, sizeof(INBUFFER));

		struct sockaddr_storage client_addr;
		socklen_t client_addr_len = sizeof(client_addr);

		int bytes_transmitted = 0;
		if ((bytes_transmitted =
			     recvfrom(sockfd, INBUFFER, BUFFER_SIZE, 0,
				      (struct sockaddr *)&client_addr,
				      &client_addr_len)) == -1)
			log_print(ERROR, "Cannot read from socket");
		log_print(LOG, "%d bytes received", bytes_transmitted);

		struct connection_t *conn = 0;
		if (!(conn = find_connection(conn_list_head,
					     (struct sockaddr *)&client_addr))) {
			last_conn = conn = add_connection(
				last_conn, (struct sockaddr *)&client_addr,
				client_addr_len);
			log_print(LOG, "New connection added");

			if (!conn_list_head)
				conn_list_head = last_conn;

			int err = 0;
			if ((err = pthread_create(&last_conn->thread_id, 0,
						  &flush_buffer, last_conn)))
				log_print(ERROR,
					  "Cannot create thread, error no %d",
					  err);
		}

		/* TODO: Replace simple printing with GBN receiver */
		printf("Client %d sent:\n", conn->id);
		printf("%s", INBUFFER);
	}

	close(sockfd);
	free_connection_list(last_conn);

	return EXIT_SUCCESS;
}
