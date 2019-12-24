/*C
  (c) 2005 bl0rg.net
**/

#include "conf.h"

#include <signal.h>
#include <stdio.h>

#include "sig_set_handler.h"

/*M
  \emph{Set a new handler for a signal.}

  Returns the old handler on success, \verb|SIG_ERR| on error. Uses
  the new \verb|sigaction| API.
**/
sig_handler_t *sig_set_handler(int signo, sig_handler_t *handler) {
  struct sigaction act, old_act;

  act.sa_handler = handler;
  sigemptyset(&act.sa_mask);
  act.sa_flags = 0;

  if (sigaction(signo, &act, &old_act) < 0) {
    perror("sigaction");
    return SIG_ERR;
  }

  return old_act.sa_handler;
}

/*C
**/
