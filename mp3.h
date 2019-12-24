/*C
  (c) 2005 bl0rg.net
**/

#ifndef MP3_H__
#define MP3_H__

/*M
  \emph{Size allocated for the ''raw'' content of an MP3 frame.}

  This is an arbitrary size.
**/
#define MP3_RAW_SIZE 4000
/*M
  \emph{Maximal number of synchronisation errors allowed while
  reading an MP3 stream.}
**/
#define MP3_MAX_SYNC 4000
/*M
  \emph{MPEG Audio Frame Header size.}
**/
#define MP3_HDR_SIZE 4

#define MPEG_VERSION_25 0x0
#define MPEG_VERSION_RESERVED 0x1
#define MPEG_VERSION_2 0x2
#define MPEG_VERSION_1 0x3

/*M
  \emph{Scalefactor information contained in a single granule.}
**/
typedef struct {
  unsigned int l[23];     /* long window */
  unsigned int s[3][13];  /* short window */
} mp3_sf_t;

typedef struct {
  int   s;
  float x;
} mp3_sample_t;

/*M
  \emph{MP3 granule.}
**/
typedef struct mp3_granule_s {
  int part2_3_length;
  int part2_length;
  int part3_length;
  
  unsigned int big_values;
  unsigned int global_gain;

  unsigned int scale_comp;
  unsigned int slen0;
  unsigned int slen1;
  
  unsigned int blocksplit_flag;
  unsigned int block_type;
  unsigned int switch_point;
  unsigned int tbl_sel[3];
  unsigned int reg0_cnt;
  unsigned int reg1_cnt;
  unsigned int sub_gain[3];
  unsigned int maxband[3];
  unsigned int maxbandl;
  unsigned int maxb;
  unsigned int region1start;
  unsigned int region2start;
  unsigned int preflag;
  unsigned int scale_scale;
  unsigned int cnt1tbl_sel;
  float        *full_gain[3];
  float        *pow2gain;

  mp3_sf_t     sf; /* scale factor */
  mp3_sample_t samples[576];
} mp3_granule_t;

/*M
  \emph{MP3 stereo channel.}
**/
typedef struct mp3_channel_s {
  unsigned char scfsi[4];
  mp3_granule_t granule[2];
} mp3_channel_t;

/*M
  \emph{MP3 side information.}

  In mono MP3s only the first channel structure is used.
**/
typedef struct mp3_si_s {
  unsigned int  main_data_end;
  unsigned int  private_bits;
  
  mp3_channel_t channel[2];
} mp3_si_t;

/*M
  \emph{Single MP3 frame.}
**/
typedef struct mp3_frame_s {
  unsigned char id;
  unsigned char layer;
  unsigned char protected;
  unsigned char bitrate_index;
  unsigned char samplerfindex;
  unsigned char padding_bit;
  unsigned char private_bit; 
  unsigned char mode;
  unsigned char mode_ext;
  unsigned char copyright;    
  unsigned char original;    
  unsigned char emphasis;

  unsigned char crc[2];
  
  mp3_si_t      si;
  unsigned long si_size;
  unsigned long si_bitsize;
  unsigned long adu_bitsize;
  unsigned long adu_size;
  
  /* calculated information */
  unsigned short syncskip;
  unsigned long  bitrate;
  unsigned long  samplerate;
  unsigned long  samplelen;
  unsigned long  frame_size;
  unsigned long  frame_data_size;
  unsigned long  usec;
  
  /* XXX add length counter for overflow checking */
  unsigned char  raw[MP3_RAW_SIZE];
} mp3_frame_t;

/*M
  \emph{Prototypes and macros.}
**/
#define mp3_frame_data_begin(f) \
  ((f)->raw + 4 + ((f)->protected ? 0 : 2) + (f)->si_size)

#include "file.h"

int mp3_read_si(mp3_frame_t *frame);
int mp3_read_hdr(mp3_frame_t *frame);
int mp3_read_sf(mp3_frame_t *frame);
int mp3_next_frame(file_t *mp3, mp3_frame_t *frame);
int mp3_unpack(mp3_frame_t *frame);

int mp3_fill_si(mp3_frame_t *frame);
int mp3_fill_hdr(mp3_frame_t *frame);
int mp3_write_frame(file_t *file, mp3_frame_t *frame);

int mp3_trans_frame(mp3_frame_t *frame);

void mp3_calc_hdr(mp3_frame_t *frame);
unsigned long mp3_frame_size(mp3_frame_t *frame);

/*M
**/
#endif /* MP3_H__ */
