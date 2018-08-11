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
 * This file contains plugin entries for several visualization post plugins.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <xine/xine_internal.h>
#include <xine/post.h>

#include "visualizations.h"

/*
 * exported plugin catalog entries
 */

/* plugin catalog information */
static const post_info_t gen_special_info = {
  .type = XINE_POST_TYPE_AUDIO_VISUALIZATION,
};

const plugin_info_t xine_plugin_info[] EXPORTED = {
  /* type, API, "name", version, special_info, init_function */
  { PLUGIN_POST, 10, "oscope",          XINE_VERSION_CODE, &gen_special_info, &oscope_init_plugin },
  { PLUGIN_POST, 10, "fftscope",        XINE_VERSION_CODE, &gen_special_info, &fftscope_init_plugin },
  { PLUGIN_POST, 10, "fftgraph",        XINE_VERSION_CODE, &gen_special_info, &fftgraph_init_plugin },
  { PLUGIN_POST, 10, "tdaudioanalyzer", XINE_VERSION_CODE, &gen_special_info, &tdaan_init_plugin },
  { PLUGIN_NONE, 0, NULL, 0, NULL, NULL }
};
