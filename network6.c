/*C
  (c) 2003 Institut fuer Telematik, Universitaet Karlsruhe
**/

#ifdef WITH_IPV6

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

/*M
  \emph{Create an ipv6 UDP socket.}

  Returns the filedescriptor of the socket, or -1 on error.
**/
static int net_udp6_socket(struct sockaddr_in6 *saddr,
                           unsigned short port,
                           unsigned int hops) {
  assert(saddr != NULL);
  
  /*M
    Create UDP socket.
  **/
  int msock;
  if ((msock = socket(PF_INET6, SOCK_DGRAM, 0)) < 0) {
    perror("socket");
    return -1;
  }

  /*M
    Set socket to reuse addresses.
  **/
  int on = 1;
  if (setsockopt(msock, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on)) < 0) {
    perror("setsockopt");
    goto error;
  }

  saddr->sin6_family = AF_INET6;
  saddr->sin6_port   = htons(port);

  /*M
    If the address is a multicast address, set the TTL and turn on
    multicast loop so the local host can receive the UDP packets.
  **/
  if (IN6_IS_ADDR_MULTICAST(&saddr->sin6_addr)) {
    unsigned int loop = 1;
    if ((setsockopt(msock,
                    IPPROTO_IPV6,
                    IPV6_MULTICAST_HOPS,
                    &hops, sizeof(hops)) < 0) ||
        (setsockopt(msock,
                    IPPROTO_IPV6,
                    IPV6_MULTICAST_LOOP,
                    &loop, sizeof(loop)) < 0)) {
      perror("setsockopt");
      goto error;
    }
  }

  return msock;

 error:
  if (close(msock) < 0)
    perror("close");
  return -1;
}

/*M
  \emph{Create a ipv6 sending UDP socket.}

  Connects the created UDP socket to hostname. Returns the
  filedescriptor of the socket, or -1 on error.
**/
int net_udp6_send_socket(char *hostname,
                         unsigned short port,
                         unsigned int hops) {
  /*M
    Get hostname address.
  **/
  struct hostent *host;
  if (NULL == (host = gethostbyname2(hostname, AF_INET6))) {
    perror("gethostbyname");
    return -1;
  }

  /*M
    Init sockaddr structure.
  **/
  struct sockaddr_in6 saddr;
  memcpy(&saddr.sin6_addr, host->h_addr_list[0], (size_t)host->h_length);

  /*M
    Create udp socket.
  **/
  int msock;
  if ((msock = net_udp6_socket(&saddr, port, hops)) < 0)
    return -1;

  /*M
    Connect to hostname.
  **/
  if (connect(msock, (struct sockaddr *)&saddr, sizeof(saddr)) < 0) {
    perror("connect");
    goto error;
  }

  return msock;

 error:
  if (close(msock) < 0)
    perror("close");
  return -1;
}

/*M
  \emph{Create a receiving ipv6 UDP socket.}

  Binds the created UDP socket to hostname, and adds multicast
  membership if hostname is a multicast hostname. Returns the
  filedescriptor of the socket, or -1 on error.
**/
int net_udp6_recv_socket(char *hostname,
                         unsigned short port) {
  /*M
    Get hostname address.
  **/
  struct hostent *host;
  if (NULL == (host = gethostbyname2(hostname, AF_INET6))) {
    perror("gethostbyname");
    return -1;
  }

  /*M
    Initialize sockaddr structure.
  **/
  struct sockaddr_in6 addr;
  memcpy(&addr.sin6_addr, host->h_addr_list[0], (size_t)host->h_length);

  /*M
    Create udp socket.
  **/
  int msock;
  if ((msock = net_udp6_socket(&addr, port, 1)) < 0)
    return -1;

  /*M
    Bind to hostname.
  **/
  if (bind(msock, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
    perror("bind");
    goto error;
  }

  /*M
    Add multicast membership if address is a multicast address.
  **/
  if (IN6_IS_ADDR_MULTICAST(&addr.sin6_addr)) {
    struct ipv6_mreq mreq;
    memcpy(&mreq.ipv6mr_multiaddr,
           &addr.sin6_addr,
           sizeof(addr.sin6_addr));
    mreq.ipv6mr_interface = 0;

    if (setsockopt(msock,
                   IPPROTO_IPV6,
                   IPV6_JOIN_GROUP,
                   &mreq, sizeof(mreq)) < 0) {
      perror("setsockopt");
      goto error;
    }
  }

  return msock;

 error:
  if (close(msock) < 0)
    perror("close");
  return -1;
}

/*M
  \emph{Create a TCP v6 socket and set it non to nonblocking IO.}
**/
int net_tcp6_nonblock_socket(void) {
  int s;

  s = socket(AF_INET6, SOCK_STREAM, 0);
  if (s == -1)
    return -1;
  if (net_tcp6_socket_nonblock(s) == -1) {
    close(s);
    return -1;
  }
  
  return s;
}

int net_tcp6_socket_nonblock(int s) {
  if (fcntl(s, F_SETFL, fcntl(s, F_GETFL, 0) | O_NONBLOCK) == -1)
    return -1;

  return 0;
}

/*M
  \emph{Bind a socket to an IPV4 adress and port.}
**/
int net_tcp6_bind(int s, unsigned char ip[16], unsigned short port) {
  struct sockaddr_in6 sa;

  memset(&sa, 0, sizeof(sa));

  sa.sin6_family = AF_INET6;

  unsigned char *ptr = (unsigned char *)&sa.sin6_port;
  UINT16_PACK(ptr, port);
  memcpy(&sa.sin6_addr, ip, 16);
  
  return bind(s, (struct sockaddr *)&sa, sizeof(sa));
}

int net_tcp6_bind_reuse(int s,
                        unsigned char ip[16],
                        unsigned short port) {
  int opt = 1;
  setsockopt(s, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
  return net_tcp6_bind(s, ip, port);
}

int net_tcp6_listen_socket(char *hostname, unsigned short port) {
  /*M
    Get hostname address.
  **/
  struct hostent *host;
  if (NULL == (host = gethostbyname2(hostname, AF_INET6))) {
    perror("gethostbyname");
    return -1;
  }

  if (host->h_length != 16) {
    perror("gethostbyname");
    return -1;
  }

  int sock;
  if (((sock = net_tcp6_nonblock_socket()) < 0) ||
      (net_tcp6_bind_reuse(sock, host->h_addr_list[0], port) < 0) ||
      (listen(sock, 16) < 0)) {
    return -1;
  }

  return sock;
}

int net_tcp6_accept_socket(int s,
                           unsigned char ip[16],
                           unsigned short *port) {
  struct sockaddr_in6 sa;
  int len = sizeof(sa);
  int fd;

  fd = accept(s, (struct sockaddr *)&sa, &len);
  if (fd == -1)
    return -1;

  memcpy(ip, (unsigned char *)&sa.sin6_addr, 16);
  unsigned char *ptr = (unsigned char *)&sa.sin6_port;
  *port = UINT16_UNPACK(ptr);

  return fd;
}

#endif

/*M
**/
