/*
 * written for xine project, 2004-2006
 *
 * public domain replacement function for strtok_r()
 *
 */

#include "config.h"

#include <stddef.h>
#include <string.h>

char *xine_private_strtok_r(char *s, const char *delim, char **ptrptr) {
  char *next;
  size_t toklen, cutlen;

  /* first or next call */
  if (s) *ptrptr = s;
  else s = *ptrptr;

  /* end of searching */
  if (!s || s == '\0') return NULL;

  /* cut the initial garbage */
  cutlen = strspn(s, delim);
  s = s + cutlen;

  /* pointer before next token */
  if ((toklen = strcspn(s, delim)) == 0) {
    *ptrptr = NULL;
    return NULL;
  }
  next = s + toklen;

  /* cut current token */
  *next = '\0';

  /* prepare next call */
  *ptrptr = next + 1;

  /* return the token */
  return s;
}
