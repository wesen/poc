/*C
  (c) 2005 bl0rg.net
**/

#include "conf.h"

#include <assert.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/uio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>

#ifdef WITH_OPENSSL
#include <openssl/rsa.h>
#include <openssl/evp.h>
#endif

#ifdef DEBUG
#include <stdio.h>
#endif

#include "pack.h"
#include "rtp.h"
#include "lib/system/misc.h"

/*@+charint@*/
/*@+boolint@*/

/*M
  \emph{Initialize a RTP packet by filling common header fields.}

  Synchronization source and sequence number are filled using random
  numbers, payload length is set to 0.
**/
void rtp_pkt_init(rtp_pkt_t *pkt) {
  assert(pkt != NULL);

  /*M
    RTP version 2.
  **/
  pkt->b.v = 2;
  /*M
    We use no padding (at first).
  **/
  pkt->b.p = 0;
  /*M
    No extensions (at first).
  **/
  pkt->b.x = 0;
  /*M
    No csrc identifiers at first.
  **/
  pkt->b.cc = 0;
  /*M
    Set the sequence number to 0.
  **/
  pkt->b.seq = 0;
  /*M
    Generate a random synchronization source.
  **/
  pkt->ssrc  = ((unsigned int)rand()) & 0xFFFFFFFF;
  /*M
    The payload length is $0$ at first.
  **/
  pkt->length = 0;
  
  /*M
    The padding length is $0$ at first.
  **/
  pkt->plen = 0;

  pkt->hlen = RTP_HDR_SIZE;
  pkt->payload = pkt->data + pkt->hlen;

  pkt->pack = NULL;
  pkt->unpack = NULL;
}

/*M
  \emph{Fill the packet data by packing the header information.}
**/
void rtp_pkt_pack(rtp_pkt_t *pkt) {
  assert(pkt != NULL);
  
  /* v: 2 bits, p: 1 bit, x: 1 bit, cc: 4 bits */
  pkt->data[0] =  (pkt->b.v  & 0x3) << 6;
  pkt->data[0] |= (pkt->b.p  & 0x1) << 5;
  pkt->data[0] |= (pkt->b.x  & 0x1) << 4;
  pkt->data[0] |= (pkt->b.cc & 0xf);
  /* m: 1 bit, pt: 7 bits */
  pkt->data[1] =  (pkt->b.m & 0x1) << 7;
  pkt->data[1] |= (pkt->b.pt & 0x7f);

  unsigned char *ptr = pkt->data + 2;
  /* seq: 16 bits */
  UINT16_PACK(ptr, pkt->b.seq);
  /* timestamp: 32 bits */
  UINT32_PACK(ptr, pkt->timestamp);
  /* ssrc: 32 bits */
  UINT32_PACK(ptr, pkt->ssrc);

  if (pkt->pack)
    pkt->pack(pkt);

  /*M
    If we use padding, add padding length at the end of the payload.
  **/
  if (pkt->b.p) {
    assert((pkt->length +
            pkt->hlen + pkt->b.cc * RTP_CSRC_SIZE +
            pkt->plen + 1) < RTP_PKT_SIZE);
    
    unsigned char *ptr = pkt->data + pkt->hlen + pkt->length + pkt->plen;
    *ptr = pkt->plen;
  }
}

/*M
  \emph{Send a RTP packet to filedescriptor using send.}

  Fills the packet data buffer with the packed header, increments the
  sequence number and sends it out.
**/
ssize_t rtp_pkt_send(rtp_pkt_t *pkt, int fd) {
  assert(pkt != NULL);
  
  rtp_pkt_pack(pkt);
  
  /*M
    Increment sequence number.
  **/
  pkt->b.seq++;

  unsigned int len = pkt->length + pkt->hlen + pkt->b.cc * RTP_CSRC_SIZE;

  /*M
    If padding is 1, then we have signed the packet.
  **/
  if (pkt->b.p)
    len += pkt->plen + 1;

  /*M
    Send pack on \verb|fd|.
  **/
  return send(fd, pkt->data, len, 0);
}

/*M
  \emph{Read a RTP packet from filedescriptor.}

  Reads a RTP packet from the filedescriptor, and unpacks the RTP
  header into the \verb|rtp_pkt_t| data fields.
**/
int rtp_pkt_read(rtp_pkt_t *pkt, int fd) {
  assert(pkt != NULL);
  
  ssize_t len;
  switch (len = unix_read(fd, pkt->data, RTP_PKT_SIZE)) {
  case 0:
    /* EOF */
    return 0;
  case -1:
    /* error */
    return -1;
  default:
    break;
  }

  /*M
    Check if the packet is long enough.
  **/
  if (len < RTP_HDR_SIZE)
    return -1;
  
  pkt->length = len;

  
  return rtp_pkt_unpack(pkt);
}

/*M
  \emph{Fill the RTP packet structure by unpacking payload
  information.}
**/
int rtp_pkt_unpack(rtp_pkt_t *pkt) {
  assert(pkt != NULL);
  
  /* v: 2 bits, p: 1 bit, x: 1 bit, cc: 4 bits */
  pkt->b.v  = (pkt->data[0] >> 6) & 0x3;
  /* check version */
  if (pkt->b.v != 2)
    return 0;
  pkt->b.p  = (pkt->data[0] >> 5) & 0x1;
  pkt->b.x  = (pkt->data[0] >> 4) & 0x1;
  pkt->b.cc = (pkt->data[0])      & 0xf;

  /* m: 1 bit, pt: 7 bits */
  pkt->b.m  = (pkt->data[1] >> 7) & 0x1;
  pkt->b.pt = (pkt->data[1])      & 0x7f;

  unsigned char *ptr = pkt->data + 2;
  /* seq: 16 bits */
  pkt->b.seq = UINT16_UNPACK(ptr);
  /* timestamp: 32 bits */
  pkt->timestamp = UINT32_UNPACK(ptr);
  /* ssrc: 32 bits, just ignore it */
  ptr += 4;

  if (pkt->unpack)
    if (!pkt->unpack(pkt))
      return 0;

  /*M
    If padding is used, then padding length is in last byte.
  **/
  if (pkt->b.p) {
    unsigned char *plen = pkt->data + pkt->length - 1;
    pkt->plen = *plen;
  } else {
    pkt->plen = 0;
  }
  

  /*M
    Ignore rest of packet for now.
  **/
  pkt->length -= pkt->hlen + pkt->b.cc * RTP_CSRC_SIZE;
  if (pkt->b.p)
    pkt->length -= pkt->plen + 1;

  return 1;
}

/*M
  \emph{Sign a packet using RSA.}

  First compute a SHA1 hash of the packet payload, and sign it using
  the RSA private key in \verb|rsa|.
**/
#ifdef WITH_OPENSSL
int rtp_pkt_sign(rtp_pkt_t *pkt, RSA *rsa) {
  assert(pkt != NULL);
  assert(rsa != NULL);

  if (pkt->length + RSA_size(rsa) + 1 > RTP_PKT_SIZE)
    return 0;

  EVP_MD_CTX    ctx;
  EVP_DigestInit(&ctx, EVP_sha1());
  EVP_DigestUpdate(&ctx, pkt->data + pkt->hlen, pkt->length);

  unsigned char md[EVP_MAX_MD_SIZE];
  unsigned int  mdlen;
  EVP_DigestFinal(&ctx, md, &mdlen);

  int slen;
  if (!RSA_sign(NID_sha1, md, mdlen,
                pkt->data + pkt->hlen + pkt->length, &slen, rsa))
    return 0;

  pkt->plen = slen;
  pkt->b.p  = 1;

  return 1;
}

/*M
  \emph{Verify a packet using RSA.}

  First compute a SHA1 hash of the packet payload, and verify it
  against the signed hash value in the padding using the public key
  in \verb|rsa|.
**/
int rtp_pkt_verify(rtp_pkt_t *pkt, RSA *rsa) {
  assert(pkt != NULL);
  assert(rsa != NULL);

  if (pkt->plen != RSA_size(rsa))
    return 0;
  
  EVP_MD_CTX    ctx;
  EVP_DigestInit(&ctx, EVP_sha1());
  EVP_DigestUpdate(&ctx, pkt->data + pkt->hlen, pkt->length);
   
  unsigned char md[EVP_MAX_MD_SIZE];
  unsigned int  mdlen;
  EVP_DigestFinal(&ctx, md, &mdlen);

  if (!RSA_verify(NID_sha1, md, mdlen,
                  pkt->data + pkt->hlen + pkt->length, pkt->plen, rsa))
    return 0;

  return 1;
}
#endif /* WITH_OPENSSL */

/*M
  \emph{Fill the packet data by packing the header information.}

  Fills the MPEG header with 0s.
**/
void    rtp_rfc2250_pkt_pack(rtp_pkt_t *pkt) {
  assert(pkt != NULL);

  unsigned char *ptr = pkt->data + RTP_HDR_SIZE;
  UINT32_PACK(ptr, 0x00000000);
}

/*M
  \emph{Unpack a RTP RFC2250 packet.}
**/
int rtp_rfc2250_pkt_unpack(rtp_pkt_t *pkt) {
  if ((pkt->b.pt != RTP_PT_SMPA) &&
      (pkt->b.pt != RTP_PT_MPA))
    return 0;
  
  return 1;
}

/*M
  \emph{Initialize a RFC2250 RTP packet by filling common header
  fields.}

**/
void rtp_rfc2250_pkt_init(rtp_pkt_t *pkt) {
  rtp_pkt_init(pkt);
  pkt->hlen += RTP_RFC2250_HDR_SIZE;
  pkt->pack = rtp_rfc2250_pkt_pack;
  pkt->unpack = rtp_rfc2250_pkt_unpack;
}

/*M
  \emph{Fill the packet data by packing the header information.}
**/
void    rtp_rfc3119_pkt_pack(rtp_pkt_t *pkt) {
  assert(pkt != NULL);

  unsigned char *ptr = pkt->data + RTP_HDR_SIZE;
  
  if (pkt->length > ((1 << 6) - 1))
    pkt->rfc3119_b.t = 1;
  else
    pkt->rfc3119_b.t = 0;

  if (pkt->rfc3119_b.t) {
    /* 14 bits length */
    *ptr = pkt->rfc3119_b.c << 7;
    *ptr |= 1 << 6;
    *ptr |= ((pkt->length >> 8) & 0x3f);
    ptr++;
    pkt->length++;
    *ptr = (pkt->length & 0xFF);
  } else {
    /* 6 bits length */
    *ptr = 0;
    *ptr = (pkt->length & 0x3f);
    *ptr |= pkt->rfc3119_b.c << 7;
  }
}

/*M
  \emph{Unpack a RTP RFC3119 packet.}
**/
int rtp_rfc3119_pkt_unpack(rtp_pkt_t *pkt) {
  if (pkt->b.pt != RTP_DYN)
    return 0;

  unsigned char *ptr = pkt->data + pkt->hlen - 1;
  pkt->rfc3119_b.c = (*ptr >> 7) & 1;
  assert((pkt->rfc3119_b.c == 0) || "Continuation is not handled yet");
  pkt->rfc3119_b.t = (*ptr >> 6) & 1;

  unsigned short length;
  if (pkt->rfc3119_b.t) {
    length = (*(ptr++) & 0x3F) << 8;
    length |= (*ptr & 0xFF);
    pkt->length--;
  } else {
    length = (*ptr & 0x3F);
  }
  
  assert((pkt->length == length) || "error while decoding RFC3119 length");
  
  return 1;
}

/*M
  \emph{Initialize a RFC3119 RTP packet by filling common header
  fields.}
**/
void rtp_rfc3119_pkt_init(rtp_pkt_t *pkt) {
  rtp_pkt_init(pkt);
  pkt->rfc3119_b.c = 0;
  pkt->rfc3119_b.t = 0;
  pkt->b.pt = RTP_DYN;
  pkt->hlen += 1; /* short rfc3119 header version */
  pkt->pack = rtp_rfc3119_pkt_pack;
  pkt->unpack = rtp_rfc3119_pkt_unpack;
}

/*C
**/

#ifdef RTP_TEST
#include <stdio.h>

static void testit(char *name,
                   unsigned long result,
                   unsigned long should) {
  if (result == should) {
    printf("Test %s was successful\n", name);
  } else {
    printf("Test %s was not successful, %lx should have been %lx\n",
           name, result, should);
  }
}

int main(void) {
  rtp_pkt_t pkt1, pkt2;
  int retval = 0;

  rtp_pkt_init(&pkt1);
  rtp_pkt_init(&pkt2);
  
  pkt1.b.cc = 0;
  pkt1.b.m  = 1;
  pkt1.b.m  = 0xF;

  rtp_pkt_pack(&pkt1);
  memcpy(pkt2.data, pkt1.data, RTP_PKT_SIZE);
  pkt2.length = pkt1.length + RTP_HDR_SIZE;
  if (!rtp_pkt_unpack(&pkt2)) {
    fprintf(stderr, "Could not unpack packet\n");
    retval = 1;
    goto finished;
  }

  testit("pack unpack test 1", pkt2.b.v, pkt1.b.v);
  testit("pack unpack test 2", pkt2.b.p, pkt1.b.p);
  testit("pack unpack test 3", pkt2.b.x, pkt1.b.x);
  testit("pack unpack test 4", pkt2.b.cc, pkt1.b.cc);
  testit("pack unpack test 5", pkt2.b.m, pkt1.b.m);
  testit("pack unpack test 6", pkt2.b.pt, pkt1.b.pt);
  testit("pack unpack test 7", pkt2.b.seq, pkt1.b.seq);
  testit("pack unpack test 8", pkt2.timestamp, pkt1.timestamp);
  testit("pack unpack test 9", pkt2.b.seq, pkt1.b.seq);
  testit("pack unpack test 10", pkt2.plen, pkt1.plen);
  testit("pack unpack test 11", pkt2.length, pkt1.length);
  
  pkt1.b.p = 1;
  pkt1.plen = 10;
  rtp_pkt_pack(&pkt1);
  memcpy(pkt2.data, pkt1.data, RTP_PKT_SIZE);
  pkt2.length = pkt1.length + pkt1.plen + 1 + RTP_HDR_SIZE;
  if (!rtp_pkt_unpack(&pkt2)) {
    fprintf(stderr, "Could not unpack packet\n");
    retval = 1;
    goto finished;
  }
  testit("pack unpack test 1", pkt2.b.v, pkt1.b.v);
  testit("pack unpack test 2", pkt2.b.p, pkt1.b.p);
  testit("pack unpack test 3", pkt2.b.x, pkt1.b.x);
  testit("pack unpack test 4", pkt2.b.cc, pkt1.b.cc);
  testit("pack unpack test 5", pkt2.b.m, pkt1.b.m);
  testit("pack unpack test 6", pkt2.b.pt, pkt1.b.pt);
  testit("pack unpack test 7", pkt2.b.seq, pkt1.b.seq);
  testit("pack unpack test 8", pkt2.timestamp, pkt1.timestamp);
  testit("pack unpack test 9", pkt2.b.seq, pkt1.b.seq);
  testit("pack unpack test 10", pkt2.plen, pkt1.plen);
  testit("pack unpack test 11", pkt2.length, pkt1.length);

  rtp_rfc3119_pkt_init(&pkt1);
  rtp_rfc3119_pkt_init(&pkt2);
  pkt1.length = 0x337;
  rtp_rfc3119_pkt_pack(&pkt1);
  memcpy(pkt2.data, pkt1.data, RTP_PKT_SIZE);
  pkt2.length = pkt1.length + RTP_HDR_SIZE + 1 + pkt1.rfc3119_b.t;
  if (!rtp_rfc3119_pkt_unpack(&pkt2)) {
    fprintf(stderr, "Could not unpack packet\n");
    retval = 1;
    goto finished;
  }
  testit("pack unpack test 1", pkt2.b.v, pkt1.b.v);
  testit("pack unpack test 2", pkt2.b.p, pkt1.b.p);
  testit("pack unpack test 3", pkt2.b.x, pkt1.b.x);
  testit("pack unpack test 4", pkt2.b.cc, pkt1.b.cc);
  testit("pack unpack test 5", pkt2.b.m, pkt1.b.m);
  testit("pack unpack test 6", pkt2.b.pt, pkt1.b.pt);
  testit("pack unpack test 7", pkt2.b.seq, pkt1.b.seq);
  testit("pack unpack test 8", pkt2.timestamp, pkt1.timestamp);
  testit("pack unpack test 9", pkt2.b.seq, pkt1.b.seq);
  testit("pack unpack test 10", pkt2.plen, pkt1.plen);
  testit("pack unpack test 11", pkt2.length, pkt1.length);
  testit("pack unpack test 10", pkt2.rfc3119_b.c, pkt1.rfc3119_b.c);
  testit("pack unpack test 11", pkt2.rfc3119_b.t, pkt1.rfc3119_b.t);

 finished:
  return retval;
}
#endif /* RTP_TEST */
