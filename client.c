#include "gbn.h"
#include "log.h"

#define BUFFER_SIZE 255
char INBUFFER[BUFFER_SIZE];

int sockfd = -1;
int terminate = 0;

void *read_input(void *args)
{
	struct addrinfo *target = (struct addrinfo *)args;

	char *line = 0;
	size_t line_len = 0;
	while (terminate < 2) {
		int num_read = getline(&line, &line_len, stdin);
		if (!strcmp(line, "\n")) {
			terminate++;
		} else {
			terminate = 0;
			sendto(sockfd, line, num_read, 0, target->ai_addr,
			       target->ai_addrlen);
			log_print(LOG, "Sent %d bytes to server", num_read);
		}
	}

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
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_DGRAM;
	hints.ai_flags = AI_PASSIVE;

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

	if (bind(sockfd, res->ai_addr, res->ai_addrlen) == -1) {
		close(sockfd);
		log_print(ERROR, "Cannot bind to port");
	}

	int err = 0;
	pthread_t line_read_thread;
	pthread_create(&line_read_thread, 0, &read_input, res);
	read_input(res);

	while (terminate < 2) {
		memset(INBUFFER, 0, sizeof(INBUFFER));

		struct sockaddr_storage server_addr;
		socklen_t server_addr_len = sizeof(server_addr);

		int bytes_transmitted = 0;
		if ((bytes_transmitted =
			     recvfrom(sockfd, INBUFFER, BUFFER_SIZE, 0,
				      (struct sockaddr *)&server_addr,
				      &server_addr_len)) == -1)
			log_print(ERROR, "Cannot read from socket");
		log_print(LOG, "%d bytes received", bytes_transmitted);

		/* TODO: Replace simple printing with GBN receiver */
		printf("%s", INBUFFER);
	}

	pthread_join(line_read_thread, 0);
	return EXIT_SUCCESS;
}
