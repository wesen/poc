/*C
  (c) 2003 Institut fuer Telematik, Universitaet Karlsruhe
**/

#ifndef CRC32_H__
#define CRC32_H__

typedef struct crc32_s {
  unsigned long poly;
  unsigned long init;
  unsigned long xor;
  unsigned long table[256];
} crc32_t;

void crc32_init(crc32_t *crc, unsigned long poly,
		unsigned long init, unsigned long xor);
unsigned long crc32(crc32_t *crc,
		    unsigned char *data, unsigned long len);

#endif /* CRC32_H__ */

/*C
**/
