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

#include <xine/xine_internal.h>

#include "input_helper.h"

/*
 * mrl array alloc / free helpers
 */

void _x_input_free_mrls(xine_mrl_t ***p)
{
  if (*p) {
    xine_mrl_t **mrls;
    for (mrls = *p; *mrls; mrls++) {
      MRL_ZERO(*mrls);
    }
    _x_freep(p);
  }
}

xine_mrl_t **_x_input_alloc_mrls(size_t n)
{
  const size_t align = offsetof(struct { char dummy; xine_mrl_t mrl; }, mrl);
  xine_mrl_t **mrls;
  void *mem;
  size_t size = (n + 1) * (sizeof(*mrls) + sizeof(**mrls));
  size_t i;

  mrls = mem = calloc(1, size);
  if (!mem) {
    return NULL;
  }

  mem = (uint8_t*)mem + (n + 1) * sizeof(*mrls);       /* skip pointer array (including terminating NULL) */
  mem = (void *)((intptr_t)mem + (align - 1) % align); /* align */
  for (i = 0; i < n; i++) {
    mrls[i] = (xine_mrl_t *)((intptr_t)mem + i * sizeof(xine_mrl_t));
  }

  return mrls;
}

xine_mrl_t **_x_input_realloc_mrls(xine_mrl_t ***p, size_t n)
{
  xine_mrl_t **old_m = *p;
  xine_mrl_t **new_m;
  size_t old_n;

  if (!old_m) {
    return _x_input_alloc_mrls(n);
  }

  /* count old entries */
  for (old_n = 0; old_m[old_n]; old_n++) { }

  /* does not grow ? */
  if (old_n >= n)
    return *p;

  /* alloc new array */
  new_m = _x_input_alloc_mrls(n);
  if (!new_m)
    return NULL;

  /* copy old entries */
  for (n = 0; old_m[n]; n++) {
    *(new_m[n]) = *(old_m[n]);
  }

  /* store result */
  free(*p);
  *p = new_m;

  return *p;
}

/*
 * config helpers
 */

void _x_input_register_show_hidden_files(config_values_t *config)
{
  config->register_bool(config,
                        "media.files.show_hidden_files",
                        0, _("list hidden files"),
                        _("If enabled, the browser to select the file to "
                          "play will also show hidden files."),
                        10, NULL, NULL);
}

int _x_input_get_show_hidden_files(config_values_t *config)
{
  cfg_entry_t *entry;

  entry = config->lookup_entry(config, "media.files.show_hidden_files");
  if (entry) {
    return entry->num_value;
  }
  return 1;
}

void _x_input_register_default_servers(config_values_t *config)
{
  config->register_string(config,
                          "media.servers",
                          "",
                          _("Default servers"),
                          _("List of space-separated server urls for media browser. "
                            "(ex. \"ftp://ftp3.itu.int sftp://user:pass@host.com\")" ),
                          10, NULL, NULL);
}

xine_mrl_t **_x_input_get_default_server_mrls(config_values_t *config, const char *type, int *nFiles)
{
  xine_mrl_t **mrls;
  cfg_entry_t *entry;
  char *svrs, *pt;
  size_t n, type_len;

  *nFiles = 0;

  entry = config->lookup_entry(config, "media.servers");
  if (!entry || !entry->str_value)
    return NULL;

  svrs = strdup(entry->str_value);
  type_len = strlen(type);

  /* count entries */
  for (n = 1, pt = svrs; pt; pt = strchr(pt + 1, ' '), n++) { }
  mrls = _x_input_alloc_mrls(n);
  if (!mrls)
    return NULL;

  for (n = 0, pt = svrs; pt; ) {
    char *svr = pt;
    pt = strchr(pt, ' ');
    if (pt)
      *(pt++) = 0;
    if (!strncmp(svr, type, type_len)) {
      mrls[n]->type = mrl_net | mrl_file | mrl_file_directory;
      mrls[n]->origin = strdup(type);
      mrls[n]->mrl    = strdup(svr);
      n++;
    }
  }

  *nFiles = n;
  free(svrs);
  return mrls;
}

/*
 * default read_block function.
 * uses read() to fill the block.
 */
buf_element_t *_x_input_default_read_block (input_plugin_t *this_gen, fifo_buffer_t *fifo, off_t todo)
{
  buf_element_t *buf;
  off_t          total_bytes;

  if (todo < 0)
    return NULL;

  buf = fifo->buffer_pool_size_alloc (fifo, todo);

  if (todo > buf->max_size)
    todo = buf->max_size;

  buf->content = buf->mem;
  buf->type = BUF_DEMUX_BLOCK;

  total_bytes = this_gen->read (this_gen, buf->content, todo);

  if (total_bytes != todo) {
    buf->free_buffer (buf);
    return NULL;
  }

  buf->size = total_bytes;

  return buf;
}
