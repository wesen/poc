/*C
  (c) 2003 Institut fuer Telematik, Universitaet Karlsruhe
**/

#ifndef FEC_H__
#define FEC_H__

#include "galois.h"

/*M
  \emph{FEC parameter structure.}

  Contains the $n, k$ parameters for FEC, as well as the generator
  matrix.
**/
typedef struct fec_s {
  /*M
    FEC parameters.
  **/  
  unsigned int k, n;
  /*M
    Linear block code generator matrix.
  **/
  gf *gen_matrix; 
} fec_t;

void fec_free(fec_t *fec);
fec_t *fec_new(unsigned int k, unsigned int n);

void fec_encode(fec_t *fec,
		gf *src[], gf *dst,
		unsigned int idx, unsigned int len);
int fec_decode(fec_t *fec,
	       gf *buf,
	       unsigned int idxs[], unsigned len);

/*C
**/

#endif /* FEC_H__ */
