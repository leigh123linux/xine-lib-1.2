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
 * catalog for planar post plugins
 */

#ifndef XINE_POST_PLANAR_H
#define XINE_POST_PLANAR_H

#include <xine/xine_internal.h>

void *boxblur_init_plugin   (xine_t *xine, const void *);
void *denoise3d_init_plugin (xine_t *xine, const void *);
void *eq_init_plugin        (xine_t *xine, const void *);
void *eq2_init_plugin       (xine_t *xine, const void *);
void *expand_init_plugin    (xine_t *xine, const void *);
void *fill_init_plugin      (xine_t *xine, const void *);
void *invert_init_plugin    (xine_t *xine, const void *);
void *noise_init_plugin     (xine_t *xine, const void *);
#ifdef HAVE_POSTPROC
void *pp_init_plugin        (xine_t *xine, const void *);
#endif
void *unsharp_init_plugin   (xine_t *xine, const void *);

#endif /* XINE_POST_PLANAR_H */
