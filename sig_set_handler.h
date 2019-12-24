/*C
  (c) 2005 bl0rg.net
**/

#ifndef SIGNAL_H__
#define SIGNAL_H__

#include <signal.h>

/*M
  \emph{Function type for signal handlers.}
**/
typedef void sig_handler_t(int);

sig_handler_t *sig_set_handler(int signo, sig_handler_t *handler);

#endif /* SIGNAL_H__ **/

/*C
**/
