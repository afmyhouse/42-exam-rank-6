//PART 0: includes and globals
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <netinet/in.h>

// PART 0: Includes and globals
const size_t BUFFER_READ = 1024;
const size_t MAX_MSG_SIZE = 1000000;
const size_t MAX_CLIENTS = 100;
const size_t MSG_BUF_SIZE = 512;

typedef struct s_client {
	int		fd;
	int		id;
	char	*buf; // buffer to accumulate reveived data
	size_t buf_len; // tracking buffer len
	struct	s_client *next;
} t_client;

t_client	*clients = NULL;
int			max_fd = 0;
size_t			next_id = 0;
fd_set		active_fds, read_fds, write_fds;

//PART 1: Error handling
void fatal_error() {
	write(2, "Fatal error\n", 12);
	while (clients) {
		t_client *tmp = clients;
		clients = clients->next;
		if (tmp->buf) free(tmp->buf);
		if (tmp->fd >= 0) close(tmp->fd);
		free(tmp);
	}
	exit(1);
}

//PART 2: Client manmagement

// Add a client to the list
t_client *add_client(int fd) {

	if (next_id >= MAX_CLIENTS) {
		write(2, "Max clients reached\n", 20);
		close(fd);
		return NULL;
	}

	t_client *client_new = malloc(sizeof(t_client));

	if (!client_new)
		fatal_error();

	client_new->id = next_id++;
	client_new->fd = fd;
	client_new->buf = NULL;
	client_new->buf_len = 0;
	client_new->next = clients;
	clients = client_new;
	return client_new;
}

// Remove a client from the list
void remove_client(int fd) {
	t_client **curr = &clients;

	while (*curr) {
		if ((*curr)->fd == fd) {
			t_client *tmp = *curr;
			*curr = (*curr)->next;
			if (tmp->buf) free(tmp->buf);
			close(tmp->fd);
			FD_CLR(fd, &active_fds);
			free(tmp);
			break;
		}
		curr = &(*curr)->next;
	}

	if (fd == max_fd) {
		max_fd = 0;
		for (t_client *c = clients; c; c = c->next) {
			if (c->fd > max_fd) {
				max_fd = c->fd;
			}
		}
	}
}

//PARt 3: Broadcast function
void broadcast(int sender_fd, const char *msg) {
	for (t_client *c = clients; c; c = c->next) {
		if (c->fd != sender_fd && FD_ISSET(c->fd, &write_fds))  {
			send(c->fd, msg, strlen(msg), 0);		}
	}
}

//PART 4: Utils, extract lines
char *str_join(char *buf, const char *add) {
	size_t len = (buf ? strlen(buf) : 0) + strlen(add);
	if (len > MAX_MSG_SIZE) {
		free(buf);
		write(2, "Message too long\n", 17);
		return NULL;
	}
	char *res = malloc(len + 1);
	if (!res) fatal_error();
	res[0] = 0;
	if (buf) {
		strcat(res, buf);
		free(buf);
	}
	strcat(res, add);
	res[len] = 0;
	return res;
}

/// @brief 	Extract lines from the buffer and broadcast them
/// @param client
void extract_lines(t_client *client) {
	char *msg = client->buf;
	if (!msg)
		return;

	char *start = msg;
	while (1) {
		char *line = strchr(start, '\n');
		if (!line) break;
		*line = 0;
		char tmp[MSG_BUF_SIZE];
		sprintf(tmp, "client %d: %s\n", client->id, msg);
		broadcast(client->fd, tmp);
		start = line + 1;
		msg = line + 1;
	}
	size_t remaining = strlen(start);
	if (remaining > 0) {
		char *new_buf = malloc(remaining + 1);
		if (!new_buf) fatal_error();
		strcpy(new_buf, start);
		free(client->buf);
		client->buf = new_buf;
		client->buf_len = remaining;
	} else if (client->buf) {
		client->buf[remaining] = 0;
		client->buf_len = remaining;
	} else {
		free(client->buf);
		client->buf = NULL;
		client->buf_len = 0;
	}
}

int main(int ac, char **av)
{
	if (ac!=2) {
		write(2, "Wrong number of arguments\n", 26);
		exit(1);
	}

	int server_port = atoi(av[1]);
	int server_fd = socket(AF_INET, SOCK_STREAM, 0);
	if (server_fd < 0) fatal_error();


	FD_ZERO(&active_fds);
	FD_SET(server_fd, &active_fds);
	max_fd = server_fd;

	struct sockaddr_in server_addr;
	server_addr.sin_family = AF_INET;
	server_addr.sin_addr.s_addr = htonl(2130706433); // 127.0.0.1
	server_addr.sin_port = htons(server_port);

	if (bind(server_fd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
		close(server_fd);
		fatal_error();
	}

	if (listen(server_fd, MAX_CLIENTS) < 0) {
		close(server_fd);
		fatal_error();
	}


	while (1) {
		read_fds = write_fds = active_fds;
		if (select(max_fd + 1, &read_fds, &write_fds, NULL, NULL) < 0)
			fatal_error();

		// Check for new connections
		if (FD_ISSET(server_fd, &read_fds)) {
			int client_fd = accept(server_fd, NULL, NULL);
			if (client_fd < 0) continue ;
			t_client *client = add_client(client_fd);
			if (!client) {
				close(client_fd);
				continue;
			}
			if (client_fd > max_fd) {
				max_fd = client_fd;
			}
			FD_SET(client_fd, &active_fds);
			char msg[MSG_BUF_SIZE];
			sprintf(msg, "server: client %d just arrived\n", client->id);
			broadcast(client->fd, msg);
		}

		// Check for data from clients
		t_client *curr = clients;
		while (curr) {
			t_client *next = curr->next;
			int fd = curr->fd;
			if (FD_ISSET(fd, &read_fds)) {
				char buf[BUFFER_READ];
				int bytes = recv(fd, buf, BUFFER_READ - 1, 0);
				if (bytes <= 0) {
					char msg[MSG_BUF_SIZE];
					sprintf(msg, "server: client %d just left\n", curr->id);
					broadcast(fd, msg);
					remove_client(fd);
				} else {
					buf[bytes] = 0;
					curr->buf = str_join(curr->buf, buf);
					if (!curr->buf) {
						remove_client(fd);
					} else {
						curr->buf_len = bytes;
						extract_lines(curr);
					}
				}
			}
			curr = next;
		}
	}

}
