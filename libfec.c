#include <sys/types.h>
#include <sys/uio.h>

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <assert.h>

#include "fec.h"
#include "fec-group.h"
#include "fec-pkt.h"

#include "libfec.h"

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

fec_decode_t *libfec_new_group(unsigned char fec_k,
                              unsigned char fec_n,
                              unsigned long fec_len) {
  fec_decode_t *group = malloc(sizeof(fec_decode_t));
  if (group == NULL)
    return NULL;
  fec_group_init(group, fec_k, fec_n, 0, 0, fec_len);
  return group;
}

void libfec_add_pkt(fec_decode_t *group,
                 unsigned char pkt_seq,
                 unsigned long len,
                 unsigned char *data) {
  assert(group != NULL);
  assert(data != NULL);
  assert(len <= group->fec_len);
  assert(pkt_seq <= group->fec_n);
  
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

int libfec_decode(fec_decode_t *group) {
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

void libfec_delete_group(fec_decode_t *group) {
  assert(group != NULL);
  fec_group_destroy(group);
  free(group);
}

struct fec_encode_s {
  unsigned char fec_k;
  unsigned char fec_n;
  fec_t *fec;

  unsigned char *adus;
  unsigned int  *lengths;
  unsigned char **adu_ptrs;
  
  unsigned char count;
  unsigned int  max_length;
};

fec_encode_t *libfec_new_encode(unsigned char fec_k,
                                unsigned char fec_n) {
  if (fec_n <= fec_k)
    return NULL;
  
  fec_encode_t *encode = malloc(sizeof(fec_encode_t));
  if (encode == NULL)
    return NULL;

  encode->fec_n        = fec_n;
  encode->fec_k        = fec_k;
  encode->adus         = NULL;
  encode->adu_ptrs     = NULL;
  encode->lengths      = NULL;
  encode->count        = 0;
  encode->max_length   = 0;
  
  encode->fec = fec_new(encode->fec_k, encode->fec_n);
  if (encode->fec == NULL)
    goto exit;
  
  encode->adus = malloc(FEC_PKT_PAYLOAD_SIZE * fec_k);
  if (encode->adus == NULL)
    goto exit;
  memset(encode->adus, 0, FEC_PKT_PAYLOAD_SIZE * fec_k);

  encode->adu_ptrs = malloc(fec_k * sizeof(unsigned char *));
  if (encode->adu_ptrs == NULL)
    goto exit;
  encode->lengths = malloc(sizeof(unsigned int) * fec_k);
  if (encode->lengths == NULL)
    goto exit;
  int i;
  for (i = 0; i < encode->fec_k; i++) {
    encode->adu_ptrs[i] = encode->adus + i * FEC_PKT_PAYLOAD_SIZE;
    encode->lengths[i] = 0;
  }

  return encode;

exit:
  libfec_delete_encode(encode);
  return NULL;
}

void libfec_delete_encode(fec_encode_t *encode) {
  assert(encode != NULL);
  if (encode->adus)
    free(encode->adus);
  if (encode->lengths)
    free(encode->lengths);
  if (encode->adu_ptrs)
    free(encode->adu_ptrs);
  if (encode->fec)
    fec_free(encode->fec);
  free(encode);
}

int libfec_add_adu(fec_encode_t *encode,
                   unsigned long len,
                   unsigned char *data) {
  assert(encode != NULL);
  assert(encode->lengths != NULL);
  assert(data != NULL);

  if ((encode->adus == NULL) ||
      (encode->count >= encode->fec_k))
    return 0;

  encode->lengths[encode->count] = len;
  memcpy(encode->adus + FEC_PKT_PAYLOAD_SIZE * encode->count,
         data, len);
  encode->count++;
  if (encode->max_length < len)
    encode->max_length = len;

  return 1;
}

unsigned int libfec_max_length(fec_encode_t *encode) {
  return encode->max_length;
}

unsigned int libfec_encode(fec_encode_t *encode,
                           unsigned char *dst,
                           unsigned int idx,
                           unsigned int len) {
  assert(encode != NULL);
  if (encode->count != encode->fec_k)
    return 0;
  if (encode->max_length > len)
    return 0;
  
  assert(encode->adu_ptrs != NULL);
  assert(encode->fec != NULL);
  assert(dst != NULL);
  fec_encode(encode->fec, encode->adu_ptrs,
             dst, idx, len);

  if (idx < encode->fec_k)
    return encode->lengths[idx];
  else
    return encode->max_length;
}
