 /*
 * Copyright (C) 2018 the xine project
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
 * xine TLS provider plugin interface
 */

#ifndef _XINE_TLS_PLUGIN_H_
#define _XINE_TLS_PLUGIN_H_

#include <sys/types.h>

#include <xine/xine_module.h>

#define TLS_PLUGIN_TYPE "tls_v1"

typedef struct {
  xine_t        *xine;
  xine_stream_t *stream;
  int            fd;
} tls_plugin_params_t;

typedef struct tls_plugin_s tls_plugin_t;

struct tls_plugin_s {
  xine_module_t module;

  int     (*handshake)(tls_plugin_t *, const char *host, int verify);
  void    (*shutdown)(tls_plugin_t *);

  ssize_t (*read)(tls_plugin_t *, void *buf, size_t len);
  ssize_t (*write)(tls_plugin_t *, const void *buf, size_t len);
};

/*
 * config helpers
 */

#include <xine/configfile.h>

#define TLS_VERIFY_CERT_KEY "media.network.verify_tls_certificate"

static inline void tls_register_config_keys(config_values_t *config)
{
  config->register_bool(config,
                        TLS_VERIFY_CERT_KEY,
                        1, _("Verify server TLS certificate"),
                        _("If enabled, server TLS certificate is always checked. "
                          "If check fails, connections to server are not allowed."),
                        10, NULL, NULL);
}

static inline int tls_get_verify_tls_cert(config_values_t *config)
{
  cfg_entry_t *entry;

  entry = config->lookup_entry(config, TLS_VERIFY_CERT_KEY);
  if (entry) {
    return entry->num_value;
  }
  return 1;
}

#endif /* _XINE_TLS_PLUGIN_H_ */
