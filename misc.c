/*
 * (c) 2005 bl0rg.net
 *
 * Misc functions (mostly unix workarounds)
 */

#include <unistd.h>
#include <errno.h>

#include "misc.h"

int unix_write(int fd, unsigned char *buf, size_t size) {
  int i, len = 0;

  while ((i = write(fd, buf + len, size - len))) {
    if (i < 0) {
      if ((errno == EINTR) || (errno == EAGAIN))
        continue;
      else {
        perror("write");
        return -1;
      }
    } else {
      len += i;
      if (len == size)
        break;
    }
  }

  if (i == 0)
    return 0;
  else
    return len;
}

int unix_read(int fd, unsigned char *buf, size_t size) {
  int i, len = 0;

  while ((i = read(fd, buf + len, size - len))) {
    if (i < 0) {
      if ((errno == EINTR) || (errno == EAGAIN))
        continue;
      else {
        perror("read");
        return -1;
      }
    } else {
      len += i;
      if (len == size)
        break;
    }
  }

  if (i == 0)
    return 0;
  else
    return len;
}


  
