/*C
  (c) 2005 bl0rg.net
**/

#include "conf.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/time.h>
#include <time.h>
#include <errno.h>

#ifdef NEED_GETOPT_H__
#include <getopt.h>
#endif /* NEED_GETOPT_H__ */

#include "fec-pkt.h"
#include "aq.h"
#include "fec.h"
#include "network.h"

#include "fec-group.h"
#include "fec-rb.h"
#include "pob-fec.h"

#define FEC_MINSLEEP 20000 /* 200 ms */

/*M
  \emph{Structure to hold client accounting information.}

  This is used to count the number of wrong packets, duplicate
  packets, out of order packets.
**/
typedef struct pob_stat_s {
  /*M
    Number of packets received.
  **/  
  unsigned int rcvd_pkts; 
  /*M
    Number of duplicated packets received.
  **/
  unsigned int lost_pkts; 
  /*M
    Number of bad packets received.
  **/
  unsigned int bad_pkts;
  /*M
    Number of incomplete groups received.
  **/
  unsigned int incomplete_groups;
} pob_stat_t;

static pob_stat_t pob_stats = {
  0, 0, 0, 0
};

/*M
  \emph{Maximal buffer queue size (in packets).}
**/
static unsigned short buffer_size = 16;

/*
  - Receive FEC packets, sort them into current group structure
  - when group complete, decode group into adus and throw them into
    adu_queue
  - if older group packet arrives, discard
  - if newer group packet arrives, ???
  - 
 */

/*M
  \emph{Timestamp of last played group.}
**/
static unsigned long tstamp_last = 0;

int pob_insert_pkt(fec_pkt_t *pkt) {
  assert(pkt != NULL);

  /*M
    We have received a packet.
  **/
  pob_stats.rcvd_pkts++;

  int num = 0;
  /* insert packet into ringbuffer */
  if (fec_rb_length() > 0) {
    /* get index of first packet */
    fec_group_t *first_group = fec_rb_first();
    assert(first_group != NULL);
    assert(first_group->buf != NULL);

    num = net_seqnum_diff(first_group->seq, pkt->hdr.group_seq, 1 << 8);
  }

  if (!fec_rb_insert_pkt(pkt, num)) {
#ifdef DEBUG
    fprintf(stderr, "ring buffer full\n");
#endif

    fec_rb_clear();
    tstamp_last = pkt->hdr.group_tstamp;
    return pob_insert_pkt(pkt);
  } else {
    return 1;
  }
}

int pob_fec_recv_pkt(int sock, fec_pkt_t *pkt) {
  /*M
    Timeout in order to flush next frame to player.
  **/
  struct timeval t_out;
  t_out.tv_sec  = 0;
  t_out.tv_usec = FEC_MINSLEEP;

  /*M
    Listen on receiving socket.
  **/
  fd_set fds;
  FD_ZERO(&fds);
  FD_SET(sock, &fds);

  /*M
    Wait for network input or for timeout.
  **/
  int ret = select(sock + 1, &fds, NULL, NULL, &t_out);
  
  /*M
    Check for interrupted system call.
  **/
  if (ret == -1) {
    if ((errno == EINTR) || (errno == EAGAIN)) {
      return 0; /* timeout */
    } else {
      return -1; /* error */
    }
  }
  
  /*M
    If there is network input, read incoming packet.
  **/
  if (FD_ISSET(sock, &fds)) {
    fec_pkt_init(pkt);
    
    if (fec_pkt_read(pkt, sock) <= 0) {
      return -1;
    } else {
      return 1;
    }
  }

  return 0;
}

/*M
  \emph{Simple FEC streaming client main loop.}

  The mainloop calls the prebuffering routine each time the frame
  queue is empty, then receives the packets, sorts them into groups,
  decodes the groups and converts the ADUs into frames.
**/
int pob_mainloop(int sock, int quiet) {
  int retval = 0;
  
  /*M
    Initialize the ADU queue;
  **/
  aq_t frame_queue;
  aq_init(&frame_queue);

  fec_pkt_t pkt;
  int finished = 0;

  while (!finished) {
    static int prebuffering = 0;

    if (fec_rb_cnt == 0) {
      prebuffering = 1;
    }
      
    switch (pob_fec_recv_pkt(sock, &pkt)) {
    case -1:
      finished = 1;
      retval = -1;
      continue;
      
    case 0:
      break;
      
    default:
      /*M
        Insert the packet into the FEC buffer.
      **/
      if (!pob_insert_pkt(&pkt)) {
        finished = 1;
        retval = -1;
        goto exit;
      }
    }

    static unsigned long time_last;

    unsigned long time_now;
    /*M
      Get the current time to see which packets have to be written to
      standard out.
    **/
    struct timeval tv;
    gettimeofday(&tv, NULL);
    time_now = tv.tv_sec * 1000000 + tv.tv_usec;

    if (prebuffering == 1) {
      if (fec_rb_cnt >= (fec_rb_size / 2)) {
        fec_group_t *first_group = fec_rb_first();
        assert(first_group != NULL);
        assert(first_group->buf != NULL);

        tstamp_last = first_group->tstamp;
        time_last = time_now;

        prebuffering = 0;
        if (!quiet)
          fprintf(stderr, "\n");
      } else {
        /*M
          Print prebuffering information.
        **/
        if (!quiet)
          fprintf(stderr, "Prebuffering: %.2f%%\r",
                  (float)fec_rb_cnt / (fec_rb_size / 2.0) * 100.0);
        
        continue;
      }        
    }

    /*M
      Print client information.
    **/
    if (!quiet) {
      static int count = 0;
      if ((count++ % 10) == 0) {
        fprintf(stderr, "pkts: %.8u\tdrop: %.6u\tincomplete: %.6u\tbuf: %.6u len:%.4u\t\r",
                pob_stats.rcvd_pkts,
                pob_stats.lost_pkts,
                pob_stats.incomplete_groups,
                fec_rb_cnt,
                fec_rb_length());
      }
    }

#ifdef DEBUG
    fec_rb_print();
#endif

    unsigned long tstamp_now = tstamp_last + (time_now - time_last);

    while (fec_rb_length() > 0) {
      fec_group_t *group = fec_rb_first();
      assert(group != NULL);

      if (group->buf != NULL) {
        /* boeser hack XXX */
        if (group->tstamp > (tstamp_now + 3000))
          break;

        /* decode group into adu queue */
        if (!fec_group_decode_to_adus(group, &frame_queue)) {
          fprintf(stderr, "Could not decode group\n");
          /* XXX really continue? */
        }

        pob_stats.lost_pkts += group->fec_n - group->rcvd_pkts;
        if (group->rcvd_pkts < group->fec_k)
          pob_stats.incomplete_groups++;

        mp3_frame_t *frame;
        while ((frame = aq_get_frame(&frame_queue)) != NULL) {
          memset(frame->raw, 0, 4 + frame->si_size);
          
          /*M
            Write packet payload.
          **/
          if (!mp3_fill_hdr(frame) ||
              !mp3_fill_si(frame) ||
              (write(STDOUT_FILENO,
                     frame->raw,
                     frame->frame_size) < (int)frame->frame_size)) {
            fprintf(stderr, "Error writing to stdout\n");
            free(frame);
            
            retval = 0;
            goto exit;
          }

          free(frame);
        }

        tstamp_last = tstamp_now;
        time_last = time_now;
      } 

      fec_rb_pop();
    }
  }

 exit:
  /*M
    Destroy the ADU queue.
  **/
  aq_destroy(&frame_queue);
  
  return retval;
}

/*M
  \emph{Print FEC client usage.}
**/
static void usage(void) {
  fprintf(stderr, "Usage: ./pob [-s address] [-p port] [-b size] [-q]\n");
  
  fprintf(stderr, "\t-s address : destination address (default 0.0.0.0)\n");
  fprintf(stderr, "\t-p port    : destination port (default 1500)\n");
  fprintf(stderr, "\t-b size    : maximal number of fec groups in buffer (default 16)\n");
  fprintf(stderr, "\t-q         : quiet\n");

}

/*M
  \emph{FEC RTP client entry routine.}
**/
int main(int argc, char *argv[]) {
  char           *address = NULL;
  unsigned short port = 1500;
  int            retval = EXIT_SUCCESS, quiet = 0;

  /*M
    Process the command line arguments.
  **/
  int c;
  while ((c = getopt(argc, argv, "hs:p:b:t:q")) >= 0) {
    switch (c) {
    case 's':
      if (address != NULL)
        free(address);

      address = strdup(optarg);
      break;

    case 'p':
      port = atoi(optarg);
      break;

    case 'b':
      buffer_size = (unsigned short)atoi(optarg);
      break;

    case 'q':
      quiet = 1;
      break;

    case 'h':
    default:
      usage();
      retval = EXIT_SUCCESS;
      goto exit;
    }
  }

  /*M
    Initialize the ring buffer.
  **/
  fec_rb_init(buffer_size);

  if (address == NULL) {
#ifdef WITH_IPV6
    address = strdup("ff02::4");
#else
    address = strdup("0.0.0.0");
#endif /* WITH_IPV6 */
  }
  
  /*M
    Create the receiving socket.
  **/
  int sock;
#ifdef WITH_IPV6
  sock = net_udp6_recv_socket(address, port);
#else
  sock = net_udp4_recv_socket(address, port);  
#endif /* WITH_IPV6 */
  if (sock < 0) {
    fprintf(stderr, "Could not open socket\n");
    retval = EXIT_FAILURE;
    goto exit;
  }

  if (!pob_mainloop(sock, quiet))
    retval = EXIT_FAILURE;

  if (close(sock) < 0)
    perror("close");
  
 exit:
  fec_rb_destroy();
  
  if (address != NULL)
    free(address);

  return retval;
}

/*C
**/
