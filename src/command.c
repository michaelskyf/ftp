#include <netinet/in.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <unistd.h>
#include <errno.h>
#include <dirent.h>
#include <sys/stat.h>

#include "command.h"

#define MIN(a, b) \
	(a > b) ? (b) : (a)

static inline int data_connect(struct conn_info *c)
{
	int ret = -1;
	struct sockaddr_in addr;
	socklen_t addr_len = sizeof(addr);

	if(c->mode == CONN_PASV)
	{
		if((ret = accept(c->data_fd, (struct sockaddr *)&addr, &addr_len)) == -1)
		{
			perror("Failed to accept incoming data connection");
			close(c->data_fd);
			return -1;
		}
	}
	else if(c->mode == CONN_ACTV)
	{
		fprintf(stderr, "TODO: Active mode in %s\n", __func__);
	}
	else
	{
		fprintf(stderr, "Invalid mode\n");
		c->mode = CONN_IDLE;
	}

	return ret;
}

static inline int data_close(struct conn_info *c, int connfd)
{
	int ret = 0;

	if(c->mode != CONN_IDLE)
	{
		if(close(c->data_fd) == -1)
		{
			perror("Failed to close data_fd");
			ret = -1;
		}

		if(close(connfd) == -1)
		{
			perror("Failed to close connfd");
			ret = -1;
		}

		c->mode = CONN_IDLE;
	}

	return ret;
}

static const char *mode_to_str(int mode)
{
	const char perms[] = "drwxrwxrwx";
	static char buff[sizeof(perms)];
	buff[sizeof(buff) - 1] = '\0';
	buff[0] = S_ISDIR(mode) ? perms[0] : '-';
	for(int i = 1; i < 10; i++)
	{
		buff[i] = (mode & (1 << (9-i)) ? perms[i] : '-');
	}

	return buff;
}

cmd_func_t *cmd_get_cmd(const char *msg, const char **arg, size_t msg_len)
{
	for(size_t i = 0; i < sizeof(commands)/sizeof(*commands); i++)
	{
		const struct cmd_entry *c = &commands[i];

		if(!strncmp(c->cmd_str, msg, MIN(strlen(c->cmd_str), msg_len)))
		{
			if(arg != NULL)
				*arg = msg + strlen(c->cmd_str) + ((msg_len) ? 1 : 0);

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

	/* TODO?: Check if socket got closed */
	if(c->mode != CONN_IDLE)
	{
		fprintf(stderr, ":(\n");
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
	c->mode = CONN_PASV;

	return 0;
}

int cmd_ftp_list(struct conn_info *c, const char *arg, size_t arg_len)
{
	/* Ignored */
	(void)arg;
	(void)arg_len;

	int connfd;
	DIR *dp;
	struct dirent *dir;
	struct stat sb;

	if(c->logged_in == 0)
	{
		dprintf(c->cmd_conn_fd, "530 Not logged in\n");
		return -1;
	}

	connfd = data_connect(c);
	if(connfd == -1)
	{
		return -1;
	}

	dp = opendir(".");
	if(dp == NULL)
	{
		dprintf(c->cmd_conn_fd, "451 %s\n", strerror(errno));
		data_close(c, connfd);
		return -1;
	}


	dprintf(c->cmd_conn_fd, "150 Sending directory listing\n");

	while((dir = readdir(dp)))
	{
		if(stat(dir->d_name, &sb) == -1)
		{
			dprintf(c->cmd_conn_fd, "451 %s\n", strerror(errno));
			data_close(c, connfd);
			return -1;
		}

		/* -rwxrwxrwx 1 user group 0 Feb 9 09:38 file.txt */
		dprintf(connfd, "	%s %4lu %5u %5u %10lu %s\r\n",
				mode_to_str(sb.st_mode),
#ifdef __APPLE__
				(unsigned long)
#endif
				sb.st_nlink,
				sb.st_uid, sb.st_gid,
#ifdef __APPLE__
				(unsigned long)
#endif
				sb.st_size,
				dir->d_name
		       );
	}

	dprintf(c->cmd_conn_fd, "226 Directory send OK\n");

	closedir(dp);
	data_close(c, connfd);

	return 0;
}

int cmd_ftp_type(struct conn_info *c, const char *arg, size_t arg_len)
{
	(void)arg;
	(void)arg_len;

	if(c->logged_in == 0)
	{
		dprintf(c->cmd_conn_fd, "530 Not logged in\n");
		return -1;
	}

	dprintf(c->cmd_conn_fd, "250 OK\n");

	return 0;
}

int cmd_ftp_stor(struct conn_info *c, const char *arg, size_t arg_len)
{
	/* STOR arg1 arg2(optional) */
	FILE *fp;
	int connfd;
	char filename[FILENAME_MAX];
	char buffer[10 * 1024];
	size_t bytes_read;

	if(c->logged_in == 0)
	{
		dprintf(c->cmd_conn_fd, "530 Not logged in\n");
		return -1;
	}

	memcpy(filename, arg, MIN(arg_len, sizeof(filename) - 1));
	filename[MIN(arg_len, sizeof(filename) - 1)] = '\0';

	connfd = data_connect(c);
	if(connfd == -1)
	{
		return -1;
	}

	fp = fopen(filename, "w");
	if(fp == NULL)
	{
		perror("Failed to open file for writing");
		dprintf(c->cmd_conn_fd, "451 %s\n", strerror(errno));
		data_close(c, connfd);
		return -1;
	}

	dprintf(c->cmd_conn_fd, "150 Receiving file\n");

	while((bytes_read = read(connfd, buffer, sizeof(buffer))))
	{
		if(fwrite(buffer, 1, bytes_read, fp) != bytes_read)
		{
			if(ferror(fp))
			{
				fprintf(stderr, "Error writing file\n");
			}
		}
	}
	fflush(fp);
	fclose(fp);

	dprintf(c->cmd_conn_fd, "226 File store OK\n");

	data_close(c, connfd);

	return 0;
}

int cmd_ftp_retr(struct conn_info *c, const char *arg, size_t arg_len)
{
	/* STOR arg1 arg2(optional) */
	FILE *fp;
	int connfd;
	char filename[FILENAME_MAX];
	char buffer[10 * 1024];
	size_t read_bytes;

	if(c->logged_in == 0)
	{
		dprintf(c->cmd_conn_fd, "530 Not logged in\n");
		return -1;
	}

	memcpy(filename, arg, MIN(arg_len, sizeof(filename) - 1));
	filename[MIN(arg_len, sizeof(filename) - 1)] = '\0';

	connfd = data_connect(c);
	if(connfd == -1)
	{
		return -1;
	}

	fp = fopen(filename, "r");
	if(fp == NULL)
	{
		perror("Failed to open file for writing");
		dprintf(c->cmd_conn_fd, "451 %s\n", strerror(errno));
		data_close(c, connfd);
		return -1;
	}

	dprintf(c->cmd_conn_fd, "150 Sending file\n");

	while((read_bytes = fread(buffer, 1, sizeof(buffer), fp)))
	{
		if(write(connfd, buffer, read_bytes) == -1)
		{
			perror("Failed to write to connfd");
		}
	}

	if(ferror(fp))
	{
		fprintf(stderr, "Error reading file\n");
	}

	fclose(fp);

	dprintf(c->cmd_conn_fd, "226 File send OK\n");

	data_close(c, connfd);

	return 0;
}

int cmd_ftp_quit(struct conn_info *c, const char *arg, size_t arg_len)
{
	(void)arg;
	(void)arg_len;

	c->quit = 1;

	dprintf(c->cmd_conn_fd, "226 bye\n");

	return 0;
}
