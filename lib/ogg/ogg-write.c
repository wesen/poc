/*C
  (c) 2005 bl0rg.net
**/

#include "conf.h"

#include <assert.h>
#include <stdlib.h>
#include <string.h>

#include "pack.h"
#include "lib/system/file.h"
#include "ogg.h"
#include "lib/math/crc32.h"

int ogg_fill_page_hdr(ogg_page_t *page) {
  assert(page != NULL);

  unsigned long size = OGG_HDR_MIN_SIZE + page->page_segments;
  int i;
  for (i = 0; i < page->page_segments; i++)
    size += page->lacing_values[i];

  if (page->raw.size < size)
    if (!buf_grow(&page->raw))
      return 0;

  unsigned char *ptr = page->raw.data;
  UINT8_PACK(ptr, OGG_SYNC_HDR_BYTE1);
  UINT8_PACK(ptr, OGG_SYNC_HDR_BYTE2);
  UINT8_PACK(ptr, OGG_SYNC_HDR_BYTE3);
  UINT8_PACK(ptr, OGG_SYNC_HDR_BYTE4);
  UINT8_PACK(ptr, 0);

  unsigned char flags = 0;
  if (page->b.continuation)
    flags |= 1;
  if (page->b.first)
    flags |= 2;
  if (page->b.last)
    flags |= 4;
  UINT8_PACK(ptr, flags);

  memcpy(ptr, page->position, 8);
  ptr += 8;
  LE_UINT32_PACK(ptr, page->stream);
  LE_UINT32_PACK(ptr, page->page_no);
  LE_UINT32_PACK(ptr, 0);
  UINT8_PACK(ptr, page->page_segments);
  for (i = 0; i < page->page_segments; i++)
    UINT8_PACK(ptr, page->lacing_values[i]);

  return 1;
}

void ogg_fill_page_cksum(ogg_page_t *page) {
  assert(page != NULL);
  assert(page->raw.data != NULL);
  
  unsigned long cksum = crc32(&ogg_crc32, page->raw.data, page->size);
  unsigned char *ptr = page->raw.data + 22;
  LE_UINT32_PACK(ptr, cksum);
}

int ogg_write_page(file_t *ogg, ogg_page_t *page) {
  assert(ogg != NULL);
  assert(page != NULL);
  assert(page->raw.data != NULL);
  assert(page->size <= page->raw.size);

  if (file_write(ogg, page->raw.data, page->size) <= 0)
    return EEOF;

  return 1;
}

/*C
**/

#ifdef OGG_WRITETEST
#include <stdio.h>
#include <string.h>

int main(int argc, char *argv[]) {
  char *f[2];

  if (!(f[0] = *++argv) ||
      !(f[1] = *++argv)) {
    fprintf(stderr, "Usage: oggtest mp3in mp3out\n");
    return 1;
  }

  file_t in;
  if (!file_open_read(&in, f[0])) {
    fprintf(stderr, "Could not open ogg file for read: %s\n", f[0]);
    return 1;
  }

  file_t out;
  if (!file_open_write(&out, f[1])) {
    fprintf(stderr, "Could not open ogg file for write: %s\n", f[1]);
    file_close(&in);
    return 1;
  }

  ogg_page_t page;
  ogg_init();
  ogg_page_init(&page);

  while (ogg_next_page(&in, &page) > 0) {
    memset(page.raw.data, 0, OGG_HDR_MIN_SIZE);
    if (!ogg_fill_page_hdr(&page)) {
      ogg_page_destroy(&page);
      file_close(&in);
      file_close(&out);
      return 1;
    }

    ogg_fill_page_cksum(&page);
      
    if (ogg_write_page(&out, &page) <= 0) {
      fprintf(stderr, "Could not write page\n");
      ogg_page_destroy(&page);
      file_close(&in);
      file_close(&out);
      return 1;
    }
    fgetc(stdin);
  }

  ogg_page_destroy(&page);
  file_close(&in);
  file_close(&out);

  return 0;
}
#endif /* OGG_WRITETEST */
