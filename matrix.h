/*C
  (c) 2003 Institut fuer Telematik, Universitaet Karlsruhe

  Based on an implementation by Luigi Rizzo and Numerical Recipes in C
**/

#ifndef MATRIX_H__
#define MATRIX_H__

#include "galois.h"

void matrix_mul(gf *a, gf *b, gf *c, int n, int k, int m);
int  matrix_inv(gf *a, int k);
int  matrix_inv_vandermonde(gf *a, int k);
void matrix_print(gf *a, int m, int n);

/*C
**/

#endif /* MATRIX_H__ */
