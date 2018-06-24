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
#include <errno.h>

#include <libssh2.h>
#include <libssh2_sftp.h>

#define LOG_MODULE "input_ssh"
#define LOG_VERBOSE
/*
#define LOG
*/

#include <xine/xine_internal.h>
#include <xine/xineutils.h>
#include <xine/input_plugin.h>

#include "net_buf_ctrl.h"
#include "http_helper.h"
#include "input_helper.h"

#define DEFAULT_SSH_PORT 22

typedef struct {
  input_plugin_t   input_plugin;

  xine_t          *xine;
  xine_stream_t   *stream;
  char            *mrl;         /* mrl without credentials */
  char            *mrl_private;
  off_t            curpos;
  off_t            file_size;

  nbc_t           *nbc;

  /* ssh */
  int                  fd;
  LIBSSH2_SESSION     *session;

  /* sftp */
  LIBSSH2_SFTP        *sftp_session;
  LIBSSH2_SFTP_HANDLE *sftp_handle;

  /* scp */
  LIBSSH2_CHANNEL      *scp_channel;
  off_t                 preview_size;
  char                  preview[MAX_PREVIEW_SIZE];

} ssh_input_plugin_t;

/*
 * helper functions
 */

static int _wait_socket(ssh_input_plugin_t *this)
{
  int flags = 0;
  int dir;

  dir = libssh2_session_block_directions(this->session);

  if (dir & LIBSSH2_SESSION_BLOCK_INBOUND)
    flags |= XIO_READ_READY;
  if (dir & LIBSSH2_SESSION_BLOCK_OUTBOUND)
    flags |= XIO_WRITE_READY;

  return _x_io_select(this->stream, this->fd, flags, 500);
}

static void _emit_authentication_request(ssh_input_plugin_t *this)
{
  xprintf(this->xine, XINE_VERBOSITY_LOG, LOG_MODULE ": "
          "Authentication required for '%s'\n", this->mrl);
  if (this->stream)
    _x_message(this->stream, XINE_MSG_AUTHENTICATION_NEEDED,
               this->mrl, "Authentication required", NULL);
}

static int _ssh_connect(ssh_input_plugin_t *this,
                        const xine_url_t *url)
{
  int port = url->port;
  int rc;

  /* check parameters */

  if (!port)
    port = DEFAULT_SSH_PORT;

  if (!url->user) {
    xprintf(this->xine, XINE_VERBOSITY_LOG, LOG_MODULE ": "
            "No username in mrl '%s'\n", this->mrl);
    _emit_authentication_request(this);
    return -1;
  }

  /* connect to remote host */

  this->fd = _x_io_tcp_connect (this->stream, url->host, port);
  if (this->fd < 0) {
    return -1;
  }

  do {
    rc = _x_io_tcp_connect_finish(this->stream, this->fd, 1000);
    if (rc == XIO_READY)
      break;
    if (rc != XIO_TIMEOUT)
      return -1;
  } while (1);

  /* init ssh session */

  this->session = libssh2_session_init();
  if (!this->session) {
    return -1;
  }

  /* enable non-blocking mode (allow stopping if network is stuck) */
  libssh2_session_set_blocking(this->session, 0);

  do {
    rc = libssh2_session_handshake(this->session, this->fd);
    if (this->stream && _x_action_pending(this->stream))
      return -1;
  } while (rc == LIBSSH2_ERROR_EAGAIN);

  if (rc) {
    xprintf(this->xine, XINE_VERBOSITY_LOG, LOG_MODULE ": "
            "Failed to establish SSH session: %d\n", rc);
    return -1;
  }

  /* authenticate */

  if (url->password && url->password[0]) {

    /* password */

    do {
      rc = libssh2_userauth_password(this->session, url->user, url->password);
      if (this->stream && _x_action_pending(this->stream))
        return -1;
    } while (rc == LIBSSH2_ERROR_EAGAIN);

    if (rc) {
      xprintf(this->xine, XINE_VERBOSITY_LOG, LOG_MODULE ": "
              "Authentication by password failed.\n");
      _emit_authentication_request(this);
      return -1;
    }
  } else {

    /* public key */

    const char *home = xine_get_homedir();
    char *pub  = _x_asprintf("%s/.ssh/id_rsa.pub", home);
    char *priv = _x_asprintf("%s/.ssh/id_rsa", home);

    do {
      rc = libssh2_userauth_publickey_fromfile(this->session, url->user,
                                               pub, priv, url->password);
      if (this->stream && _x_action_pending(this->stream)) {
        free(pub);
        free(priv);
        return -1;
      }
    } while (rc == LIBSSH2_ERROR_EAGAIN);

    free(pub);
    free(priv);

    if (rc) {
      xprintf(this->xine, XINE_VERBOSITY_LOG, LOG_MODULE ": "
              "Authentication by public key failed\n");
      _emit_authentication_request(this);
      return -1;
    }
  }

  return 0;
}

static int _sftp_session_init(ssh_input_plugin_t *this)
{
  int rc;

  do {
    this->sftp_session = libssh2_sftp_init(this->session);

    if (!this->sftp_session) {
      rc = libssh2_session_last_errno(this->session);
      if (rc != LIBSSH2_ERROR_EAGAIN) {
        xprintf(this->xine, XINE_VERBOSITY_LOG, LOG_MODULE ": "
                "Unable to init SFTP session\n");
        return -1;
      }
      _wait_socket(this);
      if (this->stream && _x_action_pending(this->stream))
        return -1;
    }
  } while (!this->sftp_session);

  return 0;
}

static int _scp_channel_init(ssh_input_plugin_t *this, const char *uri)
{
#if LIBSSH2_VERSION_NUM < 0x010800
  struct stat sb;
#else
  libssh2_struct_stat sb;
#endif
  int rc;

  /* Request a file via SCP */
  do {
#if LIBSSH2_VERSION_NUM < 0x010800
    this->scp_channel = libssh2_scp_recv(this->session, uri, &sb);
#else
    this->scp_channel = libssh2_scp_recv2(this->session, uri, &sb);
#endif
    if (!this->scp_channel) {
      rc = libssh2_session_last_errno(this->session);
      if (rc != LIBSSH2_ERROR_EAGAIN) {
        xprintf(this->xine, XINE_VERBOSITY_LOG, LOG_MODULE ": "
                "Unable to init SCP channel for '%s'\n", uri);
        return -1;
      }
      _wait_socket(this);
      if (_x_action_pending(this->stream))
        return -1;
    }
  } while (!this->scp_channel);

  this->file_size = sb.st_size;

  return 0;
}

static int _sftp_open(ssh_input_plugin_t *this, const char *uri)
{
  int rc;

  /* Request a file via SFTP */
  do {
    this->sftp_handle = libssh2_sftp_open(this->sftp_session, uri, LIBSSH2_FXF_READ, 0);
    if (!this->sftp_handle) {
      rc = libssh2_session_last_errno(this->session);
      if (rc != LIBSSH2_ERROR_EAGAIN) {
        xprintf(this->xine, XINE_VERBOSITY_LOG, LOG_MODULE ": "
                "Unable to open SFTP file '%s'\n", uri);
        return -1;
      }
      _wait_socket(this);
      if (_x_action_pending(this->stream))
        return -1;
    }
  } while (!this->sftp_handle);

  return 0;
}

/*
 * plugin interface
 */

static off_t _scp_read (input_plugin_t *this_gen, void *buf_gen, off_t len)
{
  ssh_input_plugin_t *this = (ssh_input_plugin_t *) this_gen;
  uint8_t *buf = buf_gen;
  off_t got = 0;
  int rc;

  /* handle preview chunk */
  if (this->curpos < this->preview_size) {
    size_t n = this->preview_size - this->curpos;
    if (n > len)
      n = len;
    memcpy (buf, this->preview + this->curpos, n);
    this->curpos += n;
    got += n;
  }

  /* handle actual read */
  while (got < len) {

    /* check for EOF */
    if (this->curpos + got >= this->file_size) {
      goto out;
    }

    while ((rc = libssh2_channel_read(this->scp_channel, buf + got, len - got))
           == LIBSSH2_ERROR_EAGAIN) {
      if (libssh2_channel_eof(this->scp_channel)) {
        goto out;
      }
      _wait_socket(this);
      if (_x_action_pending(this->stream)) {
        errno = EINTR;
        if (got)
          goto out;
        return -1;
      }
    }

    if (rc <= 0) {
      if (rc < 0) {
        xprintf(this->xine, XINE_VERBOSITY_LOG, LOG_MODULE ": "
                "SCP read failed: %d\n", rc);
        if (got)
          goto out;
        return -1;
      }
      if (libssh2_channel_eof(this->scp_channel)) {
        goto out;
      }
    }

    got += rc;
  }

 out:
  this->curpos += got;
  return got;
}

static off_t _sftp_read (input_plugin_t *this_gen, void *buf_gen, off_t len)
{
  ssh_input_plugin_t *this = (ssh_input_plugin_t *) this_gen;
  uint8_t *buf = buf_gen;
  off_t got = 0;
  int rc;

  if (this->curpos + len >= this->file_size) {
    /* check if file growed */
    this->file_size = 0;
    this_gen->get_length(this_gen);
    if (this->curpos >= this->file_size) {
      return 0;
    }
  }

  while (got < len) {

    while ((rc = libssh2_sftp_read(this->sftp_handle, buf + got, len - got))
           == LIBSSH2_ERROR_EAGAIN) {
      _wait_socket(this);
      if (_x_action_pending(this->stream)) {
        errno = EINTR;
        if (got)
          goto out;
        return -1;
      }
    }

    if (rc <= 0) {
      if (rc < 0) {
        xprintf(this->xine, XINE_VERBOSITY_LOG, LOG_MODULE ": "
                "SCP read failed: %d\n", rc);
        if (got)
          goto out;
        return -1;
      }
      goto out;
    }
    got += rc;
  }

 out:
  this->curpos += got;
  return got;
}

static off_t _scp_get_length (input_plugin_t *this_gen)
{
  ssh_input_plugin_t *this = (ssh_input_plugin_t *) this_gen;

  return this->file_size;
}

static off_t _sftp_get_length (input_plugin_t *this_gen)
{
  ssh_input_plugin_t *this = (ssh_input_plugin_t *) this_gen;
  LIBSSH2_SFTP_ATTRIBUTES attrs;
  int rc;

  if (this->file_size)
    return this->file_size;

  memset(&attrs, 0, sizeof(attrs));

  while ((rc = libssh2_sftp_fstat_ex(this->sftp_handle, &attrs, 0)) ==
         LIBSSH2_ERROR_EAGAIN) {
    if (_x_action_pending(this->stream))
      return 0;
  }
  if (rc) {
    xprintf(this->xine, XINE_VERBOSITY_LOG, LOG_MODULE ": "
            "SFTP stat failed: %d\n", rc);
    return 0;
  }

  this->file_size = attrs.filesize;
  return this->file_size;
}

static off_t _get_current_pos (input_plugin_t *this_gen)
{
  ssh_input_plugin_t *this = (ssh_input_plugin_t *) this_gen;

  return this->curpos;
}

static off_t _scp_seek (input_plugin_t *this_gen, off_t offset, int origin)
{
  ssh_input_plugin_t *this = (ssh_input_plugin_t *) this_gen;

  return _x_input_seek_preview(this_gen, offset, origin,
                               &this->curpos, this->file_size, this->preview_size);
}

static off_t _sftp_seek (input_plugin_t *this_gen, off_t offset, int origin)
{
  ssh_input_plugin_t *this = (ssh_input_plugin_t *) this_gen;

  switch (origin) {
  case SEEK_CUR:
    offset = this->curpos + offset;
    break;
  case SEEK_END:
    offset = this->file_size + offset;
    break;
  case SEEK_SET:
    break;
  default:
    return -1;
  }

  if (offset < 0) {
    xprintf(this->xine, XINE_VERBOSITY_LOG, LOG_MODULE ": "
            "SFTP seek failed: position %" PRId64 " outside of file.\n", (int64_t)offset);
    return -1;
  }

  this->curpos = offset;
  libssh2_sftp_seek64(this->sftp_handle, offset);

  return this->curpos;
}


static const char *_get_mrl (input_plugin_t *this_gen)
{
  ssh_input_plugin_t *this = (ssh_input_plugin_t *) this_gen;

  return this->mrl;
}

static int _get_optional_data (input_plugin_t *this_gen, void *data, int data_type)
{
  ssh_input_plugin_t *this = (ssh_input_plugin_t *) this_gen;

  switch (data_type) {
    case INPUT_OPTIONAL_DATA_PREVIEW:
      if (this->preview_size > 0) {
        memcpy (data, this->preview, this->preview_size);
        return this->preview_size;
      }
  }

  return INPUT_OPTIONAL_UNSUPPORTED;
}

static void _dispose (input_plugin_t *this_gen)
{
  ssh_input_plugin_t *this = (ssh_input_plugin_t *) this_gen;

  if (this->nbc) {
    nbc_close (this->nbc);
    this->nbc = NULL;
  }

  if (this->sftp_handle) {
    while (libssh2_sftp_close(this->sftp_handle) == LIBSSH2_ERROR_EAGAIN);
    this->sftp_handle = NULL;
  }

  if (this->scp_channel) {
    while (libssh2_channel_free(this->scp_channel) == LIBSSH2_ERROR_EAGAIN);
    this->scp_channel = NULL;
  }

  if (this->sftp_session) {
    while (libssh2_sftp_shutdown(this->sftp_session) == LIBSSH2_ERROR_EAGAIN);
    this->sftp_session = NULL;
  }

  if (this->session) {
    while (libssh2_session_disconnect(this->session, "close") == LIBSSH2_ERROR_EAGAIN);
    while (libssh2_session_free(this->session) == LIBSSH2_ERROR_EAGAIN);
    this->session = NULL;
  }

  if (this->fd != -1) {
    _x_io_tcp_close(this->stream, this->fd);
    this->fd = -1;
  }

  _x_freep (&this->mrl);
  _x_freep_wipe_string(&this->mrl_private);

  free (this_gen);

  libssh2_exit();
}

static int _scp_fill_preview(ssh_input_plugin_t *this)
{
  off_t got;

  got = _scp_read (&this->input_plugin, this->preview, sizeof(this->preview));
  if (got < 1 || got > sizeof(this->preview)) {
    xprintf(this->xine, XINE_VERBOSITY_LOG, LOG_MODULE ": "
            "Unable to read preview data\n");
    return -1;
  }

  this->preview_size = got;
  return 0;
}

static int _open_plugin (input_plugin_t *this_gen)
{
  ssh_input_plugin_t *this = (ssh_input_plugin_t *)this_gen;
  xine_url_t url;
  int   result = 0, rc;
  int   is_scp;

  this->curpos = 0;

  /* parse mrl */
  rc = _x_url_parse2(this->mrl_private, &url);
  _x_freep_wipe_string(&this->mrl_private);
  if (!rc) {
    _x_message(this->stream, XINE_MSG_GENERAL_WARNING, "malformed url", NULL);
    return 0;
  }

  /* set up ssh connection */
  result = _ssh_connect(this, &url);
  if (result < 0)
    goto out;

  is_scp = !strncasecmp (url.proto, "scp", 3);
  if (is_scp) {

    /* Request a file via SCP */

    if (_scp_channel_init(this, url.uri) < 0)
      goto out;

    if (_scp_fill_preview(this) < 0)
      goto out;

  } else {

    /* Request a file via SFTP */

    if (_sftp_session_init(this) < 0)
      goto out;

    if (_sftp_open(this, url.uri) < 0)
      goto out;

    _sftp_get_length(this_gen);
  }

  /* succeed */
  result = 1;

 out:

  _x_url_cleanup(&url);
  return result;
}


/*
 * plugin class
 */

static input_plugin_t *_get_instance (input_class_t *cls_gen, xine_stream_t *stream, const char *mrl)
{
  ssh_input_plugin_t *this;
  int sftp, scp, rc;

  /* check mrl type */
  sftp = !strncasecmp (mrl, "sftp://", 7);
  scp  = !strncasecmp (mrl, "scp://", 6);
  if (!sftp && !scp)
    return NULL;

  /* initialize libssh2 */
  rc = libssh2_init(0);
  if (rc) {
    xprintf(stream ? stream->xine : NULL, XINE_VERBOSITY_LOG, LOG_MODULE ": "
            "libssh2 initialization failed (%d)\n", rc);
    return NULL;
  }

  /* initialize plugin */

  this = calloc(1, sizeof(*this));
  if (!this)
    return NULL;

  this->mrl_private = strdup(mrl);
  this->mrl         = _x_mrl_remove_auth(mrl);

  this->stream = stream;
  this->fd     = -1;
  this->xine   = stream ? stream->xine : NULL;

  if (stream) {
    /* not needed for directory browsing */
    this->nbc = nbc_init (stream);
  }

  this->input_plugin.open              = _open_plugin;
  this->input_plugin.read_block        = _x_input_default_read_block;
  this->input_plugin.get_blocksize     = _x_input_default_get_blocksize;
  this->input_plugin.get_optional_data = _get_optional_data;
  this->input_plugin.get_current_pos   = _get_current_pos;
  this->input_plugin.get_mrl           = _get_mrl;
  this->input_plugin.dispose           = _dispose;
  if (scp) {
    this->input_plugin.get_capabilities  = _x_input_get_capabilities_preview;
    this->input_plugin.read              = _scp_read;
    this->input_plugin.seek              = _scp_seek;
    this->input_plugin.get_length        = _scp_get_length;
  } else {
    this->input_plugin.get_capabilities  = _x_input_get_capabilities_seekable;
    this->input_plugin.read              = _sftp_read;
    this->input_plugin.seek              = _sftp_seek;
    this->input_plugin.get_length        = _sftp_get_length;
  }
  this->input_plugin.input_class = cls_gen;

  return &this->input_plugin;
}

/*
 * SCP class
 */

static void *scp_init_class(xine_t *xine, const void *data)
{
  static const input_class_t input_scp_class = {
    .get_instance      = _get_instance,
    .description       = N_("SCP input plugin"),
    .identifier        = "SCP",
    .dispose           = NULL,
  };

  return (void *)&input_scp_class;
}

/*
 * SFTP class
 */

typedef struct {

  input_class_t     input_class;
  xine_t           *xine;

  /* browser */
  xine_mrl_t        **mrls;

} sftp_input_class_t;

static ssh_input_plugin_t *_open_input(sftp_input_class_t *this,
                                       xine_url_t *url, const char *mrl, int *nFiles)
{
  ssh_input_plugin_t *input;

  input = (ssh_input_plugin_t *)this->input_class.get_instance(&this->input_class, NULL, mrl);
  if (!input)
    return NULL;

  input->xine = this->xine;

  if (_ssh_connect(input, url))
    goto fail;
  if (_sftp_session_init(input))
    goto fail;

  libssh2_session_set_blocking(input->session, 1);

  return input;

 fail:
  input->input_plugin.dispose(&input->input_plugin);
  return NULL;
}

static int _read_dir(sftp_input_class_t *this,
                     ssh_input_plugin_t *input,
                     const char *mrl, const char *uri, int *nFiles)
{
  LIBSSH2_SFTP_ATTRIBUTES attr;
  LIBSSH2_SFTP_HANDLE *dir;
  xine_mrl_t **mrls = NULL;
  size_t mrls_size = 0;
  size_t n = 0;
  char file[1024];
  int show_hidden_files;
  int rc;

  rc = libssh2_sftp_stat(input->sftp_session, uri, &attr);
  if (rc) {
    xprintf(this->xine, XINE_VERBOSITY_LOG, LOG_MODULE ": "
            "remote stat failed for '%s': %d\n", uri, rc);
    return -1;
  }

  if (!LIBSSH2_SFTP_S_ISDIR(attr.permissions)) {
    xprintf(this->xine, XINE_VERBOSITY_LOG, LOG_MODULE ": "
            "'%s' is not a directory\n", uri);

    this->mrls = _x_input_alloc_mrls(1);
    if (this->mrls) {
      this->mrls[0]->type   = mrl_net | mrl_file | mrl_file_normal;
      this->mrls[0]->mrl    = strdup(mrl);
      *nFiles = 1;
    }
    return 0;
  }

  dir = libssh2_sftp_opendir(input->sftp_session, uri);
  if (!dir) {
    xprintf(this->xine, XINE_VERBOSITY_LOG, LOG_MODULE ": "
            "error opening directory '%s': %d\n", uri, rc);
    return -1;
  }

  show_hidden_files = _x_input_get_show_hidden_files(this->xine->config);

  /* add link to back */

  mrls_size += 64;
  mrls = _x_input_alloc_mrls(mrls_size);
  if (!mrls)
    goto fail;

  mrls[n]->type   = mrl_net | mrl_file | mrl_file_directory;
  mrls[n]->origin = strdup(mrl);
  mrls[n]->mrl    = _x_asprintf("%s/..", mrl);
  n++;

  /* read directory */

  while ( 0 != (rc = libssh2_sftp_readdir(dir, file, sizeof(file), &attr))) {

    if (rc < 0 ) {
      if (rc == LIBSSH2_ERROR_BUFFER_TOO_SMALL) {
        xprintf(this->xine, XINE_VERBOSITY_LOG, LOG_MODULE ": "
                "ignoring too long file name");
        continue;
      }
      if (rc == LIBSSH2_ERROR_EAGAIN)
        continue;
      xprintf(this->xine, XINE_VERBOSITY_LOG, LOG_MODULE ": "
              "directory '%s' read failed: %d", uri, rc);
      break;
    }

    if (!show_hidden_files && file[0] == '.')
      continue;
    if (!strcmp(file, ".") || !strcmp(file, ".."))
      continue;

    if (n >= mrls_size) {
      mrls_size += 64;
      if (!_x_input_realloc_mrls(&mrls, mrls_size))
        break;
    }

    int type = LIBSSH2_SFTP_S_ISDIR( attr.permissions ) ? mrl_file_directory : mrl_file_normal;

    mrls[n]->type   = type | mrl_net | mrl_file;
    mrls[n]->origin = strdup(mrl);
    mrls[n]->mrl    = _x_asprintf("%s/%s", mrl, file);
    mrls[n]->size   = attr.filesize;
    n++;
  }

 fail:

  if (n > 2)
    _x_input_sort_mrls(mrls + 1, n - 1);

  if (dir)
    libssh2_sftp_close(dir);

  *nFiles = n;
  this->mrls = mrls;

  return 0;
}

static xine_mrl_t **_get_dir (input_class_t *this_gen, const char *filename, int *nFiles)
{
  sftp_input_class_t *this = (sftp_input_class_t *) this_gen;
  ssh_input_plugin_t *input = NULL;
  xine_url_t url;

  _x_input_free_mrls(&this->mrls);
  *nFiles = 0;

  if (!filename || !strcmp(filename, "sftp:/") || !strcmp(filename, "sftp://")) {
    this->mrls = _x_input_get_default_server_mrls(this->xine->config, "sftp://", nFiles);
    if (!this->mrls)
      xprintf(this->xine, XINE_VERBOSITY_LOG, LOG_MODULE ": "
              "missing sftp mrl\n");
    return this->mrls;
  }

  if (!_x_url_parse2(filename, &url)) {
    xprintf(this->xine, XINE_VERBOSITY_LOG, LOG_MODULE ": "
            "malformed url '%s'", filename);
    return NULL;
  }

  input = _open_input(this, &url, filename, nFiles);
  if (!input)
    goto out;

  _read_dir(this, input, filename, url.uri, nFiles);

 out:
  _x_url_cleanup(&url);
  if (input) {
    input->input_plugin.dispose(&input->input_plugin);
  }

  return this->mrls;
}

static void _dispose_class_sftp(input_class_t *this_gen)
{
  sftp_input_class_t *this = (sftp_input_class_t *) this_gen;

  _x_input_free_mrls(&this->mrls);
  free(this_gen);
}

static void *sftp_init_class(xine_t *xine, const void *data)
{
  sftp_input_class_t *this;

  this = calloc(1, sizeof(*this));
  if (!this)
    return NULL;

  this->input_class.get_instance      = _get_instance;
  this->input_class.description       = N_("SFTP input plugin");
  this->input_class.identifier        = "SFTP";
  this->input_class.get_dir           = _get_dir;
  this->input_class.dispose           = _dispose_class_sftp;

  this->xine = xine;

  _x_input_register_show_hidden_files(xine->config);
  _x_input_register_default_servers(xine->config);

  return this;
}

/*
 * exported plugin catalog entry
 */

const input_info_t input_info_sftp = {
  100,   /* priority */
};

const input_info_t input_info_scp = {
  100,   /* priority */
};

const plugin_info_t xine_plugin_info[] EXPORTED = {
  /* type, API, "name", version, special_info, init_function */
  { PLUGIN_INPUT, INPUT_PLUGIN_IFACE_VERSION, "SFTP", XINE_VERSION_CODE, &input_info_sftp, sftp_init_class },
  { PLUGIN_INPUT, INPUT_PLUGIN_IFACE_VERSION, "SCP",  XINE_VERSION_CODE, &input_info_scp,  scp_init_class },
  { PLUGIN_NONE, 0, "", 0, NULL, NULL }
};
