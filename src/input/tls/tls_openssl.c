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
 * xine TLS provider plugin (using openssl)
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdlib.h>
#include <pthread.h>

#include <openssl/bio.h>
#include <openssl/ssl.h>
#include <openssl/err.h>

#define LOG_MODULE "openssl"

#include <xine/xine_internal.h>
#include <xine/xineutils.h>
#include <xine/io_helper.h>

#include <xine/xine_plugin.h>

#include "xine_tls_plugin.h"


typedef struct {
  tls_plugin_t tls_plugin;

  xine_stream_t *stream;
  xine_t        *xine;

  int            fd;

  SSL_CTX       *ctx;
  SSL           *ssl;
#if OPENSSL_VERSION_NUMBER >= 0x1010000fL
  BIO_METHOD    *bio_method;
#endif

} tls_openssl_t;

typedef struct {
  xine_module_class_t module_class;

  pthread_mutex_t lock;
  int             inited;
} openssl_class_t;

/*
 * openssl I/O callbacks
 */

static int _bio_read(BIO *b, char *buf, int len)
{
#if OPENSSL_VERSION_NUMBER >= 0x1010000fL
  tls_openssl_t *this = BIO_get_data(b);
#else
  tls_openssl_t *this = b->ptr;
#endif
  return _x_io_tcp_read(this->stream, this->fd, buf, len);
}

static int _bio_write(BIO *b, const char *buf, int len)
{
#if OPENSSL_VERSION_NUMBER >= 0x1010000fL
  tls_openssl_t *this = BIO_get_data(b);
#else
  tls_openssl_t *this = b->ptr;
#endif
  return _x_io_tcp_write(this->stream, this->fd, buf, len);
}

static int _bio_puts(BIO *b, const char *str)
{
  return _bio_write(b, str, strlen(str));
}

static long _bio_ctrl(BIO *b, int cmd, long num, void *ptr)
{
  (void)b;
  (void)num;
  (void)ptr;

  if (cmd == BIO_CTRL_FLUSH)
    return 1;
  return 0;
}

static int _bio_create(BIO *b)
{
#if OPENSSL_VERSION_NUMBER >= 0x1010000fL
  BIO_set_init(b, 1);
  BIO_set_app_data(b, NULL);
  BIO_set_flags(b, 0);
#else
  b->init  = 1;
  b->ptr   = NULL;
  b->flags = 0;
#endif
  return 1;
}

static int _bio_destroy(BIO *b)
{
  (void)b;
  return 1;
}

static BIO *_bio_new(tls_openssl_t *this)
{
  BIO *b;

#if OPENSSL_VERSION_NUMBER < 0x1010000fL
  static BIO_METHOD bio_method = {
    .type    = BIO_TYPE_SOURCE_SINK,
    .name    = "xine bio",
    .bwrite  = _bio_write,
    .bread   = _bio_read,
    .bputs   = _bio_puts,
    .bgets   = NULL,
    .ctrl    = _bio_ctrl,
    .create  = _bio_create,
    .destroy = _bio_destroy,
  };

  b = BIO_new(&bio_method);
  b->ptr = this;
#else
  _x_assert(this->bio_method == NULL);

  this->bio_method = BIO_meth_new(BIO_TYPE_SOURCE_SINK, "xine bio");
  BIO_meth_set_write  (this->bio_method, _bio_write);
  BIO_meth_set_read   (this->bio_method, _bio_read);
  BIO_meth_set_puts   (this->bio_method, _bio_puts);
  BIO_meth_set_ctrl   (this->bio_method, _bio_ctrl);
  BIO_meth_set_create (this->bio_method, _bio_create);
  BIO_meth_set_destroy(this->bio_method, _bio_destroy);

  b = BIO_new(this->bio_method);
  BIO_set_data(b, this);
#endif

  return b;
}

/*
 * xine TLS API
 */

static ssize_t _openssl_write(tls_plugin_t *this_gen, const void *buf, size_t len)
{
  tls_openssl_t *this = (tls_openssl_t *)this_gen;
  int ret;

  if (!this->ssl)
    return -1;

  ret = SSL_write(this->ssl, buf, len);
  if (ret < 0)
    xprintf(this->xine, XINE_VERBOSITY_LOG, LOG_MODULE ": "
            "OpenSSL write failed: %s\n",
            ERR_error_string(ERR_get_error(), NULL));
  return ret;
}

static ssize_t _openssl_read(tls_plugin_t *this_gen, void *buf, size_t len)
{
  tls_openssl_t *this = (tls_openssl_t *)this_gen;
  int ret;

  if (!this->ssl)
    return -1;

  ret = SSL_read(this->ssl, buf, len);
  if (ret < 0)
    xprintf(this->xine, XINE_VERBOSITY_LOG, LOG_MODULE ": "
            "OpenSSL read failed: %s\n",
            ERR_error_string(ERR_get_error(), NULL));
  return ret;
}

static void _openssl_shutdown(tls_plugin_t *this_gen)
{
  tls_openssl_t *this = (tls_openssl_t *)this_gen;

  if (this->ssl) {
    SSL_shutdown(this->ssl);
    SSL_free(this->ssl);
    this->ssl = NULL;
  }
  if (this->ctx) {
    SSL_CTX_free(this->ctx);
    this->ctx = NULL;
  }
#if OPENSSL_VERSION_NUMBER >= 0x1010000fL
  if (this->bio_method) {
    BIO_method_free(this->bio_method);
    this->bio_method = NULL;
  }
#endif
}

static int _openssl_handshake(tls_plugin_t *this_gen, const char *host, int verify)
{
  tls_openssl_t *this = (tls_openssl_t *)this_gen;
  BIO *bio;
  int ret;

  _x_assert(this->ssl == NULL);

  this->ctx = SSL_CTX_new(SSLv23_client_method());
  if (!this->ctx) {
    xprintf(this->xine, XINE_VERBOSITY_LOG, LOG_MODULE ": "
            "SSL context init failed: %s\n",
            ERR_error_string(ERR_get_error(), NULL));
    return -1;
  }
  /* disable deprecated and insecure SSLv2 and SSLv3 */
  SSL_CTX_set_options(this->ctx, SSL_OP_NO_SSLv2 | SSL_OP_NO_SSLv3);


  if (verify < 0 && this->xine)
    verify = tls_get_verify_tls_cert(this->xine->config);
  if (verify)
    SSL_CTX_set_verify(this->ctx, SSL_VERIFY_PEER | SSL_VERIFY_FAIL_IF_NO_PEER_CERT, NULL);

  this->ssl = SSL_new(this->ctx);
  if (!this->ssl) {
    xprintf(this->xine, XINE_VERBOSITY_LOG, LOG_MODULE ": "
            "SSL init failed: %s\n",
            ERR_error_string(ERR_get_error(), NULL));
    _openssl_shutdown(&this->tls_plugin);
    return -1;
  }

  bio = _bio_new(this);
  SSL_set_bio(this->ssl, bio, bio);

  SSL_set_tlsext_host_name(this->ssl, host);

  ret = SSL_connect(this->ssl);
  if (ret <= 0) {
    if (ret == 0)
      xprintf(this->xine, XINE_VERBOSITY_LOG, LOG_MODULE ": "
              "Unable to negotiate TLS/SSL session\n");
    else
      xprintf(this->xine, XINE_VERBOSITY_LOG, LOG_MODULE ": "
              "OpenSSL handshake failed: %s\n",
              ERR_error_string(ERR_get_error(), NULL));
    _openssl_shutdown(&this->tls_plugin);
    return -1;
  }

  return 0;
}

static void _openssl_dispose(xine_module_t *this_gen)
{
  _openssl_shutdown((tls_plugin_t*)this_gen);
  free(this_gen);
}

static xine_module_t *_openssl_get_instance(xine_module_class_t *cls_gen, const void *params_gen)
{
  openssl_class_t *cls = (openssl_class_t *)cls_gen;
  const tls_plugin_params_t *p = params_gen;
  tls_openssl_t *this;

  pthread_mutex_lock(&cls->lock);
  if (!cls->inited) {
    SSL_library_init();
    SSL_load_error_strings();
    cls->inited = 1;
  }
  pthread_mutex_unlock(&cls->lock);

  this = calloc(1, sizeof(*this));
  if (!this) {
    return NULL;
  }

  this->tls_plugin.module.dispose = _openssl_dispose;

  this->tls_plugin.handshake = _openssl_handshake;
  this->tls_plugin.shutdown  = _openssl_shutdown;
  this->tls_plugin.read      = _openssl_read;
  this->tls_plugin.write     = _openssl_write;

  this->xine   = p->xine;
  this->fd     = p->fd;
  this->stream = p->stream;

  return &this->tls_plugin.module;
}

static void _openssl_class_dispose(xine_module_class_t *cls_gen)
{
  openssl_class_t *cls = (openssl_class_t *)cls_gen;
  pthread_mutex_destroy(&cls->lock);
  free(cls_gen);
}

static void *_openssl_init_class(xine_t *xine, const void *data)
{
  openssl_class_t *this;

  (void)data;

  this = calloc(1, sizeof(*this));
  if (!this)
    return NULL;

  this->module_class.get_instance      = _openssl_get_instance;
  this->module_class.description       = N_("TLS provider (openssl)");
  this->module_class.identifier        = "openssl";
  this->module_class.dispose           = _openssl_class_dispose;

  pthread_mutex_init(&this->lock, NULL);

  tls_register_config_keys(xine->config);

  return this;
}

/*
 * exported plugin catalog entry
 */

static const xine_module_info_t module_info_openssl = {
  .priority = 5,
  .type     = "tls_v1",
};

const plugin_info_t xine_plugin_info[] EXPORTED = {
  /* type, API, "name", version, special_info, init_function */
  { PLUGIN_XINE_MODULE, 1, "openssl", XINE_VERSION_CODE, &module_info_openssl, _openssl_init_class },
  { PLUGIN_NONE, 0, NULL, 0, NULL, NULL }
};
