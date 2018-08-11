/*      $OpenBSD: strlcpy.c,v 1.11 2006/05/05 15:27:38 millert Exp $    */

/*
 * Copyright (c) 1998 Todd C. Miller <Todd.Miller@courtesan.com>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 *
 * x86 asm version by Torsten Jager <t.jager@gmx.de>
 */

#include "config.h"

#include <sys/types.h>
#include <string.h>

/*
 * Copy src to string dst of size siz.  At most siz-1 characters
 * will be copied.  Always NUL terminates (unless siz == 0).
 * Returns strlen(src); if retval >= siz, truncation occurred.
 */
size_t
xine_private_strlcpy(char *dst, const char *src, size_t siz)
{

#if defined(ARCH_X86) && (defined(__GNUC__) || defined(__clang__))

  size_t a;

#if defined(ARCH_X86_32)
#  define DO_P(what) what"l"
#  define REG_P(reg) "%"reg
#elif defined(ARCH_X86_X32)
#  define DO_P(what) what"q"
#  define REG_P(reg) "%q"reg
#else /* ARCH_X86_64 */
#  define DO_P(what) what"q"
#  define REG_P(reg) "%"reg
#endif

  __asm__ __volatile__ (
    "cld"
    "\n\ttestl\t%%ecx, %%ecx"
    "\n\t"DO_P("mov")"\t"REG_P("0")", "REG_P("3")
    "\n\tje\t3f"
    "\n1:"
    "\n\ttestb\t$255, ("REG_P("0")")"
    "\n\tmovsb"
    "\n\tje\t4f"
    "\n\tsubl\t$1, %%ecx"
    "\n\tje\t2f"
    "\n\ttestb\t$255, ("REG_P("0")")"
    "\n\tmovsb"
    "\n\tje\t4f"
    "\n\tsubl\t$1, %%ecx"
    "\n\tjne\t1b"
    "\n2:"
    "\n\tmovb\t$0, -1("REG_P("1")")"
    "\n3:"
    "\n\ttestb\t$255, ("REG_P("0")")"
    "\n\t"DO_P("lea")"\t1("REG_P("0")"), "REG_P("0")
    "\n\tje\t4f"
    "\n\ttestb\t$255, ("REG_P("0")")"
    "\n\t"DO_P("lea")"\t1("REG_P("0")"), "REG_P("0")
    "\n\tjne\t3b"
    "\n4:"
    "\n\t"DO_P("neg")"\t"REG_P("3")
    "\n\t"DO_P("lea")"\t-1("REG_P("0")","REG_P("3")"), "REG_P("3")
    : "=S" (src), "=D" (dst), "=c" (siz), "=a" (a)
    : "0"  (src), "1"  (dst), "2"  (siz)
    : "cc", "memory"
  );
  return a;

#else

  char *d = dst;
  const char *s = src;
  size_t n = siz;

  /* Copy as many bytes as will fit */
  if (n != 0) {
    while (--n != 0) {
      if ((*d++ = *s++) == '\0')
        break;
    }
  }

  /* Not enough room in dst, add NUL and traverse rest of src */
  if (n == 0) {
    if (siz != 0)
      *d = '\0';              /* NUL-terminate dst */
    while (*s++)
      ;
  }

  return(s - src - 1);    /* count does not include NUL */

#endif
}

#ifdef TEST_THIS_FILE
#include <stdio.h>
int main (void) {
  char b[128];
  unsigned int u;
  u = xine_private_strlcpy (b, "wrong wrung wrang", 128);
  printf ("xine_private_strlcpy (b, \"wrong wrung wrang\", 128) = (\"%s\", %u)\n", b, u);
  u = xine_private_strlcpy (b, "wrong wrung wrang", 0);
  printf ("xine_private_strlcpy (b, \"wrong wrung wrang\", 0) = (\"%s\", %u)\n", b, u);
  u = xine_private_strlcpy (b, "wrong wrung wrang", 17);
  printf ("xine_private_strlcpy (b, \"wrong wrung wrang\", 17) = (\"%s\", %u)\n", b, u);
  u = xine_private_strlcpy (b, "wrong wrung wrang", 10);
  printf ("xine_private_strlcpy (b, \"wrong wrung wrang\", 10) = (\"%s\", %u)\n", b, u);
  u = xine_private_strlcpy (b, "wrong wrung wrang", 1);
  printf ("xine_private_strlcpy (b, \"wrong wrung wrang\", 1) = (\"%s\", %u)\n", b, u);
  return 0;
}
#endif

