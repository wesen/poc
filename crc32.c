/*C
  (c) 2003 Institut fuer Telematik, Universitaet Karlsruhe
**/

#include "conf.h"

#include "crc32.h"

void crc32_init(crc32_t *crc, unsigned long poly,
		unsigned long init, unsigned long xor) {
  crc->poly = poly;
  crc->init = init;
  crc->xor = xor;

  int i;
  for (i = 0; i < 256; i++) {
    unsigned long r = (unsigned long)i;
    r <<= 24;

    int j;
    for (j = 0; j < 8; j++) {
      unsigned long bit = r & (1 << 31);
      r <<= 1;
      if (bit)
	r ^= crc->poly;
    }

    r &= 0xFFFFFFFF;
    crc->table[i] = r;
  }
}

unsigned long crc32(crc32_t *crc,
		    unsigned char *data, unsigned long len) {
  unsigned long r = crc->init;
  unsigned char *ptr = data;
  while (len--)
    r = (r << 8) ^ crc->table[(r >> 24) ^ *ptr++];

  r ^= crc->xor;
  r &= 0xFFFFFFFF;
  return r;
}

#ifdef CRC32_TEST

#include <stdio.h>
#include <string.h>

static void usage(void) {
  fprintf(stderr, "Usage: ./crc32test polynom messagestring\n");
}

int main(int argc, char *argv[]) {
  if (argc != 3) {
    usage();
    return 1;
  }

  unsigned long poly;
  sscanf(argv[1], "%lx", &poly);
  crc32_t crc;
  crc32_init(&crc, poly, 0, 0);

  printf("crc: %lx\n", crc32(&crc, argv[2], strlen(argv[2])));

  return 0;
}

#endif /* CRC32_TEST */

/*C
**/
