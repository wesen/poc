/*C
  (c) 2003 Institut fuer Telematik, Universitaet Karlsruhe
**/

#ifndef NET_H__
#define NET_H__

int net_ip4_resolve_hostname(const char *hostname,
                             unsigned short port,
                             unsigned char ip[4],
                             struct sockaddr_in *saddr);

int net_udp4_send_socket(char *hostname,
			  unsigned short port,
			  unsigned char ttl);
int net_udp4_recv_socket(char *hostname,
			  unsigned short port);

int net_tcp4_nonblock_socket(void);
int net_tcp4_socket_nonblock(int s);
int net_tcp4_bind(int s,
		  unsigned char ip[4],
		  unsigned short port);
int net_tcp4_bind_reuse(int s,
			unsigned char ip[4],
			unsigned short port);
int net_tcp4_listen_socket(char *hostname, unsigned short port);
int net_tcp4_accept_socket(int s, unsigned char ip[4], unsigned short *port);

#ifdef WITH_IPV6
int net_udp6_send_socket(char *hostname,
			  unsigned short port,
			  unsigned int hops);
int net_udp6_recv_socket(char *hostname,
			  unsigned short port);

int net_tcp6_nonblock_socket(void);
int net_tcp6_socket_nonblock(int s);
int net_tcp6_bind(int s,
		  unsigned char ip[4],
		  unsigned short port);
int net_tcp6_bind_reuse(int s,
			unsigned char ip[4],
			unsigned short port);
int net_tcp6_listen_socket(char *hostname, unsigned short port);
int net_tcp6_accept_socket(int s, unsigned char ip[16], unsigned short *port);
#endif

int net_seqnum_diff(unsigned long seq1, unsigned long seq2,
		    unsigned long maxseq);

/*C
**/

#endif /* NET_H__ */
