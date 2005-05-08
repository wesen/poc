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
void format_time(unsigned long time, char *str, unsigned int len);

#endif /* MISC_H__ */
