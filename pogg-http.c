/*C
  (c) 2003 Institut fuer Telematik, Universitaet Karlsruhe
**/

#include "conf.h"

#include <assert.h>
#include <sys/types.h>
#include <sys/time.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>

#ifdef NEED_GETOPT_H__
#include <getopt.h>
#endif

#include "file.h"
#include "vorbis.h"
#include "ogg.h"
#include "network.h"
#include "signal.h"

#ifdef WITH_IPV6
static int use_ipv6 = 0;
#endif /* WITH_IPV6 */

static int finished = 0;

/*M
  \emph{Maximal size of HTTP header.}
**/
#define HTTP_MAX_HDR_LEN 8192

/*M
  \emph{Seconds before HTTP timeout.}
**/
#define HTTP_TIMEOUT     20

/*M
**/
#define OGG_BUF_LEN 65535

/*M
  \emph{Maximal synchronization latency for sending packets.}

  In usecs.
**/
#define MAX_WAIT_TIME (1 * 1000)

#define MAX_FILENAME 256

static void sig_int(int signo) {
  finished = 1;
}

/*M
  \emph{Structure used to save information about HTTP clients.}
**/
typedef struct http_client_s {
   int fd, found, in, len;
   char buf[HTTP_MAX_HDR_LEN];
   time_t fini;
} http_client_t;

/*M
  \emph{Clients array.}
**/
http_client_t *client = NULL;
int client_num = 0;

int client_http(struct http_client_s *client, vorbis_stream_t *vorbis);
void client_check(void);
int client_close(struct http_client_s *client);
void client_init(struct http_client_s *client);

/*M
  \emph{Close the connection to client with an error code.}

  Sends back the error code \verb|code| and the comment
  \verb|comment|.
**/
void bad_request(struct http_client_s *client, 
                 unsigned long code, const char *comment, const char *msg) {
   char buf[256];
   int len;

   len = snprintf(buf, 256,
		  "HTTP/1.0 %lu %s\r\nConnection: close\r\n\r\n%s\r\n", 
		  code, comment, msg);
   write(client->fd, buf, len);
}

/*M
  \emph{Accept a HTTP client connection.}

  Accept the connection on the listening socket and fill the client
  structure.
**/
int http_accept(unsigned short sock) {
  unsigned char ip[16];
  unsigned short port;
  int fd, i;
  
  /*M
    Accept the connection
  **/
  if ((fd = net_tcp4_accept_socket(sock, ip, &port)) < 0)
    return 0;

  if (net_tcp4_socket_nonblock(sock) == -1) {
    close(sock);
    return 0;
  }
  
  /*M
    Find an empty client structure and fill it with filedescriptor
    and timeout value
  **/
  for (i = 0; i < client_num; i++) {
    if (client[i].fd == -1) {
      client[i].fd = fd;
      client[i].fini = time(NULL) + HTTP_TIMEOUT;
      return 1;
    }
  }
  
  close(fd);
  
  return 1;
}

/*M
  \emph{Main HTTP server routine.}
  
  Select on the listening socket and all opened client
  sockets. Accept incoming connections and call the
  \verb|http_client| function on active clients.
**/
int http_main(unsigned short sock, vorbis_stream_t *vorbis) {
  int i;
  
  fd_set fds;
  struct timeval tout;
  
  tout.tv_usec = 0;
  tout.tv_sec = 0;
  
  FD_ZERO(&fds);
  
  /*M
    Select the listening HTTP socket.
  **/
  FD_SET(sock, &fds);
  
  /*M
    Select all active client sockets.
  **/
  for (i = 0; i < client_num; i++) {
    if (client[i].fd != -1) {
      FD_SET(client[i].fd, &fds);
    }
  }
  
  if (select(FD_SETSIZE, &fds, NULL, NULL, &tout) < 0) {
    return 0;
  }
  
  /*M
    Accept incoming connections.
  **/
  if (FD_ISSET(sock, &fds)) {
    if (!http_accept(sock))
      return 0;
  }
  
  /*M
     Read incoming client data.
   **/
   for (i = 0; i < client_num; i++) {
      if ((client[i].fd != -1) &&
	  (FD_ISSET(client[i].fd, &fds))) {
	if (client_http(client + i, vorbis) < 0) {
	  client_close(client + i);
	}
      }
   }
   
   /*M
     Check for client timeouts.
   **/
   client_check();
      
   return 1;
}

/*M
  \emph{Read data from client connection.}

  Reads the remaining header data.
**/
int client_http(struct http_client_s *client, vorbis_stream_t *vorbis) {
   time_t        now;
   int  tmp;

   /*M
     Read header data.
   **/
   tmp = read(client->fd, client->buf + client->len, 
         HTTP_MAX_HDR_LEN - client->len - 5);

   if (tmp <= 0)
      return -1;

   client->in += tmp;

   /*M
     A header was already found.
   **/
   if (client->found >= 2)
      return 0;

   now = time(0);

   /*M
     Check if the end of header is in the read data.
   **/
   for (; (client->found < 2) && (client->len < client->in);
         ++client->len) {
      if (client->buf[client->len] == '\r')
         continue;
      if (client->buf[client->len] == '\n')
         ++client->found;
      else
         client->found = 0;
   }

   /*M
     The client request was too short.
   **/
   if (client->len < 10) {
      bad_request(client, 400, "Bad Request", "Not HTTP");
      return -1;
   }
   
   client->buf[client->len] = '\0';

   /*M
     Check if the request is a ``GET /'', else discard the request.
   **/
   if (!strncasecmp(client->buf, "GET /", 5)) {
      if (write(client->fd, "HTTP/1.0 200 OK\r\n\r\n", 19) != 19)
         return -1;

      /* XXX write ogg headers */
      int i;
      for (i = 0; i < vorbis->hdr_pages_cnt; i++) {
	if (write(client->fd, vorbis->hdr_pages[i].raw.data,
		  vorbis->hdr_pages[i].size) != vorbis->hdr_pages[i].size)
	  return -1;
      }
      
      return 1;
   } else {
      bad_request(client, 400, "Bad Request", "Unsupported HTTP Method");
      return -1;
   }

   return 0;
}

/*M
  \emph{Check all clients for timeouts.}
**/
void client_check(void) {
   int i;

   for (i = 0; i < client_num; i++) {
      if ((client[i].fd != -1) && 
          (client[i].found < 2) &&
          (time(NULL) >= client[i].fini)) {
         client_close(client);
      }
   }
}

/*M
  \emph{Destroy a client structure.}
**/
int client_close(struct http_client_s *client) {
   int retval = 0;

   if (client->fd != -1) {
      retval = close(client->fd);
   }

   client_init(client);

   return retval;
}

/*M
  \emph{Initialise a client structure.}
**/
void client_init(struct http_client_s *client) {
   client->fd    = -1;
   client->found = 0;
   client->in    = 0;
   client->len   = 0;
}

/*M
  \emph{Simple HTTP streaming server main loop.}

  The mainloop opens the Vorbis OGG file \verb|filename|, reads each
  audio packet and sends it out using HTTP. After sending
  a packet, the mainloop sleeps for the duration of the packet,
  synchronizing itself when the sleep is not accurate enough. If the
  sleep desynchronizes itself from the stream more than \verb|MAX_WAIT_TIME|,
  the synchronization is reset.
**/
int pogg_mainloop(int sock, char *filename, int quiet) {
  /*M
    Open file for reading.
  **/
  vorbis_stream_t vorbis;
  vorbis_stream_init(&vorbis);
  
  if (!file_open_read(&vorbis.file, filename) ||
      !vorbis_stream_read_hdrs(&vorbis)) {
    fprintf(stderr, "Could not open ogg file: %s\n", filename);
    vorbis_stream_destroy(&vorbis);
    return 0;
  }

  if (!quiet)
    fprintf(stderr, "\rStreaming %s...\n", filename);
  
  static long wait_time = 0;
  static unsigned long page_time = 0, last_time = 0;
  
  ogg_page_t page;
  ogg_page_init(&page);

  /*M
    Cycle through the frames and send them using HTTP.
  **/
  while ((ogg_next_page(&vorbis.file, &page) >= 0) && !finished) {
    /*M
      Get start time for this page iteration.
    **/
    struct timeval tv;
    gettimeofday(&tv, NULL);
    unsigned long start_sec, start_usec;
    start_sec = tv.tv_sec;
    start_usec = tv.tv_usec;
    
    /*M
      Go through HTTP main routine and check for timeouts,
      received data, etc...
    **/
    if (!http_main(sock, &vorbis)) {
      fprintf(stderr, "Http main error\n");
      file_close(&vorbis.file);
      vorbis_stream_destroy(&vorbis);
      ogg_page_destroy(&page);
      return 0;
    }
    
    /*M
      Write frame to HTTP clients.
    **/
    int i;
    for (i = 0; i < client_num; i++) {
      if ((client[i].fd != -1) && (client[i].found >= 2)) {
	int ret;
	
	ret = write(client[i].fd, page.raw.data, page.size);
	
	if (ret != page.size) {
	  fprintf(stderr, "Error writing to client %d\n", i);
	  client_close(client + i);
	}
      }
    }
    last_time = page_time;
    page_time  = ogg_position_to_msecs(&page, vorbis.audio_sample_rate);
    wait_time += page_time - last_time;

    /*M
      Sleep for duration of frame.
    **/
    if (wait_time > 200)
      usleep((wait_time) * 1000);
    
    /*M
      Print information.
    **/
    if (!quiet) {
      if (vorbis.file.size > 0) {
	fprintf(stderr,
		"\r%02ld:%02ld/%02ld:%02ld %7ld/%7ld "
		"(%3ld%%) %3ldkbit/s %4ldb ",
		(page_time/1000) / 60,
		(page_time/1000) % 60,
		(long)((float)(page_time) / 
		       ((float)vorbis.file.offset+1) *
		       (float)vorbis.file.size) / 
		60000,
		(long)((float)(page_time) / 
		       ((float)vorbis.file.offset+1) *
		       (float)vorbis.file.size) / 
		1000 % 60,
		vorbis.file.offset,
		vorbis.file.size,
		(long)(100*(float)vorbis.file.offset/(float)vorbis.file.size),
		vorbis.bitrate_nominal/1000,
		page.size);
      } else {
	fprintf(stderr, "\r%02ld:%02ld %ld %3ldkbit/s %4ldb ",
		(page_time/1000) / 60,
		(page_time/1000) % 60,
		vorbis.file.offset,
		vorbis.bitrate_nominal/1000,
		page.size);
      }
      fflush(stderr);
    }

    /*M
      Get length of iteration.
    **/
    gettimeofday(&tv, NULL);
    unsigned long len =
      (tv.tv_sec - start_sec) * 1000 + (tv.tv_usec - start_usec) / 1000;

    wait_time -= len;
    if (abs(wait_time) > MAX_WAIT_TIME)
      wait_time = 0;
  }

  if (!file_close(&vorbis.file)) {
    fprintf(stderr, "Could not close ogg file %s\n", filename);
    vorbis_stream_destroy(&vorbis);
    ogg_page_destroy(&page);
    return 0;
  }

  vorbis_stream_destroy(&vorbis);
  ogg_page_destroy(&page);
  
  return 1;
}
  
/*M
  \emph{Print usage information.}
**/
static void usage(void) {
  fprintf(stderr, "Usage: ./pogg-http [-s address] [-p port] [-q] [-c clients]");
#ifdef WITH_IPV6
  fprintf(stderr, " [-6]");
#endif
  fprintf(stderr, " files...\n");
  fprintf(stderr, "\t-s address : source address (default 0.0.0.0)\n");
  fprintf(stderr, "\t-p port    : port to listen on (default 8000)\n");
  fprintf(stderr, "\t-q         : quiet\n");
  fprintf(stderr, "\t-c clients : maximal number of clients (default 16)\n");
#ifdef WITH_IPV6
  fprintf(stderr, "\t-6         : use ipv6\n");
#endif
}

/*M
  \emph{HTTP server main routine.}
**/
int main(int argc, char *argv[]) {
   int            retval = 0;
   
   char *address = NULL;
   unsigned short port = 8000;
   int quiet = 0;
   int clients = 16;
   
   if (argc <= 1) {
      usage();
      return 1;
   }

   int c;
   while ((c = getopt(argc, argv, "hs:p:qc:"
#ifdef WITH_IPV6
     "6"
#endif /* WITH_IPV6 */
     )) >= 0) {     
     switch (c) {
#ifdef WITH_IPV6
     case '6':
       use_ipv6 = 1;
       break;
#endif /* WITH_IPV6 */
       
     case 's':
       if (address != NULL)
	 free(address);

       address = strdup(optarg);
       break;

     case 'p':
       port = (unsigned short)atoi(optarg);
       break;

     case 'c':
       clients = (unsigned short)atoi(optarg);
       break;

     case 'q':
       quiet = 1;
       break;

     case 'h':
     default:
       usage();
       retval = EXIT_FAILURE;
       goto exit;
     }
   }

   if (optind == argc) {
     usage();
     retval = EXIT_FAILURE;
     goto exit;
   }

   if (address == NULL) {
#ifdef WITH_IPV6
     if (use_ipv6)
       address = strdup("0::0");
     else
       address = strdup("0");
#else
     address = strdup("0");
#endif /* WITH_IPV6 */
   }

   client = malloc(sizeof(http_client_t) * clients);
   assert(client != NULL);
   client_num = clients;
   
   /*M
     Initialise the client structures.
   **/
   int j;
   for (j = 0; j < clients; j++)
      client_init(client + j);

   if (sig_set_handler(SIGINT, sig_int) == SIG_ERR) {
     retval = EXIT_FAILURE;
     goto exit;
   }

   ogg_init();
   
   /*M
     Open the listening socket.
   **/
   int sock = -1;
#ifdef WITH_IPV6
   if (use_ipv6)
     sock = net_tcp6_listen_socket(address, port);
   else
     sock = net_tcp4_listen_socket(address, port);
#else
   sock  = net_tcp4_listen_socket(address, port);
#endif /* WITH_IPV6 */

   if (sock < 0) {
     perror("Could not create socket");
     retval = EXIT_FAILURE;
     goto exit;
   }

   /*M
     Read in ogg files one after the other.
   **/
   int i;
   for (i=optind; (i<argc) && !finished; i++) {
     assert(argv[i] != NULL);
     unsigned char filename[MAX_FILENAME];
     strncpy(filename, argv[i], MAX_FILENAME - 1);
     filename[MAX_FILENAME - 1] = '\0';

     if (!pogg_mainloop(sock, filename, quiet))
       continue;
   }

exit:
   /*M
     Close all HTTP client connections.
   **/
   if (client != NULL) {
     for (j = 0; j < clients; j++)
       client_close(client + j);
   }

   if (close(sock) < 0)
      perror("close");

   return retval;
}

