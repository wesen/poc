#include <sys/types.h>
#include <sys/uio.h>

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <assert.h>

#include "libfec.h"
#include "fec-group.h"
#include "fec-pkt.h"

static aq_t fec_aq;
static int initialized = 0;

void libfec_init(void) {
  aq_init(&fec_aq);
  initialized = 1;
}

void libfec_close(void) {
  aq_destroy(&fec_aq);
  initialized = 0;
}

void libfec_reset(void) {
  assert(initialized);
  libfec_close();
  libfec_init();
}

fec_group_t *libfec_new_group(unsigned char fec_k,
                              unsigned char fec_n,
                              unsigned long fec_len) {
  fec_group_t *group = malloc(sizeof(fec_group_t));
  if (group == NULL)
    return NULL;
  fec_group_init(group, fec_k, fec_n, 0, 0, fec_len);
  return group;
}

void libfec_add_pkt(fec_group_t *group,
                 unsigned char pkt_seq,
                 unsigned long len,
                 unsigned char *data) {
  assert(group != NULL);
  assert(data != NULL);
  
  fec_pkt_t pkt;
  fec_pkt_init(&pkt);
  pkt.hdr.fec_k = group->fec_k;
  pkt.hdr.fec_n = group->fec_n;
  pkt.hdr.group_seq = group->seq;
  pkt.hdr.packet_seq = pkt_seq;
  pkt.hdr.len = len;
  pkt.hdr.fec_len = group->fec_len;
  pkt.hdr.group_tstamp = group->tstamp;
  memcpy(pkt.payload, data, len);

  fec_group_insert_pkt(group, &pkt);
}

int libfec_decode(fec_group_t *group) {
  assert(initialized);
  if (!fec_group_decode(group, &fec_aq))
    return 0;

  mp3_frame_t *frame;
  while ((frame = aq_get_frame(&fec_aq)) != NULL) {
    memset(frame->raw, 0, 4 + frame->si_size);
    if (!mp3_fill_hdr(frame) ||
        !mp3_fill_si(frame)  ||
        (write(STDOUT_FILENO,
               frame->raw,
               frame->frame_size) < (int)frame->frame_size)) {
      free(frame);
      return 0;
    }
  }

  return 1;
}

void libfec_delete_group(fec_group_t *group) {
  assert(group != NULL);
  fec_group_destroy(group);
  free(group);
}
