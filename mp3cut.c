#include "conf.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#ifdef NEED_GETOPT_H__
#include <getopt.h>
#endif /* NEED_GETOPT_H__ */

#include "aq.h"
#include "file.h"
#include "mp3.h"

#define min(a, b) ((a) < (b) ? (a) : (b))

static void usage(void) {
  printf("Usage: mp3cut [-f min:sec[:ms]] [-t min:sec[:ms]] [-d min:sec[:ms]] [-o output] mp3file\n");
  printf("-f min:sec[:ms]: Cut from min:sec[:ms], default 00:00:00\n");
  printf("-t min:sec[:ms]: Cut until min:sec[:ms], default end of file\n");
  printf("-d min:sec[:ms]: Cut duration, default not set\n");
  printf("-o output: Output file, default mp3file.out.mp3\n");
}

static int parse_time(const char *str, unsigned long *time) {
  /* strtok madness */
  char strbuf[256];
  strncpy(strbuf, str, sizeof(strbuf));
  strbuf[sizeof(strbuf) - 1] = '\0';
  
  *time = 0;
  
  char buf[16];

  char *first = strtok(strbuf, ":");
  char *endptr = NULL;
  if (first == NULL)
    return -1;
  strncpy(buf, first, sizeof(buf));
  buf[sizeof(buf) - 1] = '\0';
  unsigned long minutes = strtoul(buf, &endptr, 10);
  if ((*endptr != '\0') || (endptr == buf))
    return -1;
  *time += minutes;
  *time *= 60;

  char *second = strtok(NULL, ":");
  if (second == NULL)
    return -1;
  strncpy(buf, second, sizeof(buf));
  buf[sizeof(buf)-1] = '\0';
  unsigned long seconds = strtoul(buf, &endptr, 10);
  if ((seconds >= 60) || (*endptr != '\0') || (endptr == buf))
    return -1;
  *time += seconds;
  *time *= 1000;

  char *third = strtok(NULL, ":");
  if (third == NULL)
    return 0;
  strncpy(buf, third, sizeof(buf));
  buf[sizeof(buf)-1] = '\0';
  unsigned long ms = strtoul(buf, &endptr, 10);
  if ((ms >= 1000) || (*endptr != '\0') || (endptr == buf))
    return -1;
  *time += ms;

  return 0;
}

static void format_time(unsigned long time, char *str, unsigned int len) {
  unsigned long ms = time % 1000;
  time /= 1000;
  unsigned long secs = time % 60;
  time /= 60;
  unsigned long minutes = time;

  snprintf(str, len, "%lu:%.2lu:%.3lu", minutes, secs, ms);
}

int main(int argc, char *argv[]) {
  char *mp3filename = NULL;
  int retval = EXIT_SUCCESS;
  unsigned long from = 0, to = 0, duration = 0;
  char outfilename[256];

  memset(outfilename, '\0', sizeof(outfilename));

  int c;
  while ((c = getopt(argc, argv, "f:t:d:o:h")) >= 0) {
    switch (c) {
    case 'f':
      if (parse_time(optarg, &from) < 0) {
	fprintf(stderr, "Could not parse from time: %s\n", optarg);
	usage();
	retval = EXIT_FAILURE;
	goto exit;
      }
      break;

    case 't':
      if (parse_time(optarg, &to) < 0) {
	fprintf(stderr, "Could not parse to time: %s\n", optarg);
	usage();
	retval = EXIT_FAILURE;
	goto exit;
      }
      break;

    case 'd':
      if (parse_time(optarg, &duration) < 0) {
	fprintf(stderr, "Could not parse duration time: %s\n", optarg);
	usage();
	retval = EXIT_FAILURE;
	goto exit;
      }
      break;

    case 'o':
      strncpy(outfilename, optarg, sizeof(outfilename));
      outfilename[sizeof(outfilename) - 1] = '\0';
      break;
      
    default:
      usage();
      retval = EXIT_FAILURE;
      goto exit;
    }
  }

  if ((optind == argc) || (argv[optind] == NULL)) {
    usage();
    retval = EXIT_FAILURE;
    goto exit;
  }
  mp3filename = argv[optind];

  if (strlen(outfilename) == 0) {
    char *dot = strrchr(mp3filename, '.');
    char buf[256];
    unsigned int len = dot ? (dot - mp3filename) : strlen(mp3filename);
    len = min(len + 1, sizeof(buf));
    strncpy(buf, mp3filename, len);
    buf[len - 1] = '\0';
    snprintf(outfilename, sizeof(outfilename), "%s.out%s\n", buf, dot ? dot : ".mp3");
  }

  char frombuf[256], tobuf[256], durationbuf[256];
  format_time(from, frombuf, sizeof(frombuf));
  format_time(to, tobuf, sizeof(tobuf));
  format_time(duration, durationbuf, sizeof(durationbuf));

  if (duration != 0)
    to = from + duration;
  
  if ((to != 0) && (to >= from)) {
    fprintf(stderr, "Can not cut mp3 from %s to %s\n", frombuf, tobuf);
    retval = EXIT_FAILURE;
    goto exit;
  }

  /*M
    Open the MP3 file.
  **/
  file_t mp3file;
  if (!file_open_read(&mp3file, mp3filename)) {
    fprintf(stderr, "Could not open mp3 file: %s\n", mp3filename);
    retval = EXIT_FAILURE;
    return 0;
  }
      
  aq_t qin;
  aq_init(&qin);
  aq_t qout;
  aq_init(&qout);
  
  /*M
    Open the output MP3 file.
  **/
  file_t outfile;
  if (!file_open_write(&outfile, outfilename)) {
    fprintf(stderr, "Could not open mp3 file: %s\n", outfilename);
    file_close(&mp3file);
    retval = EXIT_FAILURE;
    goto exit;
  }

  unsigned long current = 0;

  /*M
    Read in the input file
    
    Read while current < end or till the end of the file if it's the last track.
  **/
  while (current < from) {
    mp3_frame_t frame;
    if (mp3_next_frame(&mp3file, &frame) > 0) {
      if (aq_add_frame(&qin, &frame)) { 
	adu_t *adu = aq_get_adu(&qin);
	assert(adu != NULL);
	
	if (aq_add_adu(&qout, adu)) {
	  mp3_frame_t *frame_out = aq_get_frame(&qout);
	  assert(frame_out != NULL);
	  
	  memset(frame_out->raw, 0, 4 + frame_out->si_size);
	  if (!mp3_fill_hdr(frame_out) ||
	      !mp3_fill_si(frame_out) ||
	      (mp3_write_frame(&outfile, frame_out) <= 0)) {
	    fprintf(stderr, "Could not write frame\n");
	    file_close(&mp3file);
	    file_close(&outfile);
	    retval = 1;
	    goto exit;
	  }
	  
	  free(frame_out);
	}
	
	free(adu);
      }
      
      current += frame.usec / 1000;
    }
  }
  
  /*M
    Close the output file.
  **/
  file_close(&outfile);
  aq_destroy(&qout);
  
  fprintf(stderr, "%s written\n", outfilename);

  /*M
    Close the input file.
  **/
  file_close(&mp3file);
  aq_destroy(&qin);
    

  /*M
    Cleanup the data structures.
  **/
 exit:
  return retval;
}

/*M
**/
