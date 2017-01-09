/*
 * Copyright (C) 2000-2017 the xine project
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
 * Collect some very basic plugins when building them into libxine.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#ifdef XINE_MAKE_BUILTINS

#include "builtins.h"

/*
 * Some people call this technique "the amalgamation".
 * It serves to improve optimization.
 * Here, it mainly serves to avoid cross directory build trouble with automake.
 * Anyway, it requires a certain naming discipline from the participants.
 */

#undef LOG_MODULE
#include "../input/net_buf_ctrl.c"
#undef LOG_MODULE
#include "../input/input_file.c"
#undef LOG_MODULE
#include "../input/input_stdin_fifo.c"
#undef LOG_MODULE
#include "../input/input_test.c"
#undef LOG_MODULE
#include "../video_out/video_out_none.c"
#undef LOG_MODULE
#include "../audio_out/audio_none_out.c"
#undef LOG_MODULE
#include "../audio_out/audio_file_out.c"

const plugin_info_t xine_builtin_plugin_info[] = {
  INPUT_FILE_CATALOG,
  INPUT_STDIN_CATALOG,
  INPUT_TEST_CATALOG,
  VO_NONE_CATALOG,
  AO_NONE_CATALOG,
  AO_FILE_CATALOG,
  { PLUGIN_NONE, 0, "", 0, NULL, NULL }
};

#endif
