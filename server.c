#include "gbn.h"
#include "log.h"

#define BUFFER_SIZE 255
char INBUFFER[BUFFER_SIZE];
char OUTBUFFER[BUFFER_SIZE];

int connection_fd = 0;

struct connection_t {
	int id;
	pthread_t thread_id;
	struct sockaddr target_addr;
	socklen_t target_addr_len;
	pthread_mutex_t mutex;

	struct connection_t *next;
	struct connection_t *prev;
};

char connection_exist(struct connection_t *list, struct sockaddr *addr)
{
	while (list) {
		if (!strcmp(list->target_addr.sa_data, addr->sa_data))
			return 1;
		list = list->next;
	}

	return 0;
}

struct connection_t *add_connection(struct connection_t *list,
				    struct sockaddr *addr, socklen_t addr_len)
{
	if (!list) {
		struct connection_t *new_elem =
			malloc(sizeof(struct connection_t));
		new_elem->id = 0;
		new_elem->target_addr = *addr;
		new_elem->target_addr_len = addr_len;
		pthread_mutex_init(&new_elem->mutex, NULL);
		pthread_mutex_lock(&new_elem->mutex);
		new_elem->next = new_elem->prev = NULL;

		return new_elem;
	}

	while (list->next)
		list = list->next;

	struct connection_t *new_elem = malloc(sizeof(struct connection_t));
	new_elem->id = list->id + 1;
	new_elem->target_addr = *addr;
	pthread_mutex_init(&new_elem->mutex, NULL);
	pthread_mutex_lock(&new_elem->mutex);
	new_elem->prev = list;
	new_elem->next = NULL;

	list->next = new_elem;
	return new_elem;
}

void delete_connection(struct connection_t *conn)
{
	conn->prev->next = conn->next;
	conn->next->prev = conn->prev;

	free(conn);
}

void free_connection_list(struct connection_t *last)
{
	struct connection_t *temp;
	while (last) {
		temp = last;
		last = last->prev;
		free(temp);
	}
}

void *flush_buffer(void *args)
{
	struct connection_t *conn = (struct connection_t *)args;
	log_print(LOG, "Thread %d created, waiting for turn", conn->id);

	pthread_mutex_lock(&conn->mutex);
	log_print(LOG, "Thread %d got mutex, sending the message in buffer");
	pthread_mutex_unlock(&conn->mutex);

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
	if ((connection_fd = socket(res->ai_family, res->ai_socktype,
				    res->ai_protocol)) == -1)
		log_print(ERROR, "Cannot initialize socket");
	log_print(LOG, "Socket at port %s initialized", server_port);

	if (setsockopt(connection_fd, SOL_SOCKET, SO_REUSEADDR, &yes,
		       sizeof(uint32_t)) == -1)
		log_print(ERROR, "Cannot configure socket");
	log_print(LOG, "Socket configured");

	if (bind(connection_fd, res->ai_addr, res->ai_addrlen) == -1) {
		close(connection_fd);
		log_print(ERROR, "Cannot bind to port");
	}
	freeaddrinfo(res);

	struct connection_t *conn_list_head = 0;
	struct connection_t *last_conn = 0;
	log_print(LOG, "Socket binded and listening for connections");
	while (1) {
		memset(&INBUFFER, 0, sizeof(INBUFFER));

		struct sockaddr_storage client_addr;
		socklen_t client_addr_len = sizeof(client_addr);

		int bytes_transmitted = 0;
		if ((bytes_transmitted =
			     recvfrom(connection_fd, INBUFFER, BUFFER_SIZE, 0,
				      (struct sockaddr *)&client_addr,
				      &client_addr_len)) == -1)
			log_print(ERROR, "Cannot read from socket");
		log_print(LOG, "%d bytes received", bytes_transmitted);

		if (!connection_exist(conn_list_head,
				      (struct sockaddr *)&client_addr)) {
			last_conn = add_connection(
				conn_list_head, (struct sockaddr *)&client_addr,
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
		printf("Client %d sent:\n", last_conn->id);
		printf("%s", INBUFFER);
	}

	close(connection_fd);
	free_connection_list(last_conn);

	return EXIT_SUCCESS;
}
