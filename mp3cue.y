%{
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include "mp3cue.h"

int yylex();
  
extern mp3cue_file_t *yymp3_cue_file;

int lc = 0;
 
void yyerror (s)
     char *s;
{
  fprintf (stderr, "Parse error on line %d: %s\n", lc, s);
  exit(1);
}  
 %}

%token NUMBER NEWLINE STRING PERFORMER FILEID INDEX TRACK
%token AUDIO COLON TITLE
%token ISRC CATALOG

%union
{
  int number;
  char string[MP3CUE_MAX_STRING_LENGTH + 1];
}

%token <number> NUMBER
%token <string> STRING

%%

file: fileinformationlist tracklist 
| fileinformationlist  tracklist fileend
;

fileend: NEWLINE
| NEWLINE fileend
;

fileinformationlist: fileinformation NEWLINE
| fileinformation NEWLINE fileinformationlist
;

fileinformation: fileperformer 
| filetitle 
| fileid
| filecatalog
| restblubber
;

filecatalog: CATALOG NUMBER 
;

fileperformer: PERFORMER STRING  {
  strncpy(yymp3_cue_file->performer, $2, MP3CUE_MAX_STRING_LENGTH);
}
;

filetitle: TITLE STRING  {
  strncpy(yymp3_cue_file->title, $2, MP3CUE_MAX_STRING_LENGTH);
}
;

fileid: FILEID STRING STRING
;

restblubber: STRING { }
| STRING restblubber { }
;

tracklist: track tracklist
| track
| error { yyerrok; }
;

track: trackheader NEWLINE trackinformationlist {
  if (++yymp3_cue_file->track_number >= yymp3_cue_file->max_track_number) {
    int tracks = yymp3_cue_file->max_track_number * 2;
    mp3cue_track_t *oldtracks = yymp3_cue_file->tracks;
    if (!(yymp3_cue_file->tracks = realloc(yymp3_cue_file->tracks, tracks * sizeof(mp3cue_track_t)))) {
      fprintf(stderr, "Could not allocate memory for more tracks!\n");
      // XXX error
      yymp3_cue_file->tracks = oldtracks;
    } else {
       yymp3_cue_file->max_track_number = tracks;
    }
  }

}
;

trackinformationlist: trackinformation NEWLINE
| trackinformation NEWLINE trackinformationlist
;

trackinformation: tracktitle
| trackperformer
| trackindex
| trackisrc
| restblubber
//| error { yyerrok; }
;


trackheader: TRACK NUMBER AUDIO  {
  yymp3_cue_file->tracks[yymp3_cue_file->track_number].number = $2;
  yymp3_cue_file->tracks[yymp3_cue_file->track_number].type = mp3cue_audio;
  yymp3_cue_file->tracks[yymp3_cue_file->track_number].title[0] = '\0';
  yymp3_cue_file->tracks[yymp3_cue_file->track_number].performer[0] = '\0';
  yymp3_cue_file->tracks[yymp3_cue_file->track_number].index.minutes = 0;
  yymp3_cue_file->tracks[yymp3_cue_file->track_number].index.seconds = 0;
  yymp3_cue_file->tracks[yymp3_cue_file->track_number].index.centiseconds = 0;
}
;

trackisrc: ISRC STRING
;

tracktitle: TITLE STRING {
  strncpy(yymp3_cue_file->tracks[yymp3_cue_file->track_number].title,
          $2, MP3CUE_MAX_STRING_LENGTH);
}
;

trackperformer: PERFORMER STRING  {
  strncpy(yymp3_cue_file->tracks[yymp3_cue_file->track_number].performer,
          $2, MP3CUE_MAX_STRING_LENGTH);
}
;

trackindex: INDEX NUMBER NUMBER COLON NUMBER COLON NUMBER  {
  if ($2 == 1) {
    if (yymp3_cue_file->track_number > 0) {
      yymp3_cue_file->tracks[yymp3_cue_file->track_number-1].index.minutes = $3;
      yymp3_cue_file->tracks[yymp3_cue_file->track_number-1].index.seconds = $5;
      yymp3_cue_file->tracks[yymp3_cue_file->track_number-1].index.centiseconds = $7;
    }
  }
}
;

%%
