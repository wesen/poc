/*C
  (c) 2005 bl0rg.net
**/

#include "conf.h"

#include "fec-pkt.h"
#include "lib/mp3/aq.h"

/*M
  \emph{FEC group structure.}

  Used to collect packets until enough are available to decode the group.
**/
typedef struct fec_group_s {
  /*M
    $k$ FEC parameter. At least $k$ packets have to be collected in
    order to recover the complete source information.
  **/
  unsigned char fec_k;
  /*M
    $n$ FEC parameter. The group contains at least $n$ packets.
  **/
  unsigned char fec_n;
  /*M
    The maximal length of a packet. Packets with sequence numbers <
    $k$ can be shorter.
  **/
  unsigned short fec_len;
  /*M
    The group sequence number.
  **/
  unsigned char seq;
  /*M
    The group timestamp in usecs.
  **/
  unsigned long tstamp;
  /*M
    Received packets count.
  **/
  unsigned char rcvd_pkts;

  /*M
    Keeps track of received packets.
  **/
  unsigned char *pkts;
  /*M
    Buffer to be filled with packet payloads.
  **/
  unsigned char *buf;

  /* Length of the inserted packets. */
  unsigned int *lengths;

  int decoded;
} fec_group_t;

void fec_group_init(fec_group_t *group,
                    unsigned char fec_k,
                    unsigned char fec_n,
                    unsigned char seq,
                    unsigned long tstamp,
                    unsigned short fec_len);
void fec_group_destroy(fec_group_t *group);
void fec_group_clear(fec_group_t *group);
void fec_group_insert_pkt(fec_group_t *group,
                          fec_pkt_t *pkt);
int fec_group_decode(fec_group_t *group);
int fec_group_decode_to_adus(fec_group_t *group, aq_t *aq);
