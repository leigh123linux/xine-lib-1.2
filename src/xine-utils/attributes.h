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
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

/* use gcc attribs to align critical data structures */

#ifndef ATTRIBUTE_H_
#define ATTRIBUTE_H_

#ifdef ATTRIBUTE_ALIGNED_MAX
#define ATTR_ALIGN(align) __attribute__ ((__aligned__ ((ATTRIBUTE_ALIGNED_MAX < align) ? ATTRIBUTE_ALIGNED_MAX : align)))
#else
#define ATTR_ALIGN(align)
#endif

#ifdef XINE_COMPILE
# include "config.h"
#endif

/* Export protected only for libxine functions */
#if defined(XINE_LIBRARY_COMPILE) && defined(SUPPORT_ATTRIBUTE_VISIBILITY_PROTECTED)
# define XINE_PROTECTED __attribute__((visibility("protected")))
#elif defined(XINE_LIBRARY_COMPILE) && defined(SUPPORT_ATTRIBUTE_VISIBILITY_DEFAULT)
# define XINE_PROTECTED __attribute__((visibility("default")))
#else
# define XINE_PROTECTED
#endif

#ifdef SUPPORT_ATTRIBUTE_SENTINEL
# define XINE_SENTINEL __attribute__((sentinel))
#else
# define XINE_SENTINEL
#endif

#ifndef __attr_unused
# ifdef SUPPORT_ATTRIBUTE_UNUSED
#  define __attr_unused __attribute__((unused))
# else
#  define __attr_unused
# endif
#endif

/* Format attributes */
#ifdef SUPPORT_ATTRIBUTE_FORMAT
# define XINE_FORMAT_PRINTF(fmt,var) __attribute__((format(printf, fmt, var)))
#else
# define XINE_FORMAT_PRINTF(fmt,var)
#endif
#ifdef SUPPORT_ATTRIBUTE_FORMAT_ARG
# define XINE_FORMAT_PRINTF_ARG(fmt) __attribute__((format_arg(fmt)))
#else
# define XINE_FORMAT_PRINTF_ARG(fmt)
#endif

#ifdef SUPPORT_ATTRIBUTE_PACKED
# define XINE_PACKED __attribute__((packed))
#else
# define XINE_PACKED
#endif

#ifdef SUPPORT_ATTRIBUTE_MALLOC
# define XINE_MALLOC __attribute__((__malloc__))
#else
# define XINE_MALLOC
#endif

#endif /* ATTRIBUTE_H_ */
