#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <netinet/in.h>

#include "worker.h"
#include "command.h"

#define WORKER_EXIT(exit_code) \
	dprintf(conn_info.cmd_conn_fd, "221 Closing control connection"); \
	close(connfd); \
	return exit_code

int worker_func(int connfd)
{
	char msg_buffer[1024];
	ssize_t read_bytes;
	cmd_func_t *cmd;
	struct conn_info conn_info;
	const char *arg;
	size_t arg_len;

	/* Init conn_info */
	memset(&conn_info, 0, sizeof(conn_info));

	conn_info.cmd_conn_fd = connfd;
	conn_info.data_fd = -1;

	while(conn_info.quit == 0)
	{
		memset(msg_buffer, 0, sizeof(msg_buffer)); // For testing
		read_bytes = read(conn_info.cmd_conn_fd, msg_buffer, sizeof(msg_buffer));
		if(read_bytes == -1)
		{
			perror("Error reading socket");
			WORKER_EXIT(-1);
		}

		/* Connection closed */
		if(read_bytes == 0)
		{
			break;
		}

		/* Check if last 2 Bytes are "\r\n" */
		if(read_bytes < 2 || msg_buffer[read_bytes-1] != '\n' || msg_buffer[read_bytes - 2] != '\r')
		{
			dprintf(conn_info.cmd_conn_fd, "500 Invalid command");
			fprintf(stderr, "Invalid command string\n");
			continue;
		}
		read_bytes -= 2;

		printf("%s", msg_buffer); // also for testing

		if((cmd = cmd_get_cmd(msg_buffer, &arg, read_bytes)) == NULL)
		{
			dprintf(conn_info.cmd_conn_fd, "500 Unknown command\n");
			fprintf(stderr, "Unknown command received\n");
			continue;
		}

		arg_len = read_bytes - (arg - msg_buffer);

		if(cmd(&conn_info, arg, arg_len) == -1)
		{
			fprintf(stderr, "Command failed\n");
			continue;
		}
	}

	WORKER_EXIT(0);
}
