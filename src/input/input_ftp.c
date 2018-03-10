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
#include <unistd.h>
#include <errno.h>

#define LOG_MODULE "input_ftp"
#define LOG_VERBOSE
/*
#define LOG
*/

#include <xine/xine_internal.h>
#include <xine/xineutils.h>
#include <xine/input_plugin.h>

#include "group_network.h"
#include "http_helper.h"
#include "input_helper.h"

#define DEFAULT_FTP_PORT 21

typedef struct {
  input_plugin_t   input_plugin;

  xine_t          *xine;
  xine_stream_t   *stream;

  char            *mrl;
  off_t            curpos;

  int              fd;
  int              fd_data;
  char             buf[1024];

  off_t            preview_size;
  char             preview[MAX_PREVIEW_SIZE];

} ftp_input_plugin_t;

typedef struct {
  input_class_t     input_class;
  xine_t           *xine;
  xine_mrl_t      **mrls;
} ftp_input_class_t;

/*
 * FTP protocol
 */

static int _read_response(ftp_input_plugin_t *this)
{
  int rc;

  do {
    rc = _x_io_tcp_read_line(this->stream, this->fd, this->buf, sizeof(this->buf));
    if (rc < 4)
      return -1;
    lprintf("<<< '%s'\n", this->buf);
  } while (this->buf[3] == '-');

  rc = -1;
  if (this->buf[3] == ' ') {
    rc = atoi(this->buf);
  }
  return rc;
}

static int _send_command(ftp_input_plugin_t *this, const char *cmd)
{
  size_t len;
  int rc = -1;
  char *s;

  lprintf(">>> %s\n", cmd);
  this->buf[0] = 0;

  s = _x_asprintf("%s\r\n", cmd);
  if (!s)
    return -1;

  len = strlen(s);
  rc = _x_io_tcp_write(this->stream, this->fd, s, len);
  free(s);

  if (rc != len) {
    xprintf(this->xine, XINE_VERBOSITY_LOG, LOG_MODULE ": "
            "send failed\n");
    return -1;
  }

  rc = _read_response(this);
  return rc;
}

static int _connect(ftp_input_plugin_t *this, int *fd, const char *host, int port)
{
  _x_assert(*fd < 0);

  if (!port)
    port = DEFAULT_FTP_PORT;

  *fd = _x_io_tcp_connect (this->stream, host, port);
  if (*fd < 0) {
    return -1;
  }

  do {
    int rc = _x_io_tcp_connect_finish(this->stream, *fd, 1000);
    if (rc == XIO_READY)
      break;
    if (rc != XIO_TIMEOUT)
      return -1;
  } while (1);

  return 0;
}

static int _login(ftp_input_plugin_t *this,
                  const char *user, const char *pass)
{
  char *s;
  int rc;

  s = _x_asprintf("USER %s", user);
  if (!s)
    return -1;
  rc = _send_command(this, s);
  free(s);

  if (rc / 100 == 2) {
    return 0;
  }
  if (rc / 100 != 3) {
    return -1;
  }

  s = _x_asprintf("PASS %s", pass);
  if (!s)
    return -1;
  rc = _send_command(this, s);
  free(s);

  if (rc / 100 == 2) {
    return 0;
  }
  return -1;
}

static int _ftp_connect(ftp_input_plugin_t *this, xine_url_t *url)
{
  const char *user, *pass;
  int rc;

  rc = _connect(this, &this->fd, url->host, url->port);
  if (rc < 0) {
    xprintf(this->xine, XINE_VERBOSITY_LOG, LOG_MODULE ": "
            "Connect to %s failed\n", this->mrl);
    return -1;
  }

  /* check prompt */
  rc = _read_response(this);
  if (rc / 100 != 2) {
    xprintf(this->xine, XINE_VERBOSITY_LOG, LOG_MODULE ": "
            "FTP connect failed: %s\n", this->buf);
    return -1;
  }

  if (!url->user) {
    xprintf(this->xine, XINE_VERBOSITY_LOG, LOG_MODULE ": "
            "No username in mrl, logging in as anonymous\n");
  }

  user = url->user     ? url->user     : "anonymous";
  pass = url->password ? url->password : "anonymous@anonymous.org";

  rc = _login(this, user, pass);
  if (rc < 0) {
    if (!url->user || !url->password) {
      xprintf(this->xine, XINE_VERBOSITY_LOG, LOG_MODULE ": "
              "Authentication required for '%s'\n", this->mrl);
    } else {
      xprintf(this->xine, XINE_VERBOSITY_LOG, LOG_MODULE ": "
              "Authentication by password failed: %s\n", this->buf);
    }
    if (this->stream)
      _x_message(this->stream, XINE_MSG_AUTHENTICATION_NEEDED,
                 this->mrl, "Authentication required", NULL);
    return -1;
  }

  /* check passive mode support */
  rc = _send_command(this, "PASV");
  if (rc / 100 != 2) {
    xprintf(this->xine, XINE_VERBOSITY_LOG, LOG_MODULE ": "
            "Failed to set passive mode: %s\n", this->buf);
    return -1;
  }

  return 0;
}

static int _connect_data(ftp_input_plugin_t *this, char type)
{
  char ip[16];
  char *pt, *cmd;
  unsigned a1, a2, a3, a4, p1, p2;
  int rc;

  _x_assert(this->fd_data < 0);

  /* request passive mode */
  rc = _send_command(this, "PASV");
  if (rc / 100 != 2) {
    xprintf(this->xine, XINE_VERBOSITY_LOG, LOG_MODULE ": "
            "Failed to set passive mode: %s\n", this->buf);
    return -1;
  }

  /* parse address */
  pt = strchr(this->buf, '(');
  if (!pt) {
    return -1;
  }
  rc = sscanf(pt, "(%u,%u,%u,%u,%u,%u", &a1, &a2, &a3, &a4, &p1, &p2);
  if (rc != 6 ||
      a1 > 255 || a2 > 255 || a3 > 255 || a4 > 255 ||
      p1 > 255 || p2 > 255) {
    xprintf(this->xine, XINE_VERBOSITY_LOG, LOG_MODULE ": "
            "Address parsing failed (%s)\n", this->buf);
    return -1;
  }
  sprintf(ip, "%u.%u.%u.%u", a1, a2, a3, a4);

  /* set transfer type */
  cmd = _x_asprintf("TYPE %c", type);
  if (!cmd)
    return -1;
  rc = _send_command(this, cmd);
  free(cmd);
  if (rc / 100 != 2) {
    xprintf(this->xine, XINE_VERBOSITY_LOG, LOG_MODULE ": "
            "Failed to set '%c' mode: %s\n", type, this->buf);
    return -1;
  }

  /* connect data stream */
  rc = _connect(this, &this->fd_data, ip, (p1 << 8) | p2);
  if (rc < 0) {
    xprintf(this->xine, XINE_VERBOSITY_LOG, LOG_MODULE ": "
            "Failed to connect data stream.\n");
    return -1;
  }

  return 0;
}

static int _cwd(ftp_input_plugin_t *this, const char *dir)
{
  int rc;
  char *cmd;

  cmd = _x_asprintf("CWD %s", dir);
  if (!cmd)
    return -1;
  rc = _send_command(this, cmd);
  free(cmd);

  if (rc / 100 != 2) {
    xprintf(this->xine, XINE_VERBOSITY_LOG, LOG_MODULE ": "
            "Error changing current directory to %s: %s\n", dir, this->buf);
    return -1;
  }

  return 0;
}

static int _list(ftp_input_plugin_t *this)
{
  int rc;

  rc = _connect_data(this, 'A');
  if (rc < 0)
    return -1;

  rc = _send_command(this, "LIST");
  if (rc / 100 != 1) {
    xprintf(this->xine, XINE_VERBOSITY_DEBUG, LOG_MODULE ": "
            "Error listing files in verbose mode: %s\n", this->buf);
    rc = _send_command(this, "NLST");
    if (rc / 100 != 1) {
      xprintf(this->xine, XINE_VERBOSITY_LOG, LOG_MODULE ": "
              "Failed to list files: %s\n", this->buf);
      return -1;
    }
  }

  return 0;
}

static int _retr(ftp_input_plugin_t *this, const char *uri)
{
  int rc;
  char *cmd;

  rc = _connect_data(this, 'I');
  if (rc < 0)
    return -1;

  cmd = _x_asprintf("RETR %s", uri);
  if (!cmd)
    return -1;
  rc = _send_command(this, cmd);
  free(cmd);

  if (rc / 100 != 1) {
    xprintf(this->xine, XINE_VERBOSITY_LOG, LOG_MODULE ": "
            "Failed to retrive file %s: %s\n", uri, this->buf);
    return -1;
  }

  return 0;
}

/*
 * xine plugin
 */

static off_t _ftp_read (input_plugin_t *this_gen, void *buf_gen, off_t len)
{
  ftp_input_plugin_t *this = (ftp_input_plugin_t *) this_gen;
  uint8_t *buf = buf_gen;
  off_t got = 0;
  int rc;

  while (got < len) {
    rc = _x_io_tcp_read(this->stream, this->fd_data, buf + got, len - got);
    if (rc <= 0) {
      xprintf(this->xine, XINE_VERBOSITY_LOG, LOG_MODULE ": "
              "FTP read failed\n");
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

static off_t _ftp_get_length (input_plugin_t *this_gen)
{
  return 0;
}

static off_t _ftp_get_current_pos (input_plugin_t *this_gen)
{
  ftp_input_plugin_t *this = (ftp_input_plugin_t *) this_gen;

  return this->curpos;
}

static off_t _ftp_seek (input_plugin_t *this_gen, off_t offset, int origin)
{
  ftp_input_plugin_t *this = (ftp_input_plugin_t *) this_gen;

  return this->curpos;
}

static const char *_ftp_get_mrl (input_plugin_t *this_gen)
{
  ftp_input_plugin_t *this = (ftp_input_plugin_t *) this_gen;

  return this->mrl;
}

static int _ftp_get_optional_data (input_plugin_t *this_gen, void *data, int data_type)
{
  ftp_input_plugin_t *this = (ftp_input_plugin_t *)this_gen;

  switch (data_type) {
    case INPUT_OPTIONAL_DATA_PREVIEW:
      if (this->preview_size > 0) {
        memcpy (data, this->preview, this->preview_size);
        return this->preview_size;
      }
  }

  return INPUT_OPTIONAL_UNSUPPORTED;
}

static void _ftp_dispose (input_plugin_t *this_gen)
{
  ftp_input_plugin_t *this = (ftp_input_plugin_t *) this_gen;

  if (this->fd_data > 0) {
    close(this->fd_data);
    this->fd_data = -1;
  }
  if (this->fd > 0) {
    close(this->fd);
    this->fd = -1;
  }

  _x_freep (&this->mrl);
  free (this_gen);
}

static int _fill_preview(ftp_input_plugin_t *this)
{
  off_t got;

  got = _ftp_read (&this->input_plugin, this->preview, sizeof(this->preview));
  if (got < 1 || got > sizeof(this->preview)) {
    xprintf(this->xine, XINE_VERBOSITY_LOG, LOG_MODULE ": "
            "Unable to read preview data\n");
    return -1;
  }

  this->preview_size = got;
  return 0;
}

static int _ftp_open (input_plugin_t *this_gen)
{
  ftp_input_plugin_t *this = (ftp_input_plugin_t *)this_gen;
  xine_url_t url;
  int rc, result = 0;

  /* parse mrl */
  if (!_x_url_parse2(this->mrl, &url)) {
    _x_message(this->stream, XINE_MSG_GENERAL_WARNING, "malformed url", NULL);
    return 0;
  }

  this->curpos = 0;

  rc = _ftp_connect(this, &url);
  if (rc < 0)
    goto out;

  rc = _retr(this, url.uri);
  if (rc < 0)
    goto out;

  rc = _fill_preview(this);
  if (rc < 0)
    goto out;

  result = 1;

 out:
  _x_url_cleanup(&url);
  return result;
}

static input_plugin_t *_get_instance (input_class_t *cls_gen, xine_stream_t *stream, const char *mrl)
{
  ftp_input_class_t *class = (ftp_input_class_t *)cls_gen;
  ftp_input_plugin_t *this;

  if (strncasecmp (mrl, "ftp://", 6)) {
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
  this->fd            = -1;
  this->fd_data       = -1;

  this->input_plugin.open              = _ftp_open;
  this->input_plugin.get_capabilities  = _x_input_get_capabilities_preview;
  this->input_plugin.read              = _ftp_read;
  this->input_plugin.read_block        = _x_input_default_read_block;
  this->input_plugin.seek              = _ftp_seek;
  this->input_plugin.get_current_pos   = _ftp_get_current_pos;
  this->input_plugin.get_length        = _ftp_get_length;
  this->input_plugin.get_blocksize     = _x_input_default_get_blocksize;
  this->input_plugin.get_mrl           = _ftp_get_mrl;
  this->input_plugin.get_optional_data = _ftp_get_optional_data;
  this->input_plugin.dispose           = _ftp_dispose;
  this->input_plugin.input_class       = cls_gen;

  return &this->input_plugin;
}

/*
 *  plugin class
 */

static xine_mrl_t **_get_files(ftp_input_plugin_t *this, const char *uri, int *nFiles)
{
  xine_mrl_t **mrls;
  size_t n = 0, mrls_size = 64;
  int show_hidden_files;
  int rc;

  /* change working directory */
  if (uri[0] && (uri[0] != '/' || uri[1])) {
    rc = _cwd(this, uri[0] == '/' ? uri+1 : uri);
    if (rc < 0)
      return NULL;
  }

  /* open directory */
  if (_list(this) < 0)
    return NULL;

  mrls = _x_input_alloc_mrls(mrls_size);
  if (!mrls)
    return NULL;

  /* add ".." entry */
  mrls[n]->type = mrl_net | mrl_file | mrl_file_directory;
  mrls[n]->origin = strdup(this->mrl);
  mrls[n]->mrl    = _x_asprintf("%s/..", this->mrl);
  n++;

  show_hidden_files = _x_input_get_show_hidden_files(this->xine->config);

  while (1) {
    char buf[1024], *file;
    int is_dir = 0;

    rc = _x_io_tcp_read_line(this->stream, this->fd_data, buf, sizeof(buf));
    if (rc <= 0) {
      if (rc < 0)
        xprintf(this->xine, XINE_VERBOSITY_LOG, LOG_MODULE ": "
                "FTP directory read failed\n");
      break;
    }
    lprintf("<<< '%s'\n", buf);

    /* strip file info, try to detect directories */
    file = strrchr(buf, ' ');
    if (!file) {
      file = buf;
    } else {
      *file++ = 0;
      if (buf[0] == 'd' || strstr(buf, "<DIR>"))
        is_dir = 1;
    }

    if (!show_hidden_files && file[0] == '.')
      continue;

    if (n >= mrls_size) {
      mrls_size = mrls_size ? 2*mrls_size : 100;
      if (!(_x_input_realloc_mrls(&mrls, mrls_size))) {
        xprintf(this->xine, XINE_VERBOSITY_LOG, LOG_MODULE ": "
                "out of memory while listing directory '%s'\n",
                uri);
        break;
      }
    }

    if (is_dir)
      mrls[n]->type = mrl_net | mrl_file | mrl_file_directory;
    else
      mrls[n]->type = mrl_net | mrl_file | mrl_file_normal;
    mrls[n]->origin = _x_asprintf("%s/", this->mrl);
    mrls[n]->mrl    = _x_asprintf("%s/%s", this->mrl, file);
    n++;
  }

  *nFiles = n;
  return mrls;
}

static xine_mrl_t **_get_dir (input_class_t *this_gen, const char *filename, int *nFiles)
{
  ftp_input_class_t *this = (ftp_input_class_t *) this_gen;
  ftp_input_plugin_t *input;
  xine_url_t url;

  *nFiles = 0;
  _x_input_free_mrls(&this->mrls);

  if (!filename || !strcmp(filename, "ftp:/") || !strcmp(filename, "ftp://")) {
    this->mrls = _x_input_get_default_server_mrls(this->xine->config, "ftp://", nFiles);
    return this->mrls;
  }

  if (!_x_url_parse2(filename, &url)) {
    xprintf(this->xine, XINE_VERBOSITY_LOG, LOG_MODULE ": "
            "malformed url '%s'", filename);
    return NULL;
  }

  input = (ftp_input_plugin_t *)_get_instance(this_gen, NULL, filename);
  if (!input)
    goto out;

  if (_ftp_connect(input, &url) < 0)
    goto out;

  this->mrls = _get_files(input, url.uri, nFiles);

 out:
  _x_url_cleanup(&url);
  input->input_plugin.dispose(&input->input_plugin);
  return this->mrls;
}

static void _dispose_class (input_class_t *this_gen)
{
  ftp_input_class_t *this = (ftp_input_class_t *)this_gen;

  _x_input_free_mrls(&this->mrls);

  free(this_gen);
}

void *input_ftp_init_class(xine_t *xine, const void *data)
{
  ftp_input_class_t *this;

  this = calloc(1, sizeof(*this));
  if (!this)
    return NULL;

  this->xine = xine;

  this->input_class.get_instance      = _get_instance;
  this->input_class.description       = N_("FTP input plugin");
  this->input_class.identifier        = "FTP";
  this->input_class.get_dir           = _get_dir;
  this->input_class.get_autoplay_list = NULL;
  this->input_class.dispose           = _dispose_class;
  this->input_class.eject_media       = NULL;

  _x_input_register_show_hidden_files(xine->config);
  _x_input_register_default_servers(xine->config);

  return this;
}
