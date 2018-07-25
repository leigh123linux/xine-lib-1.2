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

#ifdef HAVE_GNUTLS
#include <gnutls/gnutls.h>
#include <gnutls/x509.h>
#endif /* HAVE_GNUTLS */

#include <xine/xine_internal.h>
#include <xine/xineutils.h>
#include <xine/io_helper.h>


struct xine_tls {
  xine_t        *xine;
  xine_stream_t *stream;
  int            fd;

#ifdef HAVE_GNUTLS
  int enabled;
  int need_shutdown;

  gnutls_session_t session;
  gnutls_certificate_credentials_t cred;
#endif /* HAVE_GNUTLS */
};

#ifdef HAVE_GNUTLS
static int handle_gnutls_error(xine_tls_t *t, int err)
{
  switch (err) {
    case GNUTLS_E_AGAIN:
      errno = EAGAIN;
      return -1;
    default:
      xprintf(t->xine, XINE_VERBOSITY_LOG, LOG_MODULE ": %s\n", gnutls_strerror(err));
      break;
  }

  errno = EIO;
  return -1;
}
#endif /* HAVE_GNUTLS */

ssize_t _x_tls_write(xine_tls_t *t, const void *buf, size_t len)
{
#ifdef HAVE_GNUTLS
  if (t->enabled) {
    int ret = gnutls_record_send(t->session, buf, len);
    if (ret < 0)
      return handle_gnutls_error(t, ret);
    return ret;
  }
#endif /* HAVE_GNUTLS */

  return _x_io_tcp_write(t->stream, t->fd, buf, len);
}

ssize_t _x_tls_read(xine_tls_t *t, void *buf, size_t len)
{
#ifdef HAVE_GNUTLS
  if (t->enabled) {
    int ret = gnutls_record_recv(t->session, buf, len);
    if (ret < 0)
      return handle_gnutls_error(t, ret);
    return ret;
  }
#endif /* HAVE_GNUTLS */

  return _x_io_tcp_read(t->stream, t->fd, buf, len);
}

ssize_t _x_tls_read_line(xine_tls_t *t, char *buf, size_t buf_size)
{
#ifdef HAVE_GNUTLS
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
#endif /* HAVE_GNUTLS */

  return _x_io_tcp_read_line(t->stream, t->fd, buf, buf_size);
}

/*
 * gnutls I/O callbacks
 */

#ifdef HAVE_GNUTLS
static ssize_t gnutls_tcp_pull(gnutls_transport_ptr_t tp,
                               void *buf, size_t len)
{
  xine_tls_t *t = (xine_tls_t *)tp;
  return _x_io_tcp_read(t->stream, t->fd, buf, len);
}
#endif /* HAVE_GNUTLS */

#ifdef HAVE_GNUTLS
static ssize_t gnutls_tcp_push(gnutls_transport_ptr_t tp,
                               const void *buf, size_t len)
{
  xine_tls_t *t = (xine_tls_t *)tp;
  return _x_io_tcp_write(t->stream, t->fd, buf, len);
}
#endif /* HAVE_GNUTLS */

/*
 * open / close
 */

void _x_tls_shutdown(xine_tls_t *t)
{
#ifdef HAVE_GNUTLS
  if (t->need_shutdown) {
    gnutls_bye(t->session, GNUTLS_SHUT_WR);
    t->need_shutdown = 0;
  }
  if (t->session) {
    gnutls_deinit(t->session);
    t->session = NULL;
  }
  if (t->cred) {
    gnutls_certificate_free_credentials(t->cred);
    t->cred = NULL;
  }
  t->enabled = 0;

  gnutls_global_deinit();
#else
  (void)t;
#endif /* HAVE_GNUTLS */
}

void _x_tls_close(xine_tls_t **pt)
{
  xine_tls_t *t = *pt;
  if (t) {

    _x_tls_shutdown(t);

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
#ifndef HAVE_GNUTLS
  (void)host;
  (void)verify;
  xprintf(t->xine, XINE_VERBOSITY_LOG, LOG_MODULE ": "
          "No TLS support (gnutls disabled in configure)\n");
  return -1;
#else /* HAVE_GNUTLS */
  int ret;

  _x_assert(t->enabled == 0);

  ret = gnutls_global_init();
  if (ret) {
    xprintf(t->xine, XINE_VERBOSITY_LOG, LOG_MODULE ": "
            "gnutls_global_init() failed: %s (%d)\n",
            gnutls_strerror(ret), ret);
    return -1;
  }

  gnutls_init(&t->session, GNUTLS_CLIENT);
  if (host) {
    gnutls_server_name_set(t->session, GNUTLS_NAME_DNS, host, strlen(host));
  }

  gnutls_certificate_allocate_credentials(&t->cred);
  gnutls_certificate_set_x509_system_trust(t->cred);
  gnutls_certificate_set_verify_flags(t->cred, verify ? GNUTLS_VERIFY_ALLOW_X509_V1_CA_CRT : 0);
  gnutls_credentials_set(t->session, GNUTLS_CRD_CERTIFICATE, t->cred);

  gnutls_transport_set_pull_function(t->session, gnutls_tcp_pull);
  gnutls_transport_set_push_function(t->session, gnutls_tcp_push);
  gnutls_transport_set_ptr(t->session, t);

  gnutls_priority_set_direct(t->session, "NORMAL", NULL);

  ret = gnutls_handshake(t->session);
  if (ret) {
    xprintf(t->xine, XINE_VERBOSITY_LOG, LOG_MODULE ": "
            "TLS handshake failed: %s (%d)\n",
            gnutls_strerror(ret), ret);
    return -1;
  }

  t->need_shutdown = 1;

  if (verify < 0 && t->xine)
    verify = _x_tls_get_verify_tls_cert(t->xine->config);

  if (verify) {
    unsigned int status;
    if ((ret = gnutls_certificate_verify_peers2(t->session, &status)) < 0) {
      xprintf(t->xine, XINE_VERBOSITY_LOG, LOG_MODULE ": "
              "Unable to verify peer certificate: %s (%d)\n",
              gnutls_strerror(ret), ret);
      return -2;
    }
    if (status & GNUTLS_CERT_INVALID) {
      xprintf(t->xine, XINE_VERBOSITY_LOG, LOG_MODULE ": "
              "Peer certificate failed verification\n");
      return -2;
    }
    if (gnutls_certificate_type_get(t->session) != GNUTLS_CRT_X509) {
      xprintf(t->xine, XINE_VERBOSITY_LOG, LOG_MODULE ": "
              "Unsupported certificate type\n");
      return -2;
    }
    if (host) {
      unsigned int cert_list_size;
      gnutls_x509_crt_t cert;
      const gnutls_datum_t *cert_list;
      gnutls_x509_crt_init(&cert);
      cert_list = gnutls_certificate_get_peers(t->session, &cert_list_size);
      gnutls_x509_crt_import(cert, cert_list, GNUTLS_X509_FMT_DER);
      ret = gnutls_x509_crt_check_hostname(cert, host);
      gnutls_x509_crt_deinit(cert);
      if (!ret) {
        xprintf(t->xine, XINE_VERBOSITY_LOG, LOG_MODULE ": "
                "The certificate does not match hostname %s\n",
                host);
        return -3;
      }
    }
  }

  t->enabled = 1;
  return 0;
#endif /* HAVE_GNUTLS */
}

/*
 * config helpers
 */

void _x_tls_register_config_keys(config_values_t *config)
{
  config->register_bool(config,
                        "media.network.verify_tls_certificate",
                        1, _("Verify server TLS certificate"),
                        _("If enabled, server TLS certificate is always checked. "
                          "If check fails, connections to server are not allowed."),
                        10, NULL, NULL);
}

int _x_tls_get_verify_tls_cert(config_values_t *config)
{
  cfg_entry_t *entry;

  entry = config->lookup_entry(config, "media.network.verify_tls_certificate");
  if (entry) {
    return entry->num_value;
  }
  return 1;
}
