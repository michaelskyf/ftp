#include <netinet/in.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <unistd.h>

#include "command.h"

#define MIN(a, b) \
	(a > b) ? (b) : (a)

cmd_func_t *cmd_get_cmd(const char *msg, const char **arg, size_t msg_len)
{
	for(size_t i = 0; i < sizeof(commands)/sizeof(*commands); i++)
	{
		const struct cmd_entry *c = &commands[i];

		if(!strncmp(c->cmd_str, msg, MIN(strlen(c->cmd_str), msg_len)))
		{
			if(arg != NULL)
				*arg = msg + strlen(c->cmd_str);

			return c->cmd_func;
		}
	}

	return NULL;
}

int cmd_ftp_user(struct conn_info *c, const char *arg, size_t arg_len)
{
	memcpy(c->username, arg, MIN(sizeof(c->username), arg_len));

	dprintf(c->cmd_conn_fd, "331 Username OK\n");

	return 0;
}

int cmd_ftp_pass(struct conn_info *c, const char *arg, size_t arg_len)
{
	memcpy(c->password, arg, MIN(sizeof(c->password), arg_len));

	/* TODO: Check usernames and passwords */
	c->logged_in = 1;
	dprintf(c->cmd_conn_fd, "230 Login successful\n");

	return 0;
}

int cmd_ftp_syst(struct conn_info *c, const char *arg, size_t arg_len)
{
	/* Ignored */
	(void)arg;
	(void)arg_len;

	dprintf(c->cmd_conn_fd, "215 UNIX Type: L8\n");

	return 0;
}

int cmd_ftp_pasv(struct conn_info *c, const char *arg, size_t arg_len)
{
	/* Ignored */
	(void)arg;
	(void)arg_len;

	if(c->logged_in == 0)
	{
		dprintf(c->cmd_conn_fd, "530 Not logged in\n");
		return -1;
	}

	struct sockaddr_in addr;
	socklen_t addr_len = sizeof(addr);
	int fd;
	const char *host;
	struct timeval timeout;
	uint16_t port;
    	timeout.tv_sec = 10;
    	timeout.tv_usec = 0;

	if(getsockname(c->cmd_conn_fd, (struct sockaddr *)&addr, &addr_len) == -1)
	{
		perror("Failed to get socket name of cmd_conn_fd");
		return -1;
	}

	host = inet_ntoa(addr.sin_addr);

	for(char *n = strchr(host, '.'); n != NULL; n = strchr(n, '.'))
	{
		*n = ',';
	}

	fd = socket(AF_INET, SOCK_STREAM, 0);
	if(fd == -1)
	{
		perror("Failed to create socket");
		return -1;
	}

	addr.sin_port = 0;
	if(setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout)) == -1)
	{
		perror("Failed to set socket options");
		close(fd);
		return -1;
	}

	if(bind(fd, (struct sockaddr *)&addr, sizeof(addr)) == -1)
	{
		perror("Failed to bind socket");
		close(fd);
		return -1;
	}

	if(listen(fd, 5) == -1)
	{
		perror("Failed to listen on bound socket");
		close(fd);
		return -1;
	}


	if(getsockname(fd, (struct sockaddr *)&addr, &addr_len) == -1)
	{
		perror("Failed to get socket name of data_fd");
		close(fd);
		return -1;
	}


	port = ntohs(addr.sin_port);
	dprintf(c->cmd_conn_fd, "227 Entering Passive Mode (%s,%d,%d)\n", host, (port & 0xFF00) >> 8, port & 0xFF);


	c->data_fd = fd;

	return 0;
}

int cmd_ftp_list(struct conn_info *c, const char *arg, size_t arg_len)
{
	/* Ignored */
	(void)arg;
	(void)arg_len;

	int connfd;
	struct sockaddr_in addr;
	socklen_t addr_len;

	if(c->logged_in == 0)
	{
		dprintf(c->cmd_conn_fd, "530 Not logged in\n");
		return -1;
	}

	if((connfd = accept(c->data_fd, (struct sockaddr *)&addr, &addr_len)) == -1)
	{
		perror("Failed to accept incoming data connection");
		close(c->data_fd);
		return -1;
	}

	dprintf(c->cmd_conn_fd, "150 Sending directory listing\n");
	dprintf(connfd, "It's Working!\r\n");
	dprintf(c->cmd_conn_fd, "226 Directory send OK\n");

	close(connfd);
	close(c->data_fd);

	return 0;
}
