/*C
  (c) 2003 Institut fuer Telematik, Universitaet Karlsruhe
**/

#include "conf.h"

#include <assert.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "fec.h"
#include "fec-group.h"

/*M
  \emph{Initialize a FEC group structure to hold incoming packets.}
**/
void fec_group_init(fec_group_t *group,
                    unsigned char fec_k,
                    unsigned char fec_n,
                    unsigned char seq,
                    unsigned long tstamp,
                    unsigned short fec_len) {
  assert(group != NULL);
  
  group->fec_k = fec_k;
  group->fec_n = fec_n;
  group->seq = seq;
  group->tstamp = tstamp;
  group->fec_len = fec_len;
  group->rcvd_pkts = 0;

  group->pkts = malloc(sizeof(unsigned char) * fec_n);
  assert(group->pkts != NULL);
  group->buf = malloc(sizeof(unsigned char) * fec_n * fec_len);
  assert(group->buf != NULL);

  /* init pointers */
  int i;
  for (i = 0; i < fec_n; i++) {
    group->pkts[i] = 0;
  }

  group->decoded = 0;
}

/*M
  \emph{Destroy a FEC group structure.}
**/
void fec_group_destroy(fec_group_t *group) {
  assert(group != NULL);

  if (group->buf) {
    free(group->buf);
    group->buf = NULL;
  }
  
  if (group->pkts) {
    free(group->pkts);
    group->pkts = NULL;
  }

  fec_group_clear(group);
}

/*M
  \emph{Clear a FEC group structure.}
**/
void fec_group_clear(fec_group_t *group) {
  group->buf = NULL;
  group->pkts = NULL;

  group->fec_k = group->fec_n = group->tstamp = 0;
  group->fec_len = 0;
  group->rcvd_pkts = 0;
}

/*M
  \emph{Print debug information about a FEC group.}
**/
void fec_group_print(fec_group_t *group) {
  assert(group != NULL);
  
  fprintf(stderr, "Group %p tstamp: %lu\n", group, group->tstamp);
  fprintf(stderr, "k: %d, n: %d, len: %u\n",
          group->fec_k, group->fec_n, group->fec_len);
  
  fprintf(stderr, "received packets: %u/%u\n",
          group->rcvd_pkts, group->fec_k);

  int i;
  for (i = 0; i < group->fec_n; i++) {
    if (group->pkts[i] == 0) {
      fprintf(stderr, "%d: not received\n", i);
    } else {
      fprintf(stderr, "%d: received\n", i);
    }
  }
}

/*M
  \emph{Insert a received FEC packet into a FEC group.}
**/
void fec_group_insert_pkt(fec_group_t *group,
                          fec_pkt_t *pkt) {
  assert(group != NULL);
  assert(pkt != NULL);

  /* sanity checks, no real error handling yet */
  assert(pkt->hdr.packet_seq < group->fec_n);
  assert(pkt->hdr.len <= group->fec_len);
  assert(pkt->hdr.group_tstamp == group->tstamp);

  /* check if packet already received */
  if (group->pkts[pkt->hdr.packet_seq] == 0) {
    unsigned char *ptr = group->buf + pkt->hdr.packet_seq * group->fec_len;
    memcpy(ptr, pkt->payload, pkt->hdr.len);
    if (pkt->hdr.len < group->fec_len) {
      memset(ptr + pkt->hdr.len, 0, group->fec_len - pkt->hdr.len);
    }
    group->pkts[pkt->hdr.packet_seq] = 1;
    group->rcvd_pkts++;
  }
}

/*M
  \emph{Decode a FEC group into an ADU queue.}

  If the group is not complete, the lower packets (with \verb|packet_seq| $<$
  \verb|fec_k|) are added to the ADU.
**/
int fec_group_decode(fec_group_t *group) {
  assert(group != NULL);
  
  if (group->decoded)
    return 1;
  
  /* check if enough packets in the group have been received to
   * recover the complete source data.
   */
  if (group->rcvd_pkts >= group->fec_k) {
    /* we have enough packets in the fec group */
    unsigned int idxs[group->fec_k];

    /* create index array and pointer array. */
    int i, j;
    for (i = 0, j = 0; i < group->fec_n; i++) {
      if (group->pkts[i] == 1) {
        idxs[j] = i;
        j++;
        if (j == group->fec_k)
          break;
      }
    }

    assert(j == group->fec_k);

    /* create the fec structure. */
    fec_t *fec = fec_new(group->fec_k, group->fec_n);
    assert(fec != NULL);

    /* decode the fec group. */
    if (!fec_decode(fec, group->buf, idxs, group->fec_len)) {
      fprintf(stderr, "Could not decode FEC group\n");
      fec_free(fec);
      return 0;
    }

    fec_free(fec);

    group->decoded = 1;

    return 1;
  }  else {
    return 0;
  }
}

int fec_group_decode_to_adus(fec_group_t *group,
                             aq_t *aq) {
  assert(group != NULL);
  assert(aq != NULL);

  if (fec_group_decode(group)) {
    /*M
      Add the adus to the adu queue.
    **/
    int i;
    for (i = 0; i < group->fec_k; i++) {
      adu_t adu;
      memcpy(adu.raw,
             group->buf + i * group->fec_len,
             group->fec_len);
      
      if (!mp3_unpack(&adu)) {
        fprintf(stderr, "Error unpacking the mp3 adu\n");
        return 0;
      }
      
      aq_add_adu(aq, &adu);
    }
  } else {
    /*M
      We don't have enough packets in the group to recover the whole
      source data, add only the uncoded ADUs we received (systematic
      encoding).
    **/
    int i;
    for (i = 0; i < group->fec_k; i++) {
      if (group->pkts[i] == 1) {
        adu_t adu;
        memcpy(adu.raw,
               group->buf + i * group->fec_len,
               group->fec_len);

        if (!mp3_unpack(&adu)) {
          fprintf(stderr, "Error unpacking the mp3 adu\n");
          return 0;
        }

        aq_add_adu(aq, &adu);
      }
    }
  }

  return 1;
}

#ifdef FEC_GROUP_TEST
unsigned char fec_k = 20;
unsigned char fec_n = 25;


unsigned long cksum(unsigned char *buf, int cnt) {
  unsigned long res = 0;
  
  int i;
  for (i = 0; i < cnt; i++)
    res += buf[i];

  return res;
}

int main(int argc, char *argv[]) {
  char *f[2];

  if (!(f[0] = *++argv) || !(f[1] = *++argv)) {
    fprintf(stderr, "Usage: mp3-write mp3in mp3out\n");
    return 1;
  }

  file_t in;
  if (!file_open_read(&in, f[0])) {
    fprintf(stderr, "Could not open mp3 file for read: %s\n", f[0]);
    return 1;
  }

  file_t out;
  if (!file_open_write(&out, f[1])) {
    fprintf(stderr, "Could not open mp3 file for write: %s\n", f[1]);
    file_close(&in);
    return 1;
  }

  aq_t qin, qout;
  aq_init(&qin);
  aq_init(&qout);

  adu_t *in_adus[fec_k];
  unsigned int cnt = 0;

  static unsigned long fec_time = 0;
  fec_t *fec = fec_new(fec_k, fec_n);

  mp3_frame_t frame;
  while (mp3_next_frame(&in, &frame) > 0) {
    static int cin = 0;
    if (aq_add_frame(&qin, &frame)) {
      printf("frame\n");

      in_adus[cnt] = aq_get_adu(&qin);
      assert(in_adus[cnt] != NULL);
      
      /* check if the FEC group is complete */
      if (++cnt == fec_k) {

        unsigned int max_len = 0;
        unsigned long group_duration = 0;

        int i;
        for (i = 0; i < fec_k; i++) {
          unsigned int adu_len = mp3_frame_size(in_adus[i]);
            + (in_adus[i]->protected ? 0 : 2)
            + in_adus[i]->si_size
            + in_adus[i]->adu_size;

          if (adu_len > max_len)
            max_len = adu_len;

          group_duration += in_adus[i]->usec;
        }
#if 0

        fec_time += group_duration;

        assert(max_len < FEC_PKT_PAYLOAD_SIZE);

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

        fec_group_t group;
        fec_group_init(&group, fec_k, fec_n, 0, 0, max_len);
        memset(group.buf, 0, fec_n * max_len);

        group.tstamp = fec_time;

        for (i = 0; i < fec_n; i++) {
          fec_pkt_t pkt;
          fec_pkt_init(&pkt);

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

          fec_group_insert_pkt(&group, &pkt);
        }

        if (!fec_group_decode(&group, &qout)) {
          fprintf(stderr, "Could not decode group\n");
          return 1;
        }

        fec_group_destroy(&group);
#endif

        for (i = 0; i < fec_k; i++) {
          adu_t adu;
          memcpy(adu.raw,
                 in_adus[i]->raw,
                 max_len);
          
          if (!mp3_unpack(&adu)) {
            fprintf(stderr, "Error unpacking the mp3 adu\n");
            return 0;
          }
          
          aq_add_adu(&qout, &adu);
        }

        mp3_frame_t *frame_out;
        while ((frame_out = aq_get_frame(&qout)) != NULL) {
          memset(frame_out->raw, 0, 4 + frame_out->si_size);
          
          /*M
            Write packet payload.
          **/
          if (!mp3_fill_hdr(frame_out) ||
              !mp3_fill_si(frame_out) ||
              (mp3_write_frame(&out, frame_out) <= 0)) {
            fprintf(stderr, "Error writing to stdout\n");
            free(frame_out);
            
            return 0;
          }
          
          free(frame_out);
        }

        for (i = 0; i < fec_k; i++)
          free(in_adus[i]);

        cnt = 0;
      }
    }

    /* fgetc(stdin); */
  }

  file_close(&in);
  file_close(&out);

  aq_destroy(&qin);
  aq_destroy(&qout);

  return 0;
}
#endif /* FEC_GROUP_TEST */
