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
    char *m;
} t_client;

void fatal(void) {
    write(2, "Fatal error\n", 12);
    exit(1);
}

void broadcast(int fd, int mfd, fd_set wfds, char *m) {
    for (int bfd = 0; bfd <= mfd; bfd++) {
        if (FD_ISSET(bfd, &wfds) && bfd != fd)
            send (bfd, m, strlen(m), 0);
    }
}

void clean(int mfd, t_client *cls, fd_set afds) {
    for (int fd = 0; fd <= mfd; fd++) {
        if (FD_ISSET(fd, &afds)) {
            FD_CLR(fd, &afds);
            close(fd);
        }
        if (cls[fd].m)
            free(cls[fd].m);
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
	int sockfd, fd, mfd, id;
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
    const int CSIZE = 96*4096;

    t_client  cls[CSIZE];

    fd_set afds, rfds, wfds;

    FD_ZERO(&afds);
    FD_SET(sockfd, &afds);

    mfd = sockfd;
    id = 0;

    while(1) {
        rfds = wfds = afds;
        int active = select(mfd + 1, &rfds, &wfds, NULL, NULL);
        if (active == -1) {
            clean(mfd, cls, afds);
            fatal();
        }

        if (FD_ISSET(sockfd, &rfds)) {
            fd = accept(sockfd, (struct sockaddr *)&cli, &len);
            if (fd >= 0) {
                cls[fd].id = id++;
                cls[fd].m = NULL;
                FD_SET(fd, &afds);
                if (fd > mfd) mfd = fd;

                char m[MSIZE];
                bzero(m, MSIZE);
                sprintf(m, "server: client %d just arrived\n", cls[fd].id);
                broadcast(fd, mfd, wfds, m);
            }
        }
        else {
            for (fd = 0; fd <= mfd; fd++) {
                if (FD_ISSET(fd, &rfds)) {
                    char buff[BSIZE];
                    bzero(buff, BSIZE);
                    int bytes = recv(fd, buff, BSIZE - 1, 0);
                    if (bytes <= 0) {
                        char m[MSIZE];
                        bzero(m, MSIZE);
                        sprintf(m, "server: client %d just left\n", cls[fd].id);
                        broadcast(fd, mfd, wfds, m);
                        FD_CLR(fd, &afds);
                        close(fd);
                        if (cls[fd].m) {
                            free(cls[fd].m);
                            cls[fd].m = NULL;
                        }
                    }
                    else {
                        char *line = NULL;
                        cls[fd].m = str_join(cls[fd].m, buff);
                        while(extract_message(&(cls[fd].m), &line)) {
                            char m[MSIZE + strlen(line)];
                            bzero(m, MSIZE + strlen(line));
                            sprintf(m, "client %d: %s", cls[fd].id, line);
                            broadcast(fd, mfd, wfds, m);
                            free(line);
                            line = NULL;
                        }
                        if (cls[fd].m[0] == '\0') {
                            free(cls[fd].m);
                            cls[fd].m = NULL;
                        }
                    }
                }
            }
        }
    }
    clean(mfd,cls, afds);
    return (0);
}