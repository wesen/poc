/*C
  (c) 2005 bl0rg.net
**/

#include "conf.h"

#include <stdio.h>
#include <errno.h>
#include <assert.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>

#include "mp3.h"
#include "bv.h"


/*M
**/
int mp3_fill_si(mp3_frame_t *frame) {
  assert(frame != NULL);
  assert((frame->si_bitsize != 0) ||
         "Trying to write an empty sideinfo");

  const int is_lsf = frame->id != MPEG_VERSION_1; // MPEG 2 and 2.5 are Lower Sampling Frequency extension

  unsigned char *ptr = frame->raw + 4;
  if (frame->protected == 0)
    ptr += 2;

  bv_t bv;
  bv_init(&bv, ptr, frame->si_bitsize);

  mp3_si_t *si = &frame->si;
  unsigned int nch = (frame->mode != 3) ? 2 : 1;

  bv_put_bits(&bv, si->main_data_end, is_lsf ? 8 : 9);
  const int private_bitlen = is_lsf ? ((nch == 1) ? 1 : 2) :
                             ((nch == 1) ? 5 : 3);
  bv_put_bits(&bv, si->private_bits, private_bitlen);

  unsigned int i;
  int ngr = 1;

  if (!is_lsf) {
    for (i = 0; i < nch; i++) {
      unsigned int band;
      for (band = 0; band < 4; band++)
        bv_put_bits(&bv, si->channel[i].scfsi[band], 1);
    }
  }

  unsigned int gri;
  for (gri = 0; gri < ngr; gri++) {
    for (i = 0; i < nch; i++) {
      mp3_granule_t *gr = &si->channel[i].granule[gri];

      bv_put_bits(&bv, gr->part2_3_length, 12);
      bv_put_bits(&bv, gr->big_values, 9);
      bv_put_bits(&bv, gr->global_gain, 8);
      bv_put_bits(&bv, gr->scale_comp, is_lsf ? 9 : 4);
      bv_put_bits(&bv, gr->blocksplit_flag, 1);

      if (gr->blocksplit_flag) {
        bv_put_bits(&bv, gr->block_type, 2);
        bv_put_bits(&bv, gr->switch_point, 1);
        bv_put_bits(&bv, gr->tbl_sel[0], 5);
        bv_put_bits(&bv, gr->tbl_sel[1], 5);

        unsigned int j;
        for (j = 0; j < 3; j++)
          bv_put_bits(&bv, gr->sub_gain[j], 3);
      } else {
        unsigned int j;
        for (j = 0; j < 3; j++)
          bv_put_bits(&bv, gr->tbl_sel[j], 5);

        bv_put_bits(&bv, gr->reg0_cnt, 4);
        bv_put_bits(&bv, gr->reg1_cnt, 3);
      }

      if (!is_lsf) {
        bv_put_bits(&bv, gr->preflag, 1);
      }
      bv_put_bits(&bv, gr->scale_scale, 1);
      bv_put_bits(&bv, gr->cnt1tbl_sel, 1);
    }
  }

  assert((bv.len == bv.idx) || "Bitvector is not filled completely");

  return 1;
}

extern unsigned long bitratetable[16];
extern unsigned short sampleratetable[4];

/*M
**/
int mp3_fill_hdr(mp3_frame_t *frame) {
  assert(frame != NULL);
  
  bv_t bv;
  bv_init(&bv, frame->raw, 4 * 8);

  bv_put_bits(&bv, 0xFFF, 12);
  bv_put_bits(&bv, frame->id, 1);
  bv_put_bits(&bv, frame->layer, 2);
  bv_put_bits(&bv, frame->protected, 1);
  bv_put_bits(&bv, frame->bitrate_index, 4);
  bv_put_bits(&bv, frame->samplerfindex, 2);
  bv_put_bits(&bv, frame->padding_bit, 1);
  bv_put_bits(&bv, frame->private_bit, 1);
  bv_put_bits(&bv, frame->mode, 2);
  bv_put_bits(&bv, frame->mode_ext, 2);
  bv_put_bits(&bv, frame->copyright, 1);
  bv_put_bits(&bv, frame->original, 1);
  bv_put_bits(&bv, frame->emphasis, 2);
  if (frame->protected == 0) {
    frame->raw[4] = frame->crc[0];
    frame->raw[5] = frame->crc[1];
  }

  assert((bv.idx == bv.len) || "Bitvector is not filled completely");
  
  mp3_calc_hdr(frame);

  return 1;
}

/*M
**/
int mp3_write_frame(file_t *mp3, mp3_frame_t *frame) {
  assert(mp3 != NULL);
  assert(frame != NULL);

  if (file_write(mp3, frame->raw, frame->frame_size) <= 0)
    return EEOF;

  return 1;
}

/*C
**/

#ifdef MP3_TEST
int main(int argc, char *argv[]) {
  char *f[2];

  if (!(f[0] = *++argv) || !(f[1] = *++argv)) {
    fprintf(stderr, "Usage: mp3-write mp3in mp3out\n");
    return 1;
  }

  mp3_file_t in;
  if (!mp3_open_read(&in, f[0])) {
    fprintf(stderr, "Could not open mp3 file for read: %s\n", f[0]);
    return 1;
  }

  mp3_file_t out;
  if (!mp3_open_write(&out, f[1])) {
    fprintf(stderr, "Could not open mp3 file for write: %s\n", f[1]);
    mp3_close(&in);
    return 1;
  }

  mp3_frame_t frame;
  while (mp3_next_frame(&in, &frame) > 0) {
    memset(frame.raw, 0, 4 + frame.si_size);
    if (!mp3_fill_hdr(&frame) ||
        !mp3_fill_si(&frame) ||
        (mp3_write_frame(&out, &frame) <= 0)) {
      fprintf(stderr, "Could not write frame\n");
      mp3_close(&in);
      mp3_close(&out);
      return 1;
    }
  }

  mp3_close(&in);
  mp3_close(&out);

  return 0;
}
#endif
