/*C
  (c) 2003 Institut fuer Telematik, Universitaet Karlsruhe
**/

#ifndef BV_H__
#define BV_H__

/*M
  \emph{Bit vector structure.}
**/
typedef struct bv_s {
  /*@dependent@*/ unsigned char *data;
  unsigned int  len;
  unsigned int  idx;
} bv_t;

/*C
**/

void bv_init(/*@out@*/ bv_t *bv, unsigned char *data, unsigned int len);
void bv_reset(/*@out@*/ bv_t *bv);
unsigned long bv_get_bits(bv_t *bv, unsigned int numbits);
int bv_put_bits(bv_t *bv, unsigned long bits, unsigned int numbits);

#endif /* BV_H__ */
