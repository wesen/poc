/*C
  (c) 2005 bl0rg.net
**/

#include "conf.h"

#include <assert.h>
#include <stdlib.h>
#include <stdio.h>

#include "mp3.h"

/*M
  \emph{Bitrate table for MPEG Audio Layer III version 1.0.}

  Free format MP3s are considered invalid (for now).
**/
unsigned long bitratetable[16] = {
    -1, 32, 40, 48, 56, 64, 80, 96, 112, 128, 160, 192, 224, 256, 320, -1,
};

/*M
  \emph{Samplerate table for MPEG Audio Layer III version 1.0.}
**/
unsigned short sampleratetable[4][4] = {
    {11025, 12000, 8000,  0},
    {0},
    {22050, 24000, 16000, 0},
    {44100, 48000, 32000, 0},
};

/*M
  \emph{Calculate various information about the MP3 frame.}

  \verb|frame size| and \verb|frame data size| are given in
  bytes. \verb|frame data size| is the size of the MP3 frame without
  header and without side information.
**/
void mp3_calc_hdr(mp3_frame_t *frame) {
  assert(frame != NULL);

  const int is_lsf = frame->id != MPEG_VERSION_1; // MPEG 2 and 2.5 are Lower Sampling Frequency extension

  frame->bitrate = bitratetable[frame->bitrate_index];
  frame->samplerate = sampleratetable[frame->id][frame->samplerfindex];
  frame->samplelen = 1152; /* only layer III */
  if (is_lsf) {
    frame->si_size = frame->mode != (unsigned char) 3 ? 17 : 9;
  } else {
    frame->si_size = frame->mode != (unsigned char) 3 ? 32 : 17;
  }
  frame->si_bitsize = frame->si_size * 8;

  /* calculate frame length */
  frame->frame_size = 144000 * frame->bitrate;
  frame->frame_size /= frame->samplerate;
  frame->frame_size += frame->padding_bit;

  frame->frame_data_size = frame->frame_size - 4 - frame->si_size;
  if (frame->protected == 0)
    frame->frame_data_size -= 2;


  frame->usec = (double) frame->frame_size * 8 * 1000.0 / ((double) frame->bitrate);
}

unsigned long mp3_frame_size(mp3_frame_t *frame) {
  return MP3_HDR_SIZE + frame->si_size + frame->adu_size + (frame->protected ? 0 : 2);
}

/*M
**/
