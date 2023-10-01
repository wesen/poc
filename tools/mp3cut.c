#include "conf.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "lib/mp3/aq.h"
#include "lib/system/file.h"
#include "lib/mp3/mp3.h"
#include "lib/mp3/id3.h"
#include "lib/system/misc.h"

#define min(a, b) ((a) < (b) ? (a) : (b))

static void usage(void) {
  printf("Usage: mp3cut [-o outputfile] [-T title] [-A artist] [-N album-name] [-t [hh:]mm:ss[+ms]-[hh:]mm:ss[+ms]] mp3 [-t ...] mp3\n");
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

/**
 * @brief Parses a time string and returns the total time in milliseconds.
 *
 * This function is designed to parse time strings in the following formats:
 *   MM:SS
 *   HH:MM:SS
 *   MM:SS+MS
 *   HH:MM:SS+MS
 *
 * @note
 *   - The input string 'str' should be null-terminated.
 *   - Ensure time strings have up to 4 fields and follow constraints: minutes < 60 (unless solo), seconds < 60, and milliseconds < 1000 to avoid errors.
 *
 * @param str Input time string to be parsed.
 * @param time Output pointer where the parsed time in milliseconds will be stored.
 *
 * @return Returns 0 on successful parse, -1 otherwise.
 *
 * @details The function uses strtok to split the input string based on delimiters (either ':' or '+').
 * It then uses an external function 'parse_number()' to convert each token into a number.
 * The switch case at the end determines how to interpret the parsed numbers based on the number
 * of fields in the input string and the presence of a '+' delimiter.
 */
static int parse_time(const char *str, unsigned long *time) {
  /* strtok madness */
  char strbuf[256];
  strncpy(strbuf, str, sizeof(strbuf));
  strbuf[sizeof(strbuf) - 1] = '\0';
  
  *time = 0;
  
  char *token;
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
      if (minutes >= 60)
        return -1;
    }
    break;

  case 4:
    hours = numbers[0];
    minutes = numbers[1];
    seconds = numbers[2];
    ms = numbers[3];
    if (minutes >= 60)
      return -1;
    break;

  default:
    return -1;
  }

  if ((seconds >= 60) || (ms >= 1000))
    return -1;

  *time = (((hours * 60) + minutes) * 60 + seconds) * 1000 + ms;

  return 0;
}

typedef struct mp3cut_s {
  char filename[256];
  unsigned long from, to;
} mp3cut_t;

typedef struct mp3cut_id3_s {
  char artist[256];
  char title[256];
  char album[256];
} mp3cut_id3_t;

static unsigned int parse_arguments(mp3cut_t *mp3cuts, unsigned int max_cuts,
                                    char *outfilename, unsigned int max_outfilename,
                                    mp3cut_id3_t *id3,
                                    int argc, char *argv[]) {
  int i;
  unsigned int mp3cuts_cnt = 0;

  memset(id3->title, 0, sizeof(id3->title));
  memset(id3->album, 0, sizeof(id3->album));
  memset(id3->artist, 0, sizeof(id3->artist));

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
    } else if (!strcmp(argv[i], "-T")) {
      if (argc > (i+1)) {
        strncpy(id3->title, argv[i+1], sizeof(id3->title));
        id3->title[sizeof(id3->title) - 1] = '\0';
      } else {
        goto exit_usage;
      }
      i++;
    } else if (!strcmp(argv[i], "-N")) {
      if (argc > (i+1)) {
        strncpy(id3->album, argv[i+1], sizeof(id3->album));
        id3->album[sizeof(id3->album) - 1] = '\0';
      }
      i++;
    } else if (!strcmp(argv[i], "-A")) {
      if (argc > (i+1)) {
        strncpy(id3->artist, argv[i+1], sizeof(id3->artist));
        id3->artist[sizeof(id3->artist) - 1] = '\0';
      }
      i++;
    } else if (!strcmp(argv[i], "-t")) {
      if (argc > (i+1)) {
        char *fromstr, *tostr;
        fromstr = strtok(argv[i+1], "-");
        tostr = strtok(NULL, "-");
        if (!fromstr && !tostr)
          goto exit_usage;

        if (fromstr && (strlen(fromstr) > 0)) {
          if (parse_time(fromstr, &mp3cuts[mp3cuts_cnt].from) < 0) {
            goto exit_usage;
          }
        } else {
          mp3cuts[mp3cuts_cnt].from = 0;
        }

        if (tostr && (strlen(tostr) > 0)) {
          if (parse_time(tostr, &mp3cuts[mp3cuts_cnt].to) < 0) {
            goto exit_usage;
          }
        } else {
          mp3cuts[mp3cuts_cnt].to = 0;
        }
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
  mp3cut_id3_t id3;

  memset(outfilename, '\0', sizeof(outfilename));

  mp3cuts_cnt = parse_arguments(mp3cuts, 256,
                                outfilename, sizeof(outfilename),
                                &id3, argc, argv);
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
  if (!id3_write_tag(&outfile, id3.album, id3.artist, id3.title, 0,
                     "Created by mp3cut (http://bl0rg.net/software/poc/)")) {
    fprintf(stderr, "Could not write id3 tag to file: %s\n", outfilename);
    file_close(&outfile);
    retval = EXIT_FAILURE;
    goto exit;
  }

  printf("Writing to %s\n", outfilename);

  /* cycle through the mp3cuts */
  int i;
  for (i = 0; i < mp3cuts_cnt; i++) {
    file_t mp3file = {0};
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
      int ret;
      if ((ret = mp3_next_frame(&mp3file, &frame)) > 0) {
        if (aq_add_frame(&qin, &frame)) { 
          adu_t *adu = aq_get_adu(&qin);
          assert(adu != NULL);
          
          if ((current / 1000) >= mp3cuts[i].from) {
            char curstr[256];
            format_time(current / 1000, curstr, sizeof(curstr));
            
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
          /* ignore error */
        }
      } else {
        finished = 1;
        if (ret != EEOF) {
          fprintf(stderr, "Error reading from %s: %d\n", mp3cuts[i].filename, ret);
        } else {
          if ((current / 1000) <= mp3cuts[i].from) {
            fprintf(stderr, "Could not extract data from %s, file too short\n",
                    mp3cuts[i].filename);
          } else {
            if (mp3cuts[i].to == 0)
              continue;
          
            char timebuf[256];
            unsigned long duration = (current / 1000) - mp3cuts[i].from;
            format_time(duration, timebuf, sizeof(timebuf));
            fprintf(stderr, "Could only extract %s from %s, file too short\n",
                    timebuf, mp3cuts[i].filename);
          }
        }
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
