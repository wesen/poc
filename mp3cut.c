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
  printf("Usage: mp3cut [-o outputfile] [-t [hh:]mm:ss[+ms]-[hh:]mm:ss[+ms]] mp3 [-t ...] mp3\n");
  printf("-o output: Output file, default mp3file.out.mp3\n");
}

static int parse_number(const char *str, unsigned long *result) {
  char buf[16];
  char *endptr = NULL;
  strncpy(buf, str, sizeof(buf));
  buf[sizeof(buf) - 1] = '\0';
  *result = strtoul(buf, &endptr, 10);
  if ((*endptr != '\0') || (endptr == buf))
    return -1;
  return 0;
}

static int parse_time(const char *str, unsigned long *time) {
  /* strtok madness */
  char strbuf[256];
  strncpy(strbuf, str, sizeof(strbuf));
  strbuf[sizeof(strbuf) - 1] = '\0';
  
  *time = 0;
  
  char *token, *token2;
  unsigned long numbers[4];
  unsigned long cnt = 0;

  token = strtok(strbuf, "+:");
  if ((token == NULL) || (parse_number(token, &numbers[cnt++]) < 0))
    return -1;
  token = strtok(NULL, "+:");
  if ((token == NULL) || (parse_number(token, &numbers[cnt++]) < 0))
    return -1;
  token = strtok(NULL, "+:");
  if (token && (parse_number(token, &numbers[cnt++]) < 0))
    return -1;
  token = strtok(NULL, "+:");
  if (token && (parse_number(token, &numbers[cnt++]) < 0))
    return -1;

  int mspresent = (strchr(str, '+') != NULL);
  unsigned long hours = 0, minutes = 0, seconds = 0, ms = 0;
  switch (cnt) {
  case 2:
    if (mspresent)
      return -1;
    minutes = numbers[0];
    seconds = numbers[1];
    break;

  case 3:
    if (mspresent) {
      minutes = numbers[0];
      seconds = numbers[1];
      ms = numbers[2];
    } else {
      hours = numbers[0];
      minutes = numbers[1];
      seconds = numbers[2];
    }
    break;

  case 4:
    hours = numbers[0];
    minutes = numbers[1];
    seconds = numbers[2];
    ms = numbers[3];
    break;

  default:
    return -1;
  }

  if ((minutes >= 60) || (seconds >= 60) || (ms >= 1000))
    return -1;

  *time = (((hours * 60) + minutes) * 60 + seconds) * 1000 + ms;

  return 0;
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

typedef struct mp3cut_s {
  char filename[256];
  unsigned long from, to;
} mp3cut_t;

static unsigned int parse_arguments(mp3cut_t *mp3cuts, unsigned int max_cuts,
                                    char *outfilename, unsigned int max_outfilename,
                                    int argc, char *argv[]) {
  int i;
  unsigned int mp3cuts_cnt = 0;
  for (i = 0; i < max_cuts; i++) {
    mp3cuts[i].from = mp3cuts[i].to = 0;
    memset(mp3cuts[i].filename, '\0', sizeof(mp3cuts[0].filename));
  }
  for (i = 1; i < argc; i++) {
    if (!strcmp(argv[i], "-h")) {
      goto exit_usage;
    } else if (!strcmp(argv[i], "-o")) {
      if ((i == 1) && (argc > (i+1))) {
        strncpy(outfilename, argv[i+1], max_outfilename);
        outfilename[max_outfilename - 1] = '\0';
        i++;
      } else {
        goto exit_usage;
      }
    } else if (!strcmp(argv[i], "-t")) {
      if (argc > (i+1)) {
        unsigned char *fromstr, *tostr;
        fromstr = strtok(argv[i+1], "-");
        tostr = strtok(NULL, "-");
        if (!fromstr || !tostr)
          goto exit_usage;

        if ((parse_time(fromstr, &mp3cuts[mp3cuts_cnt].from) < 0) ||
            (parse_time(tostr, &mp3cuts[mp3cuts_cnt].to) < 0))
          goto exit_usage;
      }
      i++;
    } else {
      if (mp3cuts_cnt >= max_cuts) {
        fprintf(stderr, "mp3cut cannot handle more than %d cuts\n", max_cuts);
        return -1;
      }
      
      strncpy(mp3cuts[mp3cuts_cnt].filename, argv[i], sizeof(mp3cuts[0].filename));
      mp3cuts[mp3cuts_cnt].filename[sizeof(mp3cuts[0].filename) - 1] = '\0';
      mp3cuts_cnt++;
    }
  }

  if (mp3cuts_cnt == 0)
    goto exit_usage;

  return mp3cuts_cnt;

 exit_usage:
  usage();
  return 0;
}

int main(int argc, char *argv[]) {
  int retval = EXIT_SUCCESS;
  char outfilename[256];
  mp3cut_t mp3cuts[256];
  unsigned int mp3cuts_cnt = 0;

  memset(outfilename, '\0', sizeof(outfilename));

  mp3cuts_cnt = parse_arguments(mp3cuts, 256,
                                outfilename, sizeof(outfilename),
                                argc, argv)  ;
  if (mp3cuts_cnt <= 0) {
    retval = EXIT_FAILURE;
    goto exit;
  }
  
  if (strlen(outfilename) == 0) {
    char *mp3filename = mp3cuts[0].filename;
    char *basename = strrchr(mp3filename, '/');
    if (basename)
      mp3filename = basename + 1;
    char *dot = strrchr(mp3filename, '.');
    char buf[256];
    unsigned int len = dot ? (dot - mp3filename) : strlen(mp3filename);
    len = min(len + 1, sizeof(buf));
    strncpy(buf, mp3filename, len);
    buf[len - 1] = '\0';
    snprintf(outfilename, sizeof(outfilename), "%s.out%s", buf, dot ? dot : ".mp3");
  }

  /* initialize the output stream */
  aq_t qout;
  aq_init(&qout);
  file_t outfile;
  if (!file_open_write(&outfile, outfilename)) {
    fprintf(stderr, "Could not open mp3 file: %s\n", outfilename);
    retval = EXIT_FAILURE;
    goto exit;
  }

  printf("Writing to %s\n", outfilename);

  /* cycle through the mp3cuts */
  int i;
  for (i = 0; i < mp3cuts_cnt; i++) {
    file_t mp3file;
    if (!file_open_read(&mp3file, mp3cuts[i].filename)) {
      fprintf(stderr, "Could not open mp3 file: %s\n", mp3cuts[i].filename);
      retval = EXIT_FAILURE;
      file_close(&outfile);
      aq_destroy(&qout);
      goto exit;
    }

    char fromstr[256], tostr[256];
    format_time(mp3cuts[i].from, fromstr, sizeof(fromstr));
    format_time(mp3cuts[i].to, tostr, sizeof(tostr));
    printf("Extracting %s-%s from %s\n", fromstr, tostr, mp3cuts[i].filename);
    
    aq_t qin;
    aq_init(&qin);
    
    unsigned long long current = 0;
    int finished = 0;
    
    while (!finished) {
      if (mp3cuts[i].to && ((current / 1000) >= mp3cuts[i].to)) {
        finished = 1;
        break;
      }
      
      mp3_frame_t frame;
      if (mp3_next_frame(&mp3file, &frame) > 0) {
        if (aq_add_frame(&qin, &frame)) { 
          adu_t *adu = aq_get_adu(&qin);
          assert(adu != NULL);
          
          if ((current / 1000) >= mp3cuts[i].from) {
            char curstr[256];
            format_time(current / 1000, curstr, sizeof(curstr));
            // printf("current %s\n", curstr);
            
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
          }

          current += adu->usec;
          free(adu);
          
        } else {
          finished = 1;
        }
      } else {
        finished = 1;
      }
    }

    file_close(&mp3file);
    aq_destroy(&qin);
  }
  
  file_close(&outfile);
  aq_destroy(&qout);
  
  fprintf(stderr, "%s written\n", outfilename);

 exit:
  return retval;
}
