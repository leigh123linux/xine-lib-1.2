/*
 * Copyright (C) 2001-2017 the xine project
 *
 * This file is part of xine, a free video player.
 *
 * xine is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * xine is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110, USA
 *
 * These are the MMX/MMX2/SSE optimized versions of memcpy
 *
 * This code was adapted from Linux Kernel sources by Nick Kurshev to
 * the mplayer program. (http://mplayer.sourceforge.net)
 *
 * Miguel Freitas split the #ifdefs into several specialized functions that
 * are benchmarked at runtime by xine. Some original comments from Nick
 * have been preserved documenting some MMX/SSE oddities.
 * Also added kernel memcpy function that seems faster than libc one.
 *
 */

#define XINE_ENGINE_INTERNAL

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#if defined (ARCH_PPC) && !defined (HOST_OS_DARWIN)
#include "ppcasm_string.h"
#endif

#ifdef HAVE_SYS_TIMES_H
#include <sys/times.h>
#else
#include <time.h>
#endif

#include <stdlib.h>
#include <string.h>

#define LOG_MODULE "memcpy"
#define LOG_VERBOSE
/*
#define LOG
*/

#include <xine/xine_internal.h>
#include "../xine-engine/xine_private.h"

void *(* xine_fast_memcpy)(void *to, const void *from, size_t len) = NULL;

/* Original comments from mplayer (file: aclib.c)
 This part of code was taken by me from Linux-2.4.3 and slightly modified
for MMX, MMX2, SSE instruction set. I have done it since linux uses page aligned
blocks but mplayer uses weakly ordered data and original sources can not
speedup them. Only using PREFETCHNTA and MOVNTQ together have effect!

>From IA-32 Intel Architecture Software Developer's Manual Volume 1,

Order Number 245470:
"10.4.6. Cacheability Control, Prefetch, and Memory Ordering Instructions"

Data referenced by a program can be temporal (data will be used again) or
non-temporal (data will be referenced once and not reused in the immediate
future). To make efficient use of the processor's caches, it is generally
desirable to cache temporal data and not cache non-temporal data. Overloading
the processor's caches with non-temporal data is sometimes referred to as
"polluting the caches".
The non-temporal data is written to memory with Write-Combining semantics.

The PREFETCHh instructions permits a program to load data into the processor
at a suggested cache level, so that it is closer to the processors load and
store unit when it is needed. If the data is already present in a level of
the cache hierarchy that is closer to the processor, the PREFETCHh instruction
will not result in any data movement.
But we should you PREFETCHNTA: Non-temporal data fetch data into location
close to the processor, minimizing cache pollution.

The MOVNTQ (store quadword using non-temporal hint) instruction stores
packed integer data from an MMX register to memory, using a non-temporal hint.
The MOVNTPS (store packed single-precision floating-point values using
non-temporal hint) instruction stores packed floating-point data from an
XMM register to memory, using a non-temporal hint.

The SFENCE (Store Fence) instruction controls write ordering by creating a
fence for memory store operations. This instruction guarantees that the results
of every store instruction that precedes the store fence in program order is
globally visible before any store instruction that follows the fence. The
SFENCE instruction provides an efficient way of ensuring ordering between
procedures that produce weakly-ordered data and procedures that consume that
data.

If you have questions please contact with me: Nick Kurshev: nickols_k@mail.ru.
*/

/*  mmx v.1 Note: Since we added alignment of destinition it speedups
    of memory copying on PentMMX, Celeron-1 and P2 upto 12% versus
    standard (non MMX-optimized) version.
    Note: on K6-2+ it speedups memory copying upto 25% and
          on K7 and P3 about 500% (5 times).
*/

/* Additional notes on gcc assembly and processors: [MF]
prefetch is specific for AMD processors, the intel ones should be
prefetch0, prefetch1, prefetch2 which are not recognized by my gcc.
prefetchnta is supported both on athlon and pentium 3.

therefore i will take off prefetchnta instructions from the mmx1 version
to avoid problems on pentium mmx and k6-2.

quote of the day:
"Using prefetches efficiently is more of an art than a science"
*/


#if defined(ARCH_X86)

#if defined(ARCH_X86_64)
#  define DOPTR(what,from,to) "\n\t"what"q\t"from", "to
#  define BUMPPTR(offs,num)   "\n\tleaq\t"offs"(%"num"), %"num
#  define MEM1(reg)           "(%"reg")"
#  define MEM2(offs,reg)      offs"(%"reg")"
#elif defined(ARCH_X86_X32)
   /* Mapping a pointer to a register yields a 32bit move,
    * which does extend to 64bit properly. Dereferencing as
    * 32bit (%esi) will work but takes an extra 1 byte prefix,
    * so lets use 64bit (%rsi) instead.
    * Inserting an extra letter into operand reference modifies
    * register size something like this:
    * "%2, %b2, %h2, %w2, %l2, %q2" -> "%edx, %dl, %dh, %dx, %edx, %rdx"
    * For some reason, "info gcc" silences this useful feature.
    */
#  define DOPTR(what,from,to) "\n\t"what"l\t"from", "to
#  define BUMPPTR(offs,num)   "\n\tleaq\t"offs"(%q"num"), %q"num
#  define MEM1(reg)           "(%q"reg")"
#  define MEM2(offs,reg)      offs"(%q"reg")"
#else
#  define DOPTR(what,from,to) "\n\t"what"l\t"from", "to
#  define BUMPPTR(offs,num)   "\n\tleal\t"offs"(%"num"), %"num
#  define MEM1(reg)           "(%"reg")"
#  define MEM2(offs,reg)      offs"(%"reg")"
#endif

#ifndef _MSC_VER

/* for small memory blocks (<256 bytes) this version is faster */
/* rep_movsb (void *, const void *, uintptr_t)                 */
#define rep_movsb(to,from,n)\
  __asm__ __volatile__ (\
    "cld\n\trep movsb"\
    : "=D" (to), "=S" (from), "=c" (n)\
    : "0"  (to),  "1" (from), "2"  (n)\
    : "cc", "memory"\
  )

/* idea from linux kernel __memcpy (from: /include/asm/string.h) */
/* then, added target alignment and 64bit version.               */
static __inline__ void *linux_kernel_memcpy_impl (void *to, const void *from, size_t n) {
  void *ret = to;
  uintptr_t s = n;
  if (s < 16) {
    if (s)
      rep_movsb (to, from, s);
  } else {
    uintptr_t d;
    __asm__ __volatile__ (
      "cld"
      DOPTR("mov","%1","%3")
      "\n\ttestb\t$1, %b3"
      "\n\tje\t1f"
      "\n\tmovsb"
      DOPTR("sub","$1","%2")
      DOPTR("mov","%1","%3")
      "\n1:"
      "\n\ttestb\t$2, %b3"
      "\n\tje\t2f"
      "\n\tmovsw"
      DOPTR("sub","$2","%2")
      DOPTR("mov","%1","%3")
      "\n2:"
#if defined(ARCH_X86_64) || defined(ARCH_X86_X32)
      "\n\ttestb\t$4, %b3"
      "\n\tje\t3f"
      "\n\tmovsl"
      DOPTR("sub","$4","%2")
      "\n3:"
      DOPTR("mov","%2","%3")
      DOPTR("shr","$3","%2")
      "\n\trep movsq"
      "\n\ttestb\t$4, %b3"
      "\n\tje\t4f"
      "\n\tmovsl"
      "\n4:"
#else
      DOPTR("mov","%2","%3")
      DOPTR("shr","$2","%2")
      "\n\trep movsl"
#endif
      "\n\ttestb\t$2, %b3"
      "\n\tje\t5f"
      "\n\tmovsw"
      "\n5:"
      "\n\ttestb\t$1, %b3"
      "\n\tje\t6f"
      "\n\tmovsb"
      "\n6:"
      : "=S" (from), "=D" (to), "=c" (s), "=&q" (d)
      : "0"  (from), "1"  (to), "2"  (s)
      : "cc", "memory"
    );
  }
  return ret;
}

#define AVX_MMREG_SIZE 32
#define SSE_MMREG_SIZE 16
#define MMX_MMREG_SIZE 8

#define MMX1_MIN_LEN 0x800  /* 2K blocks */
#define MIN_LEN 0x40  /* 64-byte blocks */

/* SSE note: i tried to move 128 bytes a time instead of 64 but it
didn't make any measureable difference. i'm using 64 for the sake of
simplicity. [MF] */
static void * sse_memcpy(void * to, const void * from, size_t len)
{
  const uint8_t *pf;
  void *retval = to;

  /* PREFETCH has effect even for MOVSB instruction ;) */
  pf = (const uint8_t *)((uintptr_t)from & ~(uintptr_t)31);
  __asm__ __volatile__ (
    "\n\tprefetchnta\t"MEM1(      "0")
    "\n\tprefetchnta\t"MEM2( "32","0")
    "\n\tprefetchnta\t"MEM2( "64","0")
    "\n\tprefetchnta\t"MEM2( "96","0")
    : : "r" (pf) );
  if (len < 128)
    return linux_kernel_memcpy_impl (to, from, len);

  __asm__ __volatile__ (
    "\n\tprefetchnta\t"MEM2("128","0")
    "\n\tprefetchnta\t"MEM2("160","0")
    "\n\tprefetchnta\t"MEM2("192","0")
    "\n\tprefetchnta\t"MEM2("224","0")
    "\n\tprefetchnta\t"MEM2("256","0")
    "\n\tprefetchnta\t"MEM2("288","0")
    "\n\tprefetchnta\t"MEM2("320","0")
    "\n\tprefetchnta\t"MEM2("352","0")
    "\n\tprefetchnta\t"MEM2("384","0")
    "\n\tprefetchnta\t"MEM2("416","0")
    "\n\tprefetchnta\t"MEM2("448","0")
    "\n\tprefetchnta\t"MEM2("480","0")
    : : "r" (pf) );
  pf += 512;
  {
    uintptr_t i;
    /* Align destinition to MMREG_SIZE -boundary */
    i = (uintptr_t)to & 15;
    if (i) {
      i = 16 - i;
      len -= i;
      rep_movsb (to, from, i);
    }
    i = len >> 6;
    if(((uintptr_t)from) & 15) {
      /* if SRC is misaligned */
      do {
        __asm__ __volatile__ (
          "\n\tprefetchnta\t"MEM1(     "2")
          "\n\tprefetchnta\t"MEM2("32","2")
          "\n\tmovups\t"MEM1(     "0")", %%xmm0"
          "\n\tmovups\t"MEM2("16","0")", %%xmm1"
          BUMPPTR("64","2")
          "\n\tmovups\t"MEM2("32","0")", %%xmm2"
          "\n\tmovups\t"MEM2("48","0")", %%xmm3"
          "\n\tmovntps\t%%xmm0, "MEM1(     "1")
          "\n\tmovntps\t%%xmm1, "MEM2("16","1")
          BUMPPTR("64","0")
          "\n\tmovntps\t%%xmm2, "MEM2("32","1")
          "\n\tmovntps\t%%xmm3, "MEM2("48","1")
          BUMPPTR("64","1")
          : "=r" (from), "=r" (to), "=r" (pf)
          : "0"  (from), "1"  (to), "2"  (pf)
          : "memory"
        );
      } while (--i);
    } else {
      /*
         Only if SRC is aligned on 16-byte boundary.
         It allows to use movaps instead of movups, which required data
         to be aligned or a general-protection exception (#GP) is generated.
      */
      do {
        __asm__ __volatile__ (
          "\n\tprefetchnta\t"MEM1(     "2")
          "\n\tprefetchnta\t"MEM2("32","2")
          "\n\tmovaps\t"MEM1(     "0")", %%xmm0"
          "\n\tmovaps\t"MEM2("16","0")", %%xmm1"
          BUMPPTR("64","2")
          "\n\tmovaps\t"MEM2("32","0")", %%xmm2"
          "\n\tmovaps\t"MEM2("48","0")", %%xmm3"
          "\n\tmovntps\t%%xmm0, "MEM1(     "1")
          "\n\tmovntps\t%%xmm1, "MEM2("16","1")
          BUMPPTR("64","0")
          "\n\tmovntps\t%%xmm2, "MEM2("32","1")
          "\n\tmovntps\t%%xmm3, "MEM2("48","1")
          BUMPPTR("64","1")
          : "=r" (from), "=r" (to), "=r" (pf)
          : "0"  (from), "1"  (to), "2"  (pf)
          : "memory"
        );
      } while (--i);
    }
    /* since movntq is weakly-ordered, a "sfence"
     * is needed to become ordered again. */
    __asm__ __volatile__ ("sfence":::"memory");
    i = len & 63;
    if (i)
      rep_movsb (to, from, i);
  }
  return retval;
}

#ifdef HAVE_AVX
static void * avx_memcpy(void * to, const void * from, size_t len)
{
  void *retval = to;
  const uint8_t *pf;
  uintptr_t i;

  /* PREFETCH has effect even for MOVSB instruction ;) */
  pf = (const uint8_t *)((uintptr_t)from & ~(uintptr_t)31);
  __asm__ __volatile__ (
    "\n\tprefetchnta\t"MEM1(      "0")
    "\n\tprefetchnta\t"MEM2( "32","0")
    "\n\tprefetchnta\t"MEM2( "64","0")
    "\n\tprefetchnta\t"MEM2( "96","0")
    "\n\tprefetchnta\t"MEM2("128","0")
    "\n\tprefetchnta\t"MEM2("160","0")
    "\n\tprefetchnta\t"MEM2("192","0")
    "\n\tprefetchnta\t"MEM2("224","0")
    "\n\tprefetchnta\t"MEM2("256","0")
    "\n\tprefetchnta\t"MEM2("288","0")
    : : "r" (pf));
  pf += 320;

  if(len >= MIN_LEN)
  {
    /* Align destinition to MMREG_SIZE -boundary */
    i = ((uintptr_t)to) & 31;
    if (i) {
      i = 32 - i;
      len -= i;
      rep_movsb (to, from, i);
    }
    i = len >> 7; /* len/128 */
    if(((uintptr_t)from) & 31)
      /* if SRC is misaligned */
      while (i--) {
        __asm__ __volatile__ (
        "\n\tprefetchnta\t"MEM1(     "2")
        "\n\tprefetchnta\t"MEM2("32","2")
        "\n\tprefetchnta\t"MEM2("64","2")
        "\n\tprefetchnta\t"MEM2("96","2")
        "\n\tvmovups\t"MEM1(     "0")", %%ymm0"
        "\n\tvmovups\t"MEM2("32","0")", %%ymm1"
        BUMPPTR("128","2")
        "\n\tvmovups\t"MEM2("64","0")", %%ymm2"
        "\n\tvmovups\t"MEM2("96","0")", %%ymm3"
        "\n\tvmovntps\t%%ymm0, "MEM1(     "1")
        "\n\tvmovntps\t%%ymm1, "MEM2("32","1")
        BUMPPTR("128","0")
        "\n\tvmovntps\t%%ymm2, "MEM2("64","1")
        "\n\tvmovntps\t%%ymm3, "MEM2("96","1")
        BUMPPTR("128","1")
        : "=r" (from), "=r" (to), "=r" (pf)
        : "0"  (from), "1"  (to), "2"  (pf)
        : "memory");
      }
    else
      /*
         Only if SRC is aligned on 32-byte boundary.
         It allows to use vmovaps instead of vmovups, which required data
         to be aligned or a general-protection exception (#GP) is generated.
      */
      while (i--) {
        __asm__ __volatile__ (
        "\n\tprefetchnta\t"MEM1(     "2")
        "\n\tprefetchnta\t"MEM2("32","2")
        "\n\tprefetchnta\t"MEM2("64","2")
        "\n\tprefetchnta\t"MEM2("96","2")
        "\n\tvmovaps\t"MEM1(     "0")", %%ymm0"
        "\n\tvmovaps\t"MEM2("32","0")", %%ymm1"
        BUMPPTR("128","2")
        "\n\tvmovaps\t"MEM2("64","0")", %%ymm2"
        "\n\tvmovaps\t"MEM2("96","0")", %%ymm3"
        "\n\tvmovntps\t%%ymm0, "MEM1(     "1")
        "\n\tvmovntps\t%%ymm1, "MEM2("32","1")
        BUMPPTR("128","0")
        "\n\tvmovntps\t%%ymm2, "MEM2("64","1")
        "\n\tvmovntps\t%%ymm3, "MEM2("96","1")
        BUMPPTR("128","1")
        : "=r" (from), "=r" (to), "=r" (pf)
        : "0"  (from), "1"  (to), "2"  (pf)
        : "memory");
      }
    /* since movntq is weakly-ordered, a "sfence"
     * is needed to become ordered again. */
    __asm__ __volatile__ ("sfence":::"memory");
    __asm__ __volatile__ ("vzeroupper");
    len &= 127;
  }
  /*
   *	Now do the tail of the block
   */
  i = len;
  if (i)
    rep_movsb (to, from, i);
  return retval;
}
#endif /* HAVE_AVX */

static void * mmx_memcpy(void * to, const void * from, size_t len)
{
  void *retval = to;
  uintptr_t i;

  if(len >= MMX1_MIN_LEN)
  {
    /* Align destinition to MMREG_SIZE -boundary */
    i = ((uintptr_t)to) & 7;
    if (i) {
      i = 8 - i;
      len -= i;
      rep_movsb (to, from, i);
    }
    i = len >> 6; /* len/64 */
    while (i--) {
      __asm__ __volatile__ (
      "\n\tmovq\t"MEM1(     "0")", %%mm0"
      "\n\tmovq\t"MEM2( "8","0")", %%mm1"
      "\n\tmovq\t"MEM2("16","0")", %%mm2"
      "\n\tmovq\t"MEM2("24","0")", %%mm3"
      "\n\tmovq\t"MEM2("32","0")", %%mm4"
      "\n\tmovq\t"MEM2("40","0")", %%mm5"
      "\n\tmovq\t"MEM2("48","0")", %%mm6"
      "\n\tmovq\t"MEM2("56","0")", %%mm7"
      "\n\tmovq\t%%mm0, "MEM1(     "1")
      "\n\tmovq\t%%mm1, "MEM2( "8","1")
      "\n\tmovq\t%%mm2, "MEM2("16","1")
      "\n\tmovq\t%%mm3, "MEM2("24","1")
      BUMPPTR("64","0")
      "\n\tmovq\t%%mm4, "MEM2("32","1")
      "\n\tmovq\t%%mm5, "MEM2("40","1")
      "\n\tmovq\t%%mm6, "MEM2("48","1")
      "\n\tmovq\t%%mm7, "MEM2("56","1")
      BUMPPTR("64","1")
      : "=r" (from), "=r" (to)
      : "0"  (from), "1"  (to)
      : "memory");
    }
    __asm__ __volatile__ ("emms":::"memory");
    len &= 63;
  }
  /*
   *	Now do the tail of the block
   */
  i = len;
  if (i)
    rep_movsb (to, from, i);
  return retval;
}

static void * mmx2_memcpy(void * to, const void * from, size_t len)
{
  const uint8_t *pf;
  void *retval = to;
  uintptr_t i;

  /* PREFETCH has effect even for MOVSB instruction ;) */
  pf = (const uint8_t *)((uintptr_t)from & ~(uintptr_t)31);
  __asm__ __volatile__ (
    "\n\tprefetchnta\t"MEM1(      "0")
    "\n\tprefetchnta\t"MEM2( "32","0")
    "\n\tprefetchnta\t"MEM2( "64","0")
    "\n\tprefetchnta\t"MEM2( "96","0")
    "\n\tprefetchnta\t"MEM2("128","0")
    "\n\tprefetchnta\t"MEM2("160","0")
    "\n\tprefetchnta\t"MEM2("192","0")
    "\n\tprefetchnta\t"MEM2("224","0")
    "\n\tprefetchnta\t"MEM2("256","0")
    "\n\tprefetchnta\t"MEM2("288","0")
    "\n\tprefetchnta\t"MEM2("320","0")
    "\n\tprefetchnta\t"MEM2("352","0")
    "\n\tprefetchnta\t"MEM2("384","0")
    "\n\tprefetchnta\t"MEM2("416","0")
    "\n\tprefetchnta\t"MEM2("448","0")
    "\n\tprefetchnta\t"MEM2("480","0")
    : : "r" (pf) );
  pf += 512;

  if(len >= MIN_LEN)
  {
    /* Align destinition to MMREG_SIZE -boundary */
    i = ((uintptr_t)to) & 7;
    if (i) {
      i = 8 - i;
      len -= i;
      rep_movsb (to, from, i);
    }
    i = len >> 6; /* len/64 */
    while (i--) {
      __asm__ __volatile__ (
        "\n\tprefetchnta\t"MEM1(     "2")
        "\n\tprefetchnta\t"MEM2("32","2")
        "\n\tmovq\t"MEM1(     "0")", %%mm0"
        "\n\tmovq\t"MEM2( "8","0")", %%mm1"
        "\n\tmovq\t"MEM2("16","0")", %%mm2"
        "\n\tmovq\t"MEM2("24","0")", %%mm3"
        BUMPPTR ("64", "2")
        "\n\tmovq\t"MEM2("32","0")", %%mm4"
        "\n\tmovq\t"MEM2("40","0")", %%mm5"
        "\n\tmovq\t"MEM2("48","0")", %%mm6"
        "\n\tmovq\t"MEM2("56","0")", %%mm7"
        "\n\tmovntq\t%%mm0, "MEM1(     "1")
        "\n\tmovntq\t%%mm1, "MEM2( "8","1")
        "\n\tmovntq\t%%mm2, "MEM2("16","1")
        "\n\tmovntq\t%%mm3, "MEM2("24","1")
        BUMPPTR ("64", "0")
        "\n\tmovntq\t%%mm4, "MEM2("32","1")
        "\n\tmovntq\t%%mm5, "MEM2("40","1")
        "\n\tmovntq\t%%mm6, "MEM2("48","1")
        "\n\tmovntq\t%%mm7, "MEM2("56","1")
        BUMPPTR ("64", "1")
        : "=r" (from), "=r" (to), "=r" (pf)
        : "0"  (from), "1"  (to), "2"  (pf)
        : "memory"
      );
    }
     /* since movntq is weakly-ordered, a "sfence"
     * is needed to become ordered again. */
    __asm__ __volatile__ ("sfence":::"memory");
    __asm__ __volatile__ ("emms":::"memory");
    len &= 63;
  }
  /*
   *	Now do the tail of the block
   */
  i = len;
  if (i)
    rep_movsb (to, from, i);
  return retval;
}

static void *linux_kernel_memcpy(void *to, const void *from, size_t len) {
  return linux_kernel_memcpy_impl(to,from,len);
}
#endif /* _MSC_VER */
#endif /* ARCH_X86 */

static const struct {
  const char name[16];
  void *(*const  function)(void *to, const void *from, size_t len);

  uint32_t cpu_require;
} memcpy_method[] =
{
  { "", NULL, 0 },
  { "libc", memcpy, 0 },
#if defined(ARCH_X86) && !defined(_MSC_VER)
  { "linux kernel", linux_kernel_memcpy, 0 },
  { "MMX ", mmx_memcpy, MM_MMX },
  { "MMXEXT", mmx2_memcpy, MM_MMXEXT },
  { "SSE", sse_memcpy, MM_MMXEXT|MM_SSE },
# ifdef HAVE_AVX
  { "AVX", avx_memcpy, MM_ACCEL_X86_AVX },
# endif /* HAVE_AVX */
#endif /* ARCH_X86 */
#if defined (ARCH_PPC) && !defined (HOST_OS_DARWIN)
  { "ppcasm", ppcasm_memcpy, 0 },
  { "ppcasm_cached", ppcasm_cacheable_memcpy, MM_ACCEL_PPC_CACHE32 },
#endif /* ARCH_PPC && !HOST_OS_DARWIN */
  { "", NULL, 0 }
};

static uint64_t memcpy_timing[sizeof(memcpy_method)/sizeof(memcpy_method[0])] = { 0, };

#ifdef HAVE_POSIX_TIMERS
/* Prefer clock_gettime() where available. */

# ifndef CLOCK_THREAD_CPUTIME_ID
/*  not defined in NetBSD (bug #535) */
#   define CLOCK_THREAD_CPUTIME_ID CLOCK_MONOTONIC
# endif

static int64_t _x_gettime(void)
{
  struct timespec tm;
  return (clock_gettime (CLOCK_THREAD_CPUTIME_ID, &tm) == -1)
       ? times (NULL)
       : (int64_t)tm.tv_sec * 1e9 + tm.tv_nsec;
}
#  define rdtsc(x) _x_gettime()

#elif defined(ARCH_X86) && defined(HAVE_SYS_TIMES_H)
static int64_t rdtsc(int config_flags)
{
  int64_t x;

  /* that should prevent us from trying cpuid with old cpus */
  if( config_flags & MM_MMX ) {
    __asm__ volatile (".byte 0x0f, 0x31" : "=A" (x));
    return x;
  } else {
    struct tms tp;
    return times(&tp);
  }
}
#else

static uint64_t rdtsc(int config_flags)
{
  /* FIXME: implement an equivalent for using optimized memcpy on other
            architectures */
#ifdef HAVE_SYS_TIMES_H
  struct tms tp;
  return times(&tp);
#else
	return clock();
#endif /* HAVE_SYS_TIMES_H */
}
#endif

static int xine_probe_fast_memcpy_int (xine_t *xine) {
#define BUFSIZE 1024*1024
  uint64_t     t;
  char        *buf1, *buf2;
  unsigned int i, j, best = 0;
  int          config_flags = xine_mm_accel ();

  if ((buf1 = malloc (BUFSIZE)) == NULL)
    return 0;
  if ((buf2 = malloc (BUFSIZE)) == NULL) {
    free (buf1);
    return 0;
  }

  xprintf (xine, XINE_VERBOSITY_LOG, _("Benchmarking memcpy methods (smaller is better):\n"));

  /* make sure buffers are present on physical memory */
  memset (buf1, 0, BUFSIZE);
  memset (buf2, 0, BUFSIZE);

  /* some initial activity to ensure that we're not running slowly :-) */
  for (j = 0; j < 50; j++) {
    memcpy_method[1].function (buf2, buf1, BUFSIZE);
    memcpy_method[1].function (buf1, buf2, BUFSIZE);
  }

  for (i = 1; memcpy_method[i].name[0]; i++) {
    if ((config_flags & memcpy_method[i].cpu_require) != memcpy_method[i].cpu_require)
      continue;

    t = rdtsc (config_flags);
    for (j = 0; j < 50; j++) {
      memcpy_method[i].function (buf2, buf1, BUFSIZE);
      memcpy_method[i].function (buf1, buf2, BUFSIZE);
    }
    t = rdtsc (config_flags) - t;
    memcpy_timing[i] = t;

    xprintf (xine, XINE_VERBOSITY_LOG, "\t%s memcpy() : %" PRIu64 "\n", memcpy_method[i].name, t);

    if (best == 0 || t < memcpy_timing[best])
      best = i;
  }

  free (buf1);
  free (buf2);

  return best;
}

static void update_fast_memcpy (void *user_data, xine_cfg_entry_t *entry) {
  int     config_flags = xine_mm_accel ();
  xine_t *xine = (xine_t *)user_data;
  int     method = entry->num_value;

  /* check if function is configured and valid for this machine */
  if ((method > 0) && (method < sizeof (memcpy_method) / sizeof (memcpy_method[0]) - 1) &&
     ((config_flags & memcpy_method[method].cpu_require) == memcpy_method[method].cpu_require)) {
    xprintf (xine, XINE_VERBOSITY_DEBUG, "xine_fast_memcpy (): using \"%s\"\n", memcpy_method[method].name);
    xine_fast_memcpy = memcpy_method[method].function;
    return;
  }

  method = xine_probe_fast_memcpy_int (xine);
  if (method) {
    /* should not be an endless recursion as this method will pass the test above */
    xine->config->update_num (xine->config, "engine.performance.memcpy_method", method);
  }
}

void xine_probe_fast_memcpy(xine_t *xine)
{
  unsigned int      method;
  static const char *const memcpy_methods[] = {
    "probe", "libc",
#if defined(ARCH_X86) && !defined(_MSC_VER)
    "kernel", "mmx", "mmxext", "sse",
# ifdef HAVE_AVX
    "avx",
# endif /* HAVE_AVX */
#endif
#if defined (ARCH_PPC) && !defined (HOST_OS_DARWIN)
    "ppcasm_memcpy", "ppcasm_cacheable_memcpy",
#endif
    NULL
  };

  method = xine->config->register_enum (
    xine->config,
    "engine.performance.memcpy_method",
    0, /* default to "probe" */
    (char **)memcpy_methods,
    _("memcopy method used by xine"),
    _("The copying of large memory blocks is one of the most "
      "expensive operations on todays computers. Therefore xine "
      "provides various tuned methods to do this copying. "
      "Usually, the best method is detected automatically."),
    20,
    update_fast_memcpy,
    xine
  );

  /* an earlier xine engine instance has already set this */
  if (xine_fast_memcpy)
    return;
  xine_fast_memcpy = memcpy;

  xine->config->update_num (xine->config, "engine.performance.memcpy_method", method);
}

