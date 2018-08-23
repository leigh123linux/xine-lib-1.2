/*
 * Copyright (C) 2000-2016 the xine project
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
 * A group of decoders for raw (uncompressed or slightly compressed) video.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <xine/xine_internal.h>
#include <xine/video_out.h>
#include <xine/buffer.h>

#include "group_raw.h"

/*
 * exported plugin catalog entries
 */
static const uint32_t rgb_video_types[]      = { BUF_VIDEO_RGB, 0 };
static const uint32_t yuv_video_types[]      = {
  BUF_VIDEO_YUY2, BUF_VIDEO_YV12, BUF_VIDEO_YVU9, BUF_VIDEO_GREY, BUF_VIDEO_I420, 0
};
static const uint32_t bitplane_video_types[] = { BUF_VIDEO_BITPLANE, BUF_VIDEO_BITPLANE_BR1, 0 };

static const decoder_info_t rgb_info      = { rgb_video_types, 1 }; /* supported types, priority */
static const decoder_info_t yuv_info      = { yuv_video_types, 1 };
static const decoder_info_t bitplane_info = { bitplane_video_types, 1 };


const plugin_info_t xine_plugin_info[] EXPORTED = {
  /* type, API, "name", version, special_info, init_function */
  { PLUGIN_VIDEO_DECODER, 19, "rgb",      XINE_VERSION_CODE, &rgb_info,      decode_rgb_init_class },
  { PLUGIN_VIDEO_DECODER, 19, "yuv",      XINE_VERSION_CODE, &yuv_info,      decode_yuv_init_class },
  { PLUGIN_VIDEO_DECODER, 19, "bitplane", XINE_VERSION_CODE, &bitplane_info, decode_bitplane_init_class },
  { PLUGIN_NONE, 0, NULL, 0, NULL, NULL }
};
