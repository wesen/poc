/*C
  (c) 2005 bl0rg.net
**/

#include "conf.h"

#include <assert.h>
#include <stdlib.h>
#include <string.h>

#include "buf.h"

int buf_alloc(buf_t *buf, unsigned long size) {
  assert(buf != NULL);
  assert(size > 0);

  buf->data = malloc(size);
  if (buf->data != NULL) {
    buf->size = size;
    buf->len = 0;
    return 1;
  } else 
    return 0;
}

int buf_grow(buf_t *buf) {
  assert(buf != NULL);
  assert(buf->size > 0);
  assert(buf->data != NULL);
  assert(buf->len <= buf->size);
  
  void *new = realloc(buf->data, buf->size * 2);
  if (new != NULL) {
    buf->data = new;
    buf->size *= 2;
    return 1;
  } else
    return 0;
}

int buf_append(buf_t *buf, void *data, unsigned long len) {
  assert(buf != NULL);
  assert(buf->data != NULL);
  assert(data != NULL);
  assert(len > 0);

 again:
  if ((buf->len + len) > buf->size) {
    if (!buf_grow(buf))
      return 0;
    else
      goto again;
  }

  memcpy(buf->data + buf->len, data, len);
  buf->len += len;

  return 1;
}

void buf_free(buf_t *buf) {
  assert(buf != NULL);
  assert(buf->size > 0);
  assert(buf->data != NULL);
  
  free(buf->data);
  buf->size = 0;
  buf->len = 0;
  buf->data = NULL;
}

/*C
**/
