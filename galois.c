/*C
  (c) 2003 Institut fuer Telematik, Universitaet Karlsruhe

  Based on an implementation by Luigi Rizzo
**/

#include "galois.h"

/*M
  \emph{Polynomial representation of field elements.}
**/
gf gf_polys[256];

/*M
  \emph{Logarithmic representation of field elements.}
**/
gf gf_logs[256];

/*M
  \emph{Precomputed multiplication table.}
**/
gf gf_mul[256][256];

/*M
  \emph{Precomputed inverse table.}
**/
gf gf_inv[256];

/*M
  \emph{A primitive polynomial.}

  A primitive polynomial for \gf{2^8}, namely
  \[1 + x^2 + x^3 + x^4 + x^8\]
**/
static char gf_prim_poly[] = "101110001";

/*M
  \emph{Initialize data structures.}
**/
void gf_init(void) {
  /*M
    Notice that $x^8 = x^4 + x^3 + x^2 + 1$.
    We will fill up the "eight" power of alpha from the prime
    polynomial.
  **/
  gf_polys[8] = 0;

  /*M
    The first 8 elements are just alpha shifted to the left.
  **/
  int i;
  gf  g = 1;
  for (i = 0; i < 8; i++, g <<= 1) {
    gf_polys[i] = g;
    /*M
      Remember logarithm by storing it into the logarithm lookup table.
    **/
    gf_logs[gf_polys[i]] = i;

    /*M
      Fill up the eighth element.
    **/
    if (gf_prim_poly[i] == '1')
      gf_polys[8] |= g;
  }
  /*M
    Remember logarithm of eigth element.
  **/
  gf_logs[gf_polys[8]] = 8;

  /*M
    For each further element, $a^n = a^(n-1) * a$.
    We just need to calculate the modulo \verb|gf_prim_poly|, which is of
    degree $8$.
  **/
  g = 1 << 7;
  for (i = 9; i < 255; i++) {
    if (gf_polys[i - 1] >= g)
      /*M
	$a^{n-1} * a > $ \verb|gf_prim_poly|, then
	$a^n = a^{n-1} * a = a^8 + ... = a^4 + a^3 + a^2 + 1$.
      **/
      gf_polys[i] = gf_polys[8] ^ ((gf_polys[i - 1]) << 1);
    else
      gf_polys[i] = gf_polys[i - 1] << 1;

    /*M
      Remember logarithm.
    **/
    gf_logs[gf_polys[i]] = i;
  }

  /*M
    The 0th element is undefined.
  **/
  gf_logs[0] = 0xFF;

  /*M
    Compute multiplication table.
  **/
  for (i = 0; i < 256; i++) {
    int j;
    for (j = 0; j < 256; j++) {
      if ((i == 0) || (j == 0))
	gf_mul[i][j] = 0;
      else
	gf_mul[i][j] = gf_polys[(gf_logs[i] + gf_logs[j]) % 255];
    }

    for (j = 0; j < 256; j++)
      gf_mul[0][j] = gf_mul[j][0] = 0;
  }

  /*M
    Compute inverses.
  **/
  gf_inv[0] = 0;
  gf_inv[1] = 1;
  for (i = 2; i < 256; i++)
    gf_inv[i] = gf_polys[255 - gf_logs[i]];
}

/*M
  \emph{Computes addition of a row multiplied by a constant.}

  Computes $a = a + c * b$, $a, b \in \gf{2^8}^k, c \in \gf{2^8}$.
**/
void gf_add_mul(gf *a, gf *b, gf c, int k) {
  int i;
  for (i = 0; i < k; i++)
    a[i] = GF_ADD(a[i], GF_MUL(c, b[i]));
}

/*C
**/

#ifdef GALOIS_TEST
#include <stdio.h>

void testit(char *name, int result, int should) {
  if (result == should) {
    printf("Test %s was successful\n", name);
  } else {
    printf("Test %s was not successful, %x should have been %x\n",
	   name, result, should);
  }
}

int main(void) {
  gf a, b, c;

  gf_init();
  a = 1;
  b = 37;
  c = 78;
  testit("1 * ( 37 + 78 ) = 1 * 37 + 1 * 78",
	 GF_MUL(a, GF_ADD(b, c)),
	 GF_ADD(GF_MUL(a, b), GF_MUL(a, c)));
  testit("(1 * 37) * 78 = 1 * (37 * 78)",
	 GF_MUL(GF_MUL(a, b), c),
	 GF_MUL(a, GF_MUL(b, c)));
  testit("(37 * 78) * 37 = (37 * 37) * 78",
	 GF_MUL(GF_MUL(b, c), b),
	 GF_MUL(GF_MUL(b, b), c));
  testit("b * b^-1 = 1", GF_MUL(b, GF_INV(b)), 1);

  return 0;
}

#endif /* GALOIS_TEST */
