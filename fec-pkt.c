/*C
  (c) 2003 Institut fuer Telematik, Universitaet Karlsruhe
**/

#include "conf.h"

#include <assert.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/uio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>

#include "pack.h"
#include "fec-pkt.h"

/*M
  \emph{Initialize a FEC packet by filling common header fields.}

  The version field is set to $1$ and the payload length to $0$.
**/
void fec_pkt_init(fec_pkt_t *pkt) {
  assert(pkt != NULL);

  pkt->hdr.magic = FEC_PKT_MAGIC;
  pkt->hdr.version = 1;
  pkt->hdr.len = 0;
  pkt->hdr.group_seq = 0;
  pkt->payload = pkt->data + FEC_PKT_HDR_SIZE;
}

/*M
  \emph{Send a FEC packet to filedescriptor using send.}

  Fills the packet data buffer with the packed header. Sequence
  number fields are not incremented, but have to be set by the application.
**/
ssize_t fec_pkt_send(fec_pkt_t *pkt, int fd) {
  assert(pkt != NULL);

  unsigned char *ptr = pkt->data;

  /*M
    Pack the header data into the data buffer.
  **/
  UINT8_PACK(ptr, pkt->hdr.magic);
  UINT8_PACK(ptr, pkt->hdr.version);
  UINT8_PACK(ptr, pkt->hdr.group_seq);
  UINT8_PACK(ptr, pkt->hdr.packet_seq);
  UINT8_PACK(ptr, pkt->hdr.fec_k);
  UINT8_PACK(ptr, pkt->hdr.fec_n);
  UINT16_PACK(ptr, pkt->hdr.fec_len);  
  UINT16_PACK(ptr, pkt->hdr.len);
  UINT32_PACK(ptr, pkt->hdr.group_tstamp);

  /*M
    Send the packet
  **/
  return send(fd, pkt->data, FEC_PKT_HDR_SIZE + pkt->hdr.len, 0);
}

/*M
  \emph{Read a FEC packet from filedescriptor.}

  Reads a FEC packet from the filedescriptor, and unpacks the header
  fields into the header structure.
**/
int fec_pkt_read(fec_pkt_t *pkt, int fd) {
  assert(pkt != NULL);

  /*M
    Read the packet (reading maximum packet size from the UDP socket).
  **/
  ssize_t len;
  switch (len = read(fd, pkt->data, FEC_PKT_SIZE)) {
  case 0:
    /* EOF */
    return 0;
  case -1:
    /* error */
    return -1;
  default:
    break;
  }

  if (len < FEC_PKT_HDR_SIZE)
    return -1;

  /*M
    Unpack the header data.
  **/
  unsigned char *ptr = pkt->data;
  pkt->hdr.magic = UINT8_UNPACK(ptr);
  if (pkt->hdr.magic != FEC_PKT_MAGIC)
    return -1;
  pkt->hdr.version = UINT8_UNPACK(ptr);
  pkt->hdr.group_seq = UINT8_UNPACK(ptr);
  pkt->hdr.packet_seq = UINT8_UNPACK(ptr);
  pkt->hdr.fec_k = UINT8_UNPACK(ptr);
  pkt->hdr.fec_n = UINT8_UNPACK(ptr);
  pkt->hdr.fec_len = UINT16_UNPACK(ptr);
  pkt->hdr.len = UINT16_UNPACK(ptr);
  pkt->hdr.group_tstamp = UINT32_UNPACK(ptr);

  if (pkt->hdr.len != (len - FEC_PKT_HDR_SIZE))
    return -1;

  /*M
    Update the payload pointer.
  **/
  pkt->payload = pkt->data + FEC_PKT_HDR_SIZE;

  return 1;
}

/*M
**/
