/*C
  (c) 2003 Institut fuer Telematik, Universitaet Karlsruhe
**/

#include "conf.h"

#include <stdio.h>
#include <assert.h>
#include <string.h>

#include "mp3.h"

int mp3_trans_frame(mp3_frame_t *frame) {
  assert(frame != NULL);
  
  unsigned int i;
  for (i = 0; i < 2; i++) {
    unsigned int j;
    for (j = 0; j < 4; j++)
      frame->si.channel[i].scfsi[j] = 1;
  }

  return 1;
}

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
    if (!mp3_trans_frame(&frame)) {
      fprintf(stderr, "Could not transform frame\n");
      mp3_close(&in);
      mp3_close(&out);
      return 1;
    }
    
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

