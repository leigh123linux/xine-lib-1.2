/*
 * Copyright (C) 2002-2003 the xine project
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
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA
 *
 * pnm input plugin by joschka
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <unistd.h>
#include <stdio.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#define LOG_MODULE "input_pnm"
#define LOG_VERBOSE
/*
#define LOG
*/

#include "bswap.h"
#include "xine_internal.h"
#include "xineutils.h"
#include "input_plugin.h"

#include "pnm.h"
#include "net_buf_ctrl.h"

#define BUFSIZE 4096

#if !defined(NDELAY) && defined(O_NDELAY)
#define FNDELAY O_NDELAY
#endif

typedef struct {

  input_class_t     input_class;

  xine_t           *xine;
} pnm_input_class_t;

typedef struct {
  input_plugin_t   input_plugin;

  xine_stream_t   *stream;
  
  pnm_t           *pnm;

  char            *mrl;

  off_t            curpos;

  nbc_t           *nbc; 

  char             scratch[BUFSIZE];

} pnm_input_plugin_t;


static off_t pnm_plugin_read (input_plugin_t *this_gen, 
                              char *buf, off_t len) {
  pnm_input_plugin_t *this = (pnm_input_plugin_t *) this_gen;
  off_t               n;

  lprintf ("pnm_plugin_read: %lld bytes ...\n", len);

  nbc_check_buffers (this->nbc);

  n = pnm_read (this->pnm, buf, len);
  this->curpos += n;

  return n;
}

static buf_element_t *pnm_plugin_read_block (input_plugin_t *this_gen, 
                                             fifo_buffer_t *fifo, off_t todo) {
  /*pnm_input_plugin_t   *this = (pnm_input_plugin_t *) this_gen; */
  buf_element_t        *buf = fifo->buffer_pool_alloc (fifo);
  int                   total_bytes;

  lprintf ("pnm_plugin_read_block: %lld bytes...\n", todo);

  buf->content = buf->mem;
  buf->type = BUF_DEMUX_BLOCK;
  
  total_bytes = pnm_plugin_read (this_gen, buf->content, todo);

  if (total_bytes != todo) {
    buf->free_buffer (buf);
    return NULL;
  }

  buf->size = total_bytes;

  return buf;
}

static off_t pnm_plugin_seek (input_plugin_t *this_gen, off_t offset, int origin) {
  pnm_input_plugin_t *this = (pnm_input_plugin_t *) this_gen;

  xprintf (this->stream->xine, XINE_VERBOSITY_DEBUG,
           "input_pnm: seek %" PRIdMAX " bytes, origin %d\n", (intmax_t)offset,
           origin);

  /* only realtive forward-seeking is implemented */

  if ((origin == SEEK_CUR) && (offset >= 0)) {

    for (;((int)offset) - BUFSIZE > 0; offset -= BUFSIZE) {
      this->curpos += pnm_plugin_read (this_gen, this->scratch, BUFSIZE);
    }

    this->curpos += pnm_plugin_read (this_gen, this->scratch, offset);
  }

  return this->curpos;
}

static off_t pnm_plugin_get_length (input_plugin_t *this_gen) {

  /*
  pnm_input_plugin_t   *this = (pnm_input_plugin_t *) this_gen; 
  off_t                 length;
  */

  return -1;
}

static uint32_t pnm_plugin_get_capabilities (input_plugin_t *this_gen) {
  return INPUT_CAP_PREVIEW | INPUT_CAP_RIP_FORBIDDEN;
}

static uint32_t pnm_plugin_get_blocksize (input_plugin_t *this_gen) {
  return 0;
}

static off_t pnm_plugin_get_current_pos (input_plugin_t *this_gen){
  pnm_input_plugin_t *this = (pnm_input_plugin_t *) this_gen;

  /*
  printf ("current pos is %lld\n", this->curpos);
  */

  return this->curpos;
}

static void pnm_plugin_dispose (input_plugin_t *this_gen) {
  pnm_input_plugin_t *this = (pnm_input_plugin_t *) this_gen;

  if (this->pnm) {
    pnm_close (this->pnm);
    this->pnm = NULL;
  }

  if (this->nbc) {
    nbc_close (this->nbc);
    this->nbc = NULL;
  }
  
  if(this->mrl)
    free(this->mrl);
  
  free (this);
}

static char* pnm_plugin_get_mrl (input_plugin_t *this_gen) {
  pnm_input_plugin_t *this = (pnm_input_plugin_t *) this_gen;

  return this->mrl;
}

static int pnm_plugin_get_optional_data (input_plugin_t *this_gen, 
                                         void *data, int data_type) {
  pnm_input_plugin_t *this = (pnm_input_plugin_t *) this_gen;

  switch (data_type) {
  case INPUT_OPTIONAL_DATA_PREVIEW:

    return pnm_peek_header(this->pnm, data, MAX_PREVIEW_SIZE);

    break;
  }

  return INPUT_OPTIONAL_UNSUPPORTED;
}

static int pnm_plugin_open (input_plugin_t *this_gen) {
  pnm_input_plugin_t *this = (pnm_input_plugin_t *) this_gen;
  pnm_t              *pnm;

  lprintf ("trying to open '%s'\n", this->mrl);

  pnm = pnm_connect (this->stream, this->mrl);

  if (!pnm) {
    return 0;
  }

  this->pnm    = pnm;
  
  return 1;
}

static input_plugin_t *pnm_class_get_instance (input_class_t *cls_gen, xine_stream_t *stream, 
				    const char *data) {

  /* pnm_input_class_t  *cls = (pnm_input_class_t *) cls_gen; */
  pnm_input_plugin_t *this;
  char               *mrl = strdup(data);

  if (strncasecmp (mrl, "pnm://", 6)) {
    free (mrl);
    return NULL;
  }

  this = (pnm_input_plugin_t *) xine_xmalloc (sizeof (pnm_input_plugin_t));

  this->stream = stream;
  this->pnm    = NULL;
  this->mrl    = mrl; 
  this->nbc    = nbc_init (this->stream);
  
  this->input_plugin.open              = pnm_plugin_open;
  this->input_plugin.get_capabilities  = pnm_plugin_get_capabilities;
  this->input_plugin.read              = pnm_plugin_read;
  this->input_plugin.read_block        = pnm_plugin_read_block;
  this->input_plugin.seek              = pnm_plugin_seek;
  this->input_plugin.get_current_pos   = pnm_plugin_get_current_pos;
  this->input_plugin.get_length        = pnm_plugin_get_length;
  this->input_plugin.get_blocksize     = pnm_plugin_get_blocksize;
  this->input_plugin.get_mrl           = pnm_plugin_get_mrl;
  this->input_plugin.dispose           = pnm_plugin_dispose;
  this->input_plugin.get_optional_data = pnm_plugin_get_optional_data;
  this->input_plugin.input_class       = cls_gen;
  
  return &this->input_plugin;
}

/*
 * pnm input plugin class stuff
 */

static char *pnm_class_get_description (input_class_t *this_gen) {
  return _("pnm streaming input plugin");
}

static const char *pnm_class_get_identifier (input_class_t *this_gen) {
  return "pnm";
}

static void pnm_class_dispose (input_class_t *this_gen) {
  pnm_input_class_t  *this = (pnm_input_class_t *) this_gen;

  free (this);
}

static void *init_class (xine_t *xine, void *data) {

  pnm_input_class_t  *this;

  this = (pnm_input_class_t *) xine_xmalloc (sizeof (pnm_input_class_t));

  this->xine   = xine;

  this->input_class.get_instance       = pnm_class_get_instance;
  this->input_class.get_identifier     = pnm_class_get_identifier;
  this->input_class.get_description    = pnm_class_get_description;
  this->input_class.get_dir            = NULL;
  this->input_class.get_autoplay_list  = NULL;
  this->input_class.dispose            = pnm_class_dispose;
  this->input_class.eject_media        = NULL;

  return this;
}

/*
 * exported plugin catalog entry
 */

plugin_info_t xine_plugin_info[] = {
  /* type, API, "name", version, special_info, init_function */  
  { PLUGIN_INPUT, 15, "pnm", XINE_VERSION_CODE, NULL, init_class },
  { PLUGIN_NONE, 0, "", 0, NULL, NULL }
};

