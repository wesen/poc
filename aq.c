/*C
  (c) 2003 Institut fuer Telematik, Universitaet Karlsruhe
**/

#include "conf.h"

#include <assert.h>
#include <stdlib.h>
#include <string.h>

#ifdef DMALLOC
#include <dmalloc.h>
#endif

#ifdef DEBUG
#include <stdio.h>
#endif

#include <stdio.h>

#include "aq.h"

static void aq_discard_top_frame(aq_t *q);
static void aq_discard_top_adu(aq_t *q);
static void aq_enqueue_frame(aq_t *q, mp3_frame_t *frame);
static void aq_enqueue_adu(aq_t *q, adu_t *adu);
static void aq_make_adu(aq_t *q);
static void aq_insert_dummy_adu(aq_t *q, unsigned int backptr);
static void aq_insert_dummy_adus(aq_t *q);
static int aq_need_adu(aq_t *q);
static void aq_make_frame(aq_t *q);

/*S
  General ADU queue functions
**/

/*M
  \emph{Initialize an ADU queue structure.}
**/
void aq_init(aq_t *q) {
  assert(q != NULL);
  
  dlist_init(&q->frames);
  dlist_init(&q->adus);
  q->size = 0;
}

/*M
  \emph{Destroy an ADU queue structure.}
**/
void aq_destroy(aq_t *q) {
  assert(q != NULL);
  
  dlist_destroy(&q->frames, free);
  dlist_destroy(&q->adus, free);
}

/*M
  \emph{Enqueue a new frame into the ADU queue.}
  \verb|malloc| a new MP3 frame structure and initialize it by copying
  the content of \verb|frame|. Append the new MP3 frame structure to
  the frames linked list of the ADU queue.
  Add the size of \verb|frame|'s audio data
  ($\verb|frame size| - \verb|sideinfo size| - \verb|header size|$)
  to the total queue size.
**/
static void aq_enqueue_frame(aq_t *q, mp3_frame_t *frame) {
  assert(q != NULL);
  assert(frame != NULL);

  mp3_frame_t *tmp_frame = malloc(sizeof(mp3_frame_t));
  assert(tmp_frame != NULL);

  memcpy(tmp_frame, frame, sizeof(mp3_frame_t));

  dlist_t *dlist = dlist_ins_end(&q->frames, tmp_frame);
  assert(dlist != NULL);
  q->size += tmp_frame->frame_data_size;
}

/*M
  \emph{Enqueue a new ADU into the ADU queue.}
**/
static void aq_enqueue_adu(aq_t *q, adu_t *adu) {
  assert(q   != NULL);
  assert(adu != NULL);

  adu_t *tmp_adu = malloc(sizeof(adu_t));
  assert(tmp_adu != NULL);

  memcpy(tmp_adu, adu, sizeof(adu_t));

  dlist_t *dlist = dlist_ins_end(&q->adus, tmp_adu);
  assert(dlist != NULL);
}

/*M
  \emph{Returns the tail MPEG Frame in the queue.}
**/
dlist_t *aq_tail_frame(aq_t *q) {
  assert(q != NULL);
  
  return q->frames.end;
}

/*M
  \emph{Returns the top MPEG Frame in the queue.}
**/
dlist_t *aq_top_frame(aq_t *q) {
  assert(q != NULL);

  return q->frames.dlist;
}

/*M
  \emph{Returns the tail ADU in the queue.}
**/
dlist_t *aq_tail_adu(aq_t *q) {
  assert(q != NULL);
  
  return q->adus.end;
}

/*M
  \emph{Returns the top ADU in the queue.}
**/
dlist_t *aq_top_adu(aq_t *q) {
  assert(q != NULL);

  return q->adus.dlist;
}

/*M
  \emph{Pops the front ADU off the queue.}
**/
adu_t *aq_get_adu(aq_t *q) {
  return dlist_get_front(&q->adus);
}

/*M
  \emph{Pops the front MPEG Frame off the queue.}

  Recalculates the total frame data size of the queue.
**/
mp3_frame_t *aq_get_frame(aq_t *q) {
  mp3_frame_t *res = dlist_get_front(&q->frames);
  if (res)
    q->size -= res->frame_data_size;
  
  return res;
}

/*M
  \emph{Discard the top MPEG Frame of the queue.}

  Recalculates the total frame data size of the queue.
**/
static void aq_discard_top_frame(aq_t *q) {
  assert(q->frames.dlist != NULL);
  mp3_frame_t *frame = dlist_get(&q->frames, q->frames.dlist);
  assert(frame != NULL);
  q->size -= frame->frame_data_size;
  free(frame);
}

/*M
  \emph{Discard the top ADU of the queue.}
**/
static void aq_discard_top_adu(aq_t *q) {
  assert(q->adus.dlist != NULL);
  adu_t *adu = dlist_get(&q->adus, q->adus.dlist);
  assert(adu != NULL);
  free(adu);
}

/*M
  \emph{Generate an ADU frame out of MPEG frames.}

  Enqueue frames into the queue until there is enough data
  to generate the ADU for the most recently enqueued frame.

  \begin{algorithm} \item[A1:] [Enqueue a new frame]
  \verb|size_before| $\leftarrow$ size of the queue before enqueuing, enqueue the
  new MP3 frame and add the frame size of the new MP3 frame to the
  size of queue. \verb|size_after| $\leftarrow$ size of queue after enqueuing.

  \item[A2:] [Check if new frame can be used to generate an ADU]
  If the backpointer of the new frame is bigger than \verb|size_before|
  then goto {\bf A1}.

  \item[A3:] [Check for enough data]
[  If the size of the queue is smaller than the adu size of the new
  frame then goto {\bf A1}.
  \end{algorithm}

  Now an ADU can be generated for the newest frame. The older frames
  cannot be transformed to ADUs as data is missing. Get the first
  frame in the queue containing ADU data for this frame, and get the
  offset into the frame data.

  \begin{algorithm}
  \item[B1:] [Initialize]
  \verb|prev_bytes| $\leftarrow$ backpointer of newest frame.

  \item[B2:] [Handle previous frame] 
  \verb|a| $\leftarrow$ the previous enqueued frame.
  Substract the frame size of \verb|a| from \verb|prev_bytes|.

  \item[B3:] [Check for enough data]
  If \verb|prev_bytes| $\leq$ 0 then \verb|offset| $\leftarrow$ $-$ \verb|prev_bytes|
  and stop, else goto {\bf B2}.
  \end{algorithm}

  Now \verb|a| is the first frame containing data for the ADU, and
  \verb|offset| contains the offset into the frame data of \verb|a|.
  All olders frames can be discarded.

  \begin{algorithm}
  \item[C1:] If queue head $\leftarrow$ \verb|a| then stop, else free the queue
  head and goto {\bf C1}.
  \end{algorithm}

  Get the ADU data.

  \begin{algorithm}
  \item[D1:] [Initialize]
  \verb|adu_size| $\leftarrow$ \verb|adu_size| of newest frame.

  \item[D2:] [Copy data from frame into ADU]
A:
  \verb|data_here| $\leftarrow$ frame size of \verb|a|  $-$ \verb|offset|,
  \verb|bytes_here| $\leftarrow$ minimum of \verb|data_here| and \verb|adu_size|,
  get \verb|bytes_here| bytes of data from current frame at offset
  \verb|offset|, substract \verb|bytes_here| from \verb|adu_size|.

  \item[D3:] [Get next frame]
  \verb|a| $\leftarrow$ next frame in queue, \verb|offset| $\leftarrow$ 0.
  If \verb|adu_size| $>$ 0 then goto {\bf D2}, else stop.
  \end{algorithm}

**/
static void aq_make_adu(aq_t *q) {
  /* get tail frame */
  dlist_t *dlist = aq_tail_frame(q);
  assert(dlist != NULL);
  mp3_frame_t *tail = dlist->data;
  assert(tail != NULL);

  unsigned int back_ptr = tail->si.main_data_end;

  adu_t adu;
  memcpy(&adu, tail, sizeof(adu_t));

  /* get first frame containing ADU data */
  int offset = 0;
  while (back_ptr > 0) {
    dlist = dlist->prev;
    assert(dlist != NULL);
    mp3_frame_t *prev_frame = dlist->data;
    assert(prev_frame != NULL);

    if (prev_frame->frame_data_size < back_ptr) {
      back_ptr -= prev_frame->frame_data_size;
    } else {
      offset = prev_frame->frame_data_size - back_ptr;
      break;
    }
  }

  /* discard unneeded top frames */
  while (aq_top_frame(q) != dlist)
    aq_discard_top_frame(q);

  /* fill adu data */
  int adu_size = tail->adu_size;
  unsigned char *adu_ptr = mp3_frame_data_begin(&adu);
  while (adu_size > 0) {
    assert(dlist != NULL);
    mp3_frame_t *f = dlist->data;
    assert(f != NULL);

    int data_here = f->frame_data_size - offset;
    int bytes_here = adu_size > data_here ? data_here : adu_size;

    unsigned char *ptr = mp3_frame_data_begin(f);
    
    assert(((bytes_here + offset) <= (long)f->frame_data_size) ||
	   "Frame is too short");
    
    memcpy(adu_ptr, ptr + offset, bytes_here);
    adu_ptr += bytes_here;
    adu_size -= bytes_here;
    dlist = dlist->next;
    offset = 0;
  }

  aq_enqueue_adu(q, &adu);
}

/*M
  \emph{Add a MPEG frame to the queue and generate an ADU if possible.}

  Returns 0 if no ADU could be generated, 1 if an ADU could be
  generated.
**/
int aq_add_frame(aq_t *q, mp3_frame_t *frame) {
  assert(q != NULL);
  assert(frame != NULL);

  int back_ptr    = frame->si.main_data_end;
  int size_before = q->size;

  /* discard broken frames */
  if (frame->adu_size > (frame->frame_data_size + back_ptr)) {
    fprintf(stderr, "Broken frame, skipping...\n");
    return 0;
  }

  assert(frame->adu_size <= (frame->frame_data_size + back_ptr));
  
  aq_enqueue_frame(q, frame);

  if ((size_before < back_ptr) ||
      (q->size < frame->adu_size))
    return 0;

  aq_make_adu(q);
  return 1;
}

/*M
  \emph{Algorithms to convert ADU frames back to MP3 frames.}

  Enqueue ADU frames into the queue until there is enough data to
  generate the MP3 for the head ADU. 

  \begin{algorithm}
  \item[A1:] [Initialize]
  \verb|frame_len| $\leftarrow$ \verb|frame_data_size| of head ADU,
  \verb|previous_frame_size| $\leftarrow$ 0,
  \verb|adu| $\leftarrow$ head ADU.

  \item[A2:]
A:
  \verb|data_end| $\leftarrow$ size of \verb|adu| $-$ backpointer of \verb|adu| $+$
  \verb|previous_frame_size|.

  \item[A3:] [Check for enough data] If \verb|data_end| $>$
  \verb|frame_len| then stop, else add the \verb|frame_data_size| of
  \verb|adu| to \verb|previous_frame_size|.

  \item[A4:] [Check for more ADUs]
  If there are no more ADUs in the queue stop (more ADUs are needed), else
  \verb|adu| $\leftarrow$ next ADU and goto {\bf A2}.
  \end{algorithm}

  After an ADU frame is enqueued, check if there are missing ADUs
  (the backpointer of the recently enqueued ADU overlaps the data of
  the previous ADU).

  \begin{algorithm}

  \item[B1:] [Initialize]
  \verb|adu| $\leftarrow$ tail ADU of the ADU queue.

  \item[B2:] [Get previous ADU]
  If there is no previous ADU,
  \verb|prev_adu_end| $\leftarrow$ 0 and goto {\bf B4}, else \verb|prev_adu| $\leftarrow$ previous
  ADU, \verb|prev_adu_end| $\leftarrow$ \verb|frame_data_size| of \verb|prev_adu| $+$
  \verb|main_data_end| of \verb|prev_adu| $-$ \verb|adu_size| of
  \verb|prev_adu|.
  
  \item[B3:] [Check previous ADU]
  If \verb|prev_adu_end| $<$ 0 the frame was not well formed, abort.

  \item[B4:] [Check if an ADU is missing]
  If the \verb|main_data_end| of \verb|adu| $>$
  \verb|prev_adu_end| an ADU is missing, insert a dummy ADU in front of
  the tail and goto {\bf B2}.
  \end{algorithm}

  If there are enough ADUs in the queue, generate the top frame.

  \begin{algorithm}
  \item[C1:] [Initialize the new MPEG frame]
  Copy the header and sideinfo from top adu into mp3 frame,
  zero out the frame data.

  \item[C2:] [Initialize]
  \verb|adu| $\leftarrow$ top adu, offset of data in previous adus $\leftarrow$ 0.

  \item [C3:] [Calculate beginning of data]
A:
  \verb|data_start| (beginning of data contained in adu relative to beginning
    of the mp3 frame data)
    $\leftarrow$ offset of data in previous adus $-$ \verb|main_data_end| of \verb|adu|.

  \item[C4:] []
  If \verb|data_start| $>$ \verb|frame_data_size| of mp3
  frame then goto B, else \verb|data_end| (end of data contained in
  \verb|adu| relative to begging of the mp3 frame data) $\leftarrow$ maximum of
  (\verb|data_start| $+$ size of \verb|adu|) and \verb|frame_data_size|
  of mp3 frame.

  \item[C5:] [Calculate data offsets]
  If \verb|data_start| $<$ 0
      then \verb|from_offset| $\leftarrow$ $-$ \verb|data_start|, \verb|to_offset| $\leftarrow$ 0 and
           \verb|data_length| $\leftarrow$ \verb|data_end|
      else \verb|from_offset| $\leftarrow$ 0, \verb|to_offset| $\leftarrow$ \verb|data_start| and
           \verb|data_length| $\leftarrow$ \verb|data_end| $-$ \verb|data_start|.

  \item[C6:] [Copy data from ADU to frame]
  Copy \verb|data_length|
  from \verb|adu| at offset \verb|from_offset| to mp3 frame at offset
  \verb|to_offset| and add \verb|frame_data_size| of \verb|adu| to
  offset of data in previous adus.

  \item[C7:]
  If \verb|data_end| $<$ \verb|frame_data_size| of mp3 frame
  then \verb|adu| $\leftarrow$ next adu in queue and goto A.

 \item[C8:]
B:    
  Discard top ADU.
  \end{algorithm}
**/

/*M
  \emph{Insert a dummy ADU.}

  Inserts a dummy ADU by copying the frame header and sideinfo
  information of tail ADU and zeroing out \verb|main_data_end| and the length
  fields.
**/
static void aq_insert_dummy_adu(aq_t *q, unsigned int backptr) {
  assert(q != NULL);

#ifdef DEBUG
  fprintf(stderr, "XXX insert dummy adu backptr %d\n", backptr);
#endif

  dlist_t *dlist = aq_tail_adu(q);
  assert(dlist != NULL);
  adu_t *tail = dlist->data;
  assert(tail != NULL);

  adu_t *dummy = malloc(sizeof(adu_t));
  assert(dummy != NULL);

  /* structure copy */
  *dummy = *tail;

  /* zero out backpointer and sideinfo length information */
  dummy->si.main_data_end = backptr;
  unsigned int i, j;
  for (i = 0; i < 2; i++)
    for (j = 0; j < 2; j++) {
      dummy->si.channel[i].granule[j].part2_3_length =
	dummy->si.channel[i].granule[j].big_values = 0;
      dummy->si.channel[i].granule[j].scale_comp = 0;
      dummy->si.channel[i].granule[j].tbl_sel[0] = 
	dummy->si.channel[i].granule[j].tbl_sel[1] =
	dummy->si.channel[i].granule[j].tbl_sel[2] = 0;
    }

  dummy->adu_size = dummy->adu_bitsize = 0;

  dlist_t *new = dlist_ins_before(&q->adus, dlist, dummy);
  assert(new != NULL);
}

/*M
  \emph{Insert dummy ADUs if necessary.}
**/
static void aq_insert_dummy_adus(aq_t *q) {
  assert(q != NULL);

  dlist_t *dlist = aq_tail_adu(q);
  assert(dlist != NULL);
  adu_t *tail = dlist->data;
  assert(tail != NULL);

  unsigned int prev_adu_end = 0;
  if (dlist->prev) {
    adu_t *prev = dlist->prev->data;
    assert(prev != NULL);

    prev_adu_end =
      prev->frame_data_size + prev->si.main_data_end - prev->adu_size;
    assert((prev_adu_end >= 0) || "bad formed ADU frame");
  }

  /* insert the necessary dummy adus */
  int back_ptr = tail->si.main_data_end - prev_adu_end;
  for (; back_ptr > 0;
       back_ptr -= tail->frame_data_size,
	 prev_adu_end = 0)
    aq_insert_dummy_adu(q, prev_adu_end);
}

//#define MIN(a, b) (((a) < (b)) ? (a) : (b))
#define MIN(a, b) minbla(a, b)

long minbla(long a, long b) {
  return (a < b ? a : b);
}

/*M
  \emph{Make a MP3 frame from ADUs.}
**/
static void aq_make_frame(aq_t *q) {
  assert(q != NULL);
  
  dlist_t *dlist = aq_top_adu(q);
  assert(dlist != NULL);
  adu_t *top = dlist->data;
  assert(top != NULL);

#ifdef DEBUG
  fprintf(stderr, "top adu size: %d, frame size: %d\n", top->adu_size, top->frame_data_size);
#endif
  
  mp3_frame_t frame;
  memcpy(&frame, top, sizeof(adu_t));
  memset(frame.raw, 0, MP3_RAW_SIZE);

  unsigned int frames_offset = 0;
  int data_end = 0;
  do {
    assert (dlist != NULL);
    adu_t *adu = dlist->data;
    assert(adu != NULL);

#ifdef DEBUG
    fprintf(stderr, "frames offset %d, backptr %d\n", frames_offset, adu->si.main_data_end);
#endif

    int data_start = frames_offset - adu->si.main_data_end;

    if (data_start > (long)frame.frame_data_size)
      break;

    assert(data_start <= (long)frame.frame_data_size);
    
    data_end = MIN(data_start + adu->adu_size, (long)frame.frame_data_size);
#ifdef DEBUG
    fprintf(stderr, "data_start: %d, data_end: %d\n", data_start, data_end);
#endif

    unsigned int from_offset, to_offset, data_length;
    if (data_start < 0) {
      from_offset = -data_start;
      to_offset = 0;
      
      if (data_end < 0)
	data_length = 0;
      else
	data_length = data_end;
    } else {
      from_offset = 0;
      to_offset = data_start;
      data_length = data_end - data_start;
    }

    if (data_length > 0) {
      assert(adu->adu_size >= from_offset + data_length);
#ifdef DEBUG
      fprintf(stderr, 
	      "memcpy (size: %d) from [%d:%d] to [%d:%d]\n",
	      adu->adu_size,
	      from_offset, from_offset + data_length,
	      to_offset, to_offset + data_length);
#endif
      memcpy(mp3_frame_data_begin(&frame) + to_offset,
	     mp3_frame_data_begin(adu) + from_offset,
	     data_length);
    }

    frames_offset += adu->frame_data_size;
    
    dlist = dlist->next;
  } while (data_end < (long)frame.frame_data_size);

  aq_discard_top_adu(q);
  aq_enqueue_frame(q, &frame);
}

/*M
  \emph{Check if there are enough ADUs in the queue to generate a new
  MPEG frame.}
**/
static int aq_need_adu(aq_t *q) {
  assert(q != NULL);
  
  dlist_t *dlist = aq_top_adu(q);
  assert(dlist != NULL);
  adu_t *head = dlist->data;
  assert(head != NULL);
  
  unsigned int frame_len = head->frame_data_size;

  unsigned int prev_frames_size = 0;
  while (dlist) {
    adu_t *adu = dlist->data;
    assert(adu != NULL);
    
    unsigned int data_end =
      prev_frames_size + adu->adu_size - adu->si.main_data_end;

    if (data_end >= frame_len)
      return 1;

    prev_frames_size += adu->frame_data_size;
    dlist = dlist->next;
  }

  return 0;
}

/*M
  \emph{Adds an ADU to the queue and generates a MPEG frame if possible.}
**/
int aq_add_adu(aq_t *q, adu_t *adu) {
  assert(q != NULL);
  assert(adu != NULL);

  aq_enqueue_adu(q, adu);
  aq_insert_dummy_adus(q);

  if (aq_need_adu(q)) {
    aq_make_frame(q);
    return 1;
  } else {
    return 0;
  }
}

/*C
**/

#ifdef AQ1_TEST
#include <stdio.h>

int main(int argc, char *argv[]) {
  mp3_file_t  file;
  mp3_frame_t frame;
  aq_t        qin;

  char *f;
  if (!(f = *++argv)) {
    fprintf(stderr, "Usage: aq1 mp3file\n");
    return 1;
  }

  aq_init(&qin);
  
  if (!mp3_open_read(&file, f)) {
    fprintf(stderr, "Could not open mp3 file: %s\n", f);
    return 1;
  }

  while (mp3_next_frame(&file, &frame) > 0) {
    if (!aq_add_frame(&qin, &frame)) {
      printf("could not generate an ADU\n");
    } else {
      printf("could generate an ADU\n");
    }
    fgetc(stdin);
  }

  file_close(&file);

  aq_destroy(&qin);

  return 0;
}

#endif /* AQ1_TEST */

#ifdef AQ2_TEST
#include <stdio.h>
unsigned long cksum(unsigned char *buf, int cnt) {
  unsigned long res = 0;
  
  int i;
  for (i = 0; i < cnt; i++)
    res += buf[i];

  return res;
}

int main(int argc, char *argv[]) {
  char *f[2];

  if (!(f[0] = *++argv) || !(f[1] = *++argv)) {
    fprintf(stderr, "Usage: mp3-write mp3in mp3out\n");
    return 1;
  }

  file_t in;
  if (!file_open_read(&in, f[0])) {
    fprintf(stderr, "Could not open mp3 file for read: %s\n", f[0]);
    return 1;
  }

  file_t out;
  if (!file_open_write(&out, f[1])) {
    fprintf(stderr, "Could not open mp3 file for write: %s\n", f[1]);
    file_close(&in);
    return 1;
  }

  aq_t qin, qout;
  aq_init(&qin);
  aq_init(&qout);

  mp3_frame_t frame;
  while (mp3_next_frame(&in, &frame) > 0) {
    static int cin = 0;
    printf("%d in frame_size %ld, backptr %d, adu_size %ld, cksum %ld\n",
	   cin++,
	   frame.frame_data_size, frame.si.main_data_end, frame.adu_size,
	   cksum(frame.raw, frame.frame_size));
    
    if (aq_add_frame(&qin, &frame)) {
      adu_t *adu = aq_get_adu(&qin);
      assert(adu != NULL);

      static int count = 0;
      if (count > 2000)
	break;

      if ((count++ % 25) <= 10) {
	free(adu);
	continue;
      }

      if (aq_add_adu(&qout, adu)) {
	mp3_frame_t *frame_out = aq_get_frame(&qout);
	assert(frame_out != NULL);
	
	static int cout = 0;
	
	memset(frame_out->raw, 0, 4 + frame_out->si_size);
	if (!mp3_fill_hdr(frame_out) ||
	    !mp3_fill_si(frame_out) ||
	    (mp3_write_frame(&out, frame_out) <= 0)) {
	  fprintf(stderr, "Could not write frame\n");
	  file_close(&in);
	  file_close(&out);
	  return 1;
	}
	
	printf("%d out frame_size %ld, backptr %d, adu_size %ld, cksum %ld\n",
	       cout++,
	       frame_out->frame_data_size,
	       frame_out->si.main_data_end,
	       frame_out->adu_size,
	       cksum(frame_out->raw, frame_out->frame_size));
	
	free(frame_out);
      }

      free(adu);
    }

    //    fgetc(stdin); 
  }

  file_close(&in);
  file_close(&out);

  aq_destroy(&qin);
  aq_destroy(&qout);

  return 0;
}

#endif /* AQ2_TEST */

