#define _POSIX_C_SOURCE 200112L
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/un.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <unistd.h>
#define MAXCONNECTIONS 10
#define BUFLEN 100
#define sockname "socket.sk"

int main(int argc, char** argv)
{
	struct sockaddr_un saddr; // socket address
	strncpy(saddr.sun_path, sockname, strlen(sockname) + 1);
	saddr.sun_family = AF_UNIX;
	int fd_socket = socket(AF_UNIX, SOCK_STREAM, 0);
	bind(fd_socket, (struct sockaddr*)&saddr, sizeof saddr);
	listen(fd_socket, MAXCONNECTIONS);
	int fd_client = accept(fd_socket, NULL, 0);
	char* buffer = malloc(BUFLEN);
	if (read(fd_client, (void*)buffer, BUFLEN) == -1)
		return -1;
	fprintf(stdout, "[SERVER] READ %s\n", buffer);
	memset(buffer, 0, BUFLEN);
	snprintf(buffer, BUFLEN, "socket : %d\tclient : %d\n", fd_socket, fd_client);
	if (write(fd_client, (void*)buffer, BUFLEN) == -1)
		return -1;
	fprintf(stdout, "[SERVER] WRITTEN %s\n", buffer);
	free(buffer);
	remove(sockname);
}