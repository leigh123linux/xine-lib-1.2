/*
 *  Copyright (C) 2002-2003,2007 the xine project
 *
 *  This file is part of xine, a free video player.
 *
 * The xine-lib XML parser is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * The xine-lib XML parser is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with the Gnome Library; see the file COPYING.LIB.  If not,
 * write to the Free Software Foundation, Inc., 51 Franklin Street, Fifth
 * Floor, Boston, MA 02110, USA
 */

#define LOG_MODULE "xmllexer"
#define LOG_VERBOSE
/*
#define LOG
*/

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#ifdef XINE_COMPILE
#include "xineutils.h"
#else
#define lprintf(...)
#endif
#include "xmllexer.h"
#include <stdio.h>
#include <ctype.h>
#include <string.h>
#include <stdlib.h>
#ifdef HAVE_ICONV
#include <iconv.h>
#endif

#include "bswap.h"

/* private constants*/
#define NORMAL       0  /* normal lex mode */
#define DATA         1  /* data lex mode */

/* private global variables */
struct lexer * static_lexer;

enum utf { UTF32BE, UTF32LE, UTF16BE, UTF16LE };

static void lex_convert (struct lexer * lexer, const char * buf, int size, enum utf utf)
{
  char *utf8 = malloc (size * (utf >= UTF16BE ? 3 : 6) + 1);
  char *bp = utf8;
  while (size > 0)
  {
    uint32_t c = 0;
    switch (utf)
    {
    case UTF32BE: c = _X_BE_32 (buf); buf += 4; break;
    case UTF32LE: c = _X_LE_32 (buf); buf += 4; break;
    case UTF16BE: c = _X_BE_16 (buf); buf += 2; break;
    case UTF16LE: c = _X_LE_16 (buf); buf += 2; break;
    }
    if (!c)
      break; /* embed a NUL, get a truncated string */
    if (c < 128)
      *bp++ = c;
    else
    {
      int count = (c >= 0x04000000) ? 5 :
		  (c >= 0x00200000) ? 4 :
		  (c >= 0x00010000) ? 3 :
		  (c >= 0x00000800) ? 2 : 1;
      *bp = (char)(0x1F80 >> count);
      count *= 6;
      *bp++ |= c >> count;
      while ((count -= 6) >= 0)
	*bp++ = 128 | ((c >> count) & 0x3F);
    }
  }
  *bp = 0;
  lexer->lexbuf_size = bp - utf8;
  lexer->lexbuf = lexer->lex_malloc = realloc (utf8, lexer->lexbuf_size + 1);
}

/* for ABI compatibility */
void lexer_init(const char * buf, int size) {
  if (static_lexer) {
    lexer_finalize_r(static_lexer);
  }
  static_lexer = lexer_init_r(buf, size);
}

struct lexer *lexer_init_r(const char * buf, int size) {
  static const char boms[] = { 0xFF, 0xFE, 0, 0, 0xFE, 0xFF },
		    bom_utf8[] = { 0xEF, 0xBB, 0xBF };
  struct lexer * lexer = calloc (1, sizeof (*lexer));

  lexer->lexbuf      = buf;
  lexer->lexbuf_size = size;

  if (size >= 4 && !memcmp (buf, boms + 2, 4))
    lex_convert (lexer, buf + 4, size - 4, UTF32BE);
  else if (size >= 4 && !memcmp (buf, boms, 4))
    lex_convert (lexer, buf + 4, size - 4, UTF32LE);
  else if (size >= 3 && !memcmp (buf, bom_utf8, 3))
  {
    lexer->lexbuf += 3;
    lexer->lexbuf_size -= 3;
  }
  else if (size >= 2 && !memcmp (buf, boms + 4, 2))
    lex_convert (lexer, buf + 2, size - 2, UTF16BE);
  else if (size >= 2 && !memcmp (buf, boms, 2))
    lex_convert (lexer, buf + 2, size - 2, UTF16LE);

  lexer->lexbuf_pos  = 0;
  lexer->lex_mode    = NORMAL;
  lexer->in_comment  = 0;

  lprintf("buffer length %d\n", size);
  return lexer;
}

void lexer_finalize_r(struct lexer * lexer)
{
  free(lexer->lex_malloc);
  free(lexer);
}

/* for ABI compatibility */
int lexer_get_token_d(char ** _tok, int * _tok_size, int fixed) {
  return lexer_get_token_d_r(static_lexer, _tok, _tok_size, fixed);
}

int lexer_get_token_d_r(struct lexer * lexer, char ** _tok, int * _tok_size, int fixed) {
  char *tok = *_tok;
  int tok_size = *_tok_size;

  int tok_pos = 0;
  int state = 0;
  char c;

  if (tok) {
    while ((tok_pos < tok_size) && (lexer->lexbuf_pos < lexer->lexbuf_size)) {
      c = lexer->lexbuf[lexer->lexbuf_pos];
      lprintf("c=%c, state=%d, in_comment=%d\n", c, state, lexer->in_comment);

      if (lexer->lex_mode == NORMAL) {
				/* normal mode */
	switch (state) {
	  /* init state */
	case 0:
	  switch (c) {
	  case '\n':
	  case '\r':
	    state = 1;
	    tok[tok_pos] = c;
	    tok_pos++;
	    break;

	  case ' ':
	  case '\t':
	    state = 2;
	    tok[tok_pos] = c;
	    tok_pos++;
	    break;

	  case '<':
	    state = 3;
	    tok[tok_pos] = c;
	    tok_pos++;
	    break;

	  case '>':
	    state = 4;
	    tok[tok_pos] = c;
	    tok_pos++;
	    break;

	  case '/':
	    if (!lexer->in_comment)
	      state = 5;
	    tok[tok_pos] = c;
	    tok_pos++;
	    break;

	  case '=':
	    state = 6;
	    tok[tok_pos] = c;
	    tok_pos++;
	    break;

	  case '\"': /* " */
	    state = 7;
	    break;

	  case '\'': /* " */
	    state = 12;
	    break;

	  case '-':
	    state = 10;
	    tok[tok_pos] = c;
	    tok_pos++;
	    break;

	  case '?':
	    state = 9;
	    tok[tok_pos] = c;
	    tok_pos++;
	    break;

	  default:
	    state = 100;
	    tok[tok_pos] = c;
	    tok_pos++;
	    break;
	  }
	  lexer->lexbuf_pos++;
	  break;

	  /* end of line */
	case 1:
	  if (c == '\n' || (c == '\r')) {
	    tok[tok_pos] = c;
	    lexer->lexbuf_pos++;
	    tok_pos++;
	  } else {
	    tok[tok_pos] = '\0';
	    return T_EOL;
	  }
	  break;

	  /* T_SEPAR */
	case 2:
	  if (c == ' ' || (c == '\t')) {
	    tok[tok_pos] = c;
	    lexer->lexbuf_pos++;
	    tok_pos++;
	  } else {
	    tok[tok_pos] = '\0';
	    return T_SEPAR;
	  }
	  break;

	  /* T_M_START < or </ or <! or <? */
	case 3:
	  switch (c) {
	  case '/':
	    tok[tok_pos] = c;
	    lexer->lexbuf_pos++;
	    tok_pos++; /* FIXME */
	    tok[tok_pos] = '\0';
	    return T_M_START_2;
	    break;
	  case '!':
	    tok[tok_pos] = c;
	    lexer->lexbuf_pos++;
	    tok_pos++;
	    state = 8;
	    break;
	  case '?':
	    tok[tok_pos] = c;
	    lexer->lexbuf_pos++;
	    tok_pos++; /* FIXME */
	    tok[tok_pos] = '\0';
	    return T_TI_START;
	    break;
	  default:
	    tok[tok_pos] = '\0';
	    return T_M_START_1;
	  }
	  break;

	  /* T_M_STOP_1 */
	case 4:
	  tok[tok_pos] = '\0';
	  if (!lexer->in_comment)
	    lexer->lex_mode = DATA;
	  return T_M_STOP_1;
	  break;

	  /* T_M_STOP_2 */
	case 5:
	  if (c == '>') {
	    tok[tok_pos] = c;
	    lexer->lexbuf_pos++;
	    tok_pos++; /* FIXME */
	    tok[tok_pos] = '\0';
	    if (!lexer->in_comment)
	      lexer->lex_mode = DATA;
	    return T_M_STOP_2;
	  } else {
	    tok[tok_pos] = '\0';
	    return T_ERROR;
	  }
	  break;

	  /* T_EQUAL */
	case 6:
	  tok[tok_pos] = '\0';
	  return T_EQUAL;
	  break;

	  /* T_STRING */
	case 7:
	  tok[tok_pos] = c;
	  lexer->lexbuf_pos++;
	  if (c == '\"') { /* " */
	    tok[tok_pos] = '\0'; /* FIXME */
	    return T_STRING;
	  }
	  tok_pos++;
	  break;

	  /* T_C_START or T_DOCTYPE_START */
	case 8:
	  switch (c) {
	  case '-':
	    lexer->lexbuf_pos++;
	    if (lexer->lexbuf[lexer->lexbuf_pos] == '-')
	      {
		lexer->lexbuf_pos++;
		tok[tok_pos++] = '-'; /* FIXME */
		tok[tok_pos++] = '-';
		tok[tok_pos] = '\0';
		lexer->in_comment = 1;
		return T_C_START;
	      }
	    break;
	  case 'D':
	    lexer->lexbuf_pos++;
	    if (strncmp(lexer->lexbuf + lexer->lexbuf_pos, "OCTYPE", 6) == 0) {
	      strncpy(tok + tok_pos, "DOCTYPE", 7); /* FIXME */
	      lexer->lexbuf_pos += 6;
	      return T_DOCTYPE_START;
	    } else {
	      return T_ERROR;
	    }
	    break;
	  default:
	    /* error */
	    return T_ERROR;
	  }
	  break;

	  /* T_TI_STOP */
	case 9:
	  if (c == '>') {
	    tok[tok_pos] = c;
	    lexer->lexbuf_pos++;
	    tok_pos++; /* FIXME */
	    tok[tok_pos] = '\0';
	    return T_TI_STOP;
	  } else {
	    tok[tok_pos] = '\0';
	    return T_ERROR;
	  }
	  break;

	  /* -- */
	case 10:
	  switch (c) {
	  case '-':
	    tok[tok_pos] = c;
	    tok_pos++;
	    lexer->lexbuf_pos++;
	    state = 11;
	    break;
	  default:
	    tok[tok_pos] = c;
	    tok_pos++;
	    lexer->lexbuf_pos++;
	    state = 100;
	  }
	  break;

	  /* --> */
	case 11:
	  switch (c) {
	  case '>':
	    tok[tok_pos] = c;
	    tok_pos++;
	    lexer->lexbuf_pos++;
	    tok[tok_pos] = '\0'; /* FIX ME */
	    if (strlen(tok) != 3) {
	      tok[tok_pos - 3] = '\0';
	      lexer->lexbuf_pos -= 3;
	      return T_IDENT;
	    } else {
	      lexer->in_comment = 0;
	      return T_C_STOP;
	    }
	    break;
	  default:
	    tok[tok_pos] = c;
	    tok_pos++;
	    lexer->lexbuf_pos++;
	    state = 100;
	  }
	  break;

	  /* T_STRING (single quotes) */
	case 12:
	  tok[tok_pos] = c;
	  lexer->lexbuf_pos++;
	  if (c == '\'') { /* " */
	    tok[tok_pos] = '\0'; /* FIXME */
	    return T_STRING;
	  }
	  tok_pos++;
	  break;

	  /* IDENT */
	case 100:
	  switch (c) {
	  case '<':
	  case '>':
	  case '\\':
	  case '\"': /* " */
	  case ' ':
	  case '\t':
	  case '\n':
	  case '\r':
	  case '=':
	  case '/':
	    tok[tok_pos] = '\0';
	    return T_IDENT;
	    break;
	  case '?':
	    tok[tok_pos] = c;
	    tok_pos++;
	    lexer->lexbuf_pos++;
	    state = 9;
	    break;
	  case '-':
	    tok[tok_pos] = c;
	    tok_pos++;
	    lexer->lexbuf_pos++;
	    state = 10;
	    break;
	  default:
	    tok[tok_pos] = c;
	    tok_pos++;
	    lexer->lexbuf_pos++;
	  }
	  break;
	default:
	  lprintf("expected char \'%c\'\n", tok[tok_pos - 1]); /* FIX ME */
	  return T_ERROR;
	}
      } else {
				/* data mode, stop if char equal '<' */
        switch (c)
        {
        case '<':
	  tok[tok_pos] = '\0';
	  lexer->lex_mode = NORMAL;
	  return T_DATA;
	default:
	  tok[tok_pos] = c;
	  tok_pos++;
	  lexer->lexbuf_pos++;
	}
      }
    }
    lprintf ("loop done tok_pos = %d, tok_size=%d, lexbuf_pos=%d, lexbuf_size=%d\n",
	     tok_pos, tok_size, lexer->lexbuf_pos, lexer->lexbuf_size);

    /* pb */
    if (tok_pos >= tok_size) {
      if (fixed)
        return T_ERROR;
      *_tok_size *= 2;
      *_tok = realloc (*_tok, *_tok_size);
      lprintf("token buffer is too small\n");
      lprintf("increasing buffer size to %d bytes\n", *_tok_size);
      if (*_tok) {
          return lexer_get_token_d_r (lexer, _tok, _tok_size, 0);
      } else {
          return T_ERROR;
      }
    } else {
      if (lexer->lexbuf_pos >= lexer->lexbuf_size) {
				/* Terminate the current token */
	tok[tok_pos] = '\0';
	switch (state) {
	case 0:
	case 1:
	case 2:
	  return T_EOF;
	  break;
	case 3:
	  return T_M_START_1;
	  break;
	case 4:
	  return T_M_STOP_1;
	  break;
	case 5:
	  return T_ERROR;
	  break;
	case 6:
	  return T_EQUAL;
	  break;
	case 7:
	  return T_STRING;
	  break;
	case 100:
	  return T_DATA;
	  break;
	default:
	  lprintf("unknown state, state=%d\n", state);
	}
      } else {
	lprintf("abnormal end of buffer, state=%d\n", state);
      }
    }
    return T_ERROR;
  }
  /* tok == null */
  lprintf("token buffer is null\n");
  return T_ERROR;
}

/* for ABI compatibility */
int lexer_get_token (char *tok, int tok_size)
{
  return lexer_get_token_d (&tok, &tok_size, 1);
}

static struct {
  char code;
  unsigned char namelen;
  char name[6];
} lexer_entities[] = {
  { '"',  4, "quot" },
  { '&',  3, "amp" },
  { '\'', 4, "apos" },
  { '<',  2, "lt" },
  { '>',  2, "gt" },
  { '\0', 0, "" }
};

char *lexer_decode_entities (const char *tok)
{
  char *buf = calloc (strlen (tok) + 1, sizeof(char));
  char *bp = buf;
  char c;

  while ((c = *tok++))
  {
    if (c != '&')
      *bp++ = c;
    else
    {
      /* parse the character entity (on failure, treat it as literal text) */
      const char *tp = tok;
      signed long i;

      for (i = 0; lexer_entities[i].code; ++i)
	if (!strncmp (lexer_entities[i].name, tok, lexer_entities[i].namelen)
	    && tok[lexer_entities[i].namelen] == ';')
	  break;
      if (lexer_entities[i].code)
      {
        tok += lexer_entities[i].namelen + 1;
	*bp++ = lexer_entities[i].code;
	continue;
      }

      if (*tp++ != '#')
      {
        /* not a recognised name and not numeric */
	*bp++ = '&';
	continue;
      }

      /* entity is a number
       * (note: strtol() allows "0x" prefix for hexadecimal, but we don't)
       */
      if (*tp == 'x' && tp[1] && tp[2] != 'x')
	i = strtol (tp + 1, (char **)&tp, 16);
      else
	i = strtol (tp, (char **)&tp, 10);

      if (*tp != ';' || i < 1)
      {
        /* out of range, or format error */
	*bp++ = '&';
	continue;
      }

      tok = tp + 1;

      if (i < 128)
        /* ASCII - store as-is */
	*bp++ = i;
      else
      {
	/* Non-ASCII, so convert to UTF-8 */
	int count = (i >= 0x04000000) ? 5 :
		    (i >= 0x00200000) ? 4 :
		    (i >= 0x00010000) ? 3 :
		    (i >= 0x00000800) ? 2 : 1;
	*bp = (char)(0x1F80 >> count);
	count *= 6;
	*bp++ |= i >> count;
	while ((count -= 6) >= 0)
	  *bp++ = 128 | ((i >> count) & 0x3F);
      }
    }
  }
  *bp = 0;
  return buf;
}
