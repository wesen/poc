/*C
  (c) 2003 Institut fuer Telematik, Universitaet Karlsruhe
**/

#ifndef RTP_H__
#define RTP_H__

#include <sys/types.h>

/*@-exportlocal@*/

/*M
  \emph{Structure representing the 32 first bits of a RTP packet
  header.}
**/
typedef struct rtp_bits_s {
  unsigned int v   :2;  /* version */
  unsigned int p   :1;  /* padding */
  unsigned int x   :1;  /* number of extension headers */
  unsigned int cc  :4;  /* number of CSRC identifiers */
  unsigned int m   :1;  /* marker */
  unsigned int pt  :7;  /* payload type */
  unsigned int seq :16; /* sequence number */
} rtp_bits_t;

/*M
  \emph{Structure representing the flags of a RFC3119 packet.}
**/
typedef struct rtp_rfc3119_bits_s {
  unsigned int c :1; /* continuation flag */
  unsigned int t :1; /* descriptor type flag */
} rtp_rfc3119_bits_t;

/*M
  \emph{Structure representing the flags of a Vorbis RTP packet.}
**/
typedef struct rtp_vorbis_bits_s {
  unsigned int c :1; /* continuation flag */
  unsigned int r1 :1; /* reserved bit 1 */
  unsigned int r2 :1; /* reserved bit 2 */
  /*M
    Number of complete packets in the payload. If C is set to 1, this should
    be 0. */
  unsigned int pkt_num :5;
} rtp_vorbis_bits_t;

/*M
  \emph{Default RTP packet size.}
**/
#define RTP_PKT_SIZE 1500

/*C
  \emph{RTP header structure.}

  \verbatim{
   0                   1                   2                   3
   0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
  |V=2|P|X|  CC   |M|     PT      |       sequence number         | 
  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
  |                           timestamp                           |
  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
  |           synchronization source (SSRC) identifier            |
  +=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+
  |            contributing source (CSRC) identifiers             |
  |                             ....                              |
  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 }
**/

/*M
  \emph{Structure representing a RTP packet.}

  The \verb|data| buffer is not the payload buffer, it contains the
  complete data for the packet. Be careful to update the payload
  pointer correctly when adding additional headers.
**/
typedef struct rtp_pkt_s {
  /* first 32 bits */
  rtp_bits_t    b;          
  unsigned long timestamp;
  /* synchronization source */
  unsigned long ssrc;
  /* padding length */
  unsigned char plen;
  /* header length */
  unsigned int  hlen;

  /* payload data length */
  size_t        length;
  /* packet data */
  unsigned char data[RTP_PKT_SIZE];
  /* pointer to payload inside the packet data */
  unsigned char *payload;

  /* pointer to pack and unpack routine */
  void (*pack)(struct rtp_pkt_s *);
  int (*unpack)(struct rtp_pkt_s *);
  
  /* rfc3119 header */
  rtp_rfc3119_bits_t rfc3119_b;

  /* vorbis header */
  rtp_vorbis_bits_t vorbis_b;
} rtp_pkt_t;

/*M
  \emph{RTP header size.}
**/
#define RTP_HDR_SIZE 12
/*M
  \emph{RTP CSRC list entry size.}
**/
#define RTP_CSRC_SIZE 4


void rtp_pkt_init(/*@out@*/ rtp_pkt_t *pkt);

void    rtp_pkt_pack(rtp_pkt_t *pkt);
ssize_t rtp_pkt_send(rtp_pkt_t *pkt, int fd);

int rtp_pkt_unpack(rtp_pkt_t *pkt);
int rtp_pkt_read(rtp_pkt_t *pkt, int fd);

#ifdef WITH_OPENSSL
#include <openssl/rsa.h>

int rtp_pkt_sign(rtp_pkt_t *pkt, RSA *rsa);
int rtp_pkt_verify(rtp_pkt_t *pkt, RSA *rsa);
#endif /* WITH_OPENSSL */

/*C
  \emph{RTP RFC2250 packet header.}

     0                   1                   2                   3
   0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
  |  MBZ (unused bits)            |  Fragmentation offset         |
  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+

**/

/*M
  \emph{RTP RFC2250 packet structure.}

  (RFC2250) The RTP header fields are used as follows:
  \begin{description}
  \item[Payload Type]: Distinct payload types should be assigned for
  MPEG1 Systems Streams, MPEG2 Program Streams and MPEG2 Transport
  Streams.
  \item[M bit]: Set to 1 whenever the timestamp is discontinuous
  (such as might happen when a sender switches from one data source
  to another). This allows the received and any intervening RTP
  mixers or transators that are synchronizing to the flow to ignore
  the difference between this timestamp and any previous timestamp in
  their clock phase detectors.
  \item[timestamp]: 32 bit 90 KHz timestamp representing the target
  transmission time for the first byte of the packet.
  \end{description}

  A distinct RTP payload type is assigned to MPEG1/MPEG2 Video and
  MPEG1/MPEG2 Audio, repsectively. Further indication as to whether
  the data is MPEG1 or MPEG2 need not be provided in the RTP or
  MPEG-specific headers of this encapsulation, as this information is
  available in the ES headers.
**/

/*M
  \emph{RTP RFC2250 MPEG header size.}
**/
#define RTP_RFC2250_HDR_SIZE 4
/*M
  \emph{RTP RFC2250 MPEG audio payload type.}
**/
#define RTP_PT_MPA 14
/*M
  \emph{RTP signed RFC2250 MPEG audio payload type.}

  This audio payload type was chosen arbitrarily by picking a value
  from the dynamic payload types.
**/
#define RTP_PT_SMPA 97

void rtp_rfc2250_pkt_init(rtp_pkt_t *pkt);

/*M
  \emph{RTP dynamic payload type (used by RFC3119).}
**/
#define RTP_DYN 96

void rtp_rfc3119_pkt_init(rtp_pkt_t *pkt);

void rtp_vorbis_pkt_init(rtp_pkt_t *pkt);

#endif /* RTP_H__ */

/*C
**/
