#include <errno.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <netdb.h>
#include <sys/socket.h>
#include <netinet/in.h>

typedef struct s_client {
	int id;
	char *msg;
} t_client;

void fatal(void)
{
	write(2, "Fatal error\n", 12);
	exit(1);
}

void broadcast(int fd, int maxfd, fd_set wfds, char *msg)
{
	for (int bfd = 0; bfd <= maxfd; bfd++)
	{
		if (FD_ISSET(bfd, &wfds) && bfd != fd)
		{
			send(bfd, msg, strlen(msg), 0);
		}
	}
}

void clear_all(int maxfd, t_client *clients, fd_set afds)
{
	for (int fd = 0; fd <= maxfd; fd++)
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
	if (ac != 2) 
	{
		write(2, "Wrong number of arguments\n", 26);
		exit(1);
	}

	int sockfd, fd, id, maxfd;
	unsigned int len;
	struct sockaddr_in servaddr, cli; 

	// socket create and verification 
	sockfd = socket(AF_INET, SOCK_STREAM, 0); 
	if (sockfd == -1) 
	{
		fatal(); 
	} 

	bzero(&servaddr, sizeof(servaddr)); 

	// assign IP, PORT 
	servaddr.sin_family = AF_INET; 
	servaddr.sin_addr.s_addr = htonl(2130706433); //127.0.0.1
	servaddr.sin_port = htons(atoi(av[1])); 

	// Binding newly created socket to given IP and verification 
	if ((bind(sockfd, (const struct sockaddr *)&servaddr, sizeof(servaddr))) != 0) 
	{ 
		fatal();
	} 

	if (listen(sockfd, 10) != 0) 
	{
		fatal();
	}

	len = sizeof(cli);

	const int BSIZE = 1024;
	const int MSIZE = 50;
	const int MAX_CLI = 100;

	t_client clients[MAX_CLI];

	fd_set afds, rfds, wfds;

	FD_ZERO(&afds);
	FD_SET(sockfd, &afds);

	maxfd = sockfd;
	id = 0;

	while(1)
	{
		wfds = rfds = afds;

		int active = select(maxfd + 1, &rfds, &wfds, NULL, NULL);
		if (active == -1)
		{
			clear_all(maxfd, clients, afds);
			fatal();
		}

		if (FD_ISSET(sockfd, &rfds))
		{
			fd = accept(sockfd, (struct sockaddr *)&cli, &len);
			if (fd >= 0)
			{
				//connection accepted
				clients[fd].id = id++;
				clients[fd].msg = NULL;

				if (fd > maxfd)
					maxfd = fd;

				FD_SET(fd, &afds);

				char msg[MSIZE];
				bzero(msg, MSIZE);
				sprintf(msg, "server: client %d just arrived\n", clients[fd].id);
				broadcast(fd, maxfd, wfds, msg);
			}
		}
		else
		{
			for (fd = 0; fd <= maxfd; fd++)
			{
				if (FD_ISSET(fd, &rfds))
				{
					char buff[BSIZE];
					bzero(buff, BSIZE);
					int bytes = recv(fd, buff, BSIZE - 1, 0);
					if (bytes <= 0)
					{
						char msg[MSIZE];
						bzero(msg, MSIZE);
						sprintf(msg, "server: client %d just left\n", clients[fd].id);
						broadcast(fd, maxfd, wfds, msg);
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

						clients[fd].msg = str_join(clients[fd].msg, buff);
						while(extract_message(&(clients[fd].msg), &line))
						{
							char msg[MSIZE + strlen(line)];
							bzero(msg, MSIZE + strlen(line));
							sprintf(msg, "client %d: %s", clients[fd].id, line);
							broadcast(fd, maxfd, wfds, msg);
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
	clear_all(maxfd, clients, afds);
	return (0);
}