/*C
  (c) 2005 bl0rg.net
**/

#include "conf.h"

#include <assert.h>

#include "bv.h"
#include "mp3.h"

int mp3_read_huffman(mp3_frame_t *frame) {
  assert(frame != NULL);

  unsigned int granule_offset = 0;

  unsigned int i;
  for (i = 0; i < 2; i++) {
    unsigned int nch = (frame->mode != 3) ? 2 : 1;

    unsigned int j;
    for (j = 0; j < nch; j++) {
      mp3_granule_t *gr = &frame->si.channel[j].granule[i];

      granule_offset += gr->part2_length;

      unsigned int n1 = 
      
      unsigned int bit0 = granule_offset & 7;
      bv_t bv;
      bv_init(&bv, mp3_frame_data_begin(frame) + (granule_offset >> 3),
              gr->part3_length + bit0);
    }

}
