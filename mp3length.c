#include "conf.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "file.h"
#include "mp3.h"
#include "aq.h"

static void usage(void) {
  fprintf(stderr, "Usage: mp3length mp3file\n");
}

static void format_time(unsigned long time, char *str, unsigned int len) {
  unsigned long ms = time % 1000;
  time /= 1000;
  unsigned long secs = time % 60;
  time /= 60;
  unsigned long minutes = time;
  time /= 60;
  unsigned long hours = time;

  snprintf(str, len, "%.2lu:%.2lu:%.2lu+%.3lu", hours, minutes, secs, ms);
}

int main(int argc, char *argv[]) {
  int retval = EXIT_SUCCESS;

  if (argc != 2) {
    usage();
    return EXIT_FAILURE;
  }

  file_t mp3file;
  if (!file_open_read(&mp3file, argv[1])) {
    fprintf(stderr, "Could not open mp3 file: %s\n", argv[1]);
    retval = EXIT_FAILURE;
    goto exit;
  }

  aq_t qin;
  aq_init(&qin);
  
  mp3_frame_t frame;
  int ret;
  unsigned long long time = 0;
  while ((ret = mp3_next_frame(&mp3file, &frame)) > 0) {
    if (aq_add_frame(&qin, &frame)) { 
      adu_t *adu = aq_get_adu(&qin);
      assert(adu != NULL);
      
      time += adu->usec;
      free(adu);
    }
  }
          
  file_close(&mp3file);
  aq_destroy(&qin);

  char buf[256];
  format_time(time / 1000, buf, sizeof(buf));
  printf("Length of %s: %s\n", argv[1], buf);

 exit:
  return retval;
}
