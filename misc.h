/*
 * (c) 2005 bl0rg.net
 *
 * Misc functions (mostly unix workarounds)
 */

#ifndef MISC_H__
#define MISC_H__

#include <sys/types.h>

int unix_write(int fd, unsigned char *buf, size_t size);
int unix_read(int fd, unsigned char *buf, size_t size);

#endif /* MISC_H__ */
