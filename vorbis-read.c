/*C
  (c) 2003 Institut fuer Telematik, Universitaet Karlsruhe
**/

#include "conf.h"

#include <assert.h>
#include <stdio.h>

#include "ogg.h"
#include "file.h"
#include "vorbis.h"
#include "buf.h"
#include "pack.h"
#include "bv.h"

#ifdef DEBUG
void vorbis_stream_print(vorbis_stream_t *vorbis) {
  fprintf(stderr, "audio channels: %d, sample_rate: %lu\n",
	  vorbis->audio_channels, vorbis->audio_sample_rate);
  fprintf(stderr, "bitrate max: %lu, nominal: %lu, min: %lu\n",
	  vorbis->bitrate_maximum, vorbis->bitrate_nominal,
	  vorbis->bitrate_minimum);
  fprintf(stderr, "blocksize 0: %d, blocksize 1: %d\n",
	  vorbis->blocksize_0, vorbis->blocksize_1);
}
#endif /* DEBUG */

/*M
  \emph{Unpack the Vorbis identification header and check it is
  correct.}
**/
int vorbis_unpack_id_hdr(vorbis_stream_t *vorbis) {
  assert(vorbis != NULL);
  assert(vorbis->id_hdr.data != NULL);
  assert(vorbis->id_hdr.len == VORBIS_ID_HDR_SIZE);
  
  unsigned char *ptr = vorbis->id_hdr.data + VORBIS_HDR_SIZE;
  vorbis->vorbis_version = LE_UINT32_UNPACK(ptr);
  vorbis->audio_channels = UINT8_UNPACK(ptr);
  vorbis->audio_sample_rate = LE_UINT32_UNPACK(ptr);
  vorbis->bitrate_maximum = LE_UINT32_UNPACK(ptr);
  vorbis->bitrate_nominal = LE_UINT32_UNPACK(ptr);
  vorbis->bitrate_minimum = LE_UINT32_UNPACK(ptr);

  bv_t bv;
  bv_init(&bv, ptr, 8);
  vorbis->blocksize_1 = bv_get_bits(&bv, 4);
  vorbis->blocksize_0 = bv_get_bits(&bv, 4);

#ifdef DEBUG
  vorbis_stream_print(vorbis);
#endif /* DEBUG */

  if ((vorbis->vorbis_version != 0) ||
      (vorbis->audio_channels == 0) ||
      (vorbis->audio_sample_rate == 0) ||
      (vorbis->blocksize_0 > vorbis->blocksize_1))
    return 0;

  return 1;
}

/*M
  \emph{Read the packet data in the OGG page beginning at segment
  \verb|vorbis->segment|, and append it to the buffer \verb|packet|.
**/
int vorbis_packet_in_page(vorbis_stream_t *vorbis, ogg_page_t *page,
			  buf_t *packet) {
  for (; vorbis->segment < page->page_segments; vorbis->segment++) {
    if (page->lacing_values[vorbis->segment] > 0) {
      buf_append(packet, ogg_segment(page, vorbis->segment),
		 page->lacing_values[vorbis->segment]);
    }
    if (page->lacing_values[vorbis->segment] < 255) {
      vorbis->segment++;
      return 1;
    }
  }

  /*M
    Not enough segments for complete packet.
  **/

  return 0;
}


/*M
  \emph{Read the next Vorbis packet from an OGG stream.}
**/
int vorbis_next_packet(vorbis_stream_t *vorbis, buf_t *packet) {
  assert(vorbis != NULL);
  assert(packet != NULL);
  assert(packet->data != NULL);

  packet->len = 0;

  /*M
    Check if we have to read in a new page.
  **/
  if (vorbis->segment >= vorbis->page.page_segments) {
    if (!ogg_next_page(&vorbis->file, &vorbis->page))
      return 0;
    vorbis->segment = 0;
  }

 again:
  if (vorbis_packet_in_page(vorbis, &vorbis->page, packet))
    return 1;

  /*M
    Not enough segments for complete packet in page, read a new page
    and hope it is a continuation page.
  **/
  if (!ogg_next_page(&vorbis->file, &vorbis->page))
    return 0;

  vorbis->segment = 0;

  /*M
    Check if the next page is a continuation page.
  **/
  if (vorbis->page.b.continuation == 0) {
    fprintf(stderr, "Subsequent page was not continuation page.\n");
    return 0;
  }

  goto again;
}

/*M
  \emph{Read the Vorbis headers from an OGG stream.}
**/
int vorbis_stream_read_hdrs(vorbis_stream_t *vorbis) {
  assert(vorbis != NULL);
  /*M
    Read in the first OGG page which should contain only one segment
    containing the Vorbis identification header.
  **/
  if (!ogg_next_page(&vorbis->file, &vorbis->hdr_pages[0]))
    return 0;
  if (vorbis->hdr_pages[0].page_segments > 1)
    return 0;
  if (!vorbis_packet_in_page(vorbis, vorbis->hdr_pages, &vorbis->id_hdr) ||
      !vorbis_check_packet(&vorbis->id_hdr, 1) ||
      !vorbis_unpack_id_hdr(vorbis))
    return 0;

  int i = 1;
  /* read next page containing start of comment header */
  if (!ogg_next_page(&vorbis->file, vorbis->hdr_pages + i))
    return 0;
  vorbis->segment = 0;
  while (i < VORBIS_MAX_HDR_PAGES) {
    if (!vorbis_packet_in_page(vorbis, vorbis->hdr_pages + i,
			       &vorbis->comment_hdr)) {
      i++;
      if (!ogg_next_page(&vorbis->file, vorbis->hdr_pages + i))
	return 0;
      vorbis->segment = 0;
    } else {
      if (!vorbis_check_packet(&vorbis->comment_hdr, 3))
	return 0;
      break;
    }
  }

  if (i == VORBIS_MAX_HDR_PAGES)
    return 0;

  while (i < VORBIS_MAX_HDR_PAGES) {
    if (!vorbis_packet_in_page(vorbis, vorbis->hdr_pages + i,
			       &vorbis->setup_hdr)) {
      i++;
      if (!ogg_next_page(&vorbis->file, vorbis->hdr_pages + i))
	return 0;
      vorbis->segment = 0;      
    } else {
      if (!vorbis_check_packet(&vorbis->setup_hdr, 5))
	return 0;
      /* must be the last segment in packet */
      if (vorbis->segment < (vorbis->hdr_pages[i].page_segments))
	return 0;
      break;
    }
  }

  vorbis->hdr_pages_cnt = i + 1;
  
  return 1;
}

/*C
**/

#ifdef VORBIS_TEST

#include <stdlib.h>

int main(int argc, char *argv[]) {
  int retval = EXIT_SUCCESS;
  
  char *f;
  if (!(f = *++argv)) {
    fprintf(stderr, "Usage: vorbistest oggfile\n");
    return 1;
  }
  
  ogg_init();
  
  vorbis_stream_t vorbis;
  vorbis_stream_init(&vorbis);

  buf_t packet;
  packet.len = 0;
  packet.size = 0;
  packet.data = NULL;
  buf_alloc(&packet, 512);

  if (!file_open_read(&vorbis.file, f)) {
    fprintf(stderr, "Could not open ogg file: %s\n", f);
    retval = EXIT_FAILURE;
    goto exit;
  }

  if (!vorbis_stream_read_hdrs(&vorbis)) {
    fprintf(stderr, "Stream is not a OGG encapsulated Vorbis stream\n");
    retval = EXIT_FAILURE;
    goto exit;
  }

  while (vorbis_next_packet(&vorbis, &packet) > 0) {
    fprintf(stderr, "Packet length: %lu, size %lu\n", packet.len, packet.size);
    fgetc(stdin);
  }

  file_close(&vorbis.file);
  
 exit:
  vorbis_stream_destroy(&vorbis);
  buf_free(&packet);

  return retval;
}

#endif /* VORBIS_TEST */
