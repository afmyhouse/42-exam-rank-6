#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <netinet/in.h>

// PART 0: Includes and globals
const int BUFFER_READ = 1024;
const int MAX_MSG_SIZE = 1000000;
const int MAX_CLIENTS = 5;
const int MSG_BUF_SIZE = 512;

typedef struct s_client {
	int fd;
	int id;
	char *buf; // Buffer for received data
	int buf_len; // Track buffer length
	struct s_client *next;
} t_client;

t_client *clients = NULL;
int max_fd = 0;
int next_id = 0;
int connected = 0; // Track active clients
fd_set active_fds, read_fds, write_fds;

// PART 1: Error handling
void fatal_error() {
	write(2, "Fatal error\n", 12);
	exit(1);
}

// PART 2: Client management
t_client *add_client(int fd) {
	printf("add_client: fd=%d, connected=%d, MAX_CLIENTS=%d\n", fd, connected, MAX_CLIENTS);
	if (connected >= MAX_CLIENTS) {
		write(2, "Max clients reached\n", 20);
		close(fd);
		printf("add_client: rejected fd=%d\n", fd);
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
	connected++;
	printf("add_client: added fd=%d, connected=%d\n", fd, connected);
	return client_new;
}

void remove_client(int fd) {
	printf("remove_client: fd=%d, connected=%d\n", fd, connected);
	t_client **curr = &clients;
	while (*curr) {
		if ((*curr)->fd == fd) {
			t_client *tmp = *curr;
			*curr = (*curr)->next;
			if (tmp->buf)
				free(tmp->buf);
			close(tmp->fd);
			FD_CLR(fd, &active_fds);
			free(tmp);
			connected--;
			printf("remove_client: removed fd=%d, connected=%d\n", fd, connected);
			break;
		}
		curr = &(*curr)->next;
	}
	if (fd == max_fd) {
		max_fd = 0;
		for (t_client *c = clients; c; c = c->next) {
			if (c->fd > max_fd)
				max_fd = c->fd;
		}
	}
}

// PART 3: Broadcast function
void broadcast(int sender_fd, const char *msg) {
	for (t_client *c = clients; c; c = c->next) {
		if (c->fd != sender_fd && FD_ISSET(c->fd, &write_fds)) {
			send(c->fd, msg, strlen(msg), 0);
		}
	}
}

// PART 4: Utils
char *str_join(char *buf, int buf_len, const char *add) {
	int len = buf_len + strlen(add);
	if (len > MAX_MSG_SIZE) {
		free(buf);
		write(2, "Message too long\n", 17);
		return NULL;
	}
	char *res = malloc(len + 1);
	if (!res)
		fatal_error();
	res[0] = 0;
	if (buf) {
		strcat(res, buf);
		free(buf);
	}
	strcat(res, add);
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

// PART 5: Main
int main(int ac, char **av) {
	if (ac != 2) {
		write(2, "Wrong number of arguments\n", 26);
		exit(1);
	}
	int server_port = atoi(av[1]);
	int server_fd = socket(AF_INET, SOCK_STREAM, 0);
	if (server_fd < 0)
		fatal_error();
	FD_ZERO(&active_fds);
	FD_SET(server_fd, &active_fds);
	max_fd = server_fd;
	struct sockaddr_in server_addr;
	bzero(&server_addr, sizeof(server_addr));
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
		//printf("main: connected=%d\n", connected);
		read_fds = write_fds = active_fds;
		if (select(max_fd + 1, &read_fds, &write_fds, NULL, NULL) < 0)
			fatal_error();
		if (FD_ISSET(server_fd, &read_fds)) {
			int client_fd = accept(server_fd, NULL, NULL);
			if (client_fd < 0)
				continue;
			t_client *client = add_client(client_fd);
			if (!client)
				continue;
			if (client_fd > max_fd)
				max_fd = client_fd;
			FD_SET(client_fd, &active_fds);
			char msg[MSG_BUF_SIZE];
			sprintf(msg, "server: client %d just arrived\n", client->id);
			broadcast(client->fd, msg);
		}
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
					curr->buf = str_join(curr->buf, curr->buf_len, buf);
					if (!curr->buf) {
						remove_client(fd);
					} else {
						curr->buf_len += bytes;
						extract_lines(curr);
					}
				}
			}
			curr = next;
		}
	}
}