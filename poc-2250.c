/*C
 (c) 2005 bl0rg.net
**/

#include "conf.h"

#ifdef WITH_OPENSSL
#include <openssl/rsa.h>
#include <openssl/evp.h>
#include <openssl/pem.h>
#endif

#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/time.h>
#include <string.h>
#include <errno.h>

#ifdef NEED_GETOPT_H__
#include <getopt.h>
#endif

#include "mp3.h"
#include "network.h"
#include "rtp.h"
#include "sig_set_handler.h"
#include "file.h"

#ifdef WITH_IPV6
static int use_ipv6 = 0;
#endif /* WITH_IPV6 */

#define MAX_FILENAME 256

#ifdef WITH_OPENSSL
RSA *rsa = NULL;
#endif /* WITH_OPENSSL */

#ifdef DEBUG_PLOSS
int ploss_rate = 20;
#endif /* DEBUG_PLOSS */

static int finished = 0;

/*M
  \emph{Maximal synchronization latency for sending packets.}

  In usecs.
**/
#define MAX_WAIT_TIME (1 * 1000 * 1000)

rtp_pkt_t pkt;

static void sig_int(int signo) {
  finished = 1;
}

/*M
  \emph{Simple RTP RFC2250 streaming server main loop.}

  The mainloop opens the MPEG Audio file \verb|filename|, reads each frame
  into an rtp packet and sends it out using the UDP socket
  \verb|sock|. After sending a packet, the mainloop sleeps for the duration
  of the packet, synchronizing  itself when the sleep is not accurate
  enough. If the sleep desynchronizes itself from the stream more
  than \verb|MAX_WAIT_TIME|, the synchronization is reset.
**/
int poc_mainloop(int sock, char *filename, int quiet) {
  /*M
    Open file for reading.
  **/
  file_t mp3_file;
  if (!file_open_read(&mp3_file, filename)) {
    fprintf(stderr, "Could not open mp3 file: %s\n", filename);
    return 0;
  }

  /*M
    Set the M-bit of packetheader.
  **/
  pkt.b.m = 1;

  static long wait_time = 0;
  unsigned long rtp_time = 0;

  /*M
    Get start time.
  **/
  struct timeval tv;
  gettimeofday(&tv, NULL);
  unsigned long start_sec, start_usec;
  start_sec = tv.tv_sec;
  start_usec = tv.tv_usec;
  
  /*M
    Cycle through the frames and send them using RTP.
  **/
  mp3_frame_t mp3_frame;
  while ((mp3_next_frame(&mp3_file, &mp3_frame) > 0) && !finished) {
    /*M
      Fill rtp packet.
    **/
    pkt.timestamp = (rtp_time) / 11.1111;
    memcpy(pkt.data + pkt.hlen, mp3_frame.raw, mp3_frame.frame_size);
    pkt.length = mp3_frame.frame_size;

    /*M
      Sign the packet if Openssl is activated.
    **/
#ifdef WITH_OPENSSL
    if (rsa != NULL) {
      if (!rtp_pkt_sign(&pkt, rsa)) {
        fprintf(stderr, "\nCould not sign packet\n");
        return 0;
      }
      pkt.b.pt = RTP_PT_SMPA;
    }
#endif

    /*M
      Simulate packet loss.
    **/
#ifdef DEBUG_PLOSS
    if (((random() % 100) >= ploss_rate) != 0) {
#endif /* DEBUG_PLOSS */
      /* send rtp packet */
      if (rtp_pkt_send(&pkt, sock) < 0) {
	if (errno == ENOBUFS) {
	  fprintf(stderr, "Output buffers full, waiting...\n");
	} else {
	  perror("Error while sending packet");
	  return 0;
	}
      }
#ifdef DEBUG_PLOSS
    }
#endif /* DEBUG_PLOSS */
    
    /*M
      Set M-bit to $0$ after sending the first frame (receiver
      synchronisation).
    **/
    pkt.b.m = 0;

    /*M
      Increment the MPEG Timestamp.
    **/
    rtp_time += mp3_frame.usec;
    wait_time += mp3_frame.usec;

    /*M
      Sender synchronisation (\verb|sleep| until the next frame has
      to be sent.
    **/
    if (wait_time > 0)
      usleep(wait_time);
    
    /*M
      Print sender information.
    **/
    if (!quiet) {
      static int count = 0;
      if ((count++ % 10) == 0) {
        if (mp3_file.size > 0) {
          fprintf(stdout,
                  "\r%02ld:%02ld/%02ld:%02ld %7ld/%7ld (%3ld%%) %3ldkbit/s %4ldb ",
                  (rtp_time/1000000) / 60,
                  (rtp_time/1000000) % 60,
                  (long)((float)(rtp_time/1000) / 
                         ((float)mp3_file.offset+1) * (float)mp3_file.size) / 
                  60000,
                  (long)((float)(rtp_time/1000) / 
                         ((float)mp3_file.offset+1) * (float)mp3_file.size) / 
                  1000 % 60,
                  mp3_file.offset,
                  mp3_file.size,
                  (long)(100*(float)mp3_file.offset/(float)mp3_file.size),
                  mp3_frame.bitrate,
                  mp3_frame.frame_size);
        } else {
          fprintf(stdout, "\r%02ld:%02ld %ld %3ldkbit/s %4ldb ",
                  (rtp_time/1000000) / 60,
                  (rtp_time/1000000) % 60,
                  mp3_file.offset,
                  mp3_frame.bitrate,
                  mp3_frame.frame_size);
        }
      }
      fflush(stdout);
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

    start_sec = tv.tv_sec;
    start_usec = tv.tv_usec;
  }

  /*M
    Close the MPEG file.
  **/
  if (!file_close(&mp3_file)) {
    fprintf(stderr, "Could not close mp3 file\n");
    return 0;
  }

  return 1;
}

/*M
  \emph{Print usage information.}
**/
static void usage(void) {
#ifdef WITH_OPENSSL
  fprintf(stderr,
          "Usage: ./poc [-s address] [-p port] [-q] [-t ttl] [-c pem] files...\n");
#else
  fprintf(stderr,
          "Usage: ./poc [-s address] [-p port] [-q] [-t ttl] files...\n");
#endif
  
  fprintf(stderr, "\t-s address : destination address (default 224.0.1.23)\n");
  fprintf(stderr, "\t-p port    : destination port (default 1500)\n");
  fprintf(stderr, "\t-q         : quiet\n");
  fprintf(stderr, "\t-t ttl     : multicast ttl (default 1)\n");
#ifdef WITH_OPENSSL
  fprintf(stderr, "\t-c pem     : sign with private RSA key\n");
#endif /* WITH_OPENSSL */
#ifdef DEBUG_PLOSS
  fprintf(stderr, "\t-P ploss   : packet loss interval\n");
#endif /* DEBUG_PLOSS */  
}

/*M
  \emph{Main server routine.}

  Calls the mainloop for each filename given on the command line.
**/
int main(int argc, char *argv[]) {
  int retval = 0;

  char           *address = NULL;
  unsigned short port     = 1500;
  unsigned int   ttl      = 1;
  int            quiet    = 0;

  /*M
    Process the command line arguments.
  **/
  int c;
#ifdef WITH_OPENSSL
  while ((c = getopt(argc, argv, "hs:p:t:qc:P:")) >= 0) {
#else
  while ((c = getopt(argc, argv, "hs:p:t:qP:")) >= 0) {
#endif
    switch (c) {
    case 's':
      if (address != NULL)
        free(address);

      address = strdup(optarg);
      break;

    case 'p':
      port = (unsigned short)atoi(optarg);
      break;

    case 'q':
      quiet = 1;
      break;

    case 't':
      ttl = (unsigned int)atoi(optarg);
      break;

      /*M
        If Openssl is used, read in the RSA key.
      **/
#ifdef WITH_OPENSSL
    case 'c':
      {
        if (rsa != NULL) {
          RSA_free(rsa);
          rsa = NULL;
        }

        FILE *f = NULL;
        if (!(f = fopen(optarg, "r"))) {
          fprintf(stderr,
                  "Could not open private key %s\n",
                  optarg);
          retval = EXIT_FAILURE;
          goto exit;
        }

        if (!(rsa = PEM_read_RSAPrivateKey(f, NULL, NULL, NULL))) {
          fprintf(stderr,
                  "Could not read private key %s\n",
                  optarg);
          fclose(f);
          retval = EXIT_FAILURE;
          goto exit;
        }

        fclose(f);

        OpenSSL_add_all_digests();
        break;
      }
#endif /* WITH_OPENSSL */

#ifdef DEBUG_PLOSS
    case 'P':
      {
        static struct timeval tv;
        gettimeofday(&tv, NULL);
        srandom(tv.tv_sec);
        ploss_rate = atoi(optarg);
        break;
      }
#endif /* DEBUG_PLOSS */

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
      address = strdup("ff02::4");
    else
      address = strdup("224.0.1.23");
#else
    address = strdup("224.0.1.23");
#endif /* WITH_IPV6 */
  }


  if (sig_set_handler(SIGINT, sig_int) == SIG_ERR) {
    retval = EXIT_FAILURE;
    goto exit;
  }
  
  /*M
    Open the sending socket.
  **/
  int sock;
#ifdef WITH_IPV6
   if (use_ipv6)
     sock = net_udp6_send_socket(address, port, ttl);
   else
     sock = net_udp4_send_socket(address, port, ttl);
#else
   sock  = net_udp4_send_socket(address, port, ttl);
#endif /* WITH_IPV6 */
   if (sock < 0) {
    fprintf(stderr, "Could not open socket\n");
    retval = EXIT_FAILURE;
    goto exit;
  }

  rtp_rfc2250_pkt_init(&pkt);
  pkt.b.pt = RTP_PT_MPA;
  
  /*M
    Go through all files given on command line and stream them.
  **/
  int i;
  for (i = optind; (i < argc) && !finished; i++) {
    assert(argv[i] != NULL);
    char filename[MAX_FILENAME];
    strncpy(filename, argv[i], MAX_FILENAME - 1);
    filename[MAX_FILENAME - 1] = '\0';

    if (!poc_mainloop(sock, filename, quiet))
      continue;
  }

 exit:
#ifdef WITH_OPENSSL
  if (rsa != NULL)
    RSA_free(rsa);
#endif
  
  if (address != NULL)
    free(address);
  
  return retval;
}
