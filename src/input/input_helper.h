/*
 * Copyright (C) 2000-2018 the xine project
 * Copyright (C) 2018      Petri Hintukainen <phintuka@users.sourceforge.net>
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
 * input plugin helper functions
 */

#ifndef XINE_INPUT_HELPER_H
#define XINE_INPUT_HELPER_H

#include <xine/attributes.h>
#include <xine/xine_internal.h>

/*
 * mrl array alloc / free helpers
 */

void _x_input_free_mrls(xine_mrl_t ***p);
xine_mrl_t **_x_input_alloc_mrls(size_t n);
xine_mrl_t **_x_input_realloc_mrls(xine_mrl_t ***p, size_t n);

void _x_input_sort_mrls(xine_mrl_t **mrls, ssize_t cnt /* optional, may be -1 */);

/*
 * config helpers
 */

void _x_input_register_show_hidden_files(config_values_t *config);
int _x_input_get_show_hidden_files(config_values_t *config);

void _x_input_register_default_servers(config_values_t *config);
xine_mrl_t **_x_input_get_default_server_mrls(config_values_t *config, const char *type, int *nFiles);

/*
 * default read_block function.
 * uses read() to fill the block.
 */
buf_element_t *_x_input_default_read_block (input_plugin_t *this_gen, fifo_buffer_t *fifo, off_t todo);

static inline uint32_t _x_input_get_capabilities_preview (input_plugin_t *this_gen)
{
  return INPUT_CAP_PREVIEW;
}

static inline uint32_t _x_input_get_capabilities_seekable (input_plugin_t *this_gen)
{
  return INPUT_CAP_SEEKABLE;
}

static inline uint32_t _x_input_default_get_blocksize (input_plugin_t *this_gen)
{
  return 0;
}

static inline int _x_input_default_get_optional_data (input_plugin_t *this_gen, void *data, int data_type)
{
  return INPUT_OPTIONAL_UNSUPPORTED;
}

#endif /* XINE_INPUT_HELPER_H */
