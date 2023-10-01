/*C
  (c) 2005 bl0rg.net
**/

#ifndef VORBIS_H__
#define VORBIS_H__

#include "lib/data-structures/buf.h"
#include "lib/system/file.h"
#include "lib/ogg/ogg.h"

#define VORBIS_HDR_SIZE 7
#define VORBIS_SYNC_HDR_BYTE1 ('v')
#define VORBIS_SYNC_HDR_BYTE2 ('o')
#define VORBIS_SYNC_HDR_BYTE3 ('r')
#define VORBIS_SYNC_HDR_BYTE4 ('b')
#define VORBIS_SYNC_HDR_BYTE5 ('i')
#define VORBIS_SYNC_HDR_BYTE6 ('s')

#define VORBIS_ID_HDR_SIZE 30

#define VORBIS_MAX_HDR_PAGES 10

/* actually we don't care about vorbis data */
#if 0
typedef struct vorbis_comment_hdr_s {
  buf_t raw;
} vorbis_comment_hdr_t;

/*
  0: packet type (id header: 1, comment header: 3, setup header: 5,
                  even number: audio packet)
  1, 2, 3, 4, 5, 6: 'vorbis'
  
*/


typedef struct vorbis_setup_hdr_s {
  buf_t raw;
} vorbis_setup_hdr_t;
#endif

typedef struct vorbis_stream_s {
  /* OGG pages containing the Vorbis headers, so we don't have to
     create them afterwards for HTTP streaming */
  int hdr_pages_cnt;
  ogg_page_t hdr_pages[VORBIS_MAX_HDR_PAGES];
  
  buf_t id_hdr;
  buf_t comment_hdr;
  buf_t setup_hdr;

  /*M
    Identification header data.
  **/
  unsigned long vorbis_version;
  unsigned char audio_channels;
  unsigned long audio_sample_rate;
  unsigned long bitrate_maximum;
  unsigned long bitrate_nominal;
  unsigned long bitrate_minimum;
  unsigned char blocksize_0;
  unsigned char blocksize_1;

  file_t     file;
  ogg_page_t page;
  unsigned char segment;
} vorbis_stream_t;

void vorbis_stream_init(vorbis_stream_t *vorbis);
void vorbis_stream_destroy(vorbis_stream_t *vorbis);
int vorbis_check_packet(buf_t *buf, unsigned char type);
int vorbis_stream_read_hdrs(vorbis_stream_t *vorbis);

#endif /* VORBIS_H__ */

/*C
**/
