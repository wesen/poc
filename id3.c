/*
 * id3v2 routines
 *
 * (c) 2005 bl0rg.net
 */

#include "conf.h"

#include <assert.h>
#include <stdlib.h>
#include <string.h>

#include "pack.h"
#include "id3.h"

/* create a sync safe integer (bit 7 is 0) */
static unsigned long id3_sync_safe(unsigned long num) {
  unsigned long res = 0;
  int i;
  for (i = 0; i < 4; i++) {
    res |= (num & 0x7F) << (i * 8);
    num >>= 7;
  }
  return res;
}

unsigned int id3_fill_comment(unsigned char *buf, unsigned int len,
                              unsigned char encoding,
                              unsigned char *short_comment,
                              unsigned char *long_comment,
                              unsigned char *language) {
  assert(buf != NULL);
  assert(len > 0);
  if ((short_comment == NULL) &&
      (long_comment == NULL))
    return 0;
  if (language == NULL)
    language = "eng";

  unsigned char *ptr = buf;
  unsigned int flen = 1 + 3 +
    (short_comment ? strlen(short_comment) + 1 : 1) +
    (long_comment ? strlen(long_comment) + 1 : 1);
  if (len < 6 + flen)
    return 0;
  memcpy(ptr, "COM", 3); ptr += 3;
  UINT24_PACK(ptr, id3_sync_safe(flen));
  UINT8_PACK(ptr, encoding);
  memcpy(ptr, language, 3); ptr += 3;
  if (short_comment)
    memcpy(ptr, short_comment, strlen(short_comment));
  UINT8_PACK(ptr, 0x00);
  if (long_comment)
    memcpy(ptr, long_comment, strlen(long_comment));
  UINT8_PACK(ptr, 0x00);

  return 6 + flen;
}

unsigned int id3_fill_tframe(unsigned char *buf, unsigned int len,
                             unsigned char *type,
                             unsigned char encoding,
                             unsigned char *string) {
  assert(buf != NULL);
  assert(len > 0);
  assert(type != NULL);
  assert(string != NULL);

  unsigned char *ptr = buf;
  unsigned int slen = strlen(string);
  unsigned int flen = slen + 2;
  if (len < 6 + flen)
    return 0;
  memcpy(ptr, type, 3); ptr += 3;
  UINT24_PACK(ptr, id3_sync_safe(flen));
  UINT8_PACK(ptr, encoding);
  memcpy(ptr, string, slen); ptr += slen;
  UINT8_PACK(ptr, 0x00);

  return flen + 6;
}

int id3_fill_header(unsigned char *buf, unsigned int len,
                    unsigned long id3_size) {
  assert(buf != NULL);
  assert(len > 0);
  if (len < 10)
    return 0;

  unsigned char *ptr = buf;
  memcpy(ptr, "ID3", 3); ptr += 3;
  UINT8_PACK(ptr, 0x02); UINT8_PACK(ptr, 0x00); /* version 2.0 */
  UINT8_PACK(ptr, 0x00);
  UINT32_PACK(ptr, id3_sync_safe(id3_size));
  return 10;
}
