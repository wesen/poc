%option noyywrap

%{
#include <string.h>
#include "mp3cue.h"
#include "mp3cue-y.tab.h"

extern int lc;
%}

%%
[0-9]+ { yylval.number = atoi(yytext); return NUMBER; }
[ \t] ;
\r\n { lc++; return NEWLINE; }
\n { lc++; return NEWLINE; }
\"[^\n\"]+\" { int len = strlen(yytext) - 1;
  if (len > 0)
    yytext[len] = 0;
  
  strncpy(yylval.string, yytext+1, MP3CUE_MAX_STRING_LENGTH);

  return STRING;
}
"PERFORMER" return PERFORMER;
"FILE" return FILEID;
"TRACK" return TRACK;
"INDEX" return INDEX;
"AUDIO" return AUDIO;
"TITLE" return TITLE;
"CATALOG" return CATALOG;
"ISRC" return ISRC;
[a-zA-Z0-9]+ {
  strncpy(yylval.string, yytext, MP3CUE_MAX_STRING_LENGTH);
  return STRING;
}
: return COLON;

%%
