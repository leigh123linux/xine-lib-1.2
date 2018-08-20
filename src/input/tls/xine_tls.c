/*
 * Copyright (C) 2000-2018 the xine project
 * Copyright (C) 2018 Petri Hintukainen <phintuka@users.sourceforge.net>
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
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#define LOG_MODULE "input_tls"

#include "xine_tls.h"

#include <stdlib.h>
#include <errno.h>

#include <xine/xine_internal.h>
#include <xine/xineutils.h>
#include <xine/io_helper.h>

#include "xine_tls_plugin.h"


struct xine_tls {
  xine_t        *xine;
  xine_stream_t *stream;
  int            fd;

  tls_plugin_t *tls;

  int enabled;
};

/*
 * loader wrappers
 */

static inline tls_plugin_t *_x_find_tls_plugin(xine_t *xine, tls_plugin_params_t *params)
{
  return (tls_plugin_t *)_x_find_module(xine, "tls_v1", NULL, 0, params);
}

static inline void _x_free_tls_plugin(xine_t *xine, tls_plugin_t **tls)
{
  xine_module_t **m = (xine_module_t **)tls;
  _x_free_module(xine, m);
}

/*
 *
 */

ssize_t _x_tls_write(xine_tls_t *t, const void *buf, size_t len)
{
  if (t->tls && t->enabled)
    return t->tls->write(t->tls, buf, len);

  return _x_io_tcp_write(t->stream, t->fd, buf, len);
}

ssize_t _x_tls_read(xine_tls_t *t, void *buf, size_t len)
{
  if (t->tls && t->enabled)
    return t->tls->read(t->tls, buf, len);

  return _x_io_tcp_read(t->stream, t->fd, buf, len);
}

ssize_t _x_tls_read_line(xine_tls_t *t, char *buf, size_t buf_size)
{
  if (t->enabled) {
    unsigned int i = 0;
    ssize_t r;
    char c;

    if (buf_size <= 0)
      return 0;

    while ((r = _x_tls_read(t, &c, 1)) == 1) {
      if (c == '\r' || c == '\n')
        break;
      if (i+1 == buf_size)
        break;

      buf[i] = c;
      i++;
    }

    if (r == 1 && c == '\r')
      r = _x_tls_read(t, &c, 1);

    buf[i] = '\0';

    return (r >= 0) ? (ssize_t)i : r;
  }

  return _x_io_tcp_read_line(t->stream, t->fd, buf, buf_size);
}


/*
 * open / close
 */

void _x_tls_shutdown(xine_tls_t *t)
{
  if (!t->enabled)
    return;

  t->enabled = 0;

  if (t->tls)
    t->tls->shutdown(t->tls);
}

void _x_tls_close(xine_tls_t **pt)
{
  xine_tls_t *t = *pt;
  if (t) {

    _x_tls_shutdown(t);

    if (t->tls) {
      _x_free_tls_plugin(t->xine, &t->tls);
    }
    if (t->fd) {
      _x_io_tcp_close(t->stream, t->fd);
      t->fd = -1;
    }

    _x_freep(pt);
  }
}

xine_tls_t *_x_tls_init(xine_t *xine, xine_stream_t *stream, int fd)
{
  xine_tls_t *t;

  t = calloc(1, sizeof(*t));
  if (!t) {
    return NULL;
  }

  t->stream = stream;
  t->xine   = xine;
  t->fd     = fd;

  return t;
}

xine_tls_t *_x_tls_connect(xine_t *xine, xine_stream_t *stream, const char *host, int port)
{
  xine_tls_t *tls;
  int fh;

  fh  = _x_io_tcp_connect(stream, host, port);
  if (fh == -1) {
    return NULL;
  }

  tls = _x_tls_init(xine, stream, fh);
  if (!tls) {
    _x_io_tcp_close(stream, fh);
  }

  return tls;
}

int _x_tls_handshake(xine_tls_t *t, const char *host, int verify)
{
  int ret;

  if (!t->tls) {
    tls_plugin_params_t params = {
      .xine   = t->xine,
      .stream = t->stream,
      .fd     = t->fd,
    };

    t->tls = _x_find_tls_plugin(t->xine, &params);
    if (!t->tls) {
      xprintf(t->xine, XINE_VERBOSITY_LOG, LOG_MODULE ": "
              "TLS plugin not found\n");
      return -1;
    }
  }

  ret = t->tls->handshake(t->tls, host, verify);
  if (ret < 0)
    return ret;

  t->enabled = 1;
  return 0;
}

int _x_tls_get_verify_tls_cert(config_values_t *config)
{
  return tls_get_verify_tls_cert(config);
}

int _x_tls_available(xine_t *xine)
{
  tls_plugin_params_t p = {
    .xine = xine,
    .fd = -1,
  };
  tls_plugin_t *tls = _x_find_tls_plugin(xine, &p);
  if (!tls)
    return 0;
  _x_free_tls_plugin(xine, &tls);
  return 1;
}
