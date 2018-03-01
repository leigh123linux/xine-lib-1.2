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

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include <nfsc/libnfs.h>
#include <nfsc/libnfs-raw-mount.h>

#define LOG_MODULE "input_nfs"
#define LOG_VERBOSE
/*
#define LOG
*/

#include <xine/xine_internal.h>
#include <xine/xineutils.h>
#include <xine/input_plugin.h>

#include "input_helper.h"

typedef struct {
  input_plugin_t   input_plugin;

  xine_t          *xine;
  xine_stream_t   *stream;

  char            *mrl;
  off_t            curpos;
  off_t            file_size;

  struct nfs_context *nfs;
  struct nfs_url     *url;
  struct nfsfh       *nfsfh;

} nfs_input_plugin_t;

static off_t _read (input_plugin_t *this_gen, void *buf_gen, off_t len)
{
  nfs_input_plugin_t *this = (nfs_input_plugin_t *) this_gen;
  uint8_t *buf = buf_gen;
  off_t got = 0;
  int rc;

  while (got < len) {
    rc = nfs_read(this->nfs, this->nfsfh, len - got, buf + got);
    if (rc <= 0) {
      if (errno == EAGAIN || errno == EINTR)
        continue;
      xprintf(this->xine, XINE_VERBOSITY_LOG, LOG_MODULE ": "
              "NFS read failed: %d: %s\n", rc, nfs_get_error(this->nfs));
      if (got)
        break;
      return rc;
    }

    got += rc;

    if (_x_action_pending(this->stream)) {
      errno = EINTR;
      if (got)
        break;
      return -1;
    }
  }

  this->curpos += got;
  return got;
}

static off_t _get_length (input_plugin_t *this_gen)
{
  nfs_input_plugin_t *this = (nfs_input_plugin_t *) this_gen;
  struct nfs_stat_64 st;

  if (this->file_size)
    return this->file_size;

  if (nfs_stat64(this->nfs, this->url->file, &st)) {
    xprintf(this->xine, XINE_VERBOSITY_LOG, LOG_MODULE ": "
            "stat(%s) failed: %s\n", this->url->file, nfs_get_error(this->nfs));
    return -1;
  }

  this->file_size = st.nfs_size;

  return this->file_size;
}

static off_t _get_current_pos (input_plugin_t *this_gen)
{
  nfs_input_plugin_t *this = (nfs_input_plugin_t *) this_gen;

  return this->curpos;
}

static off_t _seek (input_plugin_t *this_gen, off_t offset, int origin)
{
  nfs_input_plugin_t *this = (nfs_input_plugin_t *) this_gen;
  uint64_t pos = this->curpos;

  if (nfs_lseek(this->nfs, this->nfsfh, offset, origin, &pos) < 0) {
    xprintf(this->xine, XINE_VERBOSITY_LOG, LOG_MODULE ": "
            "seek failed: %s\n", nfs_get_error(this->nfs));
    return -1;
  }

  this->curpos = pos;
  return this->curpos;
}


static const char *_get_mrl (input_plugin_t *this_gen)
{
  nfs_input_plugin_t *this = (nfs_input_plugin_t *) this_gen;

  return this->mrl;
}

static void _dispose (input_plugin_t *this_gen)
{
  nfs_input_plugin_t *this = (nfs_input_plugin_t *) this_gen;

  if (this->nfsfh) {
    nfs_close(this->nfs, this->nfsfh);
  }
  if (this->url) {
    nfs_destroy_url(this->url);
  }
  if (this->nfs) {
    nfs_destroy_context(this->nfs);
  }
  _x_freep (&this->mrl);
  free (this_gen);
}

static int _open (input_plugin_t *this_gen)
{
  nfs_input_plugin_t *this = (nfs_input_plugin_t *)this_gen;

  this->curpos = 0;

  this->nfs = nfs_init_context();
  if (!this->nfs) {
    xprintf(this->xine, XINE_VERBOSITY_LOG, LOG_MODULE ": "
            "Error initializing nfs context\n");
    return -1;
  }

  this->url = nfs_parse_url_full(this->nfs, this->mrl);
  if (!this->url) {
    xprintf(this->xine, XINE_VERBOSITY_LOG, LOG_MODULE ": "
            "invalid nfs url '%s': %s\n", this->mrl, nfs_get_error(this->nfs));
    return 0;
  }

  if (nfs_mount(this->nfs, this->url->server, this->url->path)) {
    xprintf(this->xine, XINE_VERBOSITY_LOG, LOG_MODULE ": "
            "mounting '%s:%s' failed: %s\n",
            this->url->server, this->url->path, nfs_get_error(this->nfs));
    return -1;
  }

  if (nfs_open(this->nfs, this->url->file, O_RDONLY, &this->nfsfh)) {
    xprintf(this->xine, XINE_VERBOSITY_LOG, LOG_MODULE ": "
            "Error opening '%s': %s\n", this->mrl, nfs_get_error(this->nfs));
    return -1;
  }

  return 1;
}

static input_plugin_t *_get_instance (input_class_t *cls_gen, xine_stream_t *stream, const char *mrl)
{
  nfs_input_plugin_t *this;

  if (strncasecmp (mrl, "nfs://", 6)) {
    return NULL;
  }

  this = calloc(1, sizeof(*this));
  if (!this) {
    return NULL;
  }

  this->mrl           = strdup(mrl);
  this->stream        = stream;
  this->xine          = stream ? stream->xine : NULL;
  this->curpos        = 0;

  this->input_plugin.open              = _open;
  this->input_plugin.get_capabilities  = _x_input_get_capabilities_seekable;
  this->input_plugin.read              = _read;
  this->input_plugin.read_block        = _x_input_default_read_block;
  this->input_plugin.seek              = _seek;
  this->input_plugin.get_current_pos   = _get_current_pos;
  this->input_plugin.get_length        = _get_length;
  this->input_plugin.get_blocksize     = _x_input_default_get_blocksize;
  this->input_plugin.get_mrl           = _get_mrl;
  this->input_plugin.get_optional_data = _x_input_default_get_optional_data;
  this->input_plugin.dispose           = _dispose;
  this->input_plugin.input_class       = cls_gen;

  return &this->input_plugin;
}


/*
 *  plugin class
 */

static void *nfs_init_class(xine_t *xine, const void *data)
{
  input_class_t *this;

  this = calloc(1, sizeof(*this));
  if (!this)
    return NULL;

  this->get_instance      = _get_instance;
  this->description       = N_("Network File System (NFS) input plugin");
  this->identifier        = "NFS";
  this->get_dir           = NULL;
  this->get_autoplay_list = NULL;
  this->dispose           = default_input_class_dispose;
  this->eject_media       = NULL;

  return this;
}

/*
 * exported plugin catalog entry
 */

const input_info_t input_info_nfs = {
  10,   /* priority */
};

const plugin_info_t xine_plugin_info[] EXPORTED = {
  /* type, API, "name", version, special_info, init_function */
  { PLUGIN_INPUT, INPUT_PLUGIN_IFACE_VERSION, "NFS", XINE_VERSION_CODE, &input_info_nfs, nfs_init_class },
  { PLUGIN_NONE, 0, "", 0, NULL, NULL }
};
