/*C
  (c) 2005 bl0rg.net
**/

#include "conf.h"

#include <assert.h>
#include <stdlib.h>
#include <string.h>

#include "lib/data-structures/bv.h"
#include "mp3.h"

/*M
  \emph{Read scalefactor information.}
**/
int mp3_read_sf(mp3_frame_t *frame) {
  assert(frame != NULL);

  unsigned int granule_offset = 0;
  
  unsigned int i;
  for (i = 0; i < 2; i++) {
    unsigned int nch = (frame->mode != 3) ? 2 : 1;
    
    unsigned int j;
    for (j = 0; j < nch; j++) {
      mp3_granule_t *gr = &frame->si.channel[j].granule[i];
      mp3_sf_t *sf = &gr->sf;

      unsigned int bit0 = granule_offset & 7;
      bv_t bv;
      bv_init(&bv, mp3_frame_data_begin(frame) + (granule_offset >> 3),
              gr->part2_length + bit0);
      if (bit0)
        bv_get_bits(&bv, bit0);
      granule_offset += gr->part2_3_length;

      if ((gr->blocksplit_flag == 1) &&
          (gr->block_type == 2)) {
        /* no scale factor selection information, 3 short windows */

        if (gr->switch_point) {
          /* split of long and short transforms at 8 */
          unsigned int sfb;

          if (gr->slen0 > 0) {
            for (sfb = 0; sfb < 8; sfb++)
              sf->l[sfb] = bv_get_bits(&bv, gr->slen0);
            for (sfb = 3; sfb < 6; sfb++) {
              sf->s[0][sfb] = bv_get_bits(&bv, gr->slen0);
              sf->s[1][sfb] = bv_get_bits(&bv, gr->slen0);
              sf->s[2][sfb] = bv_get_bits(&bv, gr->slen0);
            }
          }

          if (gr->slen1 > 0) {
            for (sfb = 6; sfb < 12; sfb++) {
              sf->s[0][sfb] = bv_get_bits(&bv, gr->slen1);
              sf->s[1][sfb] = bv_get_bits(&bv, gr->slen1);
              sf->s[2][sfb] = bv_get_bits(&bv, gr->slen1);
            }
          }
        } else {
          /* no long transforms */
          unsigned int sfb;

          if (gr->slen0 > 0) {
            for (sfb = 0; sfb < 6; sfb ++) {
              sf->s[0][sfb] = bv_get_bits(&bv, gr->slen0);
              sf->s[1][sfb] = bv_get_bits(&bv, gr->slen0);
              sf->s[2][sfb] = bv_get_bits(&bv, gr->slen0);
            }
          }

          if (gr->slen1 > 0) {
            for (sfb = 6; sfb < 12; sfb++) {
              sf->s[0][sfb] = bv_get_bits(&bv, gr->slen1);
              sf->s[1][sfb] = bv_get_bits(&bv, gr->slen1);
              sf->s[2][sfb] = bv_get_bits(&bv, gr->slen1);
            }
          }
        }
      } else {
        /* long blocks types 0, 1, 3 */

        if (i == 0) {
          /* first granule, no scalefactor selection information to
             apply */
          unsigned int sfb;
          if (gr->slen0 > 0)
            for (sfb = 0; sfb < 11; sfb++)
              sf->l[sfb] = bv_get_bits(&bv, gr->slen0);

          if (gr->slen1 > 0)
            for (sfb = 11; sfb < 21; sfb++)
              sf->l[sfb] = bv_get_bits(&bv, gr->slen1);
        } else {
          /* second granule, check scalefactor selection information
             */
          mp3_channel_t *channel = &frame->si.channel[j];
          mp3_sf_t *sf1 = &channel->granule[0].sf;
          unsigned int sfb;

          if (gr->slen0 > 0) {
            /* cb = 0 */
            if (channel->scfsi[0]) {
              for (sfb = 0; sfb < 6; sfb++)
                sf->l[sfb] = sf1->l[sfb];
            } else {
              for (sfb = 0; sfb < 6; sfb++)
                sf->l[sfb] = bv_get_bits(&bv, gr->slen0);
            }
            /* cb = 1 */
            if (channel->scfsi[1]) {
              for (sfb = 6; sfb < 11; sfb++)
                sf->l[sfb] = sf1->l[sfb];
            } else {
              for (sfb = 6; sfb < 11; sfb++)
                sf->l[sfb] = bv_get_bits(&bv, gr->slen0);
            }
          }

          if (gr->slen1 > 0) {
            /* cb = 2 */
            if (channel->scfsi[2]) {
              for (sfb = 11; sfb < 16; sfb++)
                sf->l[sfb] = sf1->l[sfb];
            } else {
              for (sfb = 11; sfb < 16; sfb++)
                sf->l[sfb] = bv_get_bits(&bv, gr->slen1);
            }
            /* cb = 3 */
            if (channel->scfsi[3]) {
              for (sfb = 16; sfb < 21; sfb++)
                sf->l[sfb] = sf1->l[sfb];
            } else {
              for (sfb = 16; sfb < 21; sfb++)
                sf->l[sfb] = bv_get_bits(&bv, gr->slen1);
            }
          }
        }
      }
    }
  }

  return 1;
}

/*M
  \emph{Fill MPEG frame with scalefactor information.}
**/
int mp3_fill_sf(mp3_frame_t *frame) {
  assert(frame != NULL);

  unsigned int granule_offset = 0;

  unsigned int i;
  for (i = 0; i < 2; i++) {
    unsigned int nch = (frame->mode != 3) ? 2 : 1;
    
    unsigned int j;
    for (j = 0; j < nch; j++) {
      mp3_granule_t *gr = &frame->si.channel[j].granule[i];
      mp3_sf_t *sf = &gr->sf;

      unsigned int bit0 = granule_offset & 7;
      bv_t bv;
      bv_init(&bv, mp3_frame_data_begin(frame) + (granule_offset >> 3),
              gr->part2_length + bit0);
      if (bit0)
        bv_get_bits(&bv, bit0);
      granule_offset += gr->part2_3_length;

      if ((gr->blocksplit_flag == 1) &&
          (gr->block_type == 2)) {
        /* no scale factor selection information, 3 short windows */

        if (gr->switch_point) {
          /* split of long and short transforms at 8 */
          unsigned int sfb;

          if (gr->slen0 > 0) {
            for (sfb = 0; sfb < 8; sfb++)
              bv_put_bits(&bv, sf->l[sfb], gr->slen0);
            for (sfb = 3; sfb < 6; sfb++) {
              bv_put_bits(&bv, sf->s[0][sfb], gr->slen0);
              bv_put_bits(&bv, sf->s[1][sfb], gr->slen0);
              bv_put_bits(&bv, sf->s[2][sfb], gr->slen0);
            }
          }

          if (gr->slen1 > 0) {
            for (sfb = 6; sfb < 12; sfb++) {
              bv_put_bits(&bv, sf->s[0][sfb], gr->slen1);
              bv_put_bits(&bv, sf->s[1][sfb], gr->slen1);
              bv_put_bits(&bv, sf->s[2][sfb], gr->slen1);
            }
          }
        } else {
          /* no long transforms */
          unsigned int sfb;

          if (gr->slen0 > 0) {
            for (sfb = 0; sfb < 6; sfb ++) {
              bv_put_bits(&bv, sf->s[0][sfb], gr->slen0);
              bv_put_bits(&bv, sf->s[1][sfb], gr->slen0);
              bv_put_bits(&bv, sf->s[2][sfb], gr->slen0);
            }
          }

          if (gr->slen1 > 0) {
            for (sfb = 6; sfb < 12; sfb++) {
              bv_put_bits(&bv, sf->s[0][sfb], gr->slen1);
              bv_put_bits(&bv, sf->s[1][sfb], gr->slen1);
              bv_put_bits(&bv, sf->s[2][sfb], gr->slen1);
            }
          }
        }
      } else {
        /* long blocks types 0, 1, 3 */

        if (i == 0) {
          /* first granule, no scalefactor selection information to
             apply */
          unsigned int sfb;

          if (gr->slen0 > 0)
            for (sfb = 0; sfb < 11; sfb++)
              bv_put_bits(&bv, sf->l[sfb], gr->slen0);

          if (gr->slen1 > 0)
            for (sfb = 11; sfb < 21; sfb++)
              bv_put_bits(&bv, sf->l[sfb], gr->slen1);
        } else {
          /* second granule, check scalefactor selection information
             */
          mp3_channel_t *channel = &frame->si.channel[j];
          
          unsigned int sfb;

          if (gr->slen0 > 0) {
            /* cb = 0 */
            if (!channel->scfsi[0])
              for (sfb = 0; sfb < 6; sfb++)
                bv_put_bits(&bv, sf->l[sfb], gr->slen0);
            /* cb = 1 */
            if (!channel->scfsi[1])
              for (sfb = 6; sfb < 11; sfb++)
                bv_put_bits(&bv, sf->l[sfb], gr->slen0);
          }

          if (gr->slen1 > 0) {
            /* cb = 2 */
            if (!channel->scfsi[2])
              for (sfb = 11; sfb < 16; sfb++)
                bv_put_bits(&bv, sf->l[sfb], gr->slen1);
            /* cb = 3 */
            if (!channel->scfsi[3])
              for (sfb = 16; sfb < 21; sfb++)
                bv_put_bits(&bv, sf->l[sfb], gr->slen1);
          }
        }
      }
    }
  }

  return 1;
}

/*C
**/

#ifdef MP3SF_TEST
#include <stdio.h>
#include <strings.h>

#include "aq.h"

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

  aq_t qin, qout;
  aq_init(&qin);
  aq_init(&qout);

  mp3_frame_t frame = {0};
  while (mp3_next_frame(&in, &frame) > 0) {
    static int cin = 0;
    printf("%d in, cksum %ld\n", cin++,
           cksum(frame.raw, frame.frame_size));

    if (aq_add_frame(&qin, &frame)) {
      adu_t *adu = aq_get_adu(&qin);
      assert(adu != NULL);

      static int cadu = 0;
      mp3_read_sf(adu);
      printf("frame %d\n", cadu);
      unsigned int i;
      for (i = 0; i < 2; i++) {
        unsigned int j;
        for (j = 0; j < 2; j++) {
          unsigned int l;
          printf("ch %d, gr %d: ", j, i);
          for (l = 0; l < 22; l++)
            printf("%d, ", adu->si.channel[j].granule[i].sf.l[l]);
          printf("\n");
        }
      }
      unsigned oldck = cksum(adu->raw, MP3_RAW_SIZE);
      mp3_fill_sf(adu);
      unsigned newck = cksum(adu->raw, MP3_RAW_SIZE);
      if (newck != oldck)
        printf("corruption on frame %d\n", cadu);
      mp3_read_sf(adu);
      for (i = 0; i < 2; i++) {
        unsigned int j;
        for (j = 0; j < 2; j++) {
          unsigned int l;
          printf("ch %d, gr %d: ", j, i);          
          for (l = 0; l < 22; l++)
            printf("%d, ", adu->si.channel[j].granule[i].sf.l[l]);
          printf("\n");
        }
      }
      cadu++;
      
      if (aq_add_adu(&qout, adu)) {
        
        mp3_frame_t *frame_out = aq_get_frame(&qout);
        assert(frame_out != NULL);

        static int cout = 0;

        memset(frame_out->raw, 0, 4 + frame_out->si_size);
        if (!mp3_fill_hdr(frame_out) ||
            !mp3_fill_si(frame_out) ||
            (mp3_write_frame(&out, frame_out) <= 0)) {
          fprintf(stderr, "Could not write frame\n");
          mp3_close(&in);
          mp3_close(&out);
          return 1;
        }

        printf("%d out, cksum %ld\n", cout++,
               cksum(frame_out->raw,
                     frame_out->frame_size));
        
        free(frame_out);
      }

      free(adu);
    }

    /*    fgetc(stdin); */
  }

  mp3_close(&in);
  mp3_close(&out);

  aq_destroy(&qin);
  aq_destroy(&qout);

  return 0;
}
#endif
