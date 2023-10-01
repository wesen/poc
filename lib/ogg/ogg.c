/*C
  (c) 2005 bl0rg.net
**/

#include "conf.h"

#include <assert.h>
#include <stdlib.h>

#include "lib/data-structures/buf.h"
#include "ogg.h"
#include "lib/math/crc32.h"
#include "pack.h"

crc32_t ogg_crc32;

void ogg_init(void) {
  crc32_init(&ogg_crc32, OGG_CRC32_POLY, 0, 0);
}

void ogg_page_init(ogg_page_t *page) {
  assert(page != NULL);
  
  page->raw.size = 0;
  page->raw.data = NULL;
  page->page_segments = 0;
  page->page_no = 0;
  page->stream = 0;
  page->page_cksum = 0;
  
  buf_alloc(&page->raw, 4000);
  assert(page->raw.data != NULL);
  
  page->size = 0;
}

void ogg_page_destroy(ogg_page_t *page) {
  assert(page != NULL);
  buf_free(&page->raw);
}

/* XXX make pointer array? */
unsigned char *ogg_segment(ogg_page_t *page, int num) {
  assert(page != NULL);

  if (num > page->page_segments)
    return NULL;

  unsigned char *res = page->raw.data +
    OGG_HDR_MIN_SIZE + page->page_segments;
  int i;
  for (i = 0; i < num; i++)
    res += page->lacing_values[i];

  return res;
}

/*M
  \emph{Converts a sample position in an OGG page to a number in
  msecs.}
**/
unsigned long ogg_position_to_msecs(ogg_page_t *page,
                                    unsigned long sample_rate) {
  assert(page != NULL);
  
  unsigned char *ptr = page->position;
  unsigned long position[2];
  position[0] = LE_UINT32_UNPACK(ptr);
  position[1] = LE_UINT32_UNPACK(ptr);

  double dmsecs = (position[0] + position[1] * 2147483648.0) /
    (sample_rate / 1000.0);

  return (unsigned long)dmsecs;
}

/*C
**/
