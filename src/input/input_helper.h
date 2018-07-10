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

#include <errno.h>
#include <sys/types.h>

#include <xine/attributes.h>
#include <xine/xine_internal.h>
#include <xine/xineutils.h>

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
  (void)this_gen;
  return INPUT_CAP_PREVIEW;
}

static inline uint32_t _x_input_get_capabilities_seekable (input_plugin_t *this_gen)
{
  (void)this_gen;
  return INPUT_CAP_SEEKABLE;
}

static inline uint32_t _x_input_get_capabilities_none (input_plugin_t *this_gen)
{
  (void)this_gen;
  return INPUT_CAP_NOCAP;
}

static inline uint32_t _x_input_default_get_blocksize (input_plugin_t *this_gen)
{
  (void)this_gen;
  return 0;
}

static inline off_t _x_input_default_get_length (input_plugin_t *this_gen)
{
  (void)this_gen;
  return 0;
}

static inline int _x_input_default_get_optional_data (input_plugin_t *this_gen, void *data, int data_type)
{
  (void)this_gen;
  (void)data;
  (void)data_type;
  return INPUT_OPTIONAL_UNSUPPORTED;
}

/*
 * translate (offset, origin) to absolute position
 */
static inline off_t _x_input_translate_seek(off_t offset, int origin, off_t curpos, off_t length)
{
  switch (origin) {
    case SEEK_SET: break;
    case SEEK_CUR: offset += curpos; break;
    case SEEK_END: offset = (length <= 0) ? (-1) : (offset + length); break;
    default:       offset = -1;  break;
  }

  if (offset < 0 || (length > 0 && offset > length)) {
    errno = EINVAL;
    return (off_t)-1;
  }

  return offset;
}

/*
 * seek forward by skipping data
 */
#define MAX_SKIP_BYTES (10*1024*1024)  // 10 MB
static inline int _x_input_read_skip(input_plugin_t *input, off_t bytes)
{
  char buf[1024];
  const off_t max = sizeof(buf);

  _x_assert(bytes >= 0);

  if (bytes > MAX_SKIP_BYTES) {
    /* seeking forward gigabytes would take long time ... */
    return -1;
  }

  while (bytes > 0) {
    off_t got = input->read(input, buf, (bytes > max) ? max : bytes);
    if (got <= 0)
      return -1;
    bytes -= got;
  }

  _x_assert(bytes == 0);
  return 0;
}

/*
 * generic seek function for non-seekable input plugins
 */
static inline off_t _x_input_seek_preview(input_plugin_t *input, off_t offset, int origin,
                                          off_t *curpos, off_t length, off_t preview_size)
{
  offset = _x_input_translate_seek(offset, origin, *curpos, length);
  if (offset < 0)
    goto fail;

  /* seek inside preview */
  if (offset <= preview_size && *curpos <= preview_size) {
    *curpos = offset;
    return offset;
  }

  /* can't seek back */
  if (offset < *curpos)
    goto fail;

  if (_x_input_read_skip(input, offset - *curpos) < 0)
    return -1;

  _x_assert(offset == *curpos);
  return offset;

 fail:
  errno = EINVAL;
  return (off_t)-1;
}

#endif /* XINE_INPUT_HELPER_H */
