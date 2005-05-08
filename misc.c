/*
 * (c) 2005 bl0rg.net
 *
 * Misc functions (mostly unix workarounds)
 */

#include <unistd.h>
#include <errno.h>
#include <stdio.h>

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

void format_time(unsigned long time, char *str, unsigned int len) {
    unsigned long ms = time % 1000;
    time /= 1000;
    unsigned long secs = time % 60;
    time /= 60;
    unsigned long minutes = time % 60;
    time /= 60;
    unsigned long hours = time;

    snprintf(str, len, "%.2lu:%.2lu:%.2lu+%.3lu", hours, minutes, secs, ms);
}

