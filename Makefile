all: server client

server: server.c gbn.c
	gcc -O3 -pthread server.c gbn.c -o server
client: client.c gbn.c
	gcc -O3 -pthread client.c gbn.c -o client

server_debug: server.c gbn.c
	gcc -g -O3 -pthread server.c gbn.c -o server
client_debug: gbn.c gbn.c
	gcc -g -O3 -pthread client.c gbn.c -o client

clean:
	rm -f server client
