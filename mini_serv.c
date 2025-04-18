#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <netinet/in.h>
#include <signal.h> // Added for SIGPIPE

// PART 0: Includes and globals
const int BUFFER_READ = 16*1024;
const int MAX_MSG_SIZE = 1024*1024;
const int MAX_CLIENTS = 128*1024;
const int MSG_BUF_SIZE = 128*1024;

typedef struct s_client {
    int fd;
    int id;
    char *buf;
    int buf_len;
    struct s_client *next;
} t_client;

t_client *clients = NULL;
int max_fd = 0;
int next_id = 0;
int connected = 0;
fd_set active_fds, read_fds, write_fds;

// PART 1: Error handling
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

// PART 2: Client management
t_client *add_client(int fd) {
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

    int rcvbuf_size;
    socklen_t optlen = sizeof(rcvbuf_size);
    getsockopt(fd, SOL_SOCKET, SO_RCVBUF, &rcvbuf_size, &optlen);
    printf("SO_RCVBUF for fd %d: %d bytes\n", fd, rcvbuf_size);

    int sndbuf_size;
    getsockopt(fd, SOL_SOCKET, SO_SNDBUF, &sndbuf_size, &optlen);
    printf("SO_SNDBUF for fd %d: %d bytes\n", fd, sndbuf_size);

    return client_new;
}

void remove_client(int fd) {
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
size_t send_all(int sock, const char *buf, size_t len) {
    size_t total = 0;
    while (total < len) {
        ssize_t sent = send(sock, buf + total, len - total, 0);
        if (sent <= 0) {
            return total; // Stop on error or closed socket
        }
        total += sent;
    }
    return total;
}

void broadcast(int sender_fd, const char *msg) {
    size_t len = strlen(msg);
    for (t_client *c = clients; c; c = c->next) {
        if (c->fd != sender_fd && FD_ISSET(c->fd, &write_fds)) {
            send_all(c->fd, msg, len);
        }
    }
}

// PART 4: Utils
char *str_join(char *buf, int buf_len, const char *add) {
    int len = buf_len + strlen(add);
    if (len > MAX_MSG_SIZE) {
        free(buf);
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
        sprintf(tmp, "client %d: %s\n", client->id, start);
        printf("extract_lines: id=%d, sending %zu bytes\n", client->id, strlen(tmp));
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

    // Ignore SIGPIPE
    signal(SIGPIPE, SIG_IGN);

    int server_port = atoi(av[1]);
    int server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd < 0) fatal_error();

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
        read_fds = write_fds = active_fds;
        if (select(max_fd + 1, &read_fds, &write_fds, NULL, NULL) < 0)
            fatal_error();
        if (FD_ISSET(server_fd, &read_fds)) {
            int client_fd = accept(server_fd, NULL, NULL);
            if (client_fd < 0)
                continue;
            if (connected >= MAX_CLIENTS) {
                close(client_fd);
                continue;
            } else {
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
        }
        t_client *curr = clients;
        while (curr) {
            t_client *next = curr->next;
            int fd = curr->fd;
			if (FD_ISSET(fd, &read_fds)) {
				char buf[BUFFER_READ];
				int total_bytes = 0;
				int bytes;
				do {
					bytes = recv(fd, buf, BUFFER_READ - 1, 0);
					printf("recv: fd=%d, bytes=%d, total=%d\n", fd, bytes, total_bytes + bytes);
					if (bytes <= 0) {
						if (total_bytes == 0) { // No data read, client disconnected
							char msg[MSG_BUF_SIZE];
							sprintf(msg, "server: client %d just left\n", curr->id);
							broadcast(fd, msg);
							remove_client(fd);
						}
						break;
					}
					buf[bytes] = 0;
					curr->buf = str_join(curr->buf, curr->buf_len, buf);
					if (!curr->buf) {
						remove_client(fd);
						break;
					}
					curr->buf_len += bytes;
					total_bytes += bytes;
					// Process lines after each read
					if (strchr(curr->buf, '\n')) { // Only if newline present
						printf("before extract_lines: id=%d, buf_len=%d\n", curr->id, curr->buf_len);
						extract_lines(curr);
					}
				} while (bytes > 0);
			}
            curr = next;
        }
    }
}