/*C
  (c) 2003 bl0rg crew
**/

#ifndef MP3CUE_H__
#define MP3CUE_H__

/*M
  \emph{Arbitrary maximal length for strings in a CUE file.}

  If you don't like it, change it.
**/
#define MP3CUE_MAX_STRING_LENGTH 64

/*M
  \emph{CUE index structure.}

  This is reverse engineered and by no means correct. I also don't
  know what the correct name for ``centiseconds'' is.
**/
typedef struct mp3cue_index_s {
  unsigned int minutes;
  unsigned int seconds;
  unsigned int centiseconds;
} mp3cue_index_t;

/*M
  \emph{CUE track type.}

  Reverse engineered.
**/
typedef enum mp3cue_track_type_e {
  mp3cue_audio = 0
} mp3cue_track_type_t;

/*M
  \emph{CUE track structure.}

  Reverse engineered.
**/
typedef struct mp3cue_track_s {
  mp3cue_track_type_t type;
  int                 number;
  char                title[MP3CUE_MAX_STRING_LENGTH + 1];
  char                performer[MP3CUE_MAX_STRING_LENGTH + 1];
  mp3cue_index_t      index;
} mp3cue_track_t;

/*M
  \emph{CUE file structure.}

  Reverse engineered.
**/
typedef struct mp3cue_file_s {
  char           performer[MP3CUE_MAX_STRING_LENGTH + 1];
  char           title[MP3CUE_MAX_STRING_LENGTH + 1];
  unsigned int   track_number;
  unsigned int   max_track_number;
  mp3cue_track_t *tracks;
} mp3cue_file_t;

#define MP3CUE_DEFAULT_TRACK_NUMBER 20

#endif /* MP3CUE_H__ */
