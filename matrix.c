/*C
  (c) 2003 Institut fuer Telematik, Universitaet Karlsruhe
**/

#include "matrix.h"
#include <stdio.h>
#include <string.h>

/*M
  \emph{Print a $m \times n$ matrix.}
**/
void matrix_print(gf *a, int m, int n) {
  int row;
  for (row = 0; row < m; row++) {
    int col;
    for (col = 0; col < n; col++)
      fprintf(stderr, "%-3u ", a[row * n + col]);
    fprintf(stderr, "\n");
  }
}

/*M
  \emph{Matrix multiplication.}

  Computes $c = a * b$, with $a \in \gf{2^8}^{n \times k}, b \in
  \gf{2^8}^{k \times m}, c \in \gf{2^8}^{n \times m}$.
**/
void matrix_mul(gf *a, gf *b, gf *c, int n, int k, int m) {
  int row;

  for (row = 0; row < n; row++) {
    int col;
    for (col = 0; col < m; col++) {
      gf *pa = a + row * k;
      gf *pb = b + col;
      gf acc = 0;

      int i;
      for (i = 0; i < k; i++, pa++, pb += m)
	acc = GF_ADD(acc, GF_MUL(*pa, *pb));
      
      c[row * m + col] = acc;
    }
  }
}

/*M
  \emph{Computes the inverse of a matrix.}

  Computes the inverse of \verb|a| into \verb|a| using Gauss-Jordan
  elimination. Transform the matrix using pivoting and Gauss
  elimination to bring it into the form of the identity matrix, and
  apply the transformations to the identity matrix.
  
  Returns 0 on error, 1 on success
**/
int matrix_inv(gf *a, int k) {
  /*M
    Bookkeeping on the pivoting.
  **/
  int indxc[k];
  int indxr[k];

  /*M
    \verb|id_row| is used to compare the pivot row to the corresponding
    identity row in order to speed up computation.
  **/
  gf  id_row[k];
  
  /*M
    \verb|ipiv| marks elements already used as pivots.
  **/
  int ipiv[k];

  /*M
    Initialize \verb|id_row| and \verb|ipiv|.
  **/
  int i;
  for (i = 0; i < k; i++) {
    id_row[i] = 0;
    ipiv[i] = 0;
  }

  int col;
  for (col = 0; col < k; col++) {
    /*M
      Look for a non-zero element to use as pivot.
    **/
    int irow = -1, icol = -1;

    /*M
      First check the diagonal.
    **/
    if ((ipiv[col] != 1) &&
	(a[col * k + col] != 0)) {
      irow = col;
      icol = col;
    } else {
      /*M
	Then search the matrix.
      **/
      int row;
      for (row = 0; row < k; row++) {
	if (ipiv[row] != 1) {
	  for (i = 0; i < k; i++) {
	    if (ipiv[i] == 0) {
	      if (a[row * k + i] != 0) {
		irow = row;
		icol = i;
		goto found_pivot;
	      }
	    } else if (ipiv[i] > 1) {
	      fprintf(stderr, "Singular matrix\n");
	      return 0;
	    }
	  }
	}
      }
      fprintf(stderr, "Pivot not found\n");
      return 0;
    }

  found_pivot:
    /*M
      Now we got a pivot element in \verb|icol| and \verb|irow|.
    **/
    ++(ipiv[icol]);
    
    /*M
      Swap rows so the pivot is on the diagonal.
    **/
    if (irow != icol) {
      gf tmp;
      for (i = 0; i < k; i++) {
	tmp = a[irow * k + i];
	a[irow * k + i] = a[icol * k + i];
	a[icol * k + i] = tmp;
      }
    }

    /*M
      Remember the pivot position.
    **/
    indxr[col] = irow;
    indxc[col] = icol;
    gf *pivot_row = a + icol * k;

    /*M
      Divide pivot row with the pivot element.
    **/
    gf c = pivot_row[icol];
    if (c == 0) {
      fprintf(stderr, "Singular matrix\n");
      return 0;
    } else if (c != 1) {
      c = GF_INV(c);
      pivot_row[icol] = 1;
      for (i = 0; i < k; i++)
	pivot_row[i] = GF_MUL(c, pivot_row[i]);
    }

    /*M
      Reduce rows.
      If the pivot row is the identity row, we don't need to
      substract the pivot row
    **/
    id_row[icol] = 1;
    if (bcmp(pivot_row, id_row, k * sizeof(gf)) != 0) {
      gf *p;
      for (p = a, i = 0; i < k; i++, p += k) {
	/*M
	  Don't reduce the pivot row.
	**/
	if (i != icol) {
	  gf c = p[icol];
	  /*M
	    Zero out the element corresponding to the pivot element
	    and substract the pivot row multiplied by the zeroed out
	    element.
	  **/
	  p[icol] = 0;
	  gf_add_mul(p, pivot_row, c, k);
	}
      }
    }
    id_row[icol] = 0;
  }

  /*M
    Descramble the solution.
  **/
  for (col = k - 1; col >= 0; col--) {
    if (indxr[col] != indxc[col]) {
      int row;
      gf tmp;
      for (row = 0; row < k; row++) {
	tmp = a[row * k + indxr[col]];
	a[row * k + indxr[col]] = a[row * k + indxc[col]];
	a[row * k + indxc[col]] = tmp;
      }
    }
  }

  return 1;
}

/*M
  \emph{Computes the inverse of a Vandermonde matrix.}

  \begin{definition}
  A \emph{Vandermonde matrix} of size $N \times N$ is completely
  determined by $N$ arbitrary numbers $x_1, x_2, \dots, x_N$, in
  terms of which its $N^2$ components are the integer powers
  $x_i^{j-1}, i, j = 1, \dots, N$.
  \end{definition}

  % Write full matrix out 
  We use the $i$'s as rows, the $j$'s as columns. The linear system
  $A \cdot c = y$ solves for the coefficients $c$ which fit a
  polynomial to $(x_j, y_j), j = 1, \dots, N$.

  Let $P_j(x)$ be the Lagrange polynomial of degree $N-1$ defined by
  $x_1, \dots, x_N$. We know that \[P_j(x_i) = \delta_{ij} =
  \sum_{k=1}^NA_{jk}x_i^{k-1}\], therefore the solution of $A \cdot c
  = y$ is just $c_j = \sum_{k=1}^NA_{kj}y_k$. In our routine we are
  only interested by $A_{kj}$, the inverse of the Vandermonde matrix.

  Vandermonde systems are ill-conditioned, but this doesn't affect
  us, as we work in a finite field.

  First, we calculate $P(x) = \prod_{i=1}^k (x - x_i)$, which is then
  synthetically divided by $x_j$, to obtain $\prod_{i=1\\ i\neq j}^n
  (x - x_i)$, and then divided by $\prod_{i=1\\ i \neq j}^n (x_j -
  x_i)$ to obtain $P_j(x)$, which is the $j$-th row of the inverted matrix.

  Only uses the second row of the matrix \verb|a|, containing the
  $x_i$ coefficients.  Returns $1$ on success, $0$ on error.
**/
int matrix_inv_vandermonde(gf *a, int k) {
  /*M
    Check for a degenerate case.
  **/
  if (k == 1)
    return 0;

  /*M
    \verb|p| holds the matrix coefficients \verb|x_i|.
  **/
  gf p[k];
  /*M
    \verb|c| holds the coefficient of
    $P(x) = \prod_{i=0}^{k-1} (x - p_i)$
  **/
  gf c[k];
  int i, j;
  for (i = 0, j = 1; i < k; i++, j += k) {
    c[i] = 0;
    p[i] = a[j];
  }
  
  /*M
    Construct coefficients. We know \verb|c[k] = 1| implicitly.
    Start with $P_0 = x - x_0$. We are in $2^m$, so $x_0 = - x_0$.
  **/
  c[k-1] = p[0];
  for (i = 1; i < k; i++) {
    gf p_i = p[i];

    /*M
      At each step $P_i = x \cdot P_{i - 1} - p_i \cdot P_{i - 1}$,
       so \verb|c[j] = c[j] + p[i] * c[j+1]|,
          \verb|c[k] = 1| (implicit),
	  and \verb|c[k-1] = c[k-1] + p_i|.
    **/
    for (j = k - 1 - i; j < k - 1; j++)
      c[j] = GF_ADD(c[j], GF_MUL(p_i, c[j+1]));
    c[k-1] = GF_ADD(c[k-1], p_i);
  }

  /*M
    \verb|b| holds the coefficient for the matrix inversion.
  **/
  gf b[k];

  /*M
    Do the synthetic division.
  **/
  int row;
  for (row = 0; row < k; row++) {
    gf x = p[row];
    gf t = 1;

    b[k-1] = 1; /* c[k] */
    for (i = k - 2; i >= 0; i--) {
      b[i] = GF_ADD(c[i+1], GF_MUL(x, b[i+1]));
      t = GF_ADD(GF_MUL(x, t), b[i]);
    }

    int col;
    for (col = 0; col < k; col++)
      a[col * k + row] = GF_MUL(GF_INV(t), b[col]);
  }

  return 1;
}

/*C
**/

#ifdef MATRIX_TEST

void testit(char *name, unsigned int result, unsigned int should) {
  if (result == should) {
    printf("Test %s was successful\n", name);
  } else {
    printf("Test %s was not successful, %u should have been %u\n",
	   name, result, should);
  }
}

int main(void) {
  gf matrix1[4*4] = { 1, 0, 0, 0,
		      0, 1, 0, 0,
		      0, 0, 1, 0,
		      0, 0, 0, 1 };
  gf matrix2[4*4];
  gf matrix3[4*4] = { 1, 5, 3, 18,
		      5, 6, 19, 21,
		      9, 0, 0, 7,
		      4, 5, 4, 83 };
  /* from mathematica */
  gf matrix4[4*4] = { 148, 39, 173, 174,
		      55, 134, 87, 159,
		      170, 142, 46, 94,
		      161, 105, 80, 239 };
  gf matrix5[4*4] = {1, 0, 0, 0,
		     1, 0, 0, 0,
		     1, 0, 0, 0,
		     1, 0, 0, 0};

  gf_init();

  gf vand1[4*4] = { 1, 2, GF_MUL(2, 2), GF_MUL(2, GF_MUL(2, 2)),
		    1, 3, GF_MUL(3, 3), GF_MUL(3, GF_MUL(3, 3)),
		    1, 5, GF_MUL(5, 5), GF_MUL(5, GF_MUL(5, 5)),
		    1, 7, GF_MUL(7, 7), GF_MUL(7, GF_MUL(7, 7)) };
  gf vand2[4*4];
      
  testit("invert singular matrix", matrix_inv(matrix5, 4), 0);
  
  memcpy(matrix2, matrix1, sizeof(matrix1));
  testit("invert matrix", matrix_inv(matrix2, 4), 1);

  int i;
  for (i = 0; i < 16; i++)
    testit("invert matrix", matrix2[i], matrix1[i]);
  testit("invert matrix", matrix_inv(matrix3, 4), 1);
  for (i = 0; i < 16; i++)
    testit("invert matrix", matrix3[i], matrix4[i]);

  memcpy(vand2, vand1, sizeof(vand1));
  testit("vandermonde invert matrix", matrix_inv_vandermonde(vand1, 4), 1);
  testit("invert matrix", matrix_inv(vand2, 4), 1);
  for (i = 0; i < 16; i++)
    testit("vandermonde invert matrix", vand1[i], vand2[i]);

  return 0;
}

#endif /* MATRIX_TEST */
