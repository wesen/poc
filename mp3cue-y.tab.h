/* A Bison parser, made by GNU Bison 1.875b.  */

/* Skeleton parser for Yacc-like parsing with Bison,
   Copyright (C) 1984, 1989, 1990, 2000, 2001, 2002, 2003 Free Software Foundation, Inc.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2, or (at your option)
   any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place - Suite 330,
   Boston, MA 02111-1307, USA.  */

/* As a special exception, when this file is copied by Bison into a
   Bison output file, you may use that output file without restriction.
   This special exception was added by the Free Software Foundation
   in version 1.24 of Bison.  */

/* Tokens.  */
#ifndef YYTOKENTYPE
# define YYTOKENTYPE
   /* Put the tokens into the symbol table, so that GDB and other debuggers
      know about them.  */
   enum yytokentype {
     NUMBER = 258,
     NEWLINE = 259,
     STRING = 260,
     PERFORMER = 261,
     FILEID = 262,
     INDEX = 263,
     TRACK = 264,
     AUDIO = 265,
     COLON = 266,
     TITLE = 267,
     ISRC = 268,
     CATALOG = 269
   };
#endif
#define NUMBER 258
#define NEWLINE 259
#define STRING 260
#define PERFORMER 261
#define FILEID 262
#define INDEX 263
#define TRACK 264
#define AUDIO 265
#define COLON 266
#define TITLE 267
#define ISRC 268
#define CATALOG 269




#if ! defined (YYSTYPE) && ! defined (YYSTYPE_IS_DECLARED)
#line 24 "mp3cue.y"
typedef union YYSTYPE {
  int number;
  char string[MP3CUE_MAX_STRING_LENGTH + 1];
} YYSTYPE;
/* Line 1252 of yacc.c.  */
#line 70 "mp3cue-y.tab.h"
# define yystype YYSTYPE /* obsolescent; will be withdrawn */
# define YYSTYPE_IS_DECLARED 1
# define YYSTYPE_IS_TRIVIAL 1
#endif

extern YYSTYPE yylval;



