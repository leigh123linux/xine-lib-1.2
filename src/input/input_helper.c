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

#include <stdlib.h>

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
 * Sorting function, it comes from GNU fileutils package.
 */
#define S_N        0x0
#define S_I        0x4
#define S_F        0x8
#define S_Z        0xC
#define CMP          2
#define LEN          3
#define ISDIGIT(c)   ((unsigned) (c) - '0' <= 9)
static int _input_strverscmp (const char *s1, const char *s2) {
  const unsigned char *p1 = (const unsigned char *) s1;
  const unsigned char *p2 = (const unsigned char *) s2;
  unsigned char c1, c2;
  int state;
  int diff;
  static const unsigned int next_state[] = {
    S_N, S_I, S_Z, S_N,
    S_N, S_I, S_I, S_I,
    S_N, S_F, S_F, S_F,
    S_N, S_F, S_Z, S_Z
  };
  static const int result_type[] = {
    CMP, CMP, CMP, CMP, CMP, LEN, CMP, CMP,
    CMP, CMP, CMP, CMP, CMP, CMP, CMP, CMP,
    CMP,  -1,  -1, CMP,   1, LEN, LEN, CMP,
      1, LEN, LEN, CMP, CMP, CMP, CMP, CMP,
    CMP, CMP, CMP, CMP, CMP, LEN, CMP, CMP,
    CMP, CMP, CMP, CMP, CMP, CMP, CMP, CMP,
    CMP,   1,   1, CMP,  -1, CMP, CMP, CMP,
     -1, CMP, CMP, CMP
  };

  if(p1 == p2)
    return 0;

  c1 = *p1++;
  c2 = *p2++;

  state = S_N | ((c1 == '0') + (ISDIGIT(c1) != 0));

  while((diff = c1 - c2) == 0 && c1 != '\0') {
    state = next_state[state];
    c1 = *p1++;
    c2 = *p2++;
    state |= (c1 == '0') + (ISDIGIT(c1) != 0);
  }

  state = result_type[state << 2 | ((c2 == '0') + (ISDIGIT(c2) != 0))];

  switch(state) {
  case CMP:
    return diff;

  case LEN:
    while(ISDIGIT(*p1++))
      if(!ISDIGIT(*p2++))
        return 1;

    return ISDIGIT(*p2) ? -1 : diff;

  default:
    return state;
  }
}

static int _mrl_cmp (const void *p1, const void *p2)
{
  const xine_mrl_t * const *m1 = p1;
  const xine_mrl_t * const *m2 = p2;
  int dir = ((*m2)->type & mrl_file_directory) - ((*m1)->type & mrl_file_directory);
  if (dir) {
    return dir;
  }
  return _input_strverscmp((*m1)->mrl, (*m2)->mrl);
}

void _x_input_sort_mrls(xine_mrl_t **mrls, ssize_t cnt)
{
  _x_assert(mrls);

  /* count entries */
  if (cnt < 0)
    for (cnt = 0; mrls[cnt]; cnt++) ;

  if (cnt < 2)
    return;

  qsort(mrls, cnt, sizeof(xine_mrl_t *), _mrl_cmp);
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

  if (n > 1)
    _x_input_sort_mrls(mrls, n);

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
