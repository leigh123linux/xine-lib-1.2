/*
 * yuv2rgb.h
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

#ifndef XINE_YUV2RGB_H
#define XINE_YUV2RGB_H

#include <inttypes.h>

typedef struct yuv2rgb_s yuv2rgb_t;

typedef struct yuv2rgb_factory_s yuv2rgb_factory_t;

/*
 * function types for functions which can be replaced
 * by hardware-accelerated versions
 */

typedef void (*yuv2rgb_fun_t) (yuv2rgb_t *this, uint8_t * image, uint8_t * py, uint8_t * pu, uint8_t * pv) ;

typedef void (*yuy22rgb_fun_t) (yuv2rgb_t *this, uint8_t * image, uint8_t * p);

typedef uint32_t (*yuv2rgb_single_pixel_fun_t) (yuv2rgb_t *this, uint8_t y, uint8_t u, uint8_t v);

/*
 * modes supported - feel free to implement yours
 */

#define MODE_8_RGB    1
#define MODE_8_BGR    2
#define MODE_15_RGB   3
#define MODE_15_BGR   4
#define MODE_16_RGB   5
#define MODE_16_BGR   6
#define MODE_24_RGB   7
#define MODE_24_BGR   8
#define MODE_32_RGB   9
#define MODE_32_BGR  10
#define MODE_8_GRAY  11
#define MODE_PALETTE 12

/*
 * colormatrix values - (mpeg_matrix_index << 1) | fullrange
 */

#define CM_DEFAULT   10
#define CM_SD        10
#define CM_HD         2
#define CM_FULLRANGE  1

struct yuv2rgb_s {
  /*
   * configure converter for scaling factors
   */
  int (*configure) (yuv2rgb_t *this,
                    int source_width, int source_height,
                    int y_stride, int uv_stride,
                    int dest_width, int dest_height,
                    int rgb_stride);

  /*
   * start a new field or frame if dest is NULL
   */
  int (*next_slice) (yuv2rgb_t *this, uint8_t **dest);

  /*
   * free resources
   */
  void (*dispose) (yuv2rgb_t *this);

  /*
   * this is the function to call for the yuv2rgb and scaling process
   */
  yuv2rgb_fun_t     yuv2rgb_fun;

  /*
   * this is the function to call for the yuy2->rgb and scaling process
   */
  yuy22rgb_fun_t    yuy22rgb_fun;

  /*
   * this is the function to call for the yuv2rgb for a single pixel
   * (used for converting clut colors)
   */

  yuv2rgb_single_pixel_fun_t yuv2rgb_single_pixel_fun;
};

/*
 * convenience class to easily create a lot of converters
 */

struct yuv2rgb_factory_s {
  yuv2rgb_t* (*create_converter) (yuv2rgb_factory_t *this);

  /*
   * set color space conversion levels
   * for all converters produced by this factory
   */
  void (*set_csc_levels) (yuv2rgb_factory_t *this,
    int brightness, int contrast, int saturation, int colormatrix);

  /*
   * free resources
   */
  void (*dispose) (yuv2rgb_factory_t *this);
};

yuv2rgb_factory_t *yuv2rgb_factory_init (int mode, int swapped, uint8_t *colormap);


#endif /* XINE_YUV2RGB_H */
