/*
 * Copyright (C) 2000-2011 the xine project
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
 */

#ifndef HAVE_XINE_PRIVATE_H
#define HAVE_XINE_PRIVATE_H

#ifndef XINE_LIBRARY_COMPILE
# error xine_private.h is for libxine private use only!
#endif

#include "configure.h"

/* Export internal only for functions that are unavailable to plugins */
#if defined(SUPPORT_ATTRIBUTE_VISIBILITY_INTERNAL)
# define INTERNAL __attribute__((__visibility__("internal")))
#elif defined(SUPPORT_ATTRIBUTE_VISIBILITY_DEFAULT)
# define INTERNAL __attribute__((__visibility__("default")))
#else
# define INTERNAL
#endif

#ifdef __cplusplus
extern "C" {
#endif

/*
 * make file descriptors and sockets uninheritable
 */
int _x_set_file_close_on_exec(int fd) INTERNAL;

int _x_set_socket_close_on_exec(int s) INTERNAL;

#ifdef __cplusplus
}
#endif

#endif
