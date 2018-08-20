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
 * input plugin TLS helpers
 */

#ifndef XINE_INPUT_TLS_H
#define XINE_INPUT_TLS_H

/*
 * xine_tls layer is a simple wrapper for _x_io_tcp_*() functions.
 * _x_io_*() functions can be simply replaced with _x_tls_*() functions.
 *
 * Connection is unencrypted until _x_tls_handshake() is called.
 * Unencrypted TCP connection can be used even if TLS support was not compiled in.
 */

#include <xine/xine_internal.h>

typedef struct xine_tls xine_tls_t;

/*
 * TCP connection
 */

int _x_tls_available(xine_t *xine);

xine_tls_t *_x_tls_connect(xine_t *xine, xine_stream_t *stream, const char *host, int port);
xine_tls_t *_x_tls_init(xine_t *xine, xine_stream_t *stream, int fd);
void        _x_tls_close(xine_tls_t **);  /* note: associated socket is also closed */

ssize_t _x_tls_read(xine_tls_t *, void *data, size_t len);
ssize_t _x_tls_write(xine_tls_t *, const void *data, size_t len);
ssize_t _x_tls_read_line(xine_tls_t *, char *buf, size_t buf_size);

/*
 * TLS
 */

/**
 * Initialize TLS
 *
 * @param host  Host name to check certificate against (may be NULL ex. if numeric address).
 * @param verify  verify certificate. 0 - no, 1 - yes, < 0 - fetch value from xine config.
 * @return < 0 on error.
 */
int _x_tls_handshake(xine_tls_t *, const char *host, int verify);

/**
 * Shutdown TLS (stop encryption).
 *
 * Underlying socket remains open.
 */
void _x_tls_shutdown(xine_tls_t *);

/*
 * config helpers
 */

int _x_tls_get_verify_tls_cert(config_values_t *);

#endif /* XINE_INPUT_TLS_H */
