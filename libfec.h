#ifndef LIBFEC_H__
#define LIBFEC_H__

struct fec_group_s;
typedef struct fec_group_s fec_decode_t;

struct fec_encode_s;
typedef struct fec_encode_s fec_encode_t;

void libfec_init(void);
void libfec_close(void);
void libfec_reset(void);

/* fec_len is the maximum packet size in the group */
fec_decode_t *libfec_new_group(unsigned char fec_k,
                               unsigned char fec_n,
                               unsigned long fec_len);
void libfec_delete_group(fec_decode_t *group);

fec_encode_t *libfec_new_encode(unsigned char fec_k,
                                unsigned char fec_n);
void libfec_delete_encode(fec_encode_t *encode);

/****** Decoding routines ******/

/*
 * Add the packet with sequence number (0 <= pkt_seq < fec_n) to the
 * FEC group.
 */
void libfec_add_pkt(fec_decode_t *group,
                    unsigned char pkt_seq,
                    unsigned long len,
                    unsigned char *data);
/*
 * Decode the FEC packets in the group, and write the resulting MP3
 * frames on stdout.
 *
 * Return 1 on success, 0 on error
 */
int libfec_decode(fec_decode_t *group);

/****** Encoding routines ******/

/*
 * Add an ADU (in sequence) into the encoding buffer.
 *
 * Return 0 on error, 1 on success.
 */
int libfec_add_adu(fec_encode_t *encode,
                   unsigned long len,
                   unsigned char *data);
/*
 * after fec_k adus have been added, encode the packet with index IDX
 * into dst, with maximal length len.
 *
 * If idx < fec_k, then the original ADUs is copied.
 *
 * Returns 0 on error, length of the packet on success.
 */
unsigned int libfec_encode(fec_encode_t *encode,
                           unsigned char *dst,
                           unsigned int idx,
                           unsigned int len);

/*
 * Return the length of the biggest ADU in the group (and thus the
 * length of the FEC pkts with index >= fec_k.
 */
unsigned int libfec_max_length(fec_encode_t *encode);

#endif /* LIBFEC_H__ */
