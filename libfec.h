#ifndef LIBFEC_H__
#define LIBFEC_H__

void libfec_init(void);
void libfec_close(void);
void libfec_reset(void);

/* fec_len is the maximum packet size in the group */
fec_group_t *libfec_new_group(unsigned char fec_k,
                              unsigned char fec_n,
                              unsigned long fec_len);
void libfec_delete_group(fec_group_t *group);

/* Decoding routines */
void libfec_add_pkt(fec_group_t *group,
                    unsigned char pkt_seq,
                    unsigned long len,
                    unsigned char *data);
/* return 1 on success, 0 on error */
int libfec_decode(fec_group_t *group);

/* Encoding routines */

#endif /* LIBFEC_H__ */
