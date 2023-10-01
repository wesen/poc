/*C
  (c) 2005 bl0rg.net
**/

#include "conf.h"

#include <string.h>
#include <assert.h>
#include <stdio.h>

#include "lib/system/file.h"
#include "pack.h"
#include "ogg.h"
#include "lib/math/crc32.h"

#ifdef DEBUG
void ogg_print_page_hdr(ogg_page_t *page) {
  assert(page != NULL);
  
  fprintf(stderr, "page (size: %lu)\n", page->size);
  fprintf(stderr, "flags: c:%d, f:%d, l:%d\n",
          page->b.continuation,
          page->b.first,
          page->b.last);
  fprintf(stderr, "stream: %lx, page_no: %lx\n",
          page->stream, page->page_no);
  fprintf(stderr, "position: %x%x %x%x %x%x %x%x\n",
          page->position[0], page->position[1],
          page->position[2], page->position[3],
          page->position[4], page->position[5],
          page->position[6], page->position[7]);          
  fprintf(stderr, "cksum: %lx, page_segments: %d\n",
          page->page_cksum, page->page_segments);
}

void ogg_print_page_segment_table(ogg_page_t *page) {
  assert(page != NULL);

  int i;
  for (i = 0; i < page->page_segments; i++) {
    fprintf(stderr, "segment %d, lacing value: %d\n",
            i, page->lacing_values[i]);
  }
}

#endif /* DEBUG */

int ogg_unpack_page_hdr(ogg_page_t *page) {
  assert(page != NULL);
  assert(page->size >= OGG_HDR_MIN_SIZE);

  unsigned char *ptr = page->raw.data + 4;
  if (*ptr++ != 0)
    return 0;

  page->b.continuation = (*ptr & 1) ? 1 : 0;
  page->b.first = (*ptr & 2) ? 1 : 0;
  page->b.last = (*ptr & 4) ? 1 : 0;
  ptr++;
  memcpy(page->position, ptr, 8);
  ptr += 8;
  page->stream = LE_UINT32_UNPACK(ptr);
  page->page_no = LE_UINT32_UNPACK(ptr);
  page->page_cksum = LE_UINT32_UNPACK(ptr);
  page->page_segments = UINT8_UNPACK(ptr);
  
  return 1;
}

void ogg_unpack_page_segment_table(ogg_page_t *page) {
  assert(page != NULL);
  assert(page->size >= (OGG_HDR_MIN_SIZE + page->page_segments));

  unsigned char *ptr = page->raw.data + OGG_HDR_MIN_SIZE;
  int i;
  for (i = 0; i < page->page_segments; i++)
    page->lacing_values[i] = UINT8_UNPACK(ptr);
}

int ogg_check_page_cksum(ogg_page_t *page) {
  assert(page != NULL);
  assert(page->size >= OGG_HDR_MIN_SIZE);
  
  /*M
    Fill the checksum in the raw frame with 0 bytes.
  **/
  unsigned char *ptr = page->raw.data + 22;
  memset(ptr, 0, 4);

  /*M
    Check the CRC32 sum of the whole page.
  **/
  unsigned long crc = crc32(&ogg_crc32, page->raw.data, page->size);

  LE_UINT32_PACK(ptr, page->page_cksum);
  if (crc == page->page_cksum)
    return 1;
  else
    return 0;
}

/*M
  \emph{Read next OGG page from the OGG file.}
**/
int ogg_next_page(file_t *ogg, ogg_page_t *page) {
  assert(ogg != NULL);
  assert(page != NULL);

  page->size = 0;
  
  if (page->raw.size < OGG_HDR_MIN_SIZE) {
    if (!buf_grow(&page->raw))
      return 0;
  }

  page->raw.len = 0;
  
  /*M
    Try to sync on OGG stream.
  **/
  unsigned int resync = 0;
 again:
  if (resync != 0) {
    /*M
      Read the next byte and try to sync on the new header.
    **/
    if (file_read(ogg, page->raw.data + 3, 1) <= 0)
      return EEOF;
  } else {
    if (file_read(ogg, page->raw.data, OGG_SYNC_HDR_SIZE) <= 0)
      return EEOF;
  }

  /*M
    Check if the header magic bytes are correct.
  **/
  if ((page->raw.data[0] != OGG_SYNC_HDR_BYTE1) ||
      (page->raw.data[1] != OGG_SYNC_HDR_BYTE2) ||
      (page->raw.data[2] != OGG_SYNC_HDR_BYTE3) ||
      (page->raw.data[3] != OGG_SYNC_HDR_BYTE4)) {
    if (resync++ > OGG_MAX_SYNC) {
      fprintf(stderr, "Max sync exceeded: %d\n", resync);
      return ESYNC;
    } else
      goto again;
  } else {
    resync = 0;
  }
  page->size = OGG_SYNC_HDR_SIZE;
  page->raw.len = OGG_SYNC_HDR_SIZE;

  /*M
    Read rest of header.
  **/
  if (file_read(ogg, page->raw.data + OGG_SYNC_HDR_SIZE,
                OGG_HDR_MIN_SIZE - OGG_SYNC_HDR_SIZE) <= 0)
    return EEOF;

  page->size = OGG_HDR_MIN_SIZE;
  page->raw.len = OGG_HDR_MIN_SIZE;  
  if (!ogg_unpack_page_hdr(page)) {
    fprintf(stderr, "Could not unpack the OGG page header\n");
    return 0;
  }

#ifdef DEBUG
  ogg_print_page_hdr(page);
#endif /* DEBUG */

  /*M
    Read segment table.
  **/
  if (page->raw.size < (OGG_HDR_MIN_SIZE + page->page_segments)) {
    if (!buf_grow(&page->raw))
      return 0;
  }

  if (page->page_segments > 0) {
    if (file_read(ogg, page->raw.data + OGG_HDR_MIN_SIZE,
                  page->page_segments) <= 0)
      return EEOF;
    
    page->size += page->page_segments;
    page->raw.len += page->page_segments;
    ogg_unpack_page_segment_table(page);
    
#ifdef DEBUG
    ogg_print_page_segment_table(page);
#endif /* DEBUG */
    
    /*M
      Read segments.
    **/
    int i;
    for (i = 0; i < page->page_segments; i++) {
      if (page->lacing_values[i] > 0) {
        if ((page->raw.size - page->size) <= page->lacing_values[i]) {
          if (!buf_grow(&page->raw))
            return 0;
        }
        if (file_read(ogg, page->raw.data + page->size,
                      page->lacing_values[i]) <= 0)
          return EEOF;
        page->size += page->lacing_values[i];
        page->raw.len += page->lacing_values[i];        
      }
    }
  }

  ogg_check_page_cksum(page);

  return 1;
}

#ifdef OGG_TEST
int main(int argc, char *argv[]) {
  file_t file;
  ogg_page_t page;

  ogg_init();
  ogg_page_init(&page);
  
  char *f;
  if (!(f = *++argv)) {
    fprintf(stderr, "Usage: oggtest oggfile\n");
    return 1;
  }

  if (!file_open_read(&file, f)) {
    fprintf(stderr, "Could not open ogg file: %s\n", f);
    return 1;
  }

  unsigned long last_msecs = 0;
  while (ogg_next_page(&file, &page) > 0) {

    unsigned long msecs = ogg_position_to_msecs(&page, 44100);
    
    fprintf(stderr, "msecs: %lu\n", msecs - last_msecs);
    last_msecs = msecs;
    fgetc(stdin);
  }

  ogg_page_destroy(&page);
  file_close(&file);

  return 0;
}
#endif /* OGG_TEST */

/*C
**/
