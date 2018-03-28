/*
 * Copyright (C) 2000-2018 the xine project
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
 * mplayer's noise filter, ported by Jason Tackaberry.  Original filter
 * is copyright 2002 Michael Niedermayer <michaelni@gmx.at>
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "noise.h"

#ifndef ARCH_X86
#error this file is X86 only
#endif

#if defined(ARCH_X86_64)
#  define TYPEA int64_t
#  define REGA "%rax"
#  define MOVA(val) "\n\tmovq\t"val", %%rax"
#  define MEMA(reg) "(%"reg", %%rax)"
#  define ADDA(val) "\n\taddq\t"val", %%rax"
#elif defined(ARCH_X86_X32)
#  define TYPEA int64_t
#  define REGA "%rax"
#  define MOVA(val) "\n\tmovq\t"val", %%rax"
#  define MEMA(reg) "(%q"reg", %%rax)"
#  define ADDA(val) "\n\taddq\t"val", %%rax"
#else
#  define TYPEA int32_t
#  define REGA "%eax"
#  define MOVA(val) "\n\tmovl\t"val", %%eax"
#  define MEMA(reg) "(%"reg", %%eax)"
#  define ADDA(val) "\n\taddl\t"val", %%eax"
#endif

static inline int saturate(int v, int min, int max)
{
  if (v > max) return max;
  if (v < min) return min;
  return v;
}

void lineNoise_MMX(uint8_t *dst, const uint8_t *src, const int8_t *noise, int len, int shift)
{
  TYPEA mmx_len = len & (~7);
  noise += shift;

  src += mmx_len;
  dst += mmx_len;
  noise += mmx_len;

  __asm__ __volatile__ (
    MOVA("%3")
    "\n\tpcmpeqb\t%%mm7, %%mm7"
    "\n\tpsllw\t$15, %%mm7"
    "\n\tpacksswb\t%%mm7, %%mm7"
    "\n\t"ASMALIGN(4)
    "\n1:"
    "\n\tmovq\t"MEMA("0")", %%mm0"
    "\n\tmovq\t"MEMA("1")", %%mm1"
    "\n\tpxor\t%%mm7, %%mm0"
    "\n\tpaddsb\t%%mm1, %%mm0"
    "\n\tpxor\t%%mm7, %%mm0"
    "\n\tmovq\t%%mm0, "MEMA("2")
    ADDA("$8")
    "\n\tjs\t1b"
    :: "r" (src), "r" (noise), "r" (dst), "g" (-mmx_len)
    : REGA
  );
  if (mmx_len != len) {
    int i;
    for (i = 0; i < (len & 7); i++) {
      dst[i] = saturate(src[i] + noise[i], 0, 255);
    }
  }
}

//duplicate of previous except movntq
void lineNoise_MMX2(uint8_t *dst, const uint8_t *src, const int8_t *noise, int len, int shift)
{
  TYPEA mmx_len = len & (~7);
  noise += shift;

  src += mmx_len;
  dst += mmx_len;
  noise += mmx_len;

  __asm__ __volatile__ (
    MOVA("%3")
    "\n\tpcmpeqb\t%%mm7, %%mm7"
    "\n\tpsllw\t$15, %%mm7"
    "\n\tpacksswb\t%%mm7, %%mm7"
    "\n\t"ASMALIGN(4)
    "\n1:"
    "\n\tmovq\t"MEMA("0")", %%mm0"
    "\n\tmovq\t"MEMA("1")", %%mm1"
    "\n\tpxor\t%%mm7, %%mm0"
    "\n\tpaddsb\t%%mm1, %%mm0"
    "\n\tpxor\t%%mm7, %%mm0"
    "\n\tmovntq\t%%mm0, "MEMA("2")
    ADDA("$8")
    "\n\tjs\t1b"
    :: "r" (src), "r" (noise), "r" (dst), "g" (-mmx_len)
    : REGA
  );
  if (mmx_len != len) {
    int i;
    for (i = 0; i < (len & 7); i++) {
      dst[i] = saturate(src[i] + noise[i], 0, 255);
    }
  }
}

void lineNoiseAvg_MMX(uint8_t *dst, const uint8_t *src, int len, int8_t **shift)
{
  TYPEA mmx_len = len & (~7);

  __asm__ __volatile__ (
    MOVA("%5")
    "\n\t"ASMALIGN(4)
    "\n1:"
    "\n\tmovq\t"MEMA("1")", %%mm1"
    "\n\tmovq\t"MEMA("0")", %%mm0"
    "\n\tpaddb\t"MEMA("2")", %%mm1"
    "\n\tpaddb\t"MEMA("3")", %%mm1"
    "\n\tmovq\t%%mm0, %%mm2"
    "\n\tmovq\t%%mm1, %%mm3"
    "\n\tpunpcklbw\t%%mm0, %%mm0"
    "\n\tpunpckhbw\t%%mm2, %%mm2"
    "\n\tpunpcklbw\t%%mm1, %%mm1"
    "\n\tpunpckhbw\t%%mm3, %%mm3"
    "\n\tpmulhw\t%%mm0, %%mm1"
    "\n\tpmulhw\t%%mm2, %%mm3"
    "\n\tpaddw\t%%mm1, %%mm1"
    "\n\tpaddw\t%%mm3, %%mm3"
    "\n\tpaddw\t%%mm0, %%mm1"
    "\n\tpaddw\t%%mm2, %%mm3"
    "\n\tpsrlw\t$8, %%mm1"
    "\n\tpsrlw\t$8, %%mm3"
    "\n\tpackuswb\t%%mm3, %%mm1"
    "\n\tmovq\t%%mm1, "MEMA("4")
    ADDA("$8")
    "\n\tjs\t1b"
    :: "r" (src + mmx_len), "r" (shift[0] + mmx_len), "r" (shift[1] + mmx_len), "r" (shift[2] + mmx_len),
       "r" (dst + mmx_len), "g" (-mmx_len)
    : REGA
  );

  if (mmx_len != len) {
    const int8_t *src2 = (const int8_t*)src;
    int i;

    for (i = mmx_len; i < len; i++) {
      const int n = shift[0][i] + shift[1][i] + shift[2][i];
      dst[i] = src2[i] + ((n * src2[i]) >> 7);
    }
  }
}

