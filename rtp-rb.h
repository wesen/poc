/*C
  (c) 2003 Institut fuer Telematik, Universitaet Karlsruhe
**/

#ifndef RTP_RB_H__
#define RTP_RB_H__

extern unsigned int rtp_rb_cnt;
extern unsigned int rtp_rb_size;

void rtp_rb_clear(void);
void rtp_rb_destroy(void);
void rtp_rb_init(unsigned int size);
unsigned int rtp_rb_length(void);
void rtp_rb_pop(void);
void rtp_rb_print(void);
int rtp_rb_insert_pkt(rtp_pkt_t *pkt, int idx);
rtp_pkt_t *rtp_rb_first(void);

#endif /* RTP_RB_H__ */
