#ifndef ADU_QUEUE_H__
#define ADU_QUEUE_H__

#include "adu.h"
#include "dlist.h"
#include "mp3.h"

/*M
  \emph{MPEG Frame and ADU queue structure.}
**/
typedef struct {
  dlist_head_t  frames;
  dlist_head_t  adus;
  unsigned long size; /* total size of data in queue */
} aq_t;

/*C
**/

void aq_init(aq_t *q);
void aq_destroy(aq_t *q);

int aq_add_frame(aq_t *q, mp3_frame_t *frame);
int aq_add_adu(aq_t *q, adu_t *adu);
mp3_frame_t *aq_get_frame(aq_t *q);
adu_t *aq_get_adu(aq_t *q);

dlist_t *aq_top_frame(aq_t *q);
dlist_t *aq_tail_frame(aq_t *q);

dlist_t *aq_top_adu(aq_t *q);
dlist_t *aq_tail_adu(aq_t *q);

#endif /* ADU_QUEUE_H__ */
