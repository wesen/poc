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

  int fdfifo = open(argv[1], O_WRONLY);
  assert(fdfifo >= 0);

  struct flock fl;
  fl.l_start = 0;
  fl.l_len = 0;
  fl.l_pid = getpid();
  fl.l_type = F_WRLCK;
  fl.l_whence = SEEK_SET;
  int ret = fcntl(fdfifo, F_SETLKW, &fl);
  assert(ret >= 0);

  for (;;) {
    unsigned char buf[BUFSIZE];
    ret = read(0, buf, sizeof(buf));
    if (ret < 0) {
      if (errno != EINTR)
	break;
      else
	continue;
    }
    if (ret == 0)
      break;
    ret = unix_write(fdfifo, buf, ret);
    if (!ret)
      break;
  }

  close(fdfifo);

  return 0;
}
