#include <sys/file.h>
#include <stdio.h>
#include <assert.h>

#include <unistd.h>
#include <errno.h>
#include <stdio.h>

#include "misc.h"

#define BUFSIZE 4096

int main(int argc, char *argv[]) {
  if (argc != 2)
    return 1;

  int fdfifo;
  
  for (;;) {
    fdfifo = open(argv[1], O_RDONLY);
    assert(fdfifo >= 0);
    int ret;
    
    for (;;) {
      unsigned char buf[BUFSIZE];
      ret = read(fdfifo, buf, sizeof(buf));
      if (ret < 0) {
	if (errno != EINTR)
	  goto exit;
	else
	  continue;
      }
      if (ret == 0)
	break;
      ret = unix_write(1, buf, ret);
      if (!ret)
	break;
    }
    
    close(fdfifo);
  }

 exit:
  close(fdfifo);

  return 0;
}
