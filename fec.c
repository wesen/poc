/*C
  (c) 2003 Institut fuer Telematik, Universitaet Karlsruhe
**/

#include <stdlib.h>
#include <assert.h>
#include <string.h>

#include "fec.h"
#include "matrix.h"

#ifdef DEBUG
#include <stdio.h>
#endif

/*M
  \emph{Free a FEC parameter structure.}
**/
void fec_free(fec_t *fec) {
  assert(fec != NULL);
  assert(fec->gen_matrix != NULL);
  free(fec->gen_matrix);
  free(fec);
}

/*M
  \emph{Initialize a FEC parameter structure.}

  Create a generator matrix.
  % XXX Documentation for generator matrix
**/
fec_t *fec_new(unsigned int k, unsigned int n) {
  assert((k <= n ) || "k is too big");
  assert((k <= 256) || "k is too big");
  assert((n <= 256) || "n is too big");

  /*M
    Init Galois arithmetic if not already initialized.
  **/
  static int gf_initialized = 0;
  if (!gf_initialized) {
    gf_init();
    gf_initialized = 1;
  }

  fec_t *res;
  res = malloc(sizeof(fec_t));
  assert(res != NULL);
  res->gen_matrix = malloc(sizeof(gf)*k*n);
  assert(res->gen_matrix != NULL);

  res->k = k;
  res->n = n;

  /*M
    Fill the matrix with powers of field elements.
  **/
  gf tmp[k*n];
  /*  gf *tmp = res->gen_matrix; */

  /*M
    First row is special (powers of $0$).
  **/
  tmp[0] = 1;
  unsigned int col;
  for (col = 1; col < k; col++)
    tmp[col] = 0;
  
  gf *p;
  unsigned int row;
  for (p = tmp + k, row = 0; row < n - 1; row++, p += k) {
    for (col = 0; col < k; col++)
      p[col] = gf_polys[(row * col) % 255];
  }

#ifdef DEBUG
  fprintf(stderr, "first vandermonde matrix\n");
  matrix_print(tmp, res->n, res->k);
#endif

  /*M
    Invert the upper $k \times k$ vandermonde matrix.
  **/
  matrix_inv_vandermonde(tmp, k);

#ifdef DEBUG
  fprintf(stderr, "\ninverted vandermonde matrix\n");
  matrix_print(tmp, res->n, res->k);
#endif

  /*M
    Multiply the inverted upper $k \times k$ vandermonde matrix with
    the lower band of the matrix.
  **/
  matrix_mul(tmp + k * k, tmp, res->gen_matrix + k * k, n - k, k, k);

  /*M
    Fill the upper $k \times k$ matrix with the identity matrix to
    generate a systematic matrix.
  **/
  for (row = 0; row < k; row++)
    for (col = 0; col < k; col++)
      if (col == row)
        res->gen_matrix[row * k + col] = 1;
      else
        res->gen_matrix[row * k + col] = 0;

#ifdef DEBUG
  fprintf(stderr, "\ngenerated matrix\n");
  matrix_print(res->gen_matrix, res->n, res->k);
#endif
  
  return res;
}

/*M
  \emph{Produce encoded output packet.}

  Encodes the \verb|idx|'th output data packet from the \verb|k| data
  packets in \verb|src| and the generator matrix in \verb|fec|. For
  \verb|idx| $<$ \verb|k|, we just copy the data (systematic matrix).
**/
void fec_encode(fec_t *fec,
                gf *src[], gf *dst,
                unsigned int idx, unsigned int len) {
  assert((idx < fec->n) || "Index of output packet to high");
  
  if (idx < fec->k) {
    memcpy(dst, src[idx], len * sizeof(gf));
  } else {
    gf *p = fec->gen_matrix + idx * fec->k;

    bzero(dst, len * sizeof(gf));
    unsigned int i;
    for (i = 0; i < fec->k; i++)
      gf_add_mul(dst, src[i], p[i], len);
  }
}

/*M
  \emph{Builds the decoding matrix.}

  Builds the decoding matrix into \verb|matrix| out of the indexes
  stored in \verb|idxs|.

  Returns 0 on error, 1 on success.
**/
int fec_decode_matrix(fec_t *fec,
                      gf *matrix,
                      unsigned int idxs[]) {
  gf *p;

  unsigned int i;
  for (p = matrix, i = 0; i < fec->k; i++, p += fec->k) {
    assert((idxs[i] < fec->n) || "index of packet to high for FEC");

    memcpy(p, fec->gen_matrix + idxs[i] * fec->k, fec->k * sizeof(gf));
  }

#ifdef DEBUG
  matrix_print(matrix, fec->k, fec->k);
#endif
  
  if (!matrix_inv(matrix, fec->k))
    return 0;

  return 1;
}

/*M
  \emph{Put straight packets at the right place.}

  Packets with index $<$ k are put at the right place.
**/
static int fec_shuffle(fec_t *fec, unsigned int idxs[]) {
  unsigned int i;
  for (i = 0; i < fec->k; ) {
    if ((idxs[i] >= fec->k) ||
        (idxs[i] == i)) {
      i++;
    } else {
      unsigned int c = idxs[i];

      /* check for conflicts */
      if (idxs[c] == c)
        return 0;

      idxs[i] = idxs[c];
      idxs[c] = c;
    }
  }

  return 1;
}


/*M
  \emph{Decode the received packets.}

  % XXXX
**/
int fec_decode(fec_t *fec,
               gf *pkts,
               unsigned int idxs[], unsigned len) {
  assert(fec != NULL);
  
  if (!fec_shuffle(fec, idxs))
    return 0;

  /*M
    Build decoding matrix.
  **/
  gf dec_matrix[fec->k * fec->k];
  if (!fec_decode_matrix(fec, dec_matrix, idxs))
    return 0;

  unsigned int row;
  for (row = 0; row < fec->k; row++) {
    if (idxs[row] >= fec->k) {
      gf *pkt = pkts + row * len;
      
      bzero(pkt, len * sizeof(gf));
      unsigned int col;
      for (col = 0; col < fec->k; col++) {
        gf_add_mul(pkt, pkts + idxs[col] * len,
                   dec_matrix[row * fec->k + col], len);
      }
    }
  }

  return 1;
}

/*C
**/

#ifdef FEC_TEST
#include <stdio.h>

void testit(char *name, unsigned int result, unsigned int should) {
  if (result == should) {
    printf("Test %s was successful\n", name);
  } else {
    printf("Test %s was not successful, %u should have been %u\n",
           name, result, should);
  }
}

int main(void) {
  fec_t *fec;

  gf_init();
  fec = fec_new(4, 8);
  printf("\n");

  gf src_pkts[4][4] =
    { {1, 2, 3, 4},
      {5, 6, 7, 8},
      {9, 10, 11, 12},
      {13, 14, 15, 16} };
  gf dst_pkts[8 * 4];
  gf *src_ptrs[4] = { src_pkts[0], src_pkts[1], src_pkts[2], src_pkts[3] };
  unsigned int idxs[4] = {3, 5, 1, 0}; /* from 0 ?? */

  int i;
  for (i = 0; i < 8; i++) {
    fec_encode(fec, src_ptrs, dst_pkts + i * 4, i, 4);

    int j;
    for (j = 0; j < 4; j++)
      printf("%u ", dst_pkts[i * 4 + j]);
    printf("\n");
  }

  memset(dst_pkts + 2 * 4, 0, 4);
  memset(dst_pkts + 4 * 4, 0, 4);
  memset(dst_pkts + 6 * 4, 0, 4);
  memset(dst_pkts + 7 * 4, 0, 4);
      
  testit("fec decode", fec_decode(fec, dst_pkts, idxs, 4), 1);

  for (i = 0; i < 4; i++) {
    int j;
    for (j = 0; j < 4; j++)
      testit("fec decode", dst_pkts[i * 4 + j], src_pkts[i][j]);
  }
  
  fec_free(fec);
  
  return 0;
}
#endif /* FEC_TEST */

