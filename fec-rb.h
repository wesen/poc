/*C
  (c) 2003 Institut fuer Telematik, Universitaet Karlsruhe
**/

#ifndef RTP_RB_H__
#define RTP_RB_H__

extern unsigned int fec_rb_cnt;
extern unsigned int fec_rb_size;

void fec_rb_clear(void);
void fec_rb_destroy(void);
void fec_rb_init(unsigned int size);
unsigned int fec_rb_length(void);
void fec_rb_pop(void);
void fec_rb_print(void);
int fec_rb_insert_pkt(fec_pkt_t *pkt, int idx);
fec_group_t *fec_rb_first(void);

#endif /* RTP_RB_H__ */
