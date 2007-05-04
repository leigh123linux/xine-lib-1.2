#include "config.h"

#ifdef HAVE_STDLIB_H
#include <stdlib.h>
#endif

#include <string.h>

char *xine_private_strndup(const char *s, size_t n) {
  char *ret;
  
  ret = malloc (n + 1);
  strncpy(ret, s, n);
  ret[n] = '\0';
  return ret;
}
