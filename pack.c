/*C
  (c) 2003 Institut fuer Telematik, Universitaet Karlsruhe
**/

#include "conf.h"

#include "pack.h"

/*@+charint@*/

/*M
  \emph{Unpack a big endian 16 bit integer.}

  This could not be done in a macro.
**/
unsigned int uint16_unpack__(/*@out@*/ unsigned char **ptr) {
   int     i = (*((*ptr)++) & 0xFF) << 8;
   return (i |= *((*ptr)++) & 0xFF);
}

/*M
  \emph{Unpack a big endian 32 bit integer.}

  This could not be done in a macro.
**/
unsigned int uint32_unpack__(/*@out@*/ unsigned char **ptr) {
   int     i =  (*((*ptr)++) & 0xFF) << 24;
           i |= (*((*ptr)++) & 0xFF) << 16;
           i |= (*((*ptr)++) & 0xFF) << 8;
   return (i |=  *((*ptr)++) & 0xFF);
}

/*M
  \emph{Unpack a little endian 16 bit integer.}

  This could not be done in a macro.
**/
unsigned int le_uint16_unpack__(/*@out@*/ unsigned char **ptr) {
   int     i = (*((*ptr)++) & 0xFF);
   return (i |= (*((*ptr)++) & 0xFF) << 8);
}

/*M
  \emph{Unpack a little endian 32 bit integer.}

  This could not be done in a macro.
**/
unsigned int le_uint32_unpack__(/*@out@*/ unsigned char **ptr) {
  int     i =  (*((*ptr)++) & 0xFF);
  i |= (*((*ptr)++) & 0xFF) << 8;
  i |= (*((*ptr)++) & 0xFF) << 16;
  return (i |=  (*((*ptr)++) & 0xFF) << 24);
}

/*C
**/

#ifdef PACK_TEST
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
  int i16 = 0x1234,
      i32 = 0x12345678;
  unsigned char test[4], *ptr;

  ptr = test;
  UINT16_PACK(ptr, i16);
  ptr = test;
  testit("UNPACK after PACK 16", UINT16_UNPACK(ptr), i16);
  ptr = test;
  UINT32_PACK(ptr, i32);
  ptr = test;
  testit("UNPACK after PACK 32", UINT32_UNPACK(ptr), i32);

  ptr = test;
  UINT16_PACK(ptr, i16);
  testit("Big Endianness 16 1/2", test[0], 0x12);
  testit("Big Endianness 16 2/2", test[1], 0x34);
  ptr = test;
  UINT32_PACK(ptr, i32);
  testit("Big Endianness 32 1/4", test[0], 0x12);
  testit("Big Endianness 32 2/4", test[1], 0x34);
  testit("Big Endianness 32 3/4", test[2], 0x56);
  testit("Big Endianness 32 4/4", test[3], 0x78);

  return 0;
}
#endif

