
/*C
  (c) 2003 Institut fuer Telematik, Universitaet Karlsruhe
**/

#ifndef PACK_H__
#define PACK_H__

/*M
  \emph{8 bits value packing macro.}

  This macro advances the buffer pointer it is given as first
  argument, and fills the buffer with the 8 bits value
  given as second argument.
**/
#define UINT8_PACK(ptr, i) { *(ptr++) = i; }

/*M
  \emph{8 bits value unpacking macro.}

  This macro advances the buffer pointer it is given as first
  argument, and returns the unpacked 8 bits value in the buffer.
**/
#define UINT8_UNPACK(ptr) (*(ptr++))

/*M
  \emph{16 bits value packing macro.}

  This macro advances the buffer pointer it is given as first
  argument, and fills the buffer with the big endian packed value
  given as second argument.
**/
#define UINT16_PACK(ptr, i)                         \
  { *(ptr++) = (unsigned char)(((i) >> 8) & 0xFF);  \
    *(ptr++) = (unsigned char)((i)        & 0xFF); }

/*M
  \emph{32 bits value packing macro.}

  This macro advances the buffer pointer it is given as first
  argument, and fills the buffer with the big endian packed value
  given as second argument.
**/
#define UINT32_PACK(ptr, i)                         \
  { *(ptr++) = (unsigned char)(((i) >> 24) & 0xFF); \
    *(ptr++) = (unsigned char)(((i) >> 16) & 0xFF); \
    *(ptr++) = (unsigned char)(((i) >> 8)  & 0xFF); \
    *(ptr++) = (unsigned char)((i)         & 0xFF); }

/*M
  \emph{24 bits value packing macro.}
**/
#define UINT24_PACK(ptr, i)                         \
  { *(ptr++) = (unsigned char)(((i) >> 16) & 0xFF); \
    *(ptr++) = (unsigned char)(((i) >> 8)  & 0xFF); \
    *(ptr++) = (unsigned char)((i)         & 0xFF); }

/*M
  \emph{16 bits value unpacking macro.}

  This macro advances the buffer pointer it is given as first
  argument, and returns the unpacked big endian value in the buffer.
**/
#define UINT16_UNPACK(ptr) uint16_unpack__(&ptr)
unsigned int uint16_unpack__(/*@out@*/ unsigned char **ptr);

/*M
  \emph{32 bits value unpacking macro.}

  This macro advances the buffer pointer it is given as first
  argument, and returns the unpacked big endian value in the
  buffer.
**/
#define UINT32_UNPACK(ptr) uint32_unpack__(&ptr)
unsigned int uint32_unpack__(/*@out@*/ unsigned char **ptr);

/*M
  \emph{16 bits value packing macro.}

  This macro advances the buffer pointer it is given as first
  argument, and fills the buffer with the little endian packed value
  given as second argument.
**/
#define LE_UINT16_PACK(ptr, i)                      \
  { *(ptr++) = (unsigned char)((i) & 0xFF);         \
    *(ptr++) = (unsigned char)(((i) >> 8) & 0xFF); }

/*M
  \emph{32 bits value packing macro.}

  This macro advances the buffer pointer it is given as first
  argument, and fills the buffer with the little endian packed value
  given as second argument.
**/
#define LE_UINT32_PACK(ptr, i)                      \
  { *(ptr++) = (unsigned char)((i) & 0xFF);         \
    *(ptr++) = (unsigned char)(((i) >> 8)  & 0xFF); \
    *(ptr++) = (unsigned char)(((i) >> 16) & 0xFF); \
    *(ptr++) = (unsigned char)(((i) >> 24) & 0xFF); }

/*M
  \emph{16 bits value unpacking macro.}

  This macro advances the buffer pointer it is given as first
  argument, and returns the unpacked little endian value in the buffer.
**/
#define LE_UINT16_UNPACK(ptr) le_uint16_unpack__(&ptr)
unsigned int le_uint16_unpack__(/*@out@*/ unsigned char **ptr);

/*M
  \emph{32 bits value unpacking macro.}

  This macro advances the buffer pointer it is given as first
  argument, and returns the unpacked little endian value in the
  buffer.
**/
#define LE_UINT32_UNPACK(ptr) le_uint32_unpack__(&ptr)
unsigned int le_uint32_unpack__(/*@out@*/ unsigned char **ptr);

#endif /* PACK_H__ */

/*C
**/
