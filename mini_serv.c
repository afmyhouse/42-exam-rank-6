// #include <errno.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
// #include <netdb.h>
// #include <sys/socket.h>
#include <sys/select.h>
#include <netinet/in.h>

typedef struct s_client {
    int id;
    char *m;
} t_client;

void fatal(void) {
    write(2, "Fatal error\n", 12);
    exit(1);
}

void broadcast(int client_fd, int max_fd, fd_set write_fd_set, char *m) {
    for (int broadcast_fd = 0; broadcast_fd <= max_fd; broadcast_fd++) {
        if (FD_ISSET(broadcast_fd, &write_fd_set) && broadcast_fd != client_fd)
            send (broadcast_fd, m, strlen(m), 0);
    }
}

void clean(int max_fd, t_client *cls, fd_set active_fd_set) {
    for (int client_fd = 0; client_fd <= max_fd; client_fd++) {
        if (FD_ISSET(client_fd, &active_fd_set)) {
            FD_CLR(client_fd, &active_fd_set);
            close(client_fd);
        }
        if (cls[client_fd].m)
            free(cls[client_fd].m);
    }
}

int extract_message(char **buf, char **msg)
{
	char	*newbuf;
	int	i;

	*msg = 0;
	if (*buf == 0)
		return (0);
	i = 0;
	while ((*buf)[i])
	{
		if ((*buf)[i] == '\n')
		{
			newbuf = calloc(1, sizeof(*newbuf) * (strlen(*buf + i + 1) + 1));
			if (newbuf == 0)
				fatal();
			strcpy(newbuf, *buf + i + 1);
			*msg = *buf;
			(*msg)[i + 1] = 0;
			*buf = newbuf;
			return (1);
		}
		i++;
	}
	return (0);
}

char *str_join(char *buf, char *add)
{
	char	*newbuf;
	int		len;

	if (buf == 0)
		len = 0;
	else
		len = strlen(buf);
	newbuf = malloc(sizeof(*newbuf) * (len + strlen(add) + 1));
	if (newbuf == 0)
		fatal();
	newbuf[0] = 0;
	if (buf != 0)
		strcat(newbuf, buf);
	free(buf);
	strcat(newbuf, add);
	return (newbuf);
}


int main(int ac, char **av) {
    if (ac != 2) {
        write (2, "Wrong number of arguments\n", 26);
        exit(1);
    }
	int sockfd, client_fd, max_fd, id;
    unsigned int len;
	struct sockaddr_in servaddr, cli;

	// socket create and verification
	sockfd = socket(AF_INET, SOCK_STREAM, 0);
	if (sockfd == -1) fatal();

	// assign IP, PORT
	servaddr.sin_family = AF_INET;
	servaddr.sin_addr.s_addr = htonl(2130706433); //127.0.0.1
	servaddr.sin_port = htons(atoi(av[1]));

	// Binding newly created socket to given IP and verification
	if ((bind(sockfd, (const struct sockaddr *)&servaddr, sizeof(servaddr))) != 0) fatal();
	if (listen(sockfd, 10) != 0) fatal();
	len = sizeof(cli);

    const int BSIZE = 1024;
    const int MSIZE = 64;
    const int CSIZE = 16*4096;

    t_client  cls[CSIZE];

    fd_set active_fd_set, read_fd_set, write_fd_set;

    FD_ZERO(&active_fd_set);
    FD_SET(sockfd, &active_fd_set);

    max_fd = sockfd;
    id = 0;

    while(1) {
        read_fd_set = write_fd_set = active_fd_set;
        int active = select(max_fd + 1, &read_fd_set, &write_fd_set, NULL, NULL);
        if (active == -1) {
            clean(max_fd, cls, active_fd_set);
            fatal();
        }

        if (FD_ISSET(sockfd, &read_fd_set)) {
            client_fd = accept(sockfd, (struct sockaddr *)&cli, &len);
            if (client_fd >= 0) {
                cls[client_fd].id = id++;
                cls[client_fd].m = NULL;
                FD_SET(client_fd, &active_fd_set);
                if (client_fd > max_fd) max_fd = client_fd;

                char m[MSIZE];
                bzero(m, MSIZE);
                sprintf(m, "server: client %d just arrived\n", cls[client_fd].id);
                broadcast(client_fd, max_fd, write_fd_set, m);
            }
        }
        else {
            for (client_fd = 0; client_fd <= max_fd; client_fd++) {
                if (FD_ISSET(client_fd, &read_fd_set)) {
                    char buff[BSIZE];
                    bzero(buff, BSIZE);
                    int bytes = recv(client_fd, buff, BSIZE - 1, 0);
                    if (bytes <= 0) {
                        char m[MSIZE];
                        bzero(m, MSIZE);
                        sprintf(m, "server: client %d just left\n", cls[client_fd].id);
                        broadcast(client_fd, max_fd, write_fd_set, m);
                        FD_CLR(client_fd, &active_fd_set);
                        close(client_fd);
                        if (cls[client_fd].m) {
                            free(cls[client_fd].m);
                            cls[client_fd].m = NULL;
                        }
                    }
                    else {
                        char *line = NULL;
                        cls[client_fd].m = str_join(cls[client_fd].m, buff);
                        while(extract_message(&(cls[client_fd].m), &line)) {
                            char m[MSIZE + strlen(line)];
                            bzero(m, MSIZE + strlen(line));
                            sprintf(m, "client %d: %s", cls[client_fd].id, line);
                            broadcast(client_fd, max_fd, write_fd_set, m);
                            free(line);
                            line = NULL;
                        }
                        if (cls[client_fd].m[0] == '\0') {
                            free(cls[client_fd].m);
                            cls[client_fd].m = NULL;
                        }
                    }
                }
            }
        }
    }
    clean(max_fd, cls, active_fd_set);
    return (0);
}