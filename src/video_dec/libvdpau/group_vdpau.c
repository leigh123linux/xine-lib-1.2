/*
 * Copyright (C) 2008-2018 the xine project
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
 * A group of video stream parsers using the VDPAU hardware decoder.
 *
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "group_vdpau.h"

/*
 * This is a list of all of the internal xine video buffer types that
 * a decoder is able to handle. Check src/xine-engine/buffer.h for a
 * list of valid buffer types (and add a new one if the one you need does
 * not exist). Terminate the list with a 0.
 */
static const uint32_t video_types_h264[]      = { BUF_VIDEO_H264, 0 };
static const uint32_t video_types_vc1[]       = { BUF_VIDEO_VC1, BUF_VIDEO_WMV9, 0 };
static const uint32_t video_types_mpeg12[]    = { BUF_VIDEO_MPEG, 0 };
static const uint32_t video_types_mpeg4[]     = { BUF_VIDEO_MPEG4, BUF_VIDEO_XVID, BUF_VIDEO_DIVX5, BUF_VIDEO_3IVX, 0 };

/*
 * This data structure combines the list of supported xine buffer types and
 * the priority that the plugin should be given with respect to other
 * plugins that handle the same buffer type. A plugin with priority (n+1)
 * will be used instead of a plugin with priority (n).
 */
static const decoder_info_t dec_info_video_alterh264 = { .supported_types = video_types_h264,   .priority = 9 };
static const decoder_info_t dec_info_video_h264      = { .supported_types = video_types_h264,   .priority = 7 };
static const decoder_info_t dec_info_video_vc1       = { .supported_types = video_types_vc1,    .priority = 8 };
static const decoder_info_t dec_info_video_mpeg12    = { .supported_types = video_types_mpeg12, .priority = 8 };
static const decoder_info_t dec_info_video_mpeg4     = { .supported_types = video_types_mpeg4,  .priority = 8 };

/*
 * The plugin catalog entry. This is the only information that a plugin
 * will export to the public.
 */
const plugin_info_t xine_plugin_info[] EXPORTED = {
  /* { type, API, "name", version, special_info, init_function } */
  { PLUGIN_VIDEO_DECODER,
    19, "vdpau_h264_alter", XINE_VERSION_CODE, &dec_info_video_alterh264, h264_alter_init_plugin },
  { PLUGIN_VIDEO_DECODER,
    19, "vdpau_h264",       XINE_VERSION_CODE, &dec_info_video_h264,      h264_init_plugin },
  { PLUGIN_VIDEO_DECODER,
    19, "vdpau_vc1",        XINE_VERSION_CODE, &dec_info_video_vc1,       vc1_init_plugin },
  { PLUGIN_VIDEO_DECODER,
    19, "vdpau_mpeg12",     XINE_VERSION_CODE, &dec_info_video_mpeg12,    mpeg12_init_plugin },
  { PLUGIN_VIDEO_DECODER,
    19, "vdpau_mpeg4",      XINE_VERSION_CODE, &dec_info_video_mpeg4,     mpeg4_init_plugin },
  { PLUGIN_NONE, 0, NULL, 0, NULL, NULL }
};

