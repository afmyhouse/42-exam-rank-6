#include <errno.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <netdb.h>
#include <sys/socket.h>
#include <netinet/in.h>


typedef struct s_client
{
	int id;
	char *msg;
}	t_client;

void	put_error(char *msg)
{
	while(*msg)
		write(2, &(*msg++), 1);
	exit (1);
}

void	broadcast(int fd, int max_fd, fd_set wfds, char *msg)
{
	for (int broad_fd = 0; broad_fd <= max_fd; broad_fd++)
	{
		if (FD_ISSET(broad_fd, &wfds) && broad_fd != fd)
			send(broad_fd, msg, strlen(msg), 0);
	}
}

void	clear_all(int max_fd, struct s_client *clients, fd_set afds)
{
	for (int fd = 0; fd <= max_fd; fd++)
	{
		if (FD_ISSET(fd, &afds))
		{
			FD_CLR(fd, &afds);
			close(fd);
		}
		if (clients[fd].msg)
			free(clients[fd].msg);
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
				put_error("Fatal error\n");
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
		put_error("Fatal error\n");
	newbuf[0] = 0;
	if (buf != 0)
		strcat(newbuf, buf);
	free(buf);
	strcat(newbuf, add);
	return (newbuf);
}


int main(int argc, char *argv[]) {

	if (argc != 2)
		put_error("Wrong number of arguments\n");

	int server_fd, max_fd, id;
	unsigned int len;
	struct sockaddr_in servaddr, cli; 

	// socket create and verification 
	server_fd = socket(AF_INET, SOCK_STREAM, 0); 
	if (server_fd == -1) { 
		put_error("Fatal error\n");
		exit(0); 
	} 

	bzero(&servaddr, sizeof(servaddr)); 

	// assign IP, PORT 
	servaddr.sin_family = AF_INET; 
	servaddr.sin_addr.s_addr = htonl(2130706433); //127.0.0.1
	servaddr.sin_port = htons(atoi(argv[1]));
  
	// Binding newly created socket to given IP and verification 
	if ((bind(server_fd, (const struct sockaddr *)&servaddr, sizeof(servaddr))) != 0) { 
		put_error("Fatal error\n");
	} 

	if (listen(server_fd, 10) != 0) {
		put_error("Fatal error\n"); 
	}

	len = sizeof(cli);

	const int BUFFER_SIZE = 1024;
	const int MINI_BUFFER = 50;
	const int MAX_CLIENTS = 5;

	t_client	clients[MAX_CLIENTS];

	fd_set afds, rfds, wfds;

	FD_ZERO(&afds);
	FD_SET(server_fd, &afds);

	max_fd = server_fd;
	id = 0;

	while(1)
	{
		wfds = rfds = afds;

		int activity = select(max_fd + 1, &rfds, &wfds, NULL, NULL);
		if (activity == -1)
		{
			clear_all(max_fd, clients, afds);
			put_error("Fatal error\n");
		}

		if (FD_ISSET(server_fd, &rfds))
		{
			int client_fd = accept(server_fd, (struct sockaddr *)&cli, &len);
			if (client_fd >= 0)
			{
				clients[client_fd].id = id++;
				clients[client_fd].msg = NULL;

				if (client_fd > max_fd)
					max_fd = client_fd;

				FD_SET(client_fd, &afds);

				char msg[MINI_BUFFER];
				bzero(msg, MINI_BUFFER);
				sprintf(msg, "server: client %d just arrived\n", clients[client_fd].id);
				broadcast(client_fd, max_fd, wfds, msg);
			}
		}
		else
		{
			for (int fd = 0; fd <= max_fd; fd++)
			{
				if (FD_ISSET(fd, &rfds))
				{
					char buffer[BUFFER_SIZE];
					bzero(buffer, BUFFER_SIZE);

					int bytes_received = recv(fd, buffer, BUFFER_SIZE - 1, 0);
					if (bytes_received <= 0)
					{
						char msg[MINI_BUFFER];
						bzero(msg, MINI_BUFFER);
						sprintf(msg, "server: client %d just left\n", clients[fd].id);
						broadcast(fd, max_fd, wfds, msg);
						FD_CLR(fd, &afds);
						close(fd);
						if (clients[fd].msg)
						{
							free(clients[fd].msg);
							clients[fd].msg = NULL;
						}
					}
					else
					{
						char *line = NULL;

						clients[fd].msg = str_join(clients[fd].msg, buffer);
						while(extract_message(&(clients[fd].msg), &line))
						{
							char msg[MINI_BUFFER + strlen(line)];
							bzero(msg, MINI_BUFFER + strlen(line));
							sprintf(msg, "client %d: %s", clients[fd].id, line);
							broadcast(fd, max_fd, wfds, msg);
							free(line);
							line = NULL;
						}
						if (clients[fd].msg[0] == '\0')
						{
							free(clients[fd].msg);
							clients[fd].msg = NULL;
						}
					}
				}
			}
		}
	}
	clear_all(max_fd, clients, afds);
	return (0);
}