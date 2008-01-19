/*
 * attributes.h
 * Copyright (C) 1999-2000 Aaron Holtzman <aholtzma@ess.engr.uvic.ca>
 * Copyright (C) 2001-2007 xine developers
 *
 * This file was originally part of mpeg2dec, a free MPEG-2 video stream
 * decoder.
 *
 * mpeg2dec is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * mpeg2dec is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110, USA
 */

/* use gcc attribs to align critical data structures */

#ifndef ATTRIBUTE_H_
#define ATTRIBUTE_H_

#ifdef ATTRIBUTE_ALIGNED_MAX
#define ATTR_ALIGN(align) __attribute__ ((__aligned__ ((ATTRIBUTE_ALIGNED_MAX < align) ? ATTRIBUTE_ALIGNED_MAX : align)))
#else
#define ATTR_ALIGN(align)
#endif

/* disable GNU __attribute__ extension, when not compiling with GNU C */
#if defined(__GNUC__) || defined (__ICC)
#ifndef ATTRIBUTE_PACKED
#define	ATTRIBUTE_PACKED 1
#endif 
#else
#undef	ATTRIBUTE_PACKED
#ifndef __attribute__
#define	__attribute__(x)	/**/
#endif /* __attribute __*/
#endif

#ifdef XINE_COMPILE
# include "configure.h"
#endif

/* Export protected only for libxine functions */
#if defined(XINE_LIBRARY_COMPILE) && defined(SUPPORT_ATTRIBUTE_VISIBILITY_PROTECTED)
# define XINE_PROTECTED __attribute__((__visibility__("protected")))
#elif defined(XINE_LIBRARY_COMPILE) && defined(SUPPORT_ATTRIBUTE_VISIBILITY_DEFAULT)
# define XINE_PROTECTED __attribute__((__visibility__("default")))
#else
# define XINE_PROTECTED
#endif

#ifdef SUPPORT_ATTRIBUTE_SENTINEL
# define XINE_SENTINEL __attribute__((__sentinel__))
#else
# define XINE_SENTINEL
#endif

#ifndef __attr_unused
# ifdef SUPPORT_ATTRIBUTE_UNUSED
#  define __attr_unused __attribute__((__unused__))
# else
#  define __attr_unused
# endif
#endif

/* Format attributes */
#ifdef SUPPORT_ATTRIBUTE_FORMAT
# define XINE_FORMAT_PRINTF(fmt,var) __attribute__((__format__(__printf__, fmt, var)))
#else
# define XINE_FORMAT_PRINTF(fmt,var)
#endif
#ifdef SUPPORT_ATTRIBUTE_FORMAT_ARG
# define XINE_FORMAT_PRINTF_ARG(fmt) __attribute__((__format_arg__(fmt)))
#else
# define XINE_FORMAT_PRINTF_ARG(fmt)
#endif

#endif /* ATTRIBUTE_H_ */
