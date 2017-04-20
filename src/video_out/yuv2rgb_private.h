/*
 * yuv2rgb_private.h
 *
 * Copyright (C) 2001-2017 the xine project
 * This file is part of xine, a free video player.
 *
 * based on work from mpeg2dec:
 * Copyright (C) 1999-2001 Aaron Holtzman <aholtzma@ess.engr.uvic.ca>
 *
 * This file is part of mpeg2dec, a free MPEG-2 video stream decoder.
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

#ifndef YUV2RGB_PRIVATE_H
#define YUV2RGB_PRIVATE_H

#include <inttypes.h>

#ifdef HAVE_MLIB
#include <mlib_video.h>
#endif

#include "yuv2rgb.h"

typedef struct yuv2rgb_impl_s yuv2rgb_impl_t;
typedef struct yuv2rgb_factory_impl_s yuv2rgb_factory_impl_t;

typedef void (*scale_line_func_t) (uint8_t *source, uint8_t *dest, int width, int step);

struct yuv2rgb_impl_s {

  yuv2rgb_t         intf;

  int               source_width, source_height;
  int               y_stride, uv_stride;
  int               dest_width, dest_height;
  int               rgb_stride;
  int               slice_height, slice_offset;
  int               step_dx, step_dy;
  int               do_scale, swapped;

  uint8_t          *y_buffer;
  uint8_t          *u_buffer;
  uint8_t          *v_buffer;

  void            **table_rV;
  void            **table_gU;
  int              *table_gV;
  void            **table_bU;
  void             *table_mmx;

  uint8_t          *cmap;
  scale_line_func_t scale_line;

#ifdef HAVE_MLIB
  uint8_t          *mlib_buffer;
  uint8_t          *mlib_resize_buffer;
  mlib_filter      mlib_filter_type;
#endif
};

struct yuv2rgb_factory_impl_s {

  yuv2rgb_factory_t intf;

  int      mode;
  int      swapped;
  uint8_t *cmap;

  void    *table_base;
  void    *table_rV[256];
  void    *table_gU[256];
  int      table_gV[256];
  void    *table_bU[256];
  void    *table_mmx;

  /* preselected functions for mode/swap/hardware */
  yuv2rgb_fun_t               yuv2rgb_fun;
  yuy22rgb_fun_t              yuy22rgb_fun;
  yuv2rgb_single_pixel_fun_t  yuv2rgb_single_pixel_fun;
};

void mmx_yuv2rgb_set_csc_levels(yuv2rgb_factory_t *this,
                                int brightness, int contrast, int saturation,
                                int colormatrix);
void yuv2rgb_init_mmxext (yuv2rgb_factory_impl_t *this);
void yuv2rgb_init_mmx (yuv2rgb_factory_impl_t *this);
void yuv2rgb_init_mlib (yuv2rgb_factory_impl_t *this);


#endif /* YUV2RGB_PRIVATE_H */
