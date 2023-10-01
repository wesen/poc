/*C
  (c) 2005 bl0rg.net
**/

#include "conf.h"

#include <assert.h>
#include <stdio.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/uio.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <sys/stat.h>

#include "file.h"
#include "misc.h"

/*M
  \emph{Read and retry on interrupted system calls.}
**/ 
int file_read(file_t *file, unsigned char *buf, size_t size) {
  assert(file != NULL);
  assert(buf != NULL);
  assert(size > 0);

  int res = unix_read(file->fd, buf, size);
  if (res > 0) {
    file->pos += res;
  }
  return res;
}

/*M
  \emph{Seek forward in the file.}
**/
int file_seek_fwd(file_t *file, size_t size) {
  if (lseek(file->fd, size, SEEK_CUR) < 0)
    return 0;
  else {
    file->pos += size;
    return 1;
  }
}

/*M
  \emph{Open a file.}

  Return 0 on error, 1 on success. To read from STDIN, call with "-"
  as filename.
**/
int file_open_read(file_t *file, char *filename) {
  assert(file != NULL);
  assert(filename != NULL);

  if (strcmp(filename, "-") == 0) {
    /* read from stdin */
    file->fd   = STDIN_FILENO;
    file->size = 0;
  } else {
    file->fd = open(filename, O_RDONLY);

    if (file->fd < 0) {
      perror("open");
      return 0;
    }

    struct stat sb;
    if (stat(filename, &sb) < 0) {
      perror("stat");
      return 0;
    }
    file->size = (unsigned long)sb.st_size;
  }

  file->offset = 0;

  return 1;
}

/*M
  \emph{Write a buffer to filedescriptor fd.}
**/
int file_write(file_t *file, unsigned char *buf, size_t size) {
  assert(file != NULL);
  assert(buf != NULL);

  return unix_write(file->fd, buf, size);
}

/*M
  \emph{Open a file for writing.}
**/
int file_open_write(file_t *file, char *filename) {
  assert(file != NULL);
  assert(filename != NULL);

  if (!strcmp(filename, "-"))
    /* write to stdout */
    file->fd = STDOUT_FILENO;
  else {
    file->fd = open(filename, O_WRONLY | O_CREAT | O_TRUNC,
                    S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);

    if (file->fd < 0) {
      perror("open");
      return 0;
    }
  }

  file->offset = 0;

  return 1;
}

/*M
  \emph{Close a file.}

  Return 0 on error and 1 on success.
**/
int file_close(file_t *file) {
  assert(file != NULL);

  if (file->fd != STDIN_FILENO) {
    if (close(file->fd) < 0) {
      perror("close");
      return 0;
    }
  }

  return 1;
}

/*C
**/
