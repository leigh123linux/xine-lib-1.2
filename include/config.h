/*
 * Copyright (C) 2007-2018 the xine project
 *
 * This file is part of xine, a free video player.
 *
 * xine is free software; you can redistribute it and/or modify it under
 * the terms of the GNU General Public Licence as published by the Free
 * Software Foundation; either version 2 of the Licence, or (at your option)
 * any later version.
 *
 * xine is distributed in the hope that it will be useful, but WITHOUT ANY
 * WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE.  See the GNU General Public Licence for more
 * details.
 *
 * You should have received a copy of the GNU General Public Licence along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA
 *
 */

#include "configure.h"

/* Ugly build time sanity guard.
 * ./configure might mistake X32 mode as plain 64bit,
 * but compiler itself sets __ILP32__ when in x32.
 * Even worse: clang sets this in 32 mode as well,
 * so also test __i386__ here.
 */
#ifdef ARCH_X86
#  if defined(__ILP32__) && !defined(__i386) && !defined(__i386__) && !defined(ARCH_X86_X32)
#    ifdef ARCH_WARN
#        warning "configure did not detect ARCH_X86_X32!"
#    endif
#    undef ARCH_X86_64
#    define ARCH_X86_X32
#    undef ARCH_X86_32
#  elif defined(ARCH_X86_64) && defined(ARCH_X86_X32)
#    ifdef ARCH_WARN
#        warning "configure did set both ARCH_X86_64 and ARCH_X86_X32!"
#    endif
#    undef ARCH_X86_64
#    undef ARCH_X86_32
#  endif
#endif

#include "os_internal.h"


