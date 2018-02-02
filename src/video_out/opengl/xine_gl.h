/*
 * Copyright (C) 2018 the xine project
 * Copyright (C) 2018 Petri Hintukainen <phintuka@users.sourceforge.net>
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
 * xine_gl.h, Interface between OpenGL and native windowing system
 *
 */

#ifndef XINE_GL_H_
#define XINE_GL_H_

#include <xine.h>

typedef struct xine_gl xine_gl_t;

struct xine_gl {
  int  (*make_current)     (xine_gl_t *);
  void (*release_current)  (xine_gl_t *);
  void (*swap_buffers)     (xine_gl_t *);

  /* resize is needed only with WAYLAND visual */
  void (*resize)           (xine_gl_t *, int width, int height);
  /* set_native_window is used only with X11 */
  void (*set_native_window)(xine_gl_t *, void *);

  void (*dispose)          (xine_gl_t **);
};

#endif /* XINE_GL_H_ */
