/*C
  (c) 2005 bl0rg.net
**/

#ifndef BUF_H__
#define BUF_H__

typedef struct buf_s {
  unsigned char *data;
  unsigned long len;
  unsigned long size;
} buf_t;

int buf_alloc(buf_t *buf, unsigned long size);
int buf_grow(buf_t *buf);
int buf_append(buf_t *buf, void *data, unsigned long len);
void buf_free(buf_t *buf);

#endif /* BUF_H__ */

/*C
**/
