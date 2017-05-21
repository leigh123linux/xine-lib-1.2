/*
 * Copyright (C) 2012 Edgar Hucek <gimli|@dark-green.com>
 * Copyright (C) 2012-2016 xine developers
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
 *
 * Common acceleration definitions for vaapi
 *
 *
 */

#ifndef HAVE_XINE_ACCEL_VAAPI_H
#define HAVE_XINE_ACCEL_VAAPI_H

#ifdef __cplusplus
extern "C" {
#endif

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <va/va.h>

#define SURFACE_FREE            0
#define SURFACE_ALOC            1
#define SURFACE_RELEASE         2
#define SURFACE_RENDER          3
#define SURFACE_RENDER_RELEASE  5

struct vaapi_equalizer {
  VADisplayAttribute brightness;
  VADisplayAttribute contrast;
  VADisplayAttribute hue;
  VADisplayAttribute saturation;
};

typedef struct ff_vaapi_context_s ff_vaapi_context_t;
typedef struct ff_vaapi_surface_s ff_vaapi_surface_t;

struct ff_vaapi_context_s {
  VADisplay         va_display;
  VAContextID       va_context_id;
  VAConfigID        va_config_id;
  int               width;
  int               height;
  unsigned int      valid_context;

  /* decoding surfaces */
  VASurfaceID        *va_surface_ids;
  ff_vaapi_surface_t *va_render_surfaces;
  unsigned int      va_head;

  vo_driver_t       *driver;
  VAImageFormat     *va_image_formats;
  int               va_num_image_formats;
};

typedef struct vaapi_accel_s vaapi_accel_t;

struct ff_vaapi_surface_s {
  unsigned int        index;
  VASurfaceID         va_surface_id;
  unsigned int        status;
};

  /*
   *
   */

#define IMGFMT_VAAPI               0x56410000 /* 'VA'00 */
#define IMGFMT_VAAPI_MASK          0xFFFF0000
#define IMGFMT_VAAPI_CODEC_MASK    0x000000F0
#define IMGFMT_VAAPI_CODEC(fmt)    ((fmt) & IMGFMT_VAAPI_CODEC_MASK)
#define IMGFMT_VAAPI_CODEC_MPEG2   (0x10)
#define IMGFMT_VAAPI_CODEC_MPEG4   (0x20)
#define IMGFMT_VAAPI_CODEC_H264    (0x30)
#define IMGFMT_VAAPI_CODEC_VC1     (0x40)
#define IMGFMT_VAAPI_CODEC_HEVC    (0x50)
#define IMGFMT_VAAPI_MPEG2         (IMGFMT_VAAPI|IMGFMT_VAAPI_CODEC_MPEG2)
#define IMGFMT_VAAPI_MPEG2_IDCT    (IMGFMT_VAAPI|IMGFMT_VAAPI_CODEC_MPEG2|1)
#define IMGFMT_VAAPI_MPEG2_MOCO    (IMGFMT_VAAPI|IMGFMT_VAAPI_CODEC_MPEG2|2)
#define IMGFMT_VAAPI_MPEG4         (IMGFMT_VAAPI|IMGFMT_VAAPI_CODEC_MPEG4)
#define IMGFMT_VAAPI_H263          (IMGFMT_VAAPI|IMGFMT_VAAPI_CODEC_MPEG4|1)
#define IMGFMT_VAAPI_H264          (IMGFMT_VAAPI|IMGFMT_VAAPI_CODEC_H264)
#define IMGFMT_VAAPI_HEVC          (IMGFMT_VAAPI|IMGFMT_VAAPI_CODEC_HEVC)
#define IMGFMT_VAAPI_HEVC_MAIN10   (IMGFMT_VAAPI|IMGFMT_VAAPI_CODEC_HEVC|1)
#define IMGFMT_VAAPI_VC1           (IMGFMT_VAAPI|IMGFMT_VAAPI_CODEC_VC1)
#define IMGFMT_VAAPI_WMV3          (IMGFMT_VAAPI|IMGFMT_VAAPI_CODEC_VC1|1)

struct vaapi_accel_s {
  unsigned int        index;

  int  (*lock_vaapi)(vo_frame_t *frame_gen);
  void (*unlock_vaapi)(vo_frame_t *frame_gen);

  VAStatus (*vaapi_init)(vo_frame_t *frame_gen, int va_profile, int width, int height);
  int (*profile_from_imgfmt)(vo_frame_t *frame_gen, unsigned img_fmt);
  ff_vaapi_context_t *(*get_context)(vo_frame_t *frame_gen);
  int (*guarded_render)(vo_frame_t *frame_gen);
  ff_vaapi_surface_t *(*get_vaapi_surface)(vo_frame_t *frame_gen);
  void (*render_vaapi_surface)(vo_frame_t *frame_gen, ff_vaapi_surface_t *va_surface);
  void (*release_vaapi_surface)(vo_frame_t *frame_gen, ff_vaapi_surface_t *va_surface);
};

#ifdef __cplusplus
}
#endif

#endif

