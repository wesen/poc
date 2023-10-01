/*C
  (c) 2005 bl0rg.net
**/

#include "conf.h"

#include <limits.h>
#include <assert.h>
#include <stdlib.h>

#include "bv.h"

/*S
 Bit vector implementation
**/

/*M
  \emph{Sanity check.}

  Assume bytes are at least 8 bits long and longs are at least 32
  bits long
**/
#if (CHAR_BIT < 8)
#error "char should be at least 8 bits long"
#endif

/*M
 \emph{Get lower bits of byte.}

 Get the b lower bits of byte x.
**/
#define LOWERBITS(x, b) ((b) ? (x) & ((2 << ((b) - 1)) - 1) : 0)

/*M
 \emph{Get higher bits of byte.}

 Get the b higher bits of byte x.
**/
#define HIGHERBITS(x, b) (((x) & 0xff) >> (8 - (b)))

/*M
  \emph{Initialize a bit vector structure.}
  
 Initialize a bit vector structure bv with byte data data, and bit
 length len. The internal index of the bit vector is set to 0, so that
 future \verb|bv_get_bits| or \verb|bv_put_bits| will access the first
 bit.
**/
void bv_init(bv_t *bv, unsigned char *data, unsigned int len) {
  assert(bv != NULL);
  assert(data != NULL);
  
  bv->data = data;
  bv->len  = len;
  bv->idx  = 0;
}

/*M
  \emph{Reset bit vector.}
  
  Reset the internal index of the bit vector to 0, so that future
  \verb|bv_get_bits| or \verb|bv_put_bits| will access the first bit.
**/
void bv_reset(bv_t *bv) {
  assert(bv != NULL);
  
  bv->idx = 0;
}

/*M
  \emph{Get next bits of bit vector.}
  
  Returns the next numbits bits from bv.
**/
unsigned long bv_get_bits(bv_t *bv, unsigned int numbits) {
  assert(bv != NULL);
  assert(bv->data != NULL);
  assert((numbits <= 32) ||
         "Can not read more than 32 bits from a bit vector");
  assert(bv->len > bv->idx);
  assert(((bv->len - bv->idx) > numbits) ||
         "Bit buffer of bit vector is too small");

  unsigned int cidx = bv->idx >> 3;      /* char index */
  /*M
    Bit overflow from previous char.
  **/
  unsigned int overflow = bv->idx & 0x7;

  bv->idx += numbits;

  /*M
    Most significant bit first.
  **/
  if (numbits <= (8 - overflow))
    return HIGHERBITS(bv->data[cidx] << overflow, numbits);

  /*M
    Length in bytes of bitstring.
  **/
  unsigned int len = ((numbits + overflow) >> 3) + 1;
  /*M
    Number of bits of bitstring in first byte.
  **/
  unsigned long res = LOWERBITS(bv->data[cidx++], 8 - overflow);
  unsigned int i;
  for (i = 1; i < len - 1; i++)
    res = (res << 8) | (bv->data[cidx++] & 0xFF);

  /*M
    Number of bits in last byte.
  **/
  unsigned int lastbits = (overflow + numbits) & 0x07;
  res = (res << lastbits) | HIGHERBITS(bv->data[cidx], lastbits);

  return res;
}

/*M
  \emph{Put bits into bit vector.}
  
 Put the first numbits bits of bits into the bit vector bv.
 Returns 1 on success, 0 on error.
**/
int bv_put_bits(bv_t *bv, unsigned long bits, unsigned int numbits) {
  assert(bv != NULL);
  assert(bv->data != NULL);
  assert(bv->len > bv->idx);
  assert((numbits <= 32) ||
         "Can not write more than 32 bits into a bit vector");

  if (numbits > (bv->len - bv->idx))
    return 0;

  unsigned int cidx = bv->idx >> 3;
  unsigned int overflow = bv->idx & 0x07;

  bv->idx += numbits;
  /*M
    Put first bit as highest order bit of 32 bit long.
  **/
  bits <<= (32 - numbits);

  if ((overflow + numbits) < 8) {
    bv->data[cidx] =
      (HIGHERBITS(bv->data[cidx], overflow) << (8 - overflow)) |
      (HIGHERBITS(bits >> 24, numbits) << (8 - overflow - numbits)) |
      LOWERBITS(bv->data[cidx], 8 - overflow - numbits);
    return 1;
  } else {
    bv->data[cidx] =
      (HIGHERBITS(bv->data[cidx], overflow) << (8 - overflow)) |
      HIGHERBITS(bits >> 24, 8 - overflow);
  }
  cidx++;
  bits <<= 8 - overflow;

  unsigned int len = ((numbits + overflow) >> 3) + 1;
  unsigned int i;
  for (i = 1; i < len - 1; i++, bits <<= 8)
    bv->data[cidx++] = (bits >> 24) & 0xff;

  unsigned int lastbits = (overflow + numbits) & 0x07;
  bv->data[cidx] =
    (HIGHERBITS((bits >> 24) & 0xff, lastbits) << (8 - lastbits)) |
    LOWERBITS(bv->data[cidx], 8 - lastbits);

  return 1;
}

/*C
**/

#ifdef BV_TEST
#include <stdio.h>

void testit(char *name, int result, int should) {
  if (result == should) {
    printf("Test %s was successful\n", name);
  } else {
    printf("Test %s was not successful, %x should have been %x\n",
           name, result, should);
  }
}

int main(void) {
  unsigned char test[4] = {0xaa, 0xaa, 0xaa, 0xaa},
    test2[4] = {0};
  bv_t          bv;

  /* test LOWERBITS */
  testit("LOWERBITS 1 bit", LOWERBITS(0x1, 1), 0x1);
  testit("LOWERBITS 2 bits", LOWERBITS(0x2, 2), 0x2);
  testit("LOWERBITS 2 bits", LOWERBITS(0x2, 1), 0x0);
  testit("LOWERBITS 8 bits", LOWERBITS(0xf, 8), 0xf);

  /* test HIGHERBITS */
  testit("HIGHERBITS 1 bit", HIGHERBITS(0xff, 1), 0x1);
  testit("HIGHERBITS 2 bits", HIGHERBITS(0xff, 2), 0x3);
  testit("HIGHERBITS 2 bits", HIGHERBITS(0x7f, 2), 0x1);
  testit("HIGHERBITS 8 bits", HIGHERBITS(0xff, 8), 0xff);

  bv_init(&bv, test, 32);

  /* test bv_get_bits without char border crossing */
  testit("bv_get_bits noborder 1 bit", bv_get_bits(&bv, 1), 1);
  testit("bv_get_bits noborder 1 bit", bv_get_bits(&bv, 1), 0);
  testit("bv_get_bits noborder 2 bits", bv_get_bits(&bv, 2), 2);
  testit("bv_get_bits noborder 3 bits", bv_get_bits(&bv, 3), 5);

  /* test bv_get_bits accross border */
  testit("bv_get_bits border 2 bits", bv_get_bits(&bv, 2), 1);
  testit("bv_get_bits border 8 bits", bv_get_bits(&bv, 8), 0x55);
  testit("bv_get_bits border 12 bits", bv_get_bits(&bv, 12), 0x555);

  bv_reset(&bv);

  /* test bv_get_bits on 32 bits */
  testit("bv_get_bits border 32 bits", bv_get_bits(&bv, 32), 0xaaaaaaaa);

  bv_init(&bv, test2, 32);
  
  /* test bv_put_bits without char border crossing */
  testit("bv_put_bits noborder 4 bits", bv_put_bits(&bv, 0x5, 4), 1);
  bv_reset(&bv);
  testit("bv_put_bits noborder 4 bits get 4 bits", bv_get_bits(&bv, 4), 0x5);
  bv_reset(&bv);
  testit("bv_put_bits noborder 4 bits get 1 bit", bv_get_bits(&bv, 1), 0);
  testit("bv_put_bits noborder 4 bits get 1 bit", bv_get_bits(&bv, 1), 1);
  testit("bv_put_bits noborder 4 bits get 2 bits", bv_get_bits(&bv, 2), 1);

  bv_reset(&bv);
  testit("bv_put_bits noborder 8 bits", bv_put_bits(&bv, 0x5a, 8), 1);
  bv_reset(&bv);
  testit("bv_put_bits noborder 8 bits get 4 bits", bv_get_bits(&bv, 4), 0x5);
  testit("bv_put_bits noborder 8 bits get 1 bit", bv_get_bits(&bv, 1), 1);
  testit("bv_put_bits noborder 8 bits get 1 bit", bv_get_bits(&bv, 1), 0);
  testit("bv_put_bits noborder 8 bits get 2 bits", bv_get_bits(&bv, 2), 2);

  bv_reset(&bv);
  testit("bv_put_bits noborder 16 bits", bv_put_bits(&bv, 0x5a5a, 16), 1);
  bv_reset(&bv);
  testit("bv_put_bits noborder 16 bits get 4 bits", bv_get_bits(&bv, 4), 0x5);
  testit("bv_put_bits noborder 16 bits get 1 bit", bv_get_bits(&bv, 1), 1);
  testit("bv_put_bits noborder 16 bits get 1 bit", bv_get_bits(&bv, 1), 0);
  testit("bv_put_bits noborder 16 bits get 2 bits", bv_get_bits(&bv, 2), 2);
  testit("bv_put_bits noborder 16 bits get 4 bits", bv_get_bits(&bv, 4), 0x5);
  testit("bv_put_bits noborder 16 bits get 1 bit", bv_get_bits(&bv, 1), 1);
  testit("bv_put_bits noborder 16 bits get 1 bit", bv_get_bits(&bv, 1), 0);
  testit("bv_put_bits noborder 16 bits get 2 bits", bv_get_bits(&bv, 2), 2);

  bv_reset(&bv);
  testit("bv_put_bits border 8 bits", bv_put_bits(&bv, 0x55, 7), 1);
  testit("bv_put_bits border 8 bits", bv_put_bits(&bv, 0x0, 1), 1);
  bv_reset(&bv);
  testit("bv_put_bits border 8 bits get 4 bits", bv_get_bits(&bv, 4), 0xa);
  testit("bv_put_bits border 8 bits get 1 bit", bv_get_bits(&bv, 1), 1);
  testit("bv_put_bits border 8 bits get 1 bit", bv_get_bits(&bv, 1), 0);
  testit("bv_put_bits border 8 bits get 2 bits", bv_get_bits(&bv, 2), 2);

  bv_reset(&bv);
  testit("bv_put_bits border 16 bits", bv_put_bits(&bv, 0x55, 7), 1);
  testit("bv_put_bits border 16 bits", bv_put_bits(&bv, 0x2, 3), 1);
  testit("bv_put_bits border 16 bits", bv_put_bits(&bv, 0x2a, 6), 1);
  bv_reset(&bv);
  testit("bv_put_bits border 16 bits get 4 bits", bv_get_bits(&bv, 4), 0xa);
  testit("bv_put_bits border 16 bits get 1 bit", bv_get_bits(&bv, 1), 1);
  testit("bv_put_bits border 16 bits get 1 bit", bv_get_bits(&bv, 1), 0);
  testit("bv_put_bits border 16 bits get 2 bits", bv_get_bits(&bv, 2), 2);
  testit("bv_put_bits border 16 bits get 4 bits", bv_get_bits(&bv, 4), 0xa);
  testit("bv_put_bits border 16 bits get 1 bit", bv_get_bits(&bv, 1), 1);
  testit("bv_put_bits border 16 bits get 1 bit", bv_get_bits(&bv, 1), 0);
  testit("bv_put_bits border 16 bits get 2 bits", bv_get_bits(&bv, 2), 2);

  bv_reset(&bv);
  testit("bv_put_bits border 32 bits", bv_put_bits(&bv, 1, 4), 1);
  testit("bv_put_bits border 32 bits", bv_put_bits(&bv, 2, 4), 1);
  testit("bv_put_bits border 32 bits", bv_put_bits(&bv, 3, 4), 1);
  testit("bv_put_bits border 32 bits", bv_put_bits(&bv, 4, 4), 1);
  testit("bv_put_bits border 32 bits", bv_put_bits(&bv, 5, 4), 1);
  testit("bv_put_bits border 32 bits", bv_put_bits(&bv, 6, 4), 1);
  testit("bv_put_bits border 32 bits", bv_put_bits(&bv, 7, 4), 1);
  testit("bv_put_bits border 32 bits", bv_put_bits(&bv, 8, 4), 1);
  bv_reset(&bv);
  testit("bv_put_bits border 32 bits", bv_get_bits(&bv, 32), 0x12345678);

  bv_reset(&bv);
  bv_put_bits(&bv, 0xffff, 16);
  bv_reset(&bv);
  bv_put_bits(&bv, 0x0, 2);
  bv_put_bits(&bv, 0x3, 2);
  bv_put_bits(&bv, 0x0, 3);
  bv_reset(&bv);
  testit("bv_put_bits rest 8 bits", bv_get_bits(&bv, 16), 0x31FF);

  bv_reset(&bv);
  bv_put_bits(&bv, 0xffff, 16);
  bv_reset(&bv);
  bv_put_bits(&bv, 0x0, 9);
  bv_reset(&bv);
  testit("bv_put_bits rest 8 bits", bv_get_bits(&bv, 16), 0x7F);

  bv_reset(&bv);
  bv_put_bits(&bv, 0xffff, 16);
  bv_reset(&bv);
  bv_put_bits(&bv, 0x0, 2);
  bv_put_bits(&bv, 0x0, 2);
  bv_put_bits(&bv, 0x0, 3);
  bv_put_bits(&bv, 0x0, 2);
  bv_reset(&bv);
  testit("bv_put_bits rest 8 bits", bv_get_bits(&bv, 16), 0x7F);

  bv_reset(&bv);
  bv_put_bits(&bv, 0xffff, 16);
  bv_reset(&bv);
  bv_get_bits(&bv, 4);
  bv_put_bits(&bv, 0x0, 2);
  bv_reset(&bv);
  testit("bv_put_bits rest 8 bits", bv_get_bits(&bv, 16), 0xF3FF);

  return 0;
}
#endif
