all: server client
server: server.c conn.c
	gcc -O3 -pthread server.c conn.c -o server
client: client.c conn.c
	gcc -O3 -pthread client.c conn.c -o client

debug: server_debug client_debug
server_debug: server.c conn.c
	gcc -g -Wall -O3 -pthread server.c conn.c -o server
client_debug: client.c conn.c
	gcc -g -Wall -O3 -pthread client.c conn.c -o client

clean:
	rm -f server client
