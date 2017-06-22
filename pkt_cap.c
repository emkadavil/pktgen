#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <errno.h>

/*
 * argv[1] = port
 */
int main (int argc, char *argv[])
{
	char buf[256];
	int n;
	int sock, port, acc_fd;
	struct sockaddr_in srv_addr, cli_addr;
	socklen_t sock_len = sizeof (struct sockaddr_in);

	if (argc != 2) {
		printf ("Provide a port number to listen on!\n");
		return -1;
	}
	port = atoi (argv[1]);
	if (port > 65535) {
		printf ("Invalid port %d\n", port);
		return -1;
	}

	sock = socket (AF_INET, SOCK_STREAM, 0);
	if (sock < 0) {
		printf ("Error %d creating socket!\n", errno);
		return -1;
	}

	memset (&srv_addr, 0, sizeof(struct sockaddr_in));
	srv_addr.sin_family = AF_INET;
	srv_addr.sin_addr.s_addr = INADDR_ANY;
	srv_addr.sin_port = htons (port);
	if (bind (sock, (struct sockaddr *)&srv_addr, sizeof(srv_addr)) != 0) {
		printf ("Error %d binding to socket!\n", errno);
		return -1;
	}

	if (listen (sock, 5)) {
		printf ("Error %d listening on socket!\n", errno);
		return -1;
	}
	while (1) {
		int cntr = 0;
		printf ("Waiting for connection on port %d\n", port);
		acc_fd = accept (sock, (struct sockaddr*)&cli_addr, &sock_len);
		if (acc_fd < 0) {
			printf ("Error %d accepting connecction!\n", errno);
			return -1;
		}
		printf ("Client %s connected from remote port %d\n", 
			inet_ntoa(cli_addr.sin_addr), ntohs(cli_addr.sin_port));
		while (1) {
			bzero (buf, sizeof(buf));
			n = read (acc_fd, buf, sizeof(buf));
			if (n < 0) {
				printf ("Error %d reading from socket!\n", errno);
				break;
			} else if (n == 0) {
				printf ("Connection closed by client!\n");
				break;
			} else {
				if (++cntr%1000 == 0) {
					printf ("\rPackets received: %d", cntr);
					fflush (stdout);
				}
			}
		}
		close (acc_fd);
	}
	return 0;
}
