/*C
  (c) 2005 bl0rg.net
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
#include "mp3.h"
#include "network.h"
#include "signal.h"
#include "http.h"

#ifdef WITH_IPV6
static int use_ipv6 = 0;
#endif /* WITH_IPV6 */

static int finished = 0;

#define MP3_BUF_LEN 65535

/* Maximal synchronization latency for sending packets in usecs. */
#define MAX_WAIT_TIME (1 * 1000 * 1000)

#define MAX_FILENAME 256

static void sig_int(int signo) {
  finished = 1;
}

/*M
  \emph{Simple HTTP streaming server main loop.}

  The mainloop opens the MPEG Audio file \verb|filename|, reads each
  frame into an rtp packet and sends it out using HTTP. After sending
  a packet, the mainloop sleeps for the duration of the packet,
  synchronizing itself when the sleep is not accurate enough. If the
  sleep desynchronizes itself from the stream more than \verb|MAX_WAIT_TIME|,
  the synchronization is reset.
**/
int poc_mainloop(http_server_t *server, char *filename, int quiet) {
  /*M
    Open file for reading.
  **/
  file_t     mp3_file;
  if (!file_open_read(&mp3_file, filename)) {
    fprintf(stderr, "Could not open mp3 file: %s\n", filename);
    return 0;
  }

  if (!quiet)
    fprintf(stderr, "\rStreaming %s...\n", filename);
  
  static long wait_time = 0;
  unsigned long frame_time = 0;
  
  mp3_frame_t    frame;

  /*M
    Cycle through the frames and send them using HTTP.
  **/
  while ((mp3_next_frame(&mp3_file, &frame) >= 0) && !finished) {
    /*M
      Get start time for this frame iteration.
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
    if (!http_server_main(server, NULL)) {
      fprintf(stderr, "Http main error\n");
      return 0;
    }
    
    /*M
      Write frame to HTTP clients.
    **/
    int i;
    for (i = 0; i < server->num_clients; i++) {
      if ((server->clients[i].fd != -1) &&
          (server->clients[i].found >= 2)) {
        int ret;
        
        ret = write(server->clients[i].fd, frame.raw, frame.frame_size);
        
        if (ret != frame.frame_size) {
          fprintf(stderr, "Error writing to client %d\n", i);
          http_client_close(server, server->clients + i);
        }
      }
    }
    frame_time += frame.usec;
    wait_time += frame.usec;
    
    /*M
      Sleep for duration of frame.
    **/
    if (wait_time > 1000)
      usleep(wait_time);
    
    /*M
      Print information.
    **/
    if (!quiet) {
      static int count = 0;
      if ((count++) % 10 == 0) {
        if (mp3_file.size > 0) {
          fprintf(stderr, "\r%02ld:%02ld/%02ld:%02ld %7ld/%7ld (%3ld%%) %3ldkbit/s %4ldb ",
                  (frame_time/1000000) / 60,
                  (frame_time/1000000) % 60,

                  (long)((float)(frame_time/1000) / 
                         ((float)mp3_file.offset+1) * (float)mp3_file.size) / 
                  60000,
                  (long)((float)(frame_time/1000) / 
                         ((float)mp3_file.offset+1) * (float)mp3_file.size) / 
                  1000 % 60,
                  mp3_file.offset,
                  mp3_file.size,
                  (long)(100*(float)mp3_file.offset/(float)mp3_file.size),
                  frame.bitrate,
                  frame.frame_size);
        } else {
          fprintf(stderr, "\r%02ld:%02ld %ld %3ldkbit/s %4ldb ",
                  (frame_time/1000000) / 60,
                  (frame_time/1000000) % 60,
                  mp3_file.offset,
                  frame.bitrate,
                  frame.frame_size);
        }
      }
      fflush(stderr);
    }

    /*M
      Get length of iteration.
    **/
    gettimeofday(&tv, NULL);
    unsigned long len =
      (tv.tv_sec - start_sec) * 1000000 + (tv.tv_usec - start_usec);

    wait_time -= len;
    if (abs(wait_time) > MAX_WAIT_TIME)
      wait_time = 0;
  }
  
  if (!file_close(&mp3_file)) {
    fprintf(stderr, "Could not close mp3 file %s\n", filename);
    return 0;
  }

  return 1;
}
  
/*M
  \emph{Print usage information.}
**/
static void usage(void) {
  fprintf(stderr, "Usage: ./poc-http [-s address] [-p port] [-q] [-c clients]");
#ifdef WITH_IPV6
  fprintf(stderr, " [-6]");
#endif
  fprintf(stderr, " files...\n");
  fprintf(stderr, "\t-s address : source address (default 0.0.0.0)\n");
  fprintf(stderr, "\t-p port    : port to listen on (default 8000)\n");
  fprintf(stderr, "\t-q         : quiet\n");
  fprintf(stderr, "\t-c clients : maximal number of clients (default 0, unlimited)\n");
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
   int max_clients = 0;
   http_server_t server;

   http_server_reset(&server);
   
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
       max_clients = (unsigned short)atoi(optarg);
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

   if (!http_server_init(&server, HTTP_MIN_CLIENTS,
                         max_clients, NULL, sock)) {
     fprintf(stderr, "Could not initialise HTTP server\n");
     retval = EXIT_FAILURE;
     goto exit;
   }

   if (sig_set_handler(SIGINT, sig_int) == SIG_ERR) {
     retval = EXIT_FAILURE;
     goto exit;
   }
   
   /*M
     Read in mp3 files one after the other.
   **/
   int i;
   for (i=optind; (i<argc) && !finished; i++) {
     assert(argv[i] != NULL);
     unsigned char filename[MAX_FILENAME];
     strncpy(filename, argv[i], MAX_FILENAME - 1);
     filename[MAX_FILENAME - 1] = '\0';

     if (!poc_mainloop(&server, filename, quiet))
       continue;
   }

exit:
   http_server_close(&server);

   return retval;
}

