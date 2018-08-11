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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "planar.h"

#include <xine/xine_plugin.h>
#include <xine/post.h>

static const post_info_t gen_special_info = {
  .type = XINE_POST_TYPE_VIDEO_FILTER,
};

const plugin_info_t xine_plugin_info[] EXPORTED = {
  /* type, API, "name", version, special_info, init_function */
  { PLUGIN_POST, 10, "boxblur",   XINE_VERSION_CODE, &gen_special_info, &boxblur_init_plugin },
  { PLUGIN_POST, 10, "denoise3d", XINE_VERSION_CODE, &gen_special_info, &denoise3d_init_plugin },
  { PLUGIN_POST, 10, "eq",        XINE_VERSION_CODE, &gen_special_info, &eq_init_plugin },
  { PLUGIN_POST, 10, "eq2",       XINE_VERSION_CODE, &gen_special_info, &eq2_init_plugin },
  { PLUGIN_POST, 10, "expand",    XINE_VERSION_CODE, &gen_special_info, &expand_init_plugin },
  { PLUGIN_POST, 10, "fill",      XINE_VERSION_CODE, &gen_special_info, &fill_init_plugin },
  { PLUGIN_POST, 10, "invert",    XINE_VERSION_CODE, &gen_special_info, &invert_init_plugin },
  { PLUGIN_POST, 10, "noise",     XINE_VERSION_CODE, &gen_special_info, &noise_init_plugin },
#ifdef HAVE_POSTPROC
  { PLUGIN_POST, 10, "pp",        XINE_VERSION_CODE, &gen_special_info, &pp_init_plugin },
#endif
  { PLUGIN_POST, 10, "unsharp",   XINE_VERSION_CODE, &gen_special_info, &unsharp_init_plugin },
  { PLUGIN_NONE, 0, NULL, 0, NULL, NULL }
};
