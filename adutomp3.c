#include "conf.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "pack.h"
#include "aq.h"
#include "file.h"
#include "mp3.h"

static void usage(void) {
  printf("Usage: adutomp3 adufile\n");
}

int main(int argc, char *argv[]) {
  int retval = EXIT_SUCCESS;

  if (argc != 2) {
    usage();
    return EXIT_FAILURE;
  }

  file_t infile;
  aq_t qin;
  aq_init(&qin);
  if (!file_open_read(&infile, argv[1])) {
    fprintf(stderr, "Could not open adu file: %s\n", argv[1]);
    retval = EXIT_FAILURE;
    goto exit;
  }

  int ret;
  for (;;) {
    adu_t adu;
    unsigned char len[2];
    unsigned char *ptr = len;

    ret = file_read(&infile, len, 2);
    if (ret != 2)
      break;
    adu.adu_size = UINT16_UNPACK(ptr);
    ret = file_read(&infile, adu.raw, adu.adu_size);
    if (ret != adu.adu_size)
      break;

    if (!mp3_read_hdr(&adu) ||
        !mp3_read_si(&adu)) {
      fprintf(stderr, "Could not read adu\n");
      break;
    }

    if (aq_add_adu(&qin, &adu)) {
      mp3_frame_t *frame = aq_get_frame(&qin);
      assert(frame != NULL);

      if (!mp3_fill_hdr(frame) ||
          !mp3_fill_si(frame)) {
        assert(NULL);
      }
      ret = write(STDOUT_FILENO, frame->raw, frame->frame_size);
      assert(ret == frame->frame_size);
      free(frame);
    }
  }
  file_close(&infile);

 exit:
  aq_destroy(&qin);
  return retval;
}
