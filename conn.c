#include "conn.h"

struct packet_t *find_packet(struct packet_queue *queue, int id)
{
	struct packet_t *packet = queue->head;
	while (packet) {
		if (packet->data.id == id)
			break;
		packet = packet->next;
	}

	return packet;
}

struct packet_t *add_packet(struct packet_queue *queue,
			    struct packet_data *data)
{
	struct packet_t *new_elem = calloc(1, sizeof(struct packet_t));
	new_elem->data = *data;
	new_elem->next = new_elem->prev = NULL;
	queue->size++;

	if (!queue->head) {
		new_elem->data.id = queue->last_sent + 1;
		queue->head = queue->tail = new_elem;

		return new_elem;
	}

	new_elem->data.id = queue->tail->data.id + 1;
	queue->tail->next = new_elem;
	new_elem->prev = queue->tail;
	queue->tail = new_elem;

	return new_elem;
}

void acknowledge_packet(struct packet_queue *queue, int id)
{
	struct packet_t *packet;
	if ((packet = find_packet(queue, id))) {
		packet->acknowledged = 1;
		if (queue->head == packet)
			while (packet && packet->acknowledged) {
				queue->head = packet->next;
				if (packet->next)
					packet->next->prev = NULL;
				free(packet);

				packet = queue->head;
				queue->size--;
			}
	}
}

void free_queue(struct packet_queue *queue)
{
	struct packet_t *last = queue->tail;
	struct packet_t *temp;
	while (last) {
		temp = last;
		last = last->prev;
		free(temp);
	}
}

struct connection_t *find_connection(struct connection_t *list,
				     struct sockaddr *addr)
{
	while (list) {
		if (!strcmp(list->target_addr.sa_data, addr->sa_data))
			break;
		list = list->next;
	}
	return list;
}

struct connection_t *add_connection(struct connection_t *list,
				    struct sockaddr *addr, socklen_t addr_len)
{
	struct connection_t *new_elem = calloc(1, sizeof(struct connection_t));
	new_elem->target_addr = *addr;
	new_elem->target_addr_len = addr_len;
	pthread_cond_init(&new_elem->cond, NULL);
	new_elem->next = new_elem->prev = NULL;

	if (!list) {
		new_elem->id = 0;
		return new_elem;
	}

	while (list->next)
		list = list->next;

	new_elem->id = list->id + 1;
	new_elem->prev = list;

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
		free_queue(&last->queue);

		temp = last;
		last = last->prev;
		free(temp);
	}
}
