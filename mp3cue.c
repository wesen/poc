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
#include "mp3cue.h"
#include "mp3.h"

/*M
  MP3 Cue structure that will get filled by the parser.
**/
mp3cue_file_t *yymp3_cue_file = NULL;

int yyparse();

static void usage(void) {
  printf("Usage: mp3cue -c cuefile mp3file\n");
  printf("-c cuefile: cut according to cue file\n");
}

int main(int argc, char *argv[]) {
  char *cuefilename = NULL,
    *mp3filename = NULL;
  int retval = EXIT_SUCCESS;
  FILE *cuein = NULL;
  mp3cue_track_t *cuetracks = NULL;

  int c;
  while ((c = getopt(argc, argv, "c:C:")) >= 0) {
    switch (c) {
    case 'c':
      if (cuefilename != NULL)
	free(cuefilename);
      cuefilename = strdup(optarg);
      break;

    case 'C':
      break;

    default:
      usage();
      goto exit;
    }
  }

  if (optind == argc) {
    usage();
    goto exit;
  }
  mp3filename = argv[optind];

  if ((cuefilename == NULL) || (mp3filename == NULL)) {
    usage();
    retval = EXIT_FAILURE;
    goto exit;
  }

  /*M
    Initialize the mp3 cue structure.
  **/
  mp3cue_file_t cuefile;
  cuetracks = malloc(sizeof(mp3cue_track_t) * MP3CUE_DEFAULT_TRACK_NUMBER);
  if (cuetracks == NULL) {
    fprintf(stderr, "Could not allocate memory for tracks\n");
    retval = EXIT_FAILURE;
    goto exit;
  }
  cuefile.tracks = cuetracks;
  cuefile.track_number = 0;
  cuefile.max_track_number = MP3CUE_DEFAULT_TRACK_NUMBER;
  yymp3_cue_file = &cuefile;
  strncpy(cuefile.title, cuefilename, MP3CUE_MAX_STRING_LENGTH);

  /*M
    Open the input file.
  **/
  cuein = fopen(cuefilename, "r");
  if (cuein == NULL) {
    fprintf(stderr, "Could not open cuefile %s\n", cuefilename);
    retval = EXIT_FAILURE;
    goto exit;
  }

  /*M
    Parse the input file.
  **/
  extern FILE* yyin;
  yyin = cuein;

  if (yyparse() != 0) {
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

  unsigned long current = 0;

  /*M
    For each track, cut out the relevant part and save it.
  **/
  unsigned int i;
  for (i = 0; i < cuefile.track_number; i++) {
    char outfilename[MP3CUE_MAX_STRING_LENGTH * 3 + 1];
    if (strlen(cuefile.tracks[i].performer) > 0 &&
	strlen(cuefile.tracks[i].title) > 0) {
      snprintf(outfilename, MP3CUE_MAX_STRING_LENGTH * 3,
	       "%02d. %s - %s.mp3", cuefile.tracks[i].number,
	       cuefile.tracks[i].performer, cuefile.tracks[i].title);
    } else {
      snprintf(outfilename, MP3CUE_MAX_STRING_LENGTH * 3,
	       "%02d. %s.mp3", cuefile.tracks[i].number,
	       cuefile.title);
    }

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

    /* end time in msecs */
    unsigned long end = (((cuefile.tracks[i].index.minutes * 60) +
			  cuefile.tracks[i].index.seconds) * 100 +
			 cuefile.tracks[i].index.centiseconds) * 10;
    printf("track %d (%s), end %lu\n", i, outfilename, end);



    /*M
      Read in the input file
      
      Read while current < end or till the end of the file if it's the last track.
    **/
    while ((current < end) || (i == (cuefile.track_number - 1))) {
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
      } else {
	fprintf(stderr, "Could not read the next frame from the mp3 file...\n");
	break;
      }
    }

    /*M
      Close the output file.
    **/
    file_close(&outfile);
    aq_destroy(&qout);

    fprintf(stderr, "%s written\n", outfilename);
  }

  /*M
    Close the input file.
  **/
  file_close(&mp3file);
  aq_destroy(&qin);
    

  /*M
    Cleanup the data structures.
  **/
 exit:
  if (cuein != NULL)
    fclose(cuein);
  if (cuetracks != NULL)
    free(cuetracks);
  
  if (cuefilename != NULL)
    free(cuefilename);
  if (mp3filename != NULL)
    free(mp3filename);

  return retval;
}

/*M
**/
