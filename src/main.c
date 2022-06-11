#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <signal.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <arpa/inet.h>

#include "worker.h"

static const char *welcome_msg = "220 Good morning sir. Please kindly do the needful\n";

static int sockfd;
static struct sockaddr_in srv_addr;

static size_t children = 0;

static int create_socket(const char *address, int port)
{
	const int       optVal = 1;
	const socklen_t optLen = sizeof(optVal);

	in_addr_t bin_addr = INADDR_ANY;
	if(address && inet_pton(AF_INET, address, &bin_addr) == -1)
	{
		perror("Failed to convert address to binary form");
		return -1;
	}

	sockfd = socket(AF_INET, SOCK_STREAM, 0);
	if(sockfd == -1)
	{
		perror("Failed to create socket");
		return -1;
	}

	memset(&srv_addr, 0, sizeof(srv_addr));

	srv_addr.sin_family = AF_INET;
	srv_addr.sin_addr.s_addr = bin_addr;
	srv_addr.sin_port = htons(port);

	if(bind(sockfd, (struct sockaddr *)&srv_addr, sizeof(srv_addr)) == -1)
	{
		perror("Failed to bind socket");
		close(sockfd);
		return -1;
	}

	if(listen(sockfd, 5) == -1)
	{
		perror("Failed to listen on bound socket");
		close(sockfd);
		return -1;
	}

	if(setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &optVal, optLen) == -1)
	{
		printf("OOps :(\n");
	}

	return 0;
}

static void serve(void)
{
	pid_t pid;
	int connfd;
	struct sockaddr_in cli_addr;
	socklen_t cli_len = sizeof(cli_addr);

	/* Will exit on signal */
	while(1)
	{
		connfd = accept(sockfd, (struct sockaddr *)&cli_addr, &cli_len);
		if(connfd == -1)
		{
			perror("Failed to accept incoming connection");
		}

		pid = fork();
		if(pid == -1)
		{
			perror("Failed to create child process");
		}

		if(pid == 0)
		{
			close(sockfd);

			write(connfd, welcome_msg, strlen(welcome_msg));

			exit(worker_func(connfd));
		}
		else
		{
			close(connfd);

			printf("Creating worker with PID: %d\n", pid);
			children++;
		}
	}
}

static void sigchld_handler(int signum)
{
	(void)signum;

	pid_t pid;
	int status;

	children--;
	pid = wait(&status);
	if(pid == -1)
	{
		perror("wait failed");
		exit(EXIT_FAILURE);
	}
	else if(WIFEXITED(status))
	{
		printf("Worker with PID %d exited with exit code %d\n", pid, WEXITSTATUS(status));
	}
	else if(WIFSIGNALED(status))
	{
		fprintf(stderr, "Worker with PID %d killed with signal %d: %s\n",
				pid, WTERMSIG(status), strsignal(WTERMSIG(status)));
	}
	else
	{
		fprintf(stderr, "Unexpected status with worker %d\n", pid);
	}

}


int main(int argc, char *argv[])
{
	(void)argc;
	(void)argv;

	/* Setup signal handlers */
	signal(SIGCHLD, sigchld_handler);

	if(create_socket("127.0.0.1", 8787) == -1)
	{
		return -1;
	}

	serve();

	return 0;
}
