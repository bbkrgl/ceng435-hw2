#include "conn.h"

char connection_exist(struct connection_t *list, struct sockaddr *addr)
{
	while (list) {
		if (!strcmp(list->target_addr.sa_data, addr->sa_data))
			return 1;
		list = list->next;
	}

	return 0;
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
	if (!list) {
		struct connection_t *new_elem =
			malloc(sizeof(struct connection_t));
		new_elem->id = 0;
		new_elem->target_addr = *addr;
		new_elem->target_addr_len = addr_len;
		pthread_cond_init(&new_elem->cond, NULL);
		new_elem->next = new_elem->prev = NULL;

		return new_elem;
	}

	while (list->next)
		list = list->next;

	struct connection_t *new_elem = malloc(sizeof(struct connection_t));
	new_elem->id = list->id + 1;
	new_elem->target_addr = *addr;
	pthread_cond_init(&new_elem->cond, NULL);
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
