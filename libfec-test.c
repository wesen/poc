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
#include "libfec.h"

static void usage(void) {
  printf("Usage: libfec-test mp3file\n");
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

  int fec_k = 16;
  int fec_n = 32;
  libfec_init();

  fec_encode_t *encode = libfec_new_encode(fec_k, fec_n);
  assert(encode != NULL);
  mp3_frame_t frame;
  int ret;
  while (mp3_next_frame(&infile, &frame) > 0) {
    if (aq_add_frame(&qin, &frame)) {
      static int adu_cnt = 0;
      adu_t *adu = aq_get_adu(&qin);
      assert(adu != NULL);

      fprintf(stderr, "Adding adu with index %d\n", adu_cnt);
      ret = libfec_add_adu(encode, adu->adu_size, adu->raw);
      assert(ret);

      free(adu);
      
      /* decode */
      if (++adu_cnt == fec_k) {
        int max_length = libfec_max_length(encode);
        unsigned char fec_pkts[fec_n][max_length];
        unsigned int lengths[fec_n];
        int i;
        for (i = 0; i < fec_n; i++) {
          lengths[i] = libfec_encode(encode, fec_pkts[i], i, max_length);
          assert(lengths[i] != 0);
        }
        fec_decode_t *group = libfec_new_group(fec_k, fec_n, max_length);
        for (i = 0; i < fec_n; i++) {
          libfec_add_pkt(group, i, lengths[i], fec_pkts[i]);
        }
        ret = libfec_decode(group);
        assert(ret);
        libfec_delete_group(group);
        libfec_delete_encode(encode);
        encode = libfec_new_encode(fec_k, fec_n);
        assert(encode);
        adu_cnt = 0;
      }
    }
  }
  file_close(&infile);
  libfec_close();

 exit:
  aq_destroy(&qin);
  return retval;
}
