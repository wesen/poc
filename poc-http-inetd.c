/*C
  (c) 2005 bl0rg.net
**/

#include "conf.h"

#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/time.h>

#include "network.h"
#include "rtp.h"

/*M
  \emph{Print a HTTP failure header to standard out.}
**/
static void pob_bad_request(unsigned long code,
                            const char *comment,
                            const char *msg) {
  printf("HTTP/1.0 %lu %s\r\nConnection: close\r\n\r\n%s\r\n",
         code, comment, msg);
}

int pob_http(void) {
}

/*M
  \emph{Print usage information.}
**/
static void usage(void) {
  fprintf(stderr, "Usage: ./pob [-s address] [-p port]\n");
  fprintf(stderr, "\t-s address : destination address (default 224.0.1.23)\n");
  fprintf(stderr, "\t-p port    : destination port (default 1500)\n");
}

/*M
**/
int main(int argc, char *argv[]) {
  int retval = EXIT_SUCCESS;
  char *address = NULL;
  int  port = 1500;

  address = strdup("224.0.1.23");

  int c;
  while ((c = getopt(argc, argv, "hs:p:")) >= 0) {
    switch (c) {
    case 's':
      if (address != NULL)
        free(address);
      address = strdup(optarg);
      break;

    case 'p':
      port = atoi(optarg);
      break;

    case 'h':
    default:
      usage();
      retval = EXIT_FAILURE;
      goto exit;
    }
  }

  if (!pob_http()) {
    retval = EXIT_FAILURE;
    goto exit;
  }

  if ((sock = net_udp4_recv_socket(address, port)) < 0) {
    fprintf(stderr, "Could not open socket\n");
    retval = EXIT_FAILURE;
    pob_bad_request(500, "Internal Server Error", "");
    goto exit;
  }

  printf("HTTP/1.0 200 OK\r\n");

  if (!pob_main(sock)) {
    retval = EXIT_FAILURE;
    goto exit;
  }

exit:
  if (address != NULL)
    free(address);

  return retval;
}

/*C
**/
