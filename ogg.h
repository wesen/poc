/*C
  (c) 2003 Institut fuer Telematik, Universitaet Karlsruhe
**/

#ifndef OGG_H__
#define OGG_H__

#include "buf.h"
#include "crc32.h"
#include "file.h"

/* ogg header:
   0 'O'
   1 'g'
   2 'g'
   3 'S'
   4  0x00
   5  bit 1 = 0 (fresh), 1 (set)
      bit 2 = 0 (not first page), 1 (first page)
      bit 3 = 0 (not last page), 1 (last page)
   6-13 absolute granule position
   14-17 stream serial number
   18-21 page sequence no
   22-25 page checksum
   26 page_segments
   27... page segment_table
*/

#define OGG_SYNC_HDR_SIZE 4
#define OGG_HDR_MIN_SIZE 27

#define OGG_CRC32_POLY (0x04c11db7)

#define OGG_SYNC_HDR_BYTE1 'O'
#define OGG_SYNC_HDR_BYTE2 'g'
#define OGG_SYNC_HDR_BYTE3 'g'
#define OGG_SYNC_HDR_BYTE4 'S'

#define OGG_MAX_SYNC 40

typedef struct ogg_page_bits_s {
  /*M
    If set, the page contains a continued packet.
  **/
  unsigned int continuation :1;
  /*M
    If set, the page is the first page of the logical bitstream.
  **/
  unsigned int first :1;
  /*M
    If set, the page is the last page of the logical bitstream.
  **/
  unsigned int last :1;
} ogg_page_bits_t;

typedef struct ogg_page_s {
  /*M
    Page flags.
  **/
  ogg_page_bits_t b;
  /*M
    Absolute granule position.

    (This is packed in the same way the rest of Ogg data is packed;
    LSb of LSB first.  Note that the 'position' data specifies a
    'sample' number (eg, in a CD quality sample is four octets, 16
    bits for left and 16 bits for right; in video it would likely be
    the frame number.  It is up to the specific codec in use to define
    the semantic meaning of the granule position value).  The position
    specified is the total samples encoded after including all packets
    finished on this page (packets begun on this page but continuing
    on to the next page do not count).  The rationale here is that the
    position specified in the frame header of the last page tells how
    long the data coded by the bitstream is.  A truncated stream will
    still return the proper number of samples that can be decoded
    fully.

    A special value of '-1' (in two's complement) indicates that no
    packets  finish on this page.
  **/
  unsigned char position[8];
  /*M
    Stream serial number.

    Ogg allows for separate logical bitstreams to be mixed at page
    granularity in a physical bitstream.  The most common case would
    be sequential arrangement, but it is possible to interleave pages
    for two separate bitstreams to be decoded concurrently.  The
    serial number is the means by which pages physical pages are
    associated with a particular logical stream.  Each logical stream
    must have a unique serial number within a physical stream:
  **/
  unsigned long stream;
  /*M
    Page sequence number.

    Page counter: lets us know if a page is lost (useful where
    packets span page boundaries.
  **/
  unsigned long page_no;
  /*M
    Page checksum.

    32 bit CRC value (direct algorith, initial val and final XOR = 0,
    generator polynomial = 0x04c11db7). The value is computed over
    the entire header (with the CRC field in the header set to zero)
    and then continued over the page. The CRC field is then filled
    with the computed value.
  **/
  unsigned long page_cksum;
  /*M
    Page segments.

    The number of segment entries to appear in the segment table. The
    maximum number of 255 segments (255 bytes each) sets the maximum
    possible physical page size at 65307 bytes or just under 64kB
    (thus we know that a header corrupted so as destroy
    sizing/alignment information will not cause a runaway bitstream.
    We'll read in the page according to the corrupted size information
    that's guaranteed to be a reasonable size regardless, notice the
    checksum mismatch, drop sync and then look for recapture).
  **/
  unsigned char page_segments;
  /*M
    Segment table (containing packet lacing values).

    XXX: do we need the whole table? We could do with the lacing
    value of the last segment.
  **/
  unsigned char lacing_values[255];

  unsigned long size;

  buf_t raw;
} ogg_page_t;

void ogg_page_init(ogg_page_t *page);
void ogg_page_destroy(ogg_page_t *page);

int ogg_write_page(file_t *ogg, ogg_page_t *page);
int ogg_next_page(file_t *ogg, ogg_page_t *page);

extern crc32_t ogg_crc32;
void ogg_init(void);

unsigned char *ogg_segment(ogg_page_t *page, int num);

unsigned long ogg_position_to_msecs(ogg_page_t *page,
				    unsigned long sample_rate);

#endif /* OGG_H__ */

/*C
**/
