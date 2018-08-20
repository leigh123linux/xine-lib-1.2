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

/*
 * xine TLS provider plugin (using gnutls)
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdlib.h>
#include <errno.h>

#include <gnutls/gnutls.h>
#include <gnutls/x509.h>

#define LOG_MODULE "gnutls"

#include <xine/xine_internal.h>
#include <xine/xineutils.h>
#include <xine/io_helper.h>

#include <xine/xine_plugin.h>

#include "xine_tls_plugin.h"


typedef struct {
  tls_plugin_t tls_plugin;

  xine_stream_t *stream;
  xine_t        *xine;

  int fd;
  int need_shutdown;

  gnutls_session_t session;
  gnutls_certificate_credentials_t cred;

} tls_gnutls_t;

/*
 * gnutls I/O callbacks
 */

static ssize_t gnutls_tcp_pull(gnutls_transport_ptr_t tp,
                               void *buf, size_t len)
{
  tls_gnutls_t *this = (tls_gnutls_t *)tp;
  return _x_io_tcp_read(this->stream, this->fd, buf, len);
}

static ssize_t gnutls_tcp_push(gnutls_transport_ptr_t tp,
                               const void *buf, size_t len)
{
  tls_gnutls_t *this = (tls_gnutls_t *)tp;
  return _x_io_tcp_write(this->stream, this->fd, buf, len);
}

/*
 *
 */

static int handle_gnutls_error(tls_gnutls_t *this, int err)
{
  switch (err) {
    case GNUTLS_E_AGAIN:
      errno = EAGAIN;
      return -1;
    default:
      xprintf(this->xine, XINE_VERBOSITY_LOG, LOG_MODULE ": %s\n", gnutls_strerror(err));
      break;
  }

  errno = EIO;
  return -1;
}

/*
 *
 */

static ssize_t _gnutls_write(tls_plugin_t *this_gen, const void *buf, size_t len)
{
  tls_gnutls_t *this = (tls_gnutls_t *)this_gen;
  int ret;

  if (!this->session)
    return -1;

  ret = gnutls_record_send(this->session, buf, len);
  if (ret < 0)
    return handle_gnutls_error(this, ret);
  return ret;
}

static ssize_t _gnutls_read(tls_plugin_t *this_gen, void *buf, size_t len)
{
  tls_gnutls_t *this = (tls_gnutls_t *)this_gen;
  int ret;

  if (!this->session)
    return -1;

  ret = gnutls_record_recv(this->session, buf, len);
  if (ret < 0)
    return handle_gnutls_error(this, ret);
  return ret;
}

static void _gnutls_shutdown(tls_plugin_t *this_gen)
{
  tls_gnutls_t *this = (tls_gnutls_t *)this_gen;

  if (this->need_shutdown) {
    gnutls_bye(this->session, GNUTLS_SHUT_WR);
    this->need_shutdown = 0;
  }
  if (this->session) {
    gnutls_deinit(this->session);
    this->session = NULL;
  }
  if (this->cred) {
    gnutls_certificate_free_credentials(this->cred);
    this->cred = NULL;
  }
}

static int _gnutls_handshake(tls_plugin_t *this_gen, const char *host, int verify)
{
  tls_gnutls_t *this = (tls_gnutls_t *)this_gen;
  int ret;

  _x_assert(this->session == NULL);

  gnutls_init(&this->session, GNUTLS_CLIENT);
  if (host) {
    gnutls_server_name_set(this->session, GNUTLS_NAME_DNS, host, strlen(host));
  }

  gnutls_certificate_allocate_credentials(&this->cred);
  gnutls_certificate_set_x509_system_trust(this->cred);
  gnutls_certificate_set_verify_flags(this->cred, verify ? GNUTLS_VERIFY_ALLOW_X509_V1_CA_CRT : 0);
  gnutls_credentials_set(this->session, GNUTLS_CRD_CERTIFICATE, this->cred);

  gnutls_transport_set_pull_function(this->session, gnutls_tcp_pull);
  gnutls_transport_set_push_function(this->session, gnutls_tcp_push);
  gnutls_transport_set_ptr(this->session, this);

  gnutls_priority_set_direct(this->session, "NORMAL", NULL);

  ret = gnutls_handshake(this->session);
  if (ret) {
    xprintf(this->xine, XINE_VERBOSITY_LOG, LOG_MODULE ": "
            "TLS handshake failed: %s (%d)\n",
            gnutls_strerror(ret), ret);
    return -1;
  }

  this->need_shutdown = 1;

  if (verify < 0 && this->xine)
    verify = tls_get_verify_tls_cert(this->xine->config);

  if (verify) {
    unsigned int status;
    if ((ret = gnutls_certificate_verify_peers2(this->session, &status)) < 0) {
      xprintf(this->xine, XINE_VERBOSITY_LOG, LOG_MODULE ": "
              "Unable to verify peer certificate: %s (%d)\n",
              gnutls_strerror(ret), ret);
      return -2;
    }
    if (status & GNUTLS_CERT_INVALID) {
      xprintf(this->xine, XINE_VERBOSITY_LOG, LOG_MODULE ": "
              "Peer certificate failed verification\n");
      return -2;
    }
    if (gnutls_certificate_type_get(this->session) != GNUTLS_CRT_X509) {
      xprintf(this->xine, XINE_VERBOSITY_LOG, LOG_MODULE ": "
              "Unsupported certificate type\n");
      return -2;
    }
    if (host) {
      unsigned int          cert_list_size;
      gnutls_x509_crt_t     cert;
      const gnutls_datum_t *cert_list;
      gnutls_x509_crt_init(&cert);
      cert_list = gnutls_certificate_get_peers(this->session, &cert_list_size);
      gnutls_x509_crt_import(cert, cert_list, GNUTLS_X509_FMT_DER);
      ret = gnutls_x509_crt_check_hostname(cert, host);
      gnutls_x509_crt_deinit(cert);
      if (!ret) {
        xprintf(this->xine, XINE_VERBOSITY_LOG, LOG_MODULE ": "
                "The certificate does not match hostname %s\n",
                host);
        return -3;
      }
    }
  }

  return 0;
}

static void _gnutls_dispose(xine_module_t *this_gen)
{
  _gnutls_shutdown((tls_plugin_t*)this_gen);

  gnutls_global_deinit();

  free(this_gen);
}

static xine_module_t *gnutls_get_instance(xine_module_class_t *cls_gen, const void *params_gen)
{
  const tls_plugin_params_t *p = params_gen;
  tls_gnutls_t *this;
  int ret;

  ret = gnutls_global_init();
  if (ret) {
    xprintf(p->xine, XINE_VERBOSITY_LOG, LOG_MODULE ": "
            "gnutls_global_init() failed: %s (%d)\n",
            gnutls_strerror(ret), ret);
    return NULL;
  }

  this = calloc(1, sizeof(*this));
  if (!this) {
    gnutls_global_deinit();
    return NULL;
  }

  this->tls_plugin.module.dispose = _gnutls_dispose;

  this->tls_plugin.handshake = _gnutls_handshake;
  this->tls_plugin.shutdown  = _gnutls_shutdown;
  this->tls_plugin.read      = _gnutls_read;
  this->tls_plugin.write     = _gnutls_write;

  this->xine   = p->xine;
  this->fd     = p->fd;
  this->stream = p->stream;

  return &this->tls_plugin.module;
}

static void *gnutls_init_class(xine_t *xine, const void *data)
{
  static const xine_module_class_t tls_gnutls_class = {
    .get_instance      = gnutls_get_instance,
    .description       = N_("TLS provider (gnutls)"),
    .identifier        = "gnutls",
    .dispose           = NULL,
  };

  tls_register_config_keys(xine->config);

  return (void *)&tls_gnutls_class;
}

/*
 * exported plugin catalog entry
 */

static const xine_module_info_t module_info_gnutls = {
  .priority = 10,
  .type     = "tls_v1",
};

const plugin_info_t xine_plugin_info[] EXPORTED = {
  /* type, API, "name", version, special_info, init_function */
  { PLUGIN_XINE_MODULE, 1, "gnutls", XINE_VERSION_CODE, &module_info_gnutls, gnutls_init_class },
  { PLUGIN_NONE, 0, "", 0, NULL, NULL }
};
