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

  int fec_k = 16;
  int fec_n = 32;
  libfec_init(argv[1], "-");
  
  fec_encode_t *encode = libfec_new_encode(fec_k, fec_n);
  assert(encode != NULL);
  unsigned char buf[8192];
  unsigned int len;
  int ret;

  while ((len = libfec_read_adu(buf, sizeof(buf))) > 0) {
    ret = libfec_add_adu(encode, len, buf);
    assert(ret);
    
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
      for (i = 0; i < fec_k; i++) {
        unsigned char buf[8192];
        unsigned int len;
        len = libfec_decode(group, buf, i, sizeof(buf));
        if (len) {
          libfec_write_adu(buf, len);
        }
      }
      libfec_delete_group(group);
      libfec_delete_encode(encode);
      encode = libfec_new_encode(fec_k, fec_n);
      assert(encode);
      adu_cnt = 0;
    }
  }
  file_close(&infile);
  libfec_close();

 exit:
  aq_destroy(&qin);
  return retval;
}
