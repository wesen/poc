/*C
  (c) 2003 Institut fuer Telematik, Universitaet Karlsruhe
**/

#include "conf.h"

#include <stdio.h>
#include <errno.h>
#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "mp3.h"
#include "bv.h"

/*@-boolops@*/

/*M
  \emph{Read the MP3 frame side information.}
**/
int mp3_read_si(mp3_frame_t *frame) {
  assert(frame != NULL);
  assert((frame->si_bitsize != 0) ||
         "Trying to read an empty sideinfo");

  unsigned char *ptr = frame->raw + 4; /* skip header */
  if (/*@-type@*/ frame->protected == 0)
    ptr += 2;

  bv_t bv;
  bv_init(&bv, ptr, frame->si_bitsize);

  /* reset granule content length */
  frame->si.channel[0].granule[0].part2_3_length = 0;
  frame->si.channel[0].granule[1].part2_3_length = 0;
  frame->si.channel[1].granule[0].part2_3_length = 0;
  frame->si.channel[1].granule[1].part2_3_length = 0;

  mp3_si_t *si = &frame->si;
  /* stereo or mono */
  unsigned int nch = (frame->mode != 3) ? 2 : 1;

  frame->adu_bitsize = 0;

  /*M
    \emph{End of main data.}
    
   (ISO) The value of main data end is used to determine the
   location in the bitstream of the last bit of main data for the
   frame. The main data end value specifies the location as a negative
   offset in bytes from the next frame's frame header location in the
   main data portion of the bitstream
  **/
  si->main_data_end = bv_get_bits(&bv, 9);

  /*M
    \emph{Private bits,}
    
   (ISO) Bits for private use. These bits will not be used in the
   future by ISO
  **/
  si->private_bits  = nch == 2 ? bv_get_bits(&bv, 3)
                               : bv_get_bits(&bv, 5);

  /*M
    \emph{Scalefactor selection information.}
    
   (ISO) In Layer III the scalefactor selection information works
   similarly to Layers I and II. The main difference is the use of the
   variable scfsi band to apply scfsi to groups of scalefactors
   instead of single scalefactors. scfsi controls the use of
   scalefactors to the granules.
   \begin{itemize}
   \item '0' - scalefactors are transmitted for each granule
   \item '1' - scalefactors transmitted for granule 0 are also valid for
         granule 1
   \end{itemize}
   If short windows are switched on, i.e. block type == 2 for one
   of the granules, then scfsi is always 0 for this frame.
  **/
  
  unsigned int i;
  for (i = 0; i < nch; i++) {
    unsigned int band;
    for (band = 0; band < 4; band++)
      si->channel[i].scfsi[band] = bv_get_bits(&bv, 1);
  }
  
  unsigned int gri;
  for (gri = 0; gri < 2; gri++) {
    for (i = 0; i < nch; i++) {
      mp3_granule_t *gr = &si->channel[i].granule[gri];

      /*M
        \emph{Length of main data.}
        
       (ISO) This value contains the number of main data bits used
       for scalefactors and Huffman code data. Because the length
       of the side inforamtion is always the same, this value can
       be used to calculate the beginning of the main inforamtion
       for each granule and the position of ancillary information
       (is used).
      **/
      gr->part2_3_length = bv_get_bits(&bv, 12);
      /* sum the granule main data lengths into the adu size */
      frame->adu_bitsize += gr->part2_3_length;

      /*M
        \emph{Length of ``big'' Huffman data.}
        
       (ISO) The spectral values of each granule are coded with
       different Huffman code tables. The full frequency ranges
       from zero to the Nyquist frequency is divided into several
       regions, which then are coded using different
       tables. Partitioning is done according to the maximum
       quantized values. This is done with the assumption that
       values at higher frequencies are expected to have lower
       amplitudes or don't need to be coded at all. Starting at
       high frequencies, the pairs of quantized values equal to
       zero are counted. This number is named "rzero". Then,
       quadruples of quantized values with absolute value not
       exceeding 1 (i.e. only 3 possible quantization levels) are
       counted. This number is named "count1". Again an even number
       of values remains. Finally, the number of pairs of values in
       the region of the spectrum which extends down to zero is
       named "big values". The maximum absolute value in this range
       is constrained to 8191.
       The figure shows the partitioning:
       \begin{verbatim}
xxxxxxxxxxxxx------------------0000000000000000000000000000
|           |                 |                           |
1          bigvalues*2        bigvalues*2+count1*4
\end{verbatim}

       The values 000 are all zero.
       The values --- are -1, 0 or +1. Their number is a multiple
       of 4.
       The values xxx are not bound.
       Iblen is 576.
      **/
      gr->big_values   = bv_get_bits(&bv, 9);
      assert((gr->big_values <= 288) || "big_values are too large");

      /*M
        \emph{Global gain.}
        
       (ISO) The quantizer step size information is transmitted in
       the side information variable global gain. It is
       logarithmically quantized. For the application of
       global gain, refer to the formula in 2.4.3.4 "Formula for
       requantization and all scaling".
      **/
      gr->global_gain  = bv_get_bits(&bv, 8);

      /*M
        \emph{Scalefactor compression.}
        
       (ISO) Selects the number of bits used for the transmission
       of the scalefactors according to the following table:
       if block type is 0, 1, or 3:
       \begin{itemize}
       \item slen1 : length of scalefactors for the scalefactor bands 0
       to 10
       \item slen2 : length of scalefactors for the scalefactor bands 11
       to 20
       \end{itemize}

       if block type is 2 and switch point is 0:
       \begin{itemize}
       \item slen1 : length of scalefactors for the scalefactor bands 0
       to 5
       \item slen2 : length of scalefactors for the scalefactor bands 6
       to 11
       \end{itemize}

       if block type is 2 and switch point is 1:
       \begin{itemize}
       \item slen1 : length of scalefactors for the scalefactor bands 0
       to 7 (long window scalefactor band) and 4 to 5 (short
       scalefactor band). Note: scalefactor bands 0-7 are from the
       "long window scalefactor band" table, and scalefactor bands
       4-11 from the "short window scalefactor bands" table. This
       combination of partitions is contiguous and spans the entire
       frequency spectrum.
       \item slen2 : length of scalefactors for the scalefactor bands 6
       to 11
       \end{itemize}

       \begin{tabular}{|l|l|l|}
       \hline
       scale comp &  slen1 & slen2 \\
       \hline
       0     &       0  &   0 \\
       1     &       0  &   1 \\
       2     &       0  &   2 \\
       3     &       0  &   3 \\
       4     &       3  &   0 \\
       5     &       1  &   1 \\
       6     &       1  &   2 \\
       7     &       1  &   3 \\
       8     &       2  &   1 \\
       9     &       2  &   2 \\
       10    &       2  &   3 \\
       11    &       3  &   1 \\
       12    &       3  &   2 \\
       13    &       3  &   3 \\
       14    &       4  &   2 \\
       15    &       4  &   3 \\
       \hline
       \end{tabular}
       **/
      gr->scale_comp   = bv_get_bits(&bv, 4);

      /*M
        \emph{Block windowing split flag.}
        
       (ISO) Signals that the block uses an other than normal (type
       0) window. If blocksplit flag is set, several other
       variables are set by default:
       \begin{itemize}
       \item region address1 = 8 (in case of block type == 1 or
       block type == 3)
       \item region address1 = 9 (in case of block type == 2)
       \item region address2 = 0 In this case the length of region 2 is
       zero
       \end{itemize}

       If blocksplit flag is not set, then the value of block type
       is zero.
      **/
      gr->blocksplit_flag = bv_get_bits(&bv, 1);

      if (gr->blocksplit_flag != 0) {
        /*M
          \emph{Windowing type.}
          
          (ISO) Indicates the window type for the actual granule
          (see description of the filterbank, Layer III).
          \begin{itemize}
          \item type 0 - reserved
          \item type 1 - start block
          \item type 2 - 3 short windows
          \item type 3 - end block
          \end{itemize}
          
         Block type and switch point give the information about
         assembling of values in the block and about length and
         count of the transforms. In the case of block type == 2,
         the switch point indicates whether some polyphase filter
         subbands are coded using long transforms even in case of
         block type 2. The polyphase filterbank is described in the
         clause 2.4.3.2 of Layer I.

         In the case of long block (block type not equal to 2 or in
         the lower subbands of block type 2) the IMDCT generates an
         output of 36 values every 18 input values. The output is
         windowed depending on the block type and the first half is
         overlapped with the second half of the block before. The
         resulting vector is the input of the synthesis part of the
         polyphase filterbank of one band.

         In the case of short blocks (in the upper subbands of a
         type 2 block) three transforms are performed producing 12
         output values each. The three vectors are windowed and
         overlapped each. Concatenating 6 zeros on both ends of the
         resulting vector gives a vector of length 36, which is
         processed like the output of a long transform.
        **/
        gr->block_type = bv_get_bits(&bv, 2);

        /* if block type is reserved we have a wrong frame */
        if (gr->block_type == 0) {
          fprintf(stderr, "Frame has reserved windowing type, skipping...\n");
          return 0;
        }

        /*M
          \emph{Switch point.}
          
         (ISO) Signals the split point of short/long
         transforms. The following table shows the number of the
         scalefactor band above which window switching
         (i.e. block type different from 0 is used.

         \begin{tabular}{|l|l|l|}
         \hline
         switch point &  switch point 1 &  switch point s \\
                      & (No of sb)     &  (No of sb) \\
         \hline
          0         &    0      &         0 \\
         \hline
         \multicolumn{3}{|c|}{; switching of the whole spectrum} \\
         \hline
         1        &     8        &       3 \\
         \hline
         \multicolumn{3}{|c|}{; switching of higher frequencies only} \\
         \hline
         \end{tabular}

         \begin{description}
         \item[switching point 1]: Number of scalefactor band (long block
         scalefactor band) from which point on window switching is
         used
         \item[switching point s]: Number of scalefactor band (short
         block scalefactor band) from which point on window
         switching is used.
         \end{description}
        **/
        gr->switch_point = bv_get_bits(&bv, 1);

        /*M
          \emph{Huffman code table selection.}
          
         (ISO) Different Huffman code tables are used depending on the
         maximum quantized value and the local statistics of the
         signal. There are a total of 32 possible tables given in
         3-Annex B Table 3-B.7.
        **/
        gr->tbl_sel[0]   = bv_get_bits(&bv, 5);
        gr->tbl_sel[1]   = bv_get_bits(&bv, 5);
        gr->tbl_sel[2]   = 0;

        /*M
          \emph{Subblock gain offset.}
          
         (ISO) Indicates the gain offset (quantization: factor 4) from
         the global gain for one subblock. Used only with block type 2
         (short windows). The values of the subblock have to be
         divided by $4.^{\textrm{subblock gain(window)}}$ in the
         decoder.
        **/
        unsigned int j;
        for (j = 0; j < 3; j++)
          gr->sub_gain[j] = bv_get_bits(&bv, 3);

        /* implicitly set */
        if (gr->block_type == 2)
          gr->reg0_cnt = 9;
        else
          gr->reg0_cnt = 8;
        gr->reg1_cnt = 0;
          
      } else {
        unsigned int j;
        for (j = 0; j < 3; j++)
          gr->tbl_sel[j] = bv_get_bits(&bv, 5);

        /*M
          \emph{First region subdivision information.}
          
         (ISO) A further partitioning of the spectrum is used to
         enhance the performance of the Huffman coder. It is a
         subdivision of the region which is described by
         big values. The purpose of this subdivision is to get
         better error robustness and better coding
         efficiency. Three regions are used. Each region is coded
         using a different Huffman code table depending on the
         maximum quantized value and the loval signal
         statistics. The values region address[1,2] are used to
         point to the boundaries of the regions. The region
         boundaries are aligned with the partitioning of the
         spectrum into critical bands.

         In case of block type == 2 (short blocks) the scalefactor
         bands representing the different time slots are counted
         separately. If switch point == 0, the total amount of
         scalefactor bands for the granule in this case is $12 * 3
         = 36$. If block type == 2 and switch point == 1, the amount
         of scalefactor bands is $8 + 9 * 3 = 35$. region address1
         counts the number of scalefactor bands until the upper
         edge of the first region:

         \begin{tabular}{|l|l|}
         \hline
         region address1   &     upper edge of region is upper edge 
                                 of scalefactor band number \\
         \hline                                  
         0     &                  0 (no first region)  \\
         1     &                  1 \\
         2     &                  2 \\
         ...   &                  ... \\
         15    &                  15 \\
         \hline
         \end{tabular}
        **/
        gr->reg0_cnt = bv_get_bits(&bv, 4);

        /*M
          \emph{Second region subdivision information.}
          
         (ISO) Region address2 counts the number of scalefactor
         bands which are partially or totally in region 3. Again if
         block type == 2 the scalefactor bands representing
         different time slots are counted separately.
        **/
        gr->reg1_cnt = bv_get_bits(&bv, 3);

        /* implicitly set */
        gr->block_type   = 0;
        gr->switch_point = 0;
      }

      /*M
        \emph{Additional high frequency amplification flag.}
        
       (ISO) This is a shortcut for additional high frequency
       amplification of the quantized values. If preflag is set,
       the values of a table are added to the scalefactors (see
       3-Annex B, Table 3-B.6). This is equivalent to
       multiplication of the requantized scalefactors with tables
       values. preflag is never used if block type == 2 (short blocks).
      **/
      gr->preflag     = bv_get_bits(&bv, 1);

      /*M
        \emph{Scalefactor scale step size.}
        
       (ISO) The scalefactors are logarithmically quantized with a
       step size of 2 (or sqrt(2)) depending on scalefac scale
       
       scalefac scale = 0      stepsize sqrt(2)
       scalefac scale = 1      stepsize 2
      **/
      gr->scale_scale = bv_get_bits(&bv, 1);

      /*M
        (ISO) This flag selects one of two possible Huffman code
        tables for the region of quadruples of quantized values with
        magnitude not exceeding 1.

        \begin{itemize}
        \item count1table select = 0       Table A of 3-Annex B.7
        \item count1table select = 1       Table B of 3-Annex B.7
        \end{itemize}
      **/
      gr->cnt1tbl_sel = bv_get_bits(&bv, 1);

      /*M
        \emph{Scalefactor length compression table}

        Table to get scalefactor length information from scale comp.
      */
      static const int slen_table[2][16] = {
        { 0, 0, 0, 0,
          3, 1, 1, 1,
          2, 2, 2, 3,
          3, 3, 4, 4 },
        { 0, 1, 2, 3,
          0, 1, 2, 3,
          1, 2, 3, 1,
          2, 3, 2, 3 }
      };

      /*M
        \emph{Scalefactor length}

        Calculate the bitlength of the scalefactors.
      **/
      gr->slen0 = slen_table[0][gr->scale_comp];
      gr->slen1 = slen_table[1][gr->scale_comp];

      /*M
        \emph{Scalefactor total size}

        Calculate the total size of the scalefactor information for
        the granule.
      **/
      if (gr->block_type == 2) {
        if (gr->switch_point != 0)
          gr->part2_length = 17 * gr->slen0 + 18 * gr->slen1;
        else
          gr->part2_length = 18 * gr->slen0 + 18 * gr->slen1;
      } else {
        gr->part2_length = 11 * gr->slen0 + 10 * gr->slen1;
      }

      gr->part3_length = gr->part2_3_length - gr->part2_length;
    }
  }

  frame->adu_size = (frame->adu_bitsize + 7) / 8;
  
  return 1;
}

/*M
  \emph{Read the MP3 frame header.}
**/
int mp3_read_hdr(mp3_frame_t *frame) {
  assert(frame != NULL);
  
  bv_t bv;
  bv_init(&bv, frame->raw, 4 * 8);
  
  /*M
    \emph{Header identifaction string.}
    
   (ISO) The bit string ``\verb|1111 1111 1111|''
  **/
  if (bv_get_bits(&bv, 12) != 0xFFF)
    return 0;

  /*M
    \emph{Algorithm ID.}
    
   (ISO) one bit to indicate the ID of the algorithm. Equals ``1''
   for MPEG audio, ``0'' is reserved
  **/
  frame->id            = bv_get_bits(&bv, 1);

  if (frame->id == 0)
    return 0;
  
  /*M
    \emph{Layer information.}
    
   (ISO) 2 bits to indicate which layer is used, according to the
   following table

   \begin{tabular}{|l|l|}
   \hline
   11 & Layer I \\
   10 & Layer II \\
   01 & Layer III \\
   00 & reserved \\
   \hline
   \end{tabular}
  **/
  frame->layer         = bv_get_bits(&bv, 2);

  /* we can only handle Layer III */
  if (frame->layer != 1)
    return 0;

  /*M
    \emph{Redundancy protection flag.}

   (ISO) One bit to indicate whether redundancy has been added in
   the audio bitstream to facilitate error detection and
   concealment. Equals 1 if no redundancy has been added, 0 if
   redundancy has been added.
  **/
  frame->protected        = bv_get_bits(&bv, 1);

  /*M
    \emph{Bitrate information.}
    
   (ISO) Indicates the bitrate. The all zero value indicates the
   free format condition, in which a fixed bitrate which does not
   need to be in the list can be used. Fixed means that a frame
   contains either N or N+1 slots, depending on the value of the
   padding bit. The bit rate index is an index to a table, which is
   different for the different Layers.

   The bit rate index indicates the total bitrate irrespective of
   the mode (stereo, joint stereo, dual channel, single channel).
   For Layer II, not all combinations of total bitrate and mode
   are allowed. See 3-Annex B, Table 3-B.2 "Layer II BIT ALLOCATION
   TABLES"

   \begin{tabular}{|l|l|l|l|}
   \hline
   bit rate index &   Layer I   &     Layer II  &       Layer III \\
   \hline
   0000   &         free format &   free format  &    free format \\
   0001   &         32 kbps     &   32 kbps   &       32 kbps \\
   0010   &         64 kbps     &   48 kbps   &       40 kbps \\
   0011   &         96 kbps     &   56 kbps   &       48 kbps \\
   0100   &        128 kbps     &   64 kbps   &       56 kbps \\
   0101   &        160 kbps     &   80 kbps   &       64 kbps \\
   0110   &        192 kbps     &   96 kbps   &       80 kbps \\
   0111   &        224 kbps     &  112 kbps   &       96 kbps \\
   1000   &        256 kbps     &  128 kbps   &      112 kbps \\
   1001   &        288 kbps     &  160 kbps   &      128 kbps \\
   1010   &        320 kbps     &  192 kbps   &      160 kbps \\ 
   1011   &        352 kbps     &  224 kbps   &      192 kbps \\
   1100   &        384 kbps     &  256 kbps   &      224 kbps \\
   1110   &        416 kbps     &  320 kbps   &      256 kbps \\
   1111   &        448 kbps     &  384 kbps   &      320 kbps \\
   \hline
   \end{tabular}
   
   In order to provide the smallest possible delay and complexity,
   the decoder is not required to support a continuously variable
   bitrate when in Layer I or II. Layer III supports variable
   bitrate by switching the bit rate index. However, in free
   format, fixed bitrate is required.
  **/
  frame->bitrate_index = bv_get_bits(&bv, 4);

  /*M
    \emph{Sampling frequency information.}
    
   (ISO) Indicates the sampling frequency, according to the
   following table:

   \begin{tabular}{|l|l|}
   \hline
   00 & 44.1 kHz \\
   01 & 48 kHz \\
   10 & 32 kHz \\
   11 & reserved \\
   \hline
   \end{tabular}
   
   A reset of the decoder is required to change the sampling rate
  **/
  frame->samplerfindex = bv_get_bits(&bv, 2);

  /*M
    \emph{Padding flag.}
    
   (ISO) If this bit equals '1' the frame contains an additional
   slot to adjust the mean bitrate to the sampling frequency,
   otherwise this bit will be '0'. Padding is only necessary with a
   sampling frequency of 44.1 kHz.
  **/
  frame->padding_bit   = bv_get_bits(&bv, 1);

  /*M
    \emph{Private bit.}
    
   (ISO) Bit for private use. This bit will not be used in the
   future by ISO.
  **/
  frame->private_bit   = bv_get_bits(&bv, 1);

  /*M
    \emph{Stereo encoding mode.}
    
   (ISO) Indicates the mode according to the following table. In
   Layer I and II the joint stereo mode is intensity stereo, in
   Layer III it is intensity stereo and/or ms stereo

   \begin{tabular}{|l|l|}
   \hline
   00 & stereo \\
   01 & joint stereo (intensity stereo and/or ms stereo) \\
   10 & dual channel \\
   11 & single channel \\
   \hline
   \end{tabular}
  **/
  frame->mode          = bv_get_bits(&bv, 2);

  /*M
    \emph{Joint Stereo subband division information.}
    
   (ISO) These bits are used in joint stereo mode. In Layer I and
   II they indicate which subbands are in intensity stereo. All
   other subbands are coded in stereo.
   
   00 - subbands  4-31 in intensity stereo, bound==4
   01 - subbands  8-31 in intensity stereo, bound==8
   10 - subbands 12-31 in intensity stereo, bound==12
   11 - subbands 16-31 in intensity stereo, bound==16
   
   In Layer III they indicate which type of join stereo coding
   method is applied. The frequency ranges over which the
   intensity stereo and ms stereo modes are applied are implicit in
   the algorithm. For more information see 2.4.3.4.
   
                   intensio stereo        ms stereo
   00              off                    off
   01              on                     off
   10              off                    on
   11              on                     on
   **/
  frame->mode_ext      = bv_get_bits(&bv, 2);

  /*M
    \emph{Copyright}
    
   (ISO) If this bit equals 0 there is no copyright on the coded
    bitstream, 1 means copyright protected.
  **/
  frame->copyright     = bv_get_bits(&bv, 1);

  /*M
    \emph{Original flag.}
    
   (ISO) This bit equals 0 if the bitstream is a copy, 1 if it is
   an original.
  **/
  frame->original      = bv_get_bits(&bv, 1);

  /*M
    \emph{MPEG Audio emphasis.}
    
   (ISO) indicates the type of de-emphasis that shall be used.
   \begin{itemize}
   \item 00 - no emphasis
   \item 01 - 50/15 microsec. emphasis
   \item 10 - reserved
   \item 11 - CCITT J.17
   \end{itemize}
  **/
  frame->emphasis      = bv_get_bits(&bv, 2);

  if (frame->protected == 0) {
    frame->crc[0] = frame->raw[4];
    frame->crc[1] = frame->raw[5];
  }

  mp3_calc_hdr(frame);
  
  return 1;
}

/*M
  \emph{Skip the ID3v2 tag at the beginning.}
**/
int mp3_skip_id3v2(file_t *mp3, mp3_frame_t *frame) {
  assert(mp3 != NULL);
  assert(frame != NULL);

  unsigned char id3v2_hdr[10];
  memcpy(id3v2_hdr, frame->raw, 4);
  if ((id3v2_hdr[0] != 'I') ||
      (id3v2_hdr[1] != 'D') ||
      (id3v2_hdr[2] != '3')) 
    return 0;

  if (file_read(mp3, id3v2_hdr + 4, 6) != 6)
    return EEOF;

  /* parse synchsafe integer */
  size_t tag_size = (((id3v2_hdr[6] & 0x7F) << 21) |
                     ((id3v2_hdr[7] & 0x7F) << 14) |
                     ((id3v2_hdr[8] & 0x7F) << 7) |
                      (id3v2_hdr[9] & 0x7F)) +
                      (id3v2_hdr[5] & 0x10 ? 10 : 0);

  if (!file_seek_fwd(mp3, tag_size))
    return 0;

  return 1;
}

/*M
  \emph{Read the next MP3 frame in the MP3 file.}
**/
int mp3_next_frame(file_t *mp3, mp3_frame_t *frame) {
  assert(mp3 != NULL);
  assert(frame != NULL);

  unsigned int resync = 0;
 again:
  if (resync != 0) {
    /* read the next byte and build the new header */
    frame->raw[0] = frame->raw[1];
    frame->raw[1] = frame->raw[2];
    frame->raw[2] = frame->raw[3];
    
    if (file_read(mp3, frame->raw + 3, 1) <= 0)
      return EEOF;
  } else {
    if (file_read(mp3, frame->raw, MP3_HDR_SIZE) <= 0)
      return EEOF;
  }

  if ((frame->raw[0] == 0xFF) &&
      (((frame->raw[1] >> 4) & 0xF) == 0xF)) {
    if (!mp3_read_hdr(frame))
      goto resync;
    else 
      resync = 0;
  } else if ((frame->raw[0] == 'I') &&
             (frame->raw[1] == 'D') &&
             (frame->raw[2] == '3')) {
    if (!mp3_skip_id3v2(mp3, frame)) {
      goto resync;
    } else {
      resync = 0;
      goto again;
    }
  } else {
    goto resync;
  }

  if (file_read(mp3, frame->raw + 4, frame->frame_size - 4) <= 0)
    return EEOF;

  if (!mp3_read_si(frame))
    goto again;

  return 1;

  resync:
  frame->syncskip++;
  if (resync++ > MP3_MAX_SYNC) {
    fprintf(stderr, "Max sync exceeded: %d\n", resync);
    return ESYNC;
  } else
    goto again;
}

/*M
  %XXX
**/
int mp3_unpack(mp3_frame_t *frame) {
  if (!mp3_read_hdr(frame))
    return 0;
  if (!mp3_read_si(frame))
    return 0;

  return 1;
}

/*M
**/

#ifdef MP3_TEST
int main(int argc, char *argv[]) {
  mp3_file_t  file;
  mp3_frame_t frame;

  char *f;
  if (!(f = *++argv)) {
    fprintf(stderr, "Usage: mp3test mp3file\n");
    return 1;
  }

  if (!mp3_open_read(&file, f)) {
    fprintf(stderr, "Could not open mp3 file: %s\n", f);
    return 1;
  }

  while (mp3_next_frame(&file, &frame) > 0) {
    printf("frame main_data_end %d\n", frame.si.main_data_end);
    fgetc(stdin);
  }

  mp3_close(&file);

  return 0;
}
#endif
