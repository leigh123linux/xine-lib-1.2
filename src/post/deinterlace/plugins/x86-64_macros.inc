/*
 * Copyright (C) 2017 the xine project
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
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 * Helper macros to size x86 memory locations and hardcoded registers properly.
 * Registers may be: "ax" "bx" "cx" "dx" "si" "di" "bp" "sp".
 */

#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif

#ifndef MEMREF1

/* register only stuff */
#if defined(ARCH_X86_64) || defined(ARCH_X86_X32)
#  define BUMPPTR(offs,src,dest)   "leaq "offs"(%%r"src"), %%r"dest"\n\t"
#  define REG1(what,reg)           what"q %%r"reg"\n\t"
#  define REG2(what,reg1,reg2)     what"q %%r"reg1", %%r"reg2"\n\t"
#  define MEMREF1(reg)             "(%%r"reg")"
#  define MEMREF2(base,offs)       "(%%r"base", %%r"offs")"
#  define MEMREF3(base,index,mult) "(%%r"base", %%r"index", "mult")"
#elif defined(ARCH_X86)
#  define BUMPPTR(offs,src,dest)   "leal "offs"(%%e"src"), %%e"dest"\n\t"
#  define REG1(what,reg)           what"l %%e"reg"\n\t"
#  define REG2(what,reg1,reg2)     what"l %%e"reg1", %%e"reg2"\n\t"
#  define MEMREF1(reg)             "(%%e"reg")"
#  define MEMREF2(base,offs)       "(%%e"base", %%e"offs")"
#  define MEMREF3(base,index,mult) "(%%e"base", %%e"index", "mult")"
#endif

/* memory location or constant + register */
#if defined(ARCH_X86_64)
#  define MEMREG(what,addr,reg)   what"q "addr", %%r"reg"\n\t"
#  define REGMEM(what,reg,addr)   what"q %%r"reg", "addr"\n\t"
#  define CONSTMEM(what,val,addr) what"q $"val", "addr"\n\t"
#elif defined(ARCH_X86_X32) || defined (ARCH_X86)
#  define MEMREG(what,addr,reg)   what"l "addr", %%e"reg"\n\t"
#  define REGMEM(what,reg,addr)   what"l %%e"reg", "addr"\n\t"
#  define CONSTMEM(what,val,addr) what"l $"val", "addr"\n\t"
#endif

#ifndef MEMREF1
#  error Undefined architecture. Define either ARCH_X86, ARCH_X86_X32 or ARCH_X86_64.
#endif

#endif

