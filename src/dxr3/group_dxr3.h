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
 */

#ifndef HAVE_GROUP_DXR3_H
#define HAVE_GROUP_DXR3_H

#include <xine/xine_internal.h>

void *dxr3_spudec_init_plugin(xine_t *xine, const void* data);
void *dxr3_video_init_plugin(xine_t *xine, const void *data);
void *dxr3_aa_init_plugin(xine_t *xine, const void *visual_gen);
#ifdef HAVE_X11
void *dxr3_x11_init_plugin(xine_t *xine, const void *visual_gen);
#endif

#endif /* HAVE_GROUP_DXR3_H */
