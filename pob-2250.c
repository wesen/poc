/*C
  (c) 2003 Institut fuer Telematik, Universitaet Karlsruhe
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

#ifdef WITH_OPENSSL
#include <openssl/x509.h>
#include <openssl/rsa.h>
#include <openssl/evp.h>
#include <openssl/pem.h>
#endif /* WITH_OPENSSL */

#include "rtp.h"
#include "network.h"
#include "dlist.h"

#ifdef WITH_OPENSSL
RSA *rsa = NULL;
#endif

#define RTP_MINSLEEP 20000 /* 200 ms */

/*M
  \emph{Structure to hold client accounting information.}

  This is used to count the number of wrong packets, duplicate
  packets, out of order packets, unauthenticated packets.
**/
typedef struct pob_stat_s {
  /*M
    Number of packets received.
  **/  
  unsigned int rcvd_pkts; 
  /*M
    Number of out of order packets received.
  **/
  unsigned int ooo_pkts; 
  /*M
    Number of duplicated packets received.
  **/
  unsigned int dup_pkts; 
  /*M
    Number of bad packets received.
  **/
  unsigned int bad_pkts;
#ifdef WITH_OPENSSL
  /*M
    Badly signed packets.
  **/
  unsigned int sign_pkts;
#endif
  /*M
    Number of buffer overflows.
  **/
  unsigned int buf_ofs; 
  /*M
    Number of buffer underflows.
  **/
  unsigned int buf_ufs;
} pob_stat_t;

static pob_stat_t pob_stats = {
  0, 0, 0, 0, 0
#ifdef WITH_OPENSSL
  ,0
#endif
};

/*M
  \emph{Maximal buffer queue size (in RTP packets).}
**/
static unsigned short buffer_size = 128;

/*M
  \emph{Prebuffering time (in milliseconds).}
**/
static unsigned short buffer_time = 2000;

/*M
  \emph{Compare the sequence number of two RTP packets.}

  This function is used in order to enqueue received RTP packets in
  the correct sequence number order in the buffering queue.
**/
static int pob_rtp_pkt_cmp_seq(rtp_pkt_t *pkt, rtp_pkt_t *list_pkt) {
  assert(pkt != NULL);
  assert(list_pkt != NULL);
  
  return (list_pkt->b.seq >= pkt->b.seq);
}

/*M
  \emph{Insert a RFC2250 RTP packet at the right position in a packet
  list.}

  Freshly allocates a RTP packet structure. Returns 1 on success, 0
  on buffer overflow, -1 on error.
**/
int pob_insert_pkt(dlist_head_t *pkt_list, rtp_pkt_t *pkt) {
  assert(pkt_list != NULL);
  assert(pkt != NULL);

  /*M
    We have received a packet : update statistics.
  **/
  pob_stats.rcvd_pkts++;

  /*M
    Verify packet if Openssl is activated.
  **/
#ifdef WITH_OPENSSL
  if (rsa) {
    if ((pkt->b.pt != RTP_PT_SMPA) ||
	!rtp_pkt_verify(pkt, rsa)) {
      pob_stats.sign_pkts++;
      return 0;
    }
  }
#endif

  /*M
    Check if the buffer list is full.
  **/
  if (pkt_list->num >= buffer_size) {
    pob_stats.buf_ofs++;
    return 0;
  }

#ifdef DEBUG
  fprintf(stderr, "Packet queue: %d, tstamp %lu\n", pkt_list->num, pkt->timestamp);
#endif
  
  /*M
    Find first node in list with bigger sequence number in order to
    insert the received packet at the correct position.
  **/
  dlist_t *node;
  node = dlist_search(pkt_list, pkt, DLIST_OP2(pob_rtp_pkt_cmp_seq));
  
  /*M
    Check for duplicate packets.
  **/
  if ((node != NULL) &&
      (((rtp_pkt_t *)node->data)->b.seq == pkt->b.seq)) {
    pob_stats.dup_pkts++;
    return 1;
  }
  
  /*M
    If we found a packet with bigger sequence number in list, we
    have received an out of order packet.
  **/
  if (node != NULL)
    pob_stats.ooo_pkts++;

  /*M
    Copy packet to a newly heap allocated packet.
  **/
  rtp_pkt_t *tmp_pkt;
  tmp_pkt = malloc(sizeof(rtp_pkt_t));
  assert(tmp_pkt != NULL);
  memcpy(tmp_pkt, pkt, sizeof(rtp_pkt_t));

  /*M
     Insert the new packet at the right position.
  **/
  if (node) {
    if (!dlist_ins_before(pkt_list, node, tmp_pkt))
      goto error;
  } else {
    if (!dlist_ins_end(pkt_list, tmp_pkt))
      goto error;
  }
  
  return 1;

 error:
  free(tmp_pkt);
  return -1;
}

/*M
  \emph{Prebuffer the MPEG stream.}

  Receive packets until the total time exceeds the buffer time. As
  MPEG frame duration is fixed, this will lead to buffering a bit
  more than requested.
**/
int pob_prebuffer(int sock, dlist_head_t *pkt_list, int quiet,
		  unsigned long *wr_first) {
  assert(pkt_list != NULL);

  /*M
    Init the RFC2250 packet that will be filled with the received
    packets.
  **/
  rtp_pkt_t pkt;
  rtp_rfc2250_pkt_init(&pkt);

  /*M
    Receive the first non empty packet.
  **/
  int finished = 0;
  while (!finished) {
    switch (rtp_pkt_read(&pkt, sock)) {
    case 0:
      continue;

    case 1:
      if (pob_insert_pkt(pkt_list, &pkt) != 1)
	return 0;
      finished = 1;
      break;

    default:
      return 0;
    }
  }

  /*M
    Receive packets while length of data in list $<$ buffer time.
  **/
  while ((long)(((rtp_pkt_t *)dlist_end(pkt_list))->timestamp -
	  ((rtp_pkt_t *)dlist_front(pkt_list))->timestamp) <
	 (buffer_time * 90)) {
    switch (rtp_pkt_read(&pkt, sock)) {
    case 0:
      continue;

    case 1:
      if (pob_insert_pkt(pkt_list, &pkt) != 1)
	return 0;
      break;

    default:
      return 0;
    }

    /*M
      Print prebuffering information.
    **/
    if (!quiet)
      fprintf(stderr, "Prebuffering: %.2f%%\r",
	      (((rtp_pkt_t *)dlist_end(pkt_list))->timestamp -
	       ((rtp_pkt_t *)dlist_front(pkt_list))->timestamp) /
	      (buffer_time * 90.0) * 100.0);
  }

  if (!quiet)
    fprintf(stderr, "\n");

  /*M
    Get the timestamp of the first received packet.
  **/
  struct timeval tv;
  gettimeofday(&tv, NULL);
  *wr_first = tv.tv_sec * 90000 + (unsigned long)(tv.tv_usec / 11.111) -
    ((rtp_pkt_t *)dlist_front(pkt_list))->timestamp;
  
  return 1;
}

/*M
  \emph{Simple RTP RFC2250 streaming client main loop.}

  The mainloop calls the prebuffering routine each time the receiving
  list is empty, then receives packets and writes the packets in the
  buffering queue out to standard output.
**/
int pob_mainloop(int sock, int quiet) {
  /*M
    Time of the first received packet, is filled by the prebuffering
    routine.
  **/
  unsigned long wr_first;

  int retval = 1;

  /*M
    Initialize the buffering list.
  **/
  dlist_head_t pkt_list;
  dlist_init(&pkt_list);
  
  /*M
    Initialize the \verb|select| structures.
  **/
  struct timeval t_out;
  t_out.tv_sec  = 0;
  t_out.tv_usec = 0;
  fd_set fds;
  FD_ZERO(&fds);
  FD_SET(sock, &fds);
  int finished = 0;
  while (!finished) {
    /*M
      Wait for network input or for timeout.
    **/
    int ret = select(sock + 1, &fds, NULL, NULL, &t_out);

    /*M
      Check for interrupted system call.
    **/
    if (errno == EINTR) {
      t_out.tv_usec = RTP_MINSLEEP;
      t_out.tv_sec  = 0;
      FD_ZERO(&fds);
      FD_SET(sock, &fds);
      continue;
    } else if (ret < 0) {
      finished = 1;
      continue;
    }
    
    /*M
      Prebuffer if buffer is empty.
    **/
    if (pkt_list.num <= 0) {
      if (!quiet)
	fprintf(stderr, "\n");
      
      if (!pob_prebuffer(sock, &pkt_list, quiet, &wr_first)) {
	retval = 0;
	goto exit;
      }
    }

    /*M
      Print client information.
    **/
    if (!quiet)
      fprintf(stderr, "pkts: %.8u\tdups: %.6u\tdrop: %.6u\tbuf: %.6u\t\r",
	      pob_stats.rcvd_pkts,
	      pob_stats.dup_pkts,
	      pob_stats.ooo_pkts,
	      pkt_list.num);
    
    /*M
      If there is network input, read the incoming packet.
    */
    if (FD_ISSET(sock, &fds)) {
      rtp_pkt_t pkt;

      rtp_rfc2250_pkt_init(&pkt);

      if (rtp_pkt_read(&pkt, sock) <= 0) {
	/*M
	  XXX: handle error.
	**/
      } else {
      fprintf(stderr, "packet length: %lu\n", pkt.length);

	/*M
	  Insert new packet into the buffering list.
	**/
	switch (pob_insert_pkt(&pkt_list, &pkt)) {
	case -1:
	  retval = 0;
	  goto exit;

	default:
	  break;
	}
      }
    }

    /*M
      Get the current time to see which packets have to be written to
      standard out.
    **/
    struct timeval tv;
    gettimeofday(&tv, NULL);
    unsigned long wr_time;
    wr_time = tv.tv_sec * 90000 + (unsigned long)(tv.tv_usec / 11.111);

    /*M
      Get packets which have to be written.
    **/
    while (pkt_list.dlist && dlist_front(&pkt_list) &&
	   ((((rtp_pkt_t *)dlist_front(&pkt_list))->timestamp + wr_first) <=
	    wr_time)) {
      rtp_pkt_t *pkt = dlist_get_front(&pkt_list);
      assert(pkt != NULL);

      /*M
	Write packet payload.
      **/
      if (write(STDOUT_FILENO,
		pkt->data + pkt->hlen,
		pkt->length) < (int)pkt->length) {
	fprintf(stderr, "Error writing to stdout\n");
	retval = 0;
	goto exit;
      }

      free(pkt);
    }

    t_out.tv_usec = RTP_MINSLEEP;
    t_out.tv_sec  = 0;
    FD_ZERO(&fds);
    FD_SET(sock, &fds);
  }

  perror("select");
  retval = 0;

 exit:
  /*M
    Destroy the buffering list.
  **/
  dlist_destroy(&pkt_list, free);
  return retval;
}

/*M
  \emph{Print RFC2250 RTP client usage.}
**/
static void usage(void) {
  fprintf(stderr, "Usage: ./pob [-s address] [-p port] [-b size] [-t time] [-q]");
#ifdef WITH_OPENSSL
  fprintf(stderr, "[-c cert]");
#endif
  fprintf(stderr, "\n");
  fprintf(stderr, "\t-s address : destination address (default 224.0.1.23 or ff02::4)\n");
  fprintf(stderr, "\t-p port    : destination port (default 1500)\n");
  fprintf(stderr, "\t-b size    : maximal number of packets in buffer (default 128)\n");
  fprintf(stderr, "\t-t time    : prebuffering time in milliseconds (default 2000)\n");
  fprintf(stderr, "\t-q         : quiet\n");

#ifdef WITH_OPENSSL
  fprintf(stderr, "\t-c cert    : verify packets with rsa certificate\n");
#endif
}

/*M
  \emph{RFC2250 RTP client entry routine.}
**/
int main(int argc, char *argv[]) {
  char           *address = NULL;
  unsigned short port = 1500;
  int            retval = EXIT_SUCCESS, quiet = 0;
#ifdef WITH_OPENSSL
  X509     *x509 = NULL;
  EVP_PKEY *pkey = NULL;
#endif

  /*M
    Process the command line arguments.
  **/
  int c;
  while ((c = getopt(argc, argv, "hs:p:b:t:q"
#ifdef WITH_OPENSSL
    "c:"
#endif
		     )) >= 0) {      
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

    case 't':
      buffer_time = (unsigned int)atoi(optarg);
      break;

    case 'q':
      quiet = 1;
      break;

      /*M
	If Openssl is used, read in the RSA certificate.
      **/
#ifdef WITH_OPENSSL
    case 'c':
      {
	FILE *f;

	if (x509) {
	  X509_free(x509);
	  x509 = NULL;
	}
	if (pkey) {
	  EVP_PKEY_free(pkey);
	  pkey = NULL;
	  rsa = NULL;
	}

	if (!(f = fopen(optarg, "r")) ||
	    !PEM_read_X509(f, &x509, NULL, NULL) ||
	    !(pkey = X509_get_pubkey(x509))) {
	  fprintf(stderr,
		  "Could not read certificate %s\n",
		  optarg);
	  if (f)
	    fclose(f);
	  retval = EXIT_FAILURE;
	  goto exit;
	}

	fclose(f);

	if (pkey->type != EVP_PKEY_RSA) {
	  fprintf(stderr, "Key is not a RSA key\n");
	  retval = EXIT_FAILURE;
	  goto exit;
	}

	rsa = pkey->pkey.rsa;

	break;
      }
#endif
      
    case 'h':
    default:
      usage();
      retval = EXIT_SUCCESS;
      goto exit;
    }
  }

  if (address == NULL) {
#ifdef WITH_IPV6
    address = strdup("ff02::4");
#else
    address = strdup("224.0.1.23");
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
#ifdef WITH_OPENSSL
  if (pkey)
    EVP_PKEY_free(pkey);
  if (x509)
    X509_free(x509);
#endif
  
  if (address != NULL)
    free(address);

  return retval;
}

/*C
**/
