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

#define min(a,b) ((a) > (b) ? (b) : (a))

static int initialized = 0;

static file_t infile;
static int infile_open = 0;
static aq_t qin;

static file_t outfile;
static int outfile_open = 0;
static aq_t qout;

void libfec_init(char *infilename, char *outfilename) {
  if (infilename) {
    aq_init(&qin);
    if (!file_open_read(&infile, infilename))
      assert(NULL);
    infile_open = 1;
  }

  if (outfilename) {
    aq_init(&qout);
    if (!file_open_write(&outfile, outfilename))
      assert(NULL);
    outfile_open = 1;
  }
  
  initialized = 1;
}

void libfec_close(void) {
  if (infile_open) {
    aq_destroy(&qin);
    file_close(&infile);
    infile_open = 0;
  }
  if (outfile_open) {
    aq_destroy(&qout);
    file_close(&outfile);
    outfile_open = 0;
  }
  
  initialized = 0;
}

int libfec_read_adu(unsigned char *dst, unsigned int len) {
  assert(dst != NULL);
  assert(initialized);
  assert(infile_open);
  
  mp3_frame_t frame;
  while (mp3_next_frame(&infile, &frame) > 0) {
    if (aq_add_frame(&qin, &frame)) {
      adu_t *adu = aq_get_adu(&qin);
      assert(adu != NULL);

      if (adu->adu_size == 0) {
        free(adu);
        continue;
      }
      unsigned int retlen = min(len, adu->adu_size);
      memcpy(dst, adu->raw, retlen);

      free(adu);

      return retlen;
    }
  }

  return -1;
}

void libfec_write_adu(unsigned char *buf, unsigned int len) {
  assert(buf != NULL);
  assert(initialized);
  assert(outfile_open);

  fprintf(stderr, "write adu %p, len %d\n", buf, len);

  adu_t adu;
  memcpy(adu.raw, buf, len);
  int ret = mp3_unpack(&adu);
  assert(ret);

  if (aq_add_adu(&qout, &adu)) {
    mp3_frame_t *frame = aq_get_frame(&qout);
    assert(frame != NULL);

    if (!mp3_fill_hdr(frame) ||
        !mp3_fill_si(frame) ||
        !mp3_write_frame(&outfile, frame))
      assert(NULL);

    free(frame);
  }
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

unsigned int libfec_decode(fec_decode_t *group,
                           unsigned char *dst,
                           unsigned int idx,
                           unsigned int len) {
  assert(initialized);
  assert(group != NULL);
  assert(dst != NULL);
  assert(idx < group->fec_k);

  if (!group->decoded)
    fec_group_decode(group);

  if (group->decoded) {
    unsigned int retlen = min(len, group->fec_len);
    memcpy(dst, group->buf + idx * group->fec_len, retlen);
    return retlen;
  } else {
    if (group->lengths[idx] > 0) {
      unsigned int retlen = min(len, group->lengths[idx]);
      memcpy(dst, group->buf + idx * group->fec_len, retlen);
      return retlen;
    } else {
      return 0;
    }
  }
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
  if (fec_k > fec_n)
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

  encode->adu_ptrs = malloc(encode->fec_k * sizeof(unsigned char *));
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
  memcpy(encode->adu_ptrs[encode->count], data, len);
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
