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

typedef struct {
  input_class_t     input_class;
  xine_t           *xine;
  xine_mrl_t      **mrls;
} nfs_input_class_t;

/*
 * util
 */

static int _parse_url(nfs_input_plugin_t *this, int full)
{
  if (!this->nfs) {
    this->nfs = nfs_init_context();
    if (!this->nfs) {
      xprintf(this->xine, XINE_VERBOSITY_LOG, LOG_MODULE ": "
              "Error initializing nfs context\n");
      return -1;
    }
  }

  if (!this->url) {
    if (full) {
      this->url = nfs_parse_url_full(this->nfs, this->mrl);
    } else {
      this->url = nfs_parse_url_dir(this->nfs, this->mrl);
      if (!this->url) {
        this->url = nfs_parse_url_incomplete(this->nfs, this->mrl);
      }
    }
    if (!this->url) {
      xprintf(this->xine, XINE_VERBOSITY_LOG, LOG_MODULE ": "
              "invalid nfs url '%s': %s\n", this->mrl, nfs_get_error(this->nfs));
      return -1;
    }
  }

  return 0;
}

static int _mount(nfs_input_plugin_t *this)
{
  if (_parse_url(this, 1) < 0)
    return -1;

  if (nfs_mount(this->nfs, this->url->server, this->url->path)) {
    xprintf(this->xine, XINE_VERBOSITY_LOG, LOG_MODULE ": "
            "mounting '%s:%s' failed: %s\n",
            this->url->server, this->url->path, nfs_get_error(this->nfs));
    return -1;
  }

  return 0;
}

/*
 * xine plugin
 */

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

  if (_mount(this) < 0)
    return -1;

  if (nfs_open(this->nfs, this->url->file, O_RDONLY, &this->nfsfh)) {
    xprintf(this->xine, XINE_VERBOSITY_LOG, LOG_MODULE ": "
            "Error opening '%s': %s\n", this->mrl, nfs_get_error(this->nfs));
    return -1;
  }

  return 1;
}

static input_plugin_t *_get_instance (input_class_t *cls_gen, xine_stream_t *stream, const char *mrl)
{
  nfs_input_class_t  *class = (nfs_input_class_t *)cls_gen;
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
  this->xine          = class->xine;
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
 * server discovery
 */

static xine_mrl_t **_get_servers(xine_t *xine, int *nFiles)
{
  struct nfs_server_list *srvrs, *srv;
  xine_mrl_t **m, **mrls;
  size_t n;

  srvrs = nfs_find_local_servers();

  /* count servers */
  for (n = 0, srv = srvrs; srv; srv = srv->next, n++) {}

  m = mrls = _x_input_get_default_server_mrls(xine->config, "nfs://", nFiles);
  m = _x_input_realloc_mrls(&mrls, n + *nFiles);
  if (!m)
    goto out;
  m += *nFiles;
  n += *nFiles;

  for(srv = srvrs; srv; srv = srv->next) {
    (*m)->origin = strdup("nfs://");
    (*m)->mrl    = _x_asprintf("nfs://%s", srv->addr);
    (*m)->type   = mrl_net | mrl_file | mrl_file_directory;
    xprintf(xine, XINE_VERBOSITY_DEBUG, LOG_MODULE ": "
            "found nfs server: '%s'\n", (*m)->mrl);
    m++;
  }

  *nFiles = n;

  if (!n)
    _x_input_free_mrls(&mrls);

 out:
  if (srvrs)
    free_nfs_srvr_list(srvrs);
  return mrls;
}

/*
 * exports listing
 */
static xine_mrl_t **_get_exports(xine_t *xine, const char *server, int *nFiles)
{
  struct exportnode *exports, *export;
  xine_mrl_t **m, **mrls;
  size_t n;

  exports = mount_getexports(server);
  if (!exports) {
    xprintf(xine, XINE_VERBOSITY_LOG, LOG_MODULE ": "
            "error listing exports from '%s'\n",
            server);
    /* fall thru: return link to servers list ("..") */
  }

  /* count exports */
  for (n = 0, export = exports; export; export = export->ex_next, n++) {}

  m = mrls = _x_input_alloc_mrls(n + 1);
  if (!m)
    goto out;

  /* Add '..' entry */
  (*m)->type = mrl_net | mrl_file | mrl_file_directory;
  (*m)->origin = _x_asprintf("nfs://%s", server);
  (*m)->mrl    = _x_asprintf("nfs://%s/..", server);
  (*m)->link   = strdup("nfs://");
  m++;

  /* add exports */
  for (export = exports; export; export = export->ex_next) {
    (*m)->origin = _x_asprintf("nfs://%s", server);
    (*m)->mrl    = _x_asprintf("nfs://%s%s", server, export->ex_dir);
    (*m)->type   = mrl_net | mrl_file | mrl_file_directory;
    xprintf(xine, XINE_VERBOSITY_DEBUG, LOG_MODULE ": "
            "found export: '%s'\n", (*m)->mrl);
    m++;
  }

  *nFiles = n + 1;

 out:
  if (exports)
    mount_free_export_list(exports);
  return mrls;
}

static int _is_export(const char *server, const char *path)
{
  struct exportnode *exports, *export;

  exports = mount_getexports(server);
  if (exports) {
    for (export = exports; export; export = export->ex_next) {
      if (!strcmp(path, export->ex_dir)) {
        mount_free_export_list(exports);
        return 1;
      }
    }
    mount_free_export_list(exports);
  }
  return 0;
}

/*
 * directory listing
 */
static xine_mrl_t **_get_files(nfs_input_plugin_t *this, int *nFiles)
{
  struct nfs_stat_64 st;
  struct nfsdir *dir = NULL;
  struct nfsdirent *nfsdirent;
  xine_mrl_t **mrls;
  size_t n = 0;
  size_t mrls_size = 64;
  int show_hidden_files;

  mrls = _x_input_alloc_mrls(mrls_size);
  if (!mrls)
    goto out;

  /* add link to previous level */
  mrls[n]->type = mrl_net | mrl_file | mrl_file_directory;
  if (_is_export(this->url->server, this->url->path)) {
    /* export can be multiple directory levels. xine-ui doesn't handle this well:
       - it crashes if origin is longer than target mrl.
       - it can't handle multiple ..'s either.
       So, we create a fake mrl ... */
    /* XXX: fix xine_mrl_t to allow "title" that will be shown instead of mrl-origin ? */
    mrls[n]->origin = _x_asprintf("nfs://%s/up",    this->url->server);
    mrls[n]->mrl    = _x_asprintf("nfs://%s/up/..", this->url->server);
  } else {
    mrls[n]->origin = _x_asprintf("nfs://%s%s", this->url->server, this->url->path);
    mrls[n]->mrl    = _x_asprintf("nfs://%s%s/..", this->url->server, this->url->path);
  }
  n++;

  if (_mount(this) < 0) {
    goto out;
  }

  if (nfs_opendir(this->nfs, "/", &dir)) {
    xprintf(this->xine, XINE_VERBOSITY_LOG, LOG_MODULE ": "
            "error opening directory '%s' from '%s:%s': %s\n",
            this->url->file, this->url->server, this->url->path,
            nfs_get_error(this->nfs));
    goto out;
  }

  show_hidden_files = _x_input_get_show_hidden_files(this->xine->config);

  /* read directory */

  while ((nfsdirent = nfs_readdir(this->nfs, dir)) != NULL) {

    if (!strcmp(nfsdirent->name, ".") || !strcmp(nfsdirent->name, ".."))
      continue;
    if (!show_hidden_files && nfsdirent->name[0] == '.')
      continue;

    if (nfs_stat64(this->nfs, nfsdirent->name, &st)) {
      xprintf(this->xine, XINE_VERBOSITY_LOG, LOG_MODULE ": "
              "stat '%s' from '%s:%s' failed: %s\n",
              nfsdirent->name, this->url->server, this->url->path, nfs_get_error(this->nfs));
      continue;
    }

    if (n >= mrls_size) {
      mrls_size = mrls_size ? 2*mrls_size : 100;
      if (!(_x_input_realloc_mrls(&mrls, mrls_size))) {
        xprintf(this->xine, XINE_VERBOSITY_LOG, LOG_MODULE ": "
                "out of memory while listing directory '%s' from '%s:%s\n",
                this->url->file, this->url->server, this->url->path);
        goto out;
      }
    }

    switch (st.nfs_mode & S_IFMT) {
      case S_IFREG:
        mrls[n]->type = mrl_net | mrl_file | mrl_file_normal;
        break;
      case S_IFLNK:
        mrls[n]->type = mrl_net | mrl_file | mrl_file_symlink;
        break;
      case S_IFDIR:
        mrls[n]->type = mrl_net | mrl_file | mrl_file_directory;
        break;
      case S_IFCHR:
        mrls[n]->type = mrl_net | mrl_file | mrl_file_chardev;
        break;
      case S_IFBLK:
        mrls[n]->type = mrl_net | mrl_file | mrl_file_blockdev;
        break;
      default:
        mrls[n]->type = mrl_net;
        break;
    }

    mrls[n]->origin = _x_asprintf("nfs://%s%s", this->url->server, this->url->path);
    mrls[n]->mrl    = _x_asprintf("nfs://%s%s/%s", this->url->server, this->url->path, nfsdirent->name);
    n++;
  }

 out:
  if (dir) {
    nfs_closedir(this->nfs, dir);
  }
  *nFiles = n;

  return mrls;
}

/*
 *  plugin class
 */

static xine_mrl_t **_get_dir (input_class_t *this_gen, const char *filename, int *nFiles)
{
  nfs_input_class_t *this = (nfs_input_class_t *)this_gen;
  nfs_input_plugin_t *input = NULL;

  *nFiles = 0;
  _x_input_free_mrls(&this->mrls);

  if (!filename || !strcmp(filename, "nfs:/")) {
    this->mrls = _get_servers(this->xine, nFiles);
    goto out;
  }

  input = (nfs_input_plugin_t *)_get_instance(this_gen, NULL, filename);
  if (!input) {
    goto fail;
  }

  if (_parse_url(input, 0) < 0) {
    goto fail;
  }

  if (!input->url->server) {
    this->mrls = _get_servers(this->xine, nFiles);
  } else if (!input->url->path) {
    this->mrls = _get_exports(this->xine, input->url->server, nFiles);
  } else {
    this->mrls = _get_files(input, nFiles);
  }

 out:

  if (*nFiles > 2)
    _x_input_sort_mrls(this->mrls + 1, *nFiles - 1);

 fail:

  if (input)
    input->input_plugin.dispose(&input->input_plugin);

  return this->mrls;
}

static void _dispose_class (input_class_t *this_gen)
{
  nfs_input_class_t *this = (nfs_input_class_t *)this_gen;

  _x_input_free_mrls(&this->mrls);

  free(this_gen);
}

static void *nfs_init_class(xine_t *xine, const void *data)
{
  nfs_input_class_t *this;

  (void)data;

  this = calloc(1, sizeof(*this));
  if (!this)
    return NULL;

  this->xine = xine;

  this->input_class.get_instance      = _get_instance;
  this->input_class.description       = N_("Network File System (NFS) input plugin");
  this->input_class.identifier        = "NFS";
  this->input_class.get_dir           = _get_dir;
  this->input_class.get_autoplay_list = NULL;
  this->input_class.dispose           = _dispose_class;
  this->input_class.eject_media       = NULL;

  _x_input_register_show_hidden_files(xine->config);

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
  { PLUGIN_INPUT, 18, "NFS", XINE_VERSION_CODE, &input_info_nfs, nfs_init_class },
  { PLUGIN_NONE, 0, "", 0, NULL, NULL }
};
