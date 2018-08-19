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
 * This file contains plugin entries for several demuxers used in games
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <xine/xine_internal.h>
#include <xine/xine_plugin.h>

#include "group_dxr3.h"

/*
 * exported plugin catalog entries
 */

static const uint32_t supported_types_spu[] = {
  BUF_SPU_DVD,
  0,
};

static const decoder_info_t dxr3_spudec_info = {
  .supported_types = supported_types_spu,
  .priority        = 10,
};

static const uint32_t supported_types_video[] = {
  BUF_VIDEO_MPEG,
  0,
};

static const decoder_info_t dxr3_video_decoder_info = {
  .supported_types = supported_types_video,
  .priority        = 10,
};

#ifdef HAVE_X11
static const vo_info_t vo_info_dxr3_x11 = {
  .priority    = 10,
  .visual_type = XINE_VISUAL_TYPE_X11,
};
#endif

static const vo_info_t vo_info_dxr3_aa = {
  .priority    = 10,
  .visual_type = XINE_VISUAL_TYPE_AA,
};

const plugin_info_t xine_plugin_info[] EXPORTED = {
  /* type, API, "name", version, special_info, init_function */
  { PLUGIN_VIDEO_DECODER, 19, "dxr3-mpeg2",  XINE_VERSION_CODE, &dxr3_video_decoder_info, &dxr3_video_init_plugin },
  { PLUGIN_SPU_DECODER,   17, "dxr3-spudec", XINE_VERSION_CODE, &dxr3_spudec_info,        &dxr3_spudec_init_plugin },
#ifdef HAVE_X11
  { PLUGIN_VIDEO_OUT,     22, "dxr3",        XINE_VERSION_CODE, &vo_info_dxr3_x11,        &dxr3_x11_init_plugin },
#endif
  { PLUGIN_VIDEO_OUT,     22, "aadxr3",      XINE_VERSION_CODE, &vo_info_dxr3_aa,         &dxr3_aa_init_plugin },
  { PLUGIN_NONE, 0, NULL, 0, NULL, NULL }
};
