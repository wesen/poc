/*C
  (c) 2005 bl0rg.net
**/

#include "conf.h"

#include <assert.h>
#include <stdlib.h>

#include "ogg.h"
#include "buf.h"
#include "vorbis.h"

/*M
  \emph{Initialize a Vorbis stream.}
**/
void vorbis_stream_init(vorbis_stream_t *vorbis) {
  assert(vorbis != NULL);
  
  ogg_page_init(&vorbis->page);
  assert(vorbis->page.raw.data != NULL);
  
  vorbis->segment = 0;

  vorbis->hdr_pages_cnt = 0;
  int i;
  for (i = 0; i < VORBIS_MAX_HDR_PAGES; i++)
    ogg_page_init(vorbis->hdr_pages + i);
  
  buf_alloc(&vorbis->id_hdr, 1000);
  assert(vorbis->id_hdr.data != NULL);
  
  buf_alloc(&vorbis->comment_hdr, 1000);
  assert(vorbis->comment_hdr.data != NULL);
  
  buf_alloc(&vorbis->setup_hdr, 2000);
  assert(vorbis->setup_hdr.data != NULL);
}

/*M
  \emph{Free the structures allocated inside the Vorbis stream.}
**/
void vorbis_stream_destroy(vorbis_stream_t *vorbis) {
  ogg_page_destroy(&vorbis->page);
  vorbis->segment = 0;

  int i;
  for (i = 0; i < VORBIS_MAX_HDR_PAGES; i++)
    ogg_page_destroy(vorbis->hdr_pages + i);

  buf_free(&vorbis->id_hdr);
  buf_free(&vorbis->comment_hdr);
  buf_free(&vorbis->setup_hdr);  
}

/*M
  \emph{Check if the buffer contains a Vorbis header of type
  \verb|type|.}
**/
int vorbis_check_packet(buf_t *buf, unsigned char type) {
  assert(buf != NULL);

  if (buf->len < VORBIS_HDR_SIZE)
    return 0;
  if (buf->data[0] != type)
    return 0;
  if ((buf->data[1] != VORBIS_SYNC_HDR_BYTE1) ||
      (buf->data[2] != VORBIS_SYNC_HDR_BYTE2) ||
      (buf->data[3] != VORBIS_SYNC_HDR_BYTE3) ||
      (buf->data[4] != VORBIS_SYNC_HDR_BYTE4) ||
      (buf->data[5] != VORBIS_SYNC_HDR_BYTE5) ||
      (buf->data[6] != VORBIS_SYNC_HDR_BYTE6))
    return 0;

  return 1;
}

/*C
**/
