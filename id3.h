/*
 * id3v2 generation routines
 *
 * v2 only because this seems to be supported by most players
 *
 * (c) 2005 bl0rg.net
 */

#ifndef ID3_H__
#define ID3_H__

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

#endif /* ID3_H__ */
