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
#include "rtp-rb.h"
#include "network.h"

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
  \emph{Timestamp of last played packet.}
**/
static unsigned long tstamp_last = 0;

/*M
**/
int pob_insert_pkt(rtp_pkt_t *pkt) {
  assert(pkt != NULL);

  /*M
    We have received a packet.
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

  int num = 0;
  /* insert packet into ringbuffer */
  if (rtp_rb_length() > 0) {
    /* get index of first packet */
    rtp_pkt_t *firstpkt = rtp_rb_first();
    assert(firstpkt != NULL);
    assert(firstpkt->length != 0);
    
    num = net_seqnum_diff(firstpkt->b.seq, pkt->b.seq, 1 << 16);
  }

  if (!rtp_rb_insert_pkt(pkt, num)) {
#ifdef DEBUG
    fprintf(stderr, "ring buffer full\n");
#endif
    
    rtp_rb_clear();
    tstamp_last = pkt->timestamp;
    return pob_insert_pkt(pkt);
  } else {
    return 1;
  }
}

int pob_recv_pkt(int sock, rtp_pkt_t *pkt) {
  struct timeval t_out;
  t_out.tv_sec  = 0;
  t_out.tv_usec = RTP_MINSLEEP;

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
    if (errno == EINTR) {
      return 0;
    } else {
      perror("select");
      return -1;
    }
  }
  
  /*M
    If there is network input, read incoming packet.
  **/
  if (FD_ISSET(sock, &fds)) {
    rtp_rfc2250_pkt_init(pkt);
    
    if (rtp_pkt_read(pkt, sock) <= 0) {
      return -1;
    } else {
      return 1;
    }
  }

  return 0;
}

/*M
  \emph{Simple RTP RFC2250 streaming client main loop.}

  The mainloop calls the prebuffering routine each time the receiving
  list is empty, then receives packets and writes the packets in the
  buffering queue out to standard output.
**/
int pob_mainloop(int sock, int quiet) {
  int retval;
  
  int finished = 0;
  while (!finished) {
    static int prebuffering = 0;
    
    rtp_pkt_t pkt;

    if (rtp_rb_cnt == 0) {
      prebuffering = 1;
    }
    
    /*M
      Receive next packet.
    */
    switch (pob_recv_pkt(sock, &pkt)) {
    case 0:
      break;

    case -1:
      finished = 1;
      continue;

    default:
      /*M
	Insert new packet into the buffering list.
      **/
      switch (pob_insert_pkt(&pkt)) {
      case -1:
	retval = 0;
	goto exit;
	
      default:
	break;
      }
      
      break;
    }

    static unsigned long time_last;

    unsigned long time_now;
    struct timeval tv;
    gettimeofday(&tv, NULL);
    time_now = tv.tv_sec * 90000 +
      (unsigned long)(tv.tv_usec / 11.111);
    
    if (prebuffering == 1) {
      if (rtp_rb_cnt >= (rtp_rb_size / 2)) {
	rtp_pkt_t *firstpkt = rtp_rb_first();
	assert(firstpkt != NULL);
	assert(firstpkt->length != 0);
	
	tstamp_last = firstpkt->timestamp;
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
		  (float)rtp_rb_cnt / (rtp_rb_size / 2.0) * 100.0);

	continue;
      }
    }
    
    /*M
      Print client information.
    **/
    if (!quiet)
      fprintf(stderr, "pkts: %.8u\tdups: %.6u\tdrop: %.6u\tbuf: %.6u len:%.6u\t\r",
	      pob_stats.rcvd_pkts,
	      pob_stats.dup_pkts,
	      pob_stats.ooo_pkts,
	      rtp_rb_cnt,
	      rtp_rb_length());

#ifdef DEBUG
    rtp_rb_print();
#endif

    unsigned long tstamp_now = tstamp_last + (time_now - time_last);

    while (rtp_rb_length() > 0) {
      rtp_pkt_t *pkt = rtp_rb_first();
      assert(pkt != NULL);

      if (pkt->length != 0) {
	/* boeser hack XXX */
	if (pkt->timestamp > (tstamp_now + 3000))
	  break;

	if (write(STDOUT_FILENO, pkt->data + pkt->hlen,
		  pkt->length) < (int)pkt->length) {
	  fprintf(stderr, "Error writing to stdout\n");
	  retval = 0;
	  goto exit;
	}
      }
      
      rtp_rb_pop();
    }
  }
  
 exit:
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
  unsigned int buffer_size = 128;
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

  /*M
    Initialize the ring buffer.
  **/
  rtp_rb_init(buffer_size);

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
  rtp_rb_destroy();
  
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
