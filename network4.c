/*C
  (c) 2005 bl0rg.net
**/

#include "conf.h"

#include <assert.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <unistd.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <fcntl.h>

#include "network.h"
#include "pack.h"

/*
 * Resolve an IP4 hostname (XXX IP6 support later).
 */
int net_ip4_resolve_hostname(const char *hostname,
                             unsigned short port,
                             unsigned char ip[4],
                             struct sockaddr_in *saddr) {
  assert(hostname != NULL);

  struct hostent *host;
  if (NULL == (host = gethostbyname(hostname))) {
    perror("gethostbyname");
    return 0;
  }
  if (host->h_length != 4) {
    return 0;
  }
  if (ip != NULL)
    memcpy(ip, host->h_addr_list[0], host->h_length);
  if (saddr != NULL) {
    memcpy(&saddr->sin_addr, host->h_addr_list[0], host->h_length);
    saddr->sin_port = port;
  }

  return 1;
}

/*M
  \emph{Create an UDP socket.}

  If the given address is a multicast adress, the socket will be set
  to use the multicast TTL ttl and sets the datagrams to loop back.
  Returns the filedescriptor of the socket, or -1 on error.
**/
int net_udp4_socket(struct sockaddr_in *saddr,
		    unsigned short port,
		    unsigned char ttl) {
  assert(saddr != NULL);
  
  /*M
    Create UDP socket.
  **/
  int fd;
  if ((fd = socket(PF_INET, SOCK_DGRAM, 0)) < 0) {
    perror("socket");
    return -1;
  }

  /*M
    Set socket to reuse addresses.
  **/
  int on = 1;
  if (setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on)) < 0) {
    perror("setsockopt");
    goto error;
  }

  if ((saddr->sin_addr.s_addr == INADDR_BROADCAST) ||
      (ntohl(saddr->sin_addr.s_addr) & 0xFF == 0xFF)) {
    static int allow = 1;
    if (setsockopt(fd, SOL_SOCKET, SO_BROADCAST, (char*)&allow, sizeof(allow)) == -1) {
      perror("setsockopt");
      goto error;
    }
  }
    
  saddr->sin_family = AF_INET;
  saddr->sin_port   = htons(port);

  /*M
    If the address is a multicast address, set the TTL and turn on
    multicast loop so the local host can receive the UDP packets.
  **/
  if (IN_MULTICAST(htonl(saddr->sin_addr.s_addr))) {
    unsigned char loop = 1;
    if ((setsockopt(fd,
                    IPPROTO_IP,
                    IP_MULTICAST_TTL,
                    &ttl, sizeof(ttl)) < 0) ||
        (setsockopt(fd,
                    IPPROTO_IP,
                    IP_MULTICAST_LOOP,
                    &loop, sizeof(loop)) < 0)) {
      perror("setsockopt");
      goto error;
    }
  }

  return fd;

 error:
  if (close(fd) < 0)
    perror("close");
  return -1;
}

/*M
  \emph{Create a sending UDP socket.}

  Connects the created UDP socket to hostname. Returns the
  filedescriptor of the socket, or -1 on error.
**/
int net_udp4_send_socket(char *hostname,
                         unsigned short port,
                         unsigned char ttl) {
  struct sockaddr_in saddr;
  if (!net_ip4_resolve_hostname(hostname, port, NULL, &saddr))
    return -1;

  /*M
    Create udp socket.
  **/
  int fd;
  if ((fd = net_udp4_socket(&saddr, port, ttl)) < 0)
    return -1;

  if (saddr.sin_addr.s_addr == INADDR_BROADCAST) {
    static int allow = 1;
    if (setsockopt(fd, SOL_SOCKET, SO_BROADCAST, (char*)&allow, sizeof(allow)) == -1) {
      perror("setsockopt");
      goto error;
    }
  }
  
  /*M
    Connect to hostname.
  **/
  if (connect(fd, (struct sockaddr *)&saddr, sizeof(saddr)) < 0) {
    perror("connect");
    goto error;
  }

  return fd;

 error:
  if (close(fd) < 0)
    perror("close");
  return -1;
}

/*M
  \emph{Create a receiving UDP socket.}

  Binds the created UDP socket to hostname, and adds multicast
  membership if hostname is a multicast hostname. Returns the
  filedescriptor of the socket, or -1 on error.
**/
int net_udp4_recv_socket(char *hostname,
                         unsigned short port) {
  struct sockaddr_in addr;
  if (!net_ip4_resolve_hostname(hostname, port, NULL, &addr))
    memset(&addr.sin_addr, 0, sizeof(addr.sin_addr));

  /*M
    Create udp socket.
  **/
  int fd;
  if ((fd = net_udp4_socket(&addr, port, 1)) < 0)
    return -1;

  /*M
    Bind to hostname.
  **/
  if (bind(fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
    perror("bind");
    goto error;
  }

  /*M
    Add multicast membership if address is a multicast address.
  **/
  if (IN_MULTICAST(htonl(addr.sin_addr.s_addr))) {
    struct ip_mreq mreq;
    mreq.imr_multiaddr.s_addr = addr.sin_addr.s_addr;
    mreq.imr_interface.s_addr = htonl(INADDR_ANY);

    if (setsockopt(fd,
                   IPPROTO_IP,
                   IP_ADD_MEMBERSHIP,
                   &mreq, sizeof(mreq)) < 0) {
      perror("setsockopt");
      goto error;
    }
  }

  return fd;

 error:
  if (close(fd) < 0)
    perror("close");
  return -1;
}

/*M
  \emph{Create a TCP v4 socket and set it non to nonblocking IO.}
**/
int net_tcp4_nonblock_socket(void) {
  int s;

  s = socket(AF_INET, SOCK_STREAM, 0);
  if (s == -1)
    return -1;
  if (net_tcp4_socket_nonblock(s) == -1) {
    close(s);
    return -1;
  }
  
  return s;
}

int net_tcp4_socket_nonblock(int s) {
  if (fcntl(s, F_SETFL, fcntl(s, F_GETFL, 0) | O_NONBLOCK) == -1)
    return -1;
  return 0;
}

/*M
  \emph{Bind a socket to an IPV4 adress and port.}
**/
int net_tcp4_bind(int s, unsigned char ip[4], unsigned short port) {
  struct sockaddr_in sa;

  memset(&sa, 0, sizeof(sa));

  sa.sin_family = AF_INET;

  unsigned char *ptr = (unsigned char *)&sa.sin_port;
  UINT16_PACK(ptr, port);
  memcpy(&sa.sin_addr, ip, 4);
  
  return bind(s, (struct sockaddr *)&sa, sizeof(sa));
}

int net_tcp4_bind_reuse(int s,
                        unsigned char ip[4],
                        unsigned short port) {
  int opt = 1;
  setsockopt(s, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
  return net_tcp4_bind(s, ip, port);
}

int net_tcp4_listen_socket(char *hostname, unsigned short port) {
  unsigned char ip[4];
  if (!net_ip4_resolve_hostname(hostname, port, ip, NULL))
    memset(ip, 0, sizeof(ip));
  
  int sock;
  if (((sock = net_tcp4_nonblock_socket()) < 0) ||
      (net_tcp4_bind_reuse(sock, ip, port) < 0) ||
      (listen(sock, 16) < 0)) {
    return -1;
  }

  return sock;
}

int net_tcp4_accept_socket(int s,
                           unsigned char ip[4],
                           unsigned short *port) {
  struct sockaddr_in sa;
  int len = sizeof(sa);
  int fd;

  fd = accept(s, (struct sockaddr *)&sa, &len);
  if (fd == -1)
    return -1;

  memcpy(ip, (unsigned char *)&sa.sin_addr, 4);
  unsigned char *ptr = (unsigned char *)&sa.sin_port;
  *port = UINT16_UNPACK(ptr);

  return fd;
}

