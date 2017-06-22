#include <stdio.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <time.h>
#include <signal.h>
#include <sys/time.h>
#include <stdlib.h>
#include <fcntl.h>

#define DEF_PKT_SZ		256

#define GEN_PROTO_TCP 	1
#define GEN_PROTO_UDP 	2

typedef struct _genOpts {
	unsigned int delay_us;
	unsigned int pkt_len;
	unsigned int proto;
	unsigned int port;
	unsigned int n_pkt;
	char dest_ip[16];
	char i_file[256];
	struct sockaddr_in server;
} GenOpts;

/* stats */
unsigned long long bytes;
unsigned long pkts;

int init_opts (GenOpts *opts) 
{
	bzero (opts, sizeof(GenOpts));
	opts->delay_us = 1000;
	opts->pkt_len = DEF_PKT_SZ;
	opts->proto = GEN_PROTO_UDP;
	opts->port = 1234;
	return 0;
}

int parseOpts (int argc, char *argv[], GenOpts *opts)
{
	int c;

	opterr = 0;
	while ((c = getopt (argc, argv, "d:f:l:n:p:r:s:")) != -1)
	{
		switch (c)
		{
			case 'd':
				opts->delay_us = atoi(optarg);
				break;

			case 'f':
				strncpy (opts->i_file, optarg, 256);
				break;

			case 'l':
				opts->pkt_len = atoi(optarg);
				if ((opts->pkt_len > 65535)) {
					printf ("Invalid packet length %s\n", optarg);
					exit (EINVAL);
				}
				break;

			case 'n':
				opts->n_pkt = atoi(optarg);
				break;

			case 'p':
				opts->port = atoi(optarg);
				if ((opts->port > 65535)) {
					printf ("Invalid port number %s\n", optarg);
					exit (EINVAL);
				}
				break;

			case 'r':
				if (!strcmp (optarg, "tcp"))
					opts->proto = GEN_PROTO_TCP;
				else if (!strcmp (optarg, "udp"))
					opts->proto = GEN_PROTO_UDP;
				else {
					printf ("Invalid protocol %s (tcp/udp)\n", optarg);
					exit (EINVAL);
				}
				break;

			case 's':
			{
				struct in_addr addr;
				if (inet_aton (optarg, &addr) == 0) {
					printf ("Invalid ip address %s\n", optarg);
					exit (EINVAL);
				}
				strncpy (opts->dest_ip, optarg, 16);
				break;
			}

			case '?':
				printf ("Invalid option %c\n", optopt);
				exit (EINVAL);
				break;

			default:
				printf ("Invalid argument %d\n", c);
				exit (EINVAL);
				break;
		}
	}
	if (strlen(opts->dest_ip) <= 0) {
		printf ("Destination IP cannot be empty!\n");
		exit (EINVAL);
	}
	return 0;
}

void dump_stat () 
{
	printf ("\rTx rate: %lluB/s (%llu Kbps) %lu pkt/s", bytes, bytes/125, pkts);
	fflush (stdout);
	bytes = 0;
	pkts = 0;
}

int createSocket (int proto)
{
	if (proto == GEN_PROTO_TCP) {
		return socket(AF_INET, SOCK_STREAM, 0);
	} else {
		return socket(AF_INET, SOCK_DGRAM, 0);
	}
}

int connectSocket (int sock, GenOpts *opts) 
{
	struct in_addr srv_addr;
	
	inet_aton (opts->dest_ip, &srv_addr);

	opts->server.sin_family = AF_INET;
	opts->server.sin_port = htons(opts->port);
	opts->server.sin_addr.s_addr = srv_addr.s_addr;
	if (opts->proto == GEN_PROTO_TCP) {
		if (connect(sock, (struct sockaddr *)&opts->server, sizeof (opts->server)) < 0) {
			printf ("Error %d connecting to server %s:%d\n", errno, opts->dest_ip, opts->port);
			close (sock);
			exit (errno);
		}
	}
	return 0;
}

int make_packet (char *buf, int buf_sz, char *file)
{
	char c, byte;
	char byte_str[3]; // nibble0, nibble1, null terminator
	int i = 0, b_cntr = 0;
	int fd =  open (file, O_RDONLY);

	if (fd < 0) {
		perror ("Error opening file.");
		return -1;
	}

	while (read (fd, &c, 1) > 0) {
		if (i>=buf_sz) {
			break;
		}
		if ( ((c>='0') && (c<='9')) ||
					((c>='a') && (c<='z')) ||
					((c>='A') && (c<='Z')) )
		{
			if (b_cntr > 1) {
				printf ("Corrupted byte in file!\n");
				break;
			} else {
				byte_str[b_cntr] = c;
				b_cntr++;
			}
		} else {	// Treat all other characters as delimitters
			byte = strtoul (byte_str, NULL, 16);
			buf[i++] = byte;
			//printf ("%02x%s", byte, i%16?" ":"\n");
			b_cntr = 0;
			bzero (byte_str, 3);
			continue;
		}
	}

	close (fd);
	if (i) {
		printf ("Packet of %d bytes formed from file %s\n", i, file);
	}
	return i;
}

int stats_hookup ()
{
	struct itimerval stat_timer;
	if (getitimer (ITIMER_REAL, &stat_timer) != 0) {
		printf ("Error getting timer!\n");
	}
	stat_timer.it_interval.tv_sec  = 1;
	stat_timer.it_interval.tv_usec = 0;
	stat_timer.it_value.tv_sec     = 1;
	stat_timer.it_value.tv_usec    = 0;

	signal (SIGALRM, dump_stat);
	if (setitimer (ITIMER_REAL, &stat_timer, NULL) != 0) {
		printf ("Error starting stats display timer!\n");
	}
	return 0;
}

int send_pkt (int sock, GenOpts *opts)
{
	struct timespec ns;
	int cntr = 0;
	int send_sz = 0;

	ns.tv_sec = 0;
	ns.tv_nsec = opts->delay_us*1000;	

	char *buf = malloc (opts->pkt_len);
	if (!buf) {
		printf ("Malloc error\n");
		return -1;
	}
	memset (buf, 0xa5, opts->pkt_len);

	if (strlen (opts->i_file)) {
		send_sz = make_packet (buf, opts->pkt_len, opts->i_file);
	}
	
	// If a file is read properly, use its size as the send size
	if (send_sz <= 0) {
		send_sz = opts->pkt_len;
	}

	printf ("Sending %d byte %s packets to %s:%d with delay of %uus\n",
		send_sz, opts->proto==GEN_PROTO_UDP?"UDP":"TCP",
		opts->dest_ip, opts->port, opts->delay_us);

	stats_hookup();
	
	while (++cntr) {
		int ret = sendto (sock, buf, send_sz, 0, (struct sockaddr*)&opts->server, sizeof(opts->server));
		if (ret == 0) {
			printf ("%d: Server closed the connection!\n", getpid());
			break;
		} else if (ret < 0) {
			perror ("Socket error");
			printf ("%d: Error %d on socket write!\n", getpid(), errno);
			break;
		} else {
			bytes += ret;
			pkts++;
		}
		nanosleep (&ns, NULL);
		if (opts->n_pkt == 0)
			continue;
		else
			if (cntr == opts->n_pkt) {
				printf ("Sent %u packets. Quit.\n", opts->n_pkt);
				break;
			}
	}

	free (buf);
	return cntr;
}

int main (int argc, char *argv[]) 
{
	int sock;
	GenOpts opts;

	init_opts (&opts);

	parseOpts (argc, argv, &opts);

	sock = createSocket (opts.proto);
	if (sock < 0) {
		printf ("Error %d creating socket!\n", errno);
		return -1;
	}

	connectSocket(sock, &opts);

	send_pkt (sock, &opts);

	close (sock);
	return 0;
}
