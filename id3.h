/*
 * id3v2 generation routines
 *
 * v2 only because this seems to be supported by most players
 *
 * (c) 2005 bl0rg.net
 */

#ifndef ID3_H__
#define ID3_H__

#define ID3_TAG_SIZE    8192
#define ID3_HEADER_SIZE 10

#include "file.h"

unsigned int id3_fill_comment(unsigned char *buf, unsigned int len,
                              unsigned char encoding,
                              unsigned char *short_comment,
                              unsigned char *long_comment,
                              unsigned char *language);
unsigned int id3_fill_tframe(unsigned char *buf, unsigned int len,
                             unsigned char *type,
                             unsigned char encoding,
                             unsigned char *string);
int id3_fill_header(unsigned char *buf, unsigned int len,
                    unsigned long id3_size);

int id3_write_tag(file_t *outfile,
                  unsigned char *album_title,
                  unsigned char *artist,
                  unsigned char *title,
                  unsigned int track_number,
                  unsigned char *comment);

#endif /* ID3_H__ */
