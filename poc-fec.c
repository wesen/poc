/*C
  (c) 2003 Institut fuer Telematik, Universitaet Karlsruhe
**/

#include "conf.h"

#ifdef NEED_GETOPT_H__
#include <getopt.h>
#endif

#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/time.h>
#include <sys/wait.h>
#include <string.h>

#include "mp3.h"
#include "network.h"
#include "fec-pkt.h"
#include "fec.h"
#include "pack.h"
#include "aq.h"
#include "signal.h"

#ifdef WITH_IPV6
static int use_ipv6 = 0;
#endif /* WITH_IPV6 */

#define MAX_FILENAME 256

#ifdef DEBUG_PLOSS
int ploss_rate = 20;
#endif /* DEBUG_PLOSS */

/*M
  \emph{Maximal synchronization latency for sending packets.}

  In usecs.
**/
#define MAX_WAIT_TIME (1 * 1000 * 1000)

static int finished = 0;

int quiet = 0;

unsigned char fec_k = 20;
unsigned char fec_n = 25;

fec_pkt_t pkt;

static void sig_int(int signo) {
  finished = 1;
}

/*M
**/
int poc_encoder(int sock, char *filename) {
  int retval = 1;
  
  /*M
    Open file for reading.
  **/
  file_t mp3_file;
  if (!file_open_read(&mp3_file, filename)) {
    fprintf(stderr, "Could not open mp3 file: %s\n", filename);
    return 0;
  }

  /*M
    Initialize the FEC parameters.
  **/
  fec_t *fec = fec_new(fec_k, fec_n);

  aq_t adu_queue;
  aq_init(&adu_queue);

  adu_t *in_adus[fec_k];
  unsigned int cnt = 0;

  static long wait_time = 0;
  static unsigned long fec_time = 0;
  static unsigned long fec_time2 = 0;

  /*M
    Get start time.
  **/
  struct timeval tv;
  gettimeofday(&tv, NULL);
  unsigned long start_sec, start_usec;
  start_sec = tv.tv_sec;
  start_usec = tv.tv_usec;
  
  /*M
    Get next MP3 frame and queue it into the ADU queue.
  **/
  mp3_frame_t mp3_frame;
  while ((mp3_next_frame(&mp3_file, &mp3_frame) > 0) && !finished) {
    if (aq_add_frame(&adu_queue, &mp3_frame) > 0) {
      /* a new ADU has been produced */
      in_adus[cnt] = aq_get_adu(&adu_queue);
      assert(in_adus[cnt] != NULL);

      /* check if the FEC group is complete */
      if (++cnt == fec_k) {
	unsigned int max_len = 0;
	unsigned long group_duration = 0;

	int i;
	for (i = 0; i < fec_k; i++) {
	  unsigned int adu_len = mp3_frame_size(in_adus[i]);

	  if (adu_len > max_len)
	    max_len = adu_len;

	  group_duration += in_adus[i]->usec;
	}

	fec_time += group_duration;

	assert(max_len < FEC_PKT_PAYLOAD_SIZE);

        /* Encode the FEC group */
	unsigned char *in_ptrs[fec_k];
	unsigned char buf[fec_k * max_len];
	unsigned char *ptr = buf;
	for (i = 0; i < fec_k; i++) {
	  unsigned int adu_len = mp3_frame_size(in_adus[i]);

	  in_ptrs[i] = ptr;
	  memcpy(ptr, in_adus[i]->raw, adu_len);
	  if (adu_len < max_len)
	    memset(ptr + adu_len, 0, max_len - adu_len);
	  ptr += max_len;
	}

	for (i = 0; i < fec_n; i++) {
	  pkt.hdr.packet_seq = i;
	  pkt.hdr.fec_k = fec_k;
	  pkt.hdr.fec_n = fec_n;
	  pkt.hdr.fec_len = max_len + 2;
	  pkt.hdr.group_tstamp = fec_time;
	  
	  fec_encode(fec, in_ptrs, pkt.payload, i, max_len);

	  if (i < fec_k) {
	    pkt.hdr.len = mp3_frame_size(in_adus[i]);
	  } else {
	    pkt.hdr.len = max_len;
	  }

	  /*M
	    Simulate packet loss.
	  **/
#ifdef DEBUG_PLOSS
	  if ((random() % 100) >= ploss_rate ) {
#endif /* DEBUG_PLOSS */

#ifdef DEBUG
	    fprintf(stderr,
		    "sending fec packet group stamp %ld, gseq %d, pseq %d, size %d\n",
		    pkt.hdr.group_tstamp, pkt.hdr.group_seq, pkt.hdr.packet_seq, pkt.hdr.len);
#endif
	    
	    /* send rtp packet */
	    if (fec_pkt_send(&pkt, sock) < 0) {
	      perror("Error while sending packet");

	      retval = 0;
	      goto exit;
	    }
#ifdef DEBUG_PLOSS
	  }
#endif /* DEBUG_PLOSS */

	  /*M
	    Update the time we have to wait.
	  **/
	  wait_time += (group_duration / fec_n);
	  fec_time2 += (group_duration / fec_n);

	  /*M
	    Sender synchronisation (\verb|sleep| until the next
	    packet has to be sent.
	  **/
	  if (wait_time > 1000)
	    usleep(wait_time);

	  if (!quiet) {	
            static unsigned int count = 0;
            if ((count++ % 10) == 0) {
              if (mp3_file.size > 0) {
                unsigned long bytes_left = mp3_file.size - mp3_file.offset;
                double msec_per_byte =
                  (fec_time2 / 1000.0) / (double)mp3_file.offset;
                unsigned long msec_left = msec_per_byte * (double)bytes_left;
                
                fprintf(stdout,
                        "\r%02ld:%02ld/%02ld:%02ld %7ld/%7ld (%3ld%%) ",
                        (fec_time2/1000000) / 60,
                        (fec_time2/1000000) % 60,
                        
                        (msec_left / 1000) / 60,
                        (msec_left / 1000) % 60,
                        
                        mp3_file.offset,
                        mp3_file.size,
                        
                        (long)(100*(float)mp3_file.offset/(float)mp3_file.size));
              } else {
                fprintf(stdout, "\r%02ld:%02ld %ld ",
                        (fec_time2/1000000) / 60,
                        (fec_time2/1000000) % 60,
                        mp3_file.offset);
              }
              fflush(stdout);
            }
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

	pkt.hdr.group_seq++;

	for (i = 0; i < fec_k; i++)
	  free(in_adus[i]);

	cnt = 0;
      }
    }
  }

  int i;
 exit:
  for (i = 0; i < cnt; i++)
    free(in_adus[i]);
  
  aq_destroy(&adu_queue);
  fec_free(fec);

  file_close(&mp3_file);

  return retval;
}

/*M
  \emph{Print usage information.}
**/
static void usage(void) {
  fprintf(stderr,
	  "Usage: ./poc-fec [-s address] [-p port] [-k fec_k] [-n fec_n] [-q] [-t ttl]");
#ifdef WITH_IPV6
  fprintf(stderr, " [-6]");
#endif /* WITH_IPV6 */
  fprintf(stderr, " files...\n");
  fprintf(stderr, "\t-s address : destination address (default 224.0.1.23 or ff02::4)\n");
  fprintf(stderr, "\t-p port    : destination port (default 1500)\n");
  fprintf(stderr, "\t-q         : quiet\n");
  fprintf(stderr, "\t-t ttl     : multicast ttl (default 1)\n");
  fprintf(stderr, "\t-k fec_k   : FEC k parameter (default 20)\n");
  fprintf(stderr, "\t-n fec_n   : FEC n parameter (default 25)\n");
#ifdef WITH_IPV6
  fprintf(stderr, "\t-6         : use ipv6\n");
#endif /* WITH_IPV6 */
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

  /*M
    Process the command line arguments.
  **/
  int c;
  while ((c = getopt(argc, argv, "hs:p:t:qP:k:n:"
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

    case 'q':
      quiet = 1;
      break;

    case 't':
      ttl = (unsigned int)atoi(optarg);
      break;

    case 'k':
      fec_k = (unsigned int)atoi(optarg);
      break;

    case 'n':
      fec_n = (unsigned int)atoi(optarg);
      break;
      
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

  if (fec_n <= fec_k) {
    fprintf(stderr, "fec_n must be bigger than fec_k\n");
    retval = EXIT_FAILURE;
    goto exit;
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

  fec_pkt_init(&pkt);
  
  /*M
    Go through all files given on command line and stream them.
  **/
  int i;
  for (i = optind; (i < argc) && !finished; i++) {
    assert(argv[i] != NULL);
    unsigned char filename[MAX_FILENAME];
    strncpy(filename, argv[i], MAX_FILENAME - 1);
    filename[MAX_FILENAME - 1] = '\0';
    
    if (!poc_encoder(sock, filename))
      continue;
  }
  
 exit:
  if (address != NULL)
    free(address);
  
  return retval;
}

/*M
 */
