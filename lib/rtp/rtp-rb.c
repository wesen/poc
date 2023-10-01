/*C
  (c) 2005 bl0rg.net
**/

#include "conf.h"

#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "rtp.h"

/*M
  \emph{Maximal number of elements in the ring buffer.}
**/
unsigned int rtp_rb_size  = 0;

/*M
  \emph{Index of first valid element in ring buffer.}
**/
static unsigned int rtp_rb_start = 0;

/*M
  \emph{Index of first invalid element in ring buffer.}

**/
static unsigned int rtp_rb_end  = 0;

/*M
  \emph{Number of elements in the ring buffer.}
**/
unsigned int rtp_rb_cnt   = 0;

/*M
  \emph{Ring buffer array.}
**/
rtp_pkt_t *rtp_rb = NULL;

void rtp_rb_clear(void) {
  rtp_rb_start = 0;
  rtp_rb_end   = 0;
  rtp_rb_cnt   = 0;

  int i;
  for (i = 0; i < rtp_rb_size; i++) {
    rtp_pkt_init(rtp_rb + i);
    rtp_rb[i].length = 0;
  }
}

void rtp_rb_destroy(void) {
  if (rtp_rb != NULL) {
    free(rtp_rb);
    rtp_rb = NULL;
  }

  rtp_rb_size = 0;
  rtp_rb_clear();
}

void rtp_rb_init(unsigned int size) {
  rtp_rb_destroy();

  rtp_rb = malloc(sizeof(rtp_pkt_t) * size);
  assert(rtp_rb != NULL);

  rtp_rb_size = size;
  rtp_rb_clear();
}
  
unsigned int rtp_rb_length(void) {
  assert(rtp_rb != NULL);
  
  if (rtp_rb_end >= rtp_rb_start)
    return rtp_rb_end - rtp_rb_start;
  else
    return rtp_rb_end + (rtp_rb_size - rtp_rb_start);
}

void rtp_rb_pop(void) {
  assert(rtp_rb != NULL);
  assert(rtp_rb_end != rtp_rb_start);

  rtp_rb[rtp_rb_start].length = 0;
  rtp_rb_start = (rtp_rb_start + 1) % rtp_rb_size;

  rtp_rb_cnt--;
}

int rtp_rb_insert_pkt(rtp_pkt_t *pkt, int idx) {
  assert(pkt != NULL);
  assert(rtp_rb != NULL);

#ifdef DEBUG
  fprintf(stderr, "insert packet at idx %d\n", idx);
#endif
  
  if (idx < 0) {
    /* try to grow the buffer downwards */
    if ((rtp_rb_length() - idx) <= rtp_rb_size) {
      rtp_rb_start = (rtp_rb_start + idx) % rtp_rb_size;
      idx = 0;
    } else
      /* drop the packet silently */
      return 0;
  } else if (idx >= rtp_rb_size - 1) {
    /* not enough place left in ring buffer */
    return 0;
  }


  rtp_pkt_t *dstpkt = rtp_rb + ((idx + rtp_rb_start) % rtp_rb_size);
  assert(dstpkt->length == 0);
  memcpy(dstpkt, pkt, sizeof(rtp_pkt_t));
  rtp_rb_cnt++;

#ifdef DEBUG
  fprintf(stderr, "packet inserted at %d, end: %d\n",
          (idx + rtp_rb_start) % rtp_rb_size,
          rtp_rb_end);
#endif
  
  /* adjust the end pointer */
  if (idx >= rtp_rb_length()) 
    rtp_rb_end = ((idx + rtp_rb_start + 1)% rtp_rb_size);

  assert(rtp_rb_start != rtp_rb_end);
  assert(rtp_rb[rtp_rb_end].length == 0);

  return 1;
}

void rtp_rb_print(void) {
  assert(rtp_rb != NULL);

  fprintf(stderr, "start: %.3u, end: %.3u, len: %.3u\n",
          rtp_rb_start, rtp_rb_end, rtp_rb_length());
}

void rtp_rb_print_rb(void) {
  unsigned int i;
  for (i = rtp_rb_start; i != rtp_rb_end; i = (i + 1) % rtp_rb_size) {
    if (rtp_rb[i].length != 0)
      fprintf(stderr, "%.3u: seq %.3u\n", i, rtp_rb[i].b.seq);
  }
}

rtp_pkt_t *rtp_rb_first(void) {
  assert(rtp_rb != NULL);

  if (rtp_rb_start == rtp_rb_end)
    return NULL;

  /* first should always be the first in buffer */
  return rtp_rb + rtp_rb_start;
}
