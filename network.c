/*C
  (c) 2003 Institut fuer Telematik, Universitaet Karlsruhe
**/

#include "conf.h"

int net_seqnum_diff(unsigned long seq1, unsigned long seq2,
		 unsigned long maxseq) {
  if (seq2 >= seq1) {
    if ((seq2 - seq1) < (maxseq / 2))
      return seq2 - seq1;
    else
      return (seq2 - seq1) - maxseq;
  } else {
    int d = seq2 + maxseq - seq1;
    if (d < (maxseq / 2))
      return d;
    else
      return d - maxseq;
  }
}


/*C
**/
