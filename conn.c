/**
 * @file conn.c
 * @author Burak Köroğlu (e2448637@ceng.metu.edu.tr)
 * @brief Connection helper functions implementation
 * 
 */

#include "conn.h"

/**
 * @brief Finds and returns the packet with the given id (sequence number) in the given queue. If not found, returns NULL.
 * 
 * @param queue 
 * @param id 
 * @return struct packet_t* 
 */
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

/**
 * @brief Adds and returns the given packet data to the given queue. Id is filled from this function.
 * 
 * @param queue 
 * @param data 
 * @return struct packet_t* 
 */
struct packet_t *add_packet(struct packet_queue *queue,
			    struct packet_data *data)
{
	/** Get a lock to prevent data race with input thread */
	pthread_mutex_lock(&queue->mutex);
	struct packet_t *new_elem = calloc(1, sizeof(struct packet_t));
	new_elem->data = *data;
	new_elem->next = new_elem->prev = NULL;
	queue->size++;

	if (!queue->head) {
		new_elem->data.id = queue->last_sent + 1;
		queue->head = queue->tail = new_elem;

		pthread_mutex_unlock(&queue->mutex);
		return new_elem;
	}

	new_elem->data.id = queue->tail->data.id + 1;
	queue->tail->next = new_elem;
	new_elem->prev = queue->tail;
	queue->tail = new_elem;
	pthread_mutex_unlock(&queue->mutex);

	return new_elem;
}

/**
 * @brief Evicts the packets before the given id (sequence number) and returns the id from the given queue. If the packet is not found, returns -1.
 * 
 * @details In this implementation, eviction means ack, as it will not be sent again.
 * This function also ignores the duplicate acks from older packets implicitly as all such will be evicted.
 * 
 * @param queue 
 * @param id 
 * @param mutex 
 * @return int 
 */
int acknowledge_packet(struct packet_queue *queue, int id,
		       pthread_mutex_t *mutex)
{
	struct packet_t *packet;
	/** Get the lock to prevent the synchronization issues with sending thread */
	pthread_mutex_lock(mutex);
	/** Get another lock to prevent data race with the input thread */
	pthread_mutex_lock(&queue->mutex);
	/** Find the packet with given id (sequence number)
	 * Iterate through all packets before the packet and evict them as they are acknowledged */
	if ((packet = find_packet(queue, id))) {
		queue->head = packet->next;
		if (packet->next) {
			queue->head->prev = NULL;

			struct packet_t *temp;
			while (packet) {
				queue->size--;
				temp = packet;
				packet = packet->prev;
				free(temp);
			}
		} else {
			free(packet);
			queue->tail = queue->head;
			queue->size = 0;
		}

		pthread_mutex_unlock(&queue->mutex);
		pthread_mutex_unlock(mutex);
		return id;
	}

	pthread_mutex_unlock(&queue->mutex);
	pthread_mutex_unlock(mutex);
	return -1;
}

/**
 * @brief Free the given queue
 * 
 * @param queue 
 */
void free_queue(struct packet_queue *queue)
{
	/** Lock the queue to prevent data race */
	pthread_mutex_lock(&queue->mutex);
	struct packet_t *last = queue->tail;
	struct packet_t *temp;
	while (last) {
		temp = last;
		last = last->prev;
		free(temp);
	}

	queue->size = 0;
	queue->head = queue->tail = NULL;
	pthread_mutex_unlock(&queue->mutex);
}

/**
 * @brief Finds and returns the connection with the given id in the given queue. If not found, returns NULL.
 * 
 * @param list 
 * @param addr 
 * @return struct connection_t* 
 */
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

/**
 * @brief Adds and returns a new connection to the given list.
 * 
 * @param list 
 * @param addr 
 * @param addr_len 
 * @return struct connection_t* 
 */
struct connection_t *add_connection(struct connection_t *list,
				    struct sockaddr *addr, socklen_t addr_len)
{
	struct connection_t *new_elem = calloc(1, sizeof(struct connection_t));
	new_elem->target_addr = *addr;
	new_elem->target_addr_len = addr_len;
	pthread_cond_init(&new_elem->cond, NULL);
	pthread_cond_init(&new_elem->timeout_cond, NULL);
	pthread_mutex_init(&new_elem->timeout_mutex, NULL);
	pthread_mutex_init(&new_elem->queue.mutex, NULL);
	new_elem->next = new_elem->prev = NULL;
	new_elem->is_active = 1;

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

/**
 * @brief Deletes the given connection from the list
 * 
 * @param conn 
 */
void delete_connection(struct connection_t *conn)
{
	conn->prev->next = conn->next;
	conn->next->prev = conn->prev;

	free(conn);
}

/**
 * @brief Free the given list from its last element
 * 
 * @param last 
 */
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
