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
  printf("Usage: mp3toadu mp3file\n");
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
    fprintf(stderr, "Could not open mp3 file: %s\n", argv[1]);
    retval = EXIT_FAILURE;
    goto exit;
  }

  mp3_frame_t frame;
  int ret;
  while (mp3_next_frame(&infile, &frame) > 0) {
    if (aq_add_frame(&qin, &frame)) {
      adu_t *adu = aq_get_adu(&qin);
      assert(adu != NULL);

      unsigned char len[2];
      unsigned char *ptr = len;
      fprintf(stderr, "size: %d\n", adu->adu_size);
      UINT16_PACK(ptr, adu->adu_size);
      ret = write(STDOUT_FILENO, len, 2);
      assert(ret == 2);
      ret = write(STDOUT_FILENO, adu->raw, adu->adu_size);
      assert(ret == adu->adu_size);

      free(adu);
    }
  }
  file_close(&infile);

 exit:
  aq_destroy(&qin);
  return retval;
}
