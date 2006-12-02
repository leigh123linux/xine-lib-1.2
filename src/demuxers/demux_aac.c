/*
 * Copyright (C) 2001-2005 the xine project
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
 * Raw AAC File Demuxer by Mike Melanson (melanson@pcisys.net)
 * This demuxer detects ADIF and ADTS headers in AAC files.
 * Then it shovels buffer-sized chunks over to the AAC decoder.
 *
 * $Id: demux_aac.c,v 1.12 2006/11/14 21:51:32 dsalt Exp $
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>

#define LOG_MODULE "demux_aac"
#define LOG_VERBOSE
/*
#define LOG
*/

#include "xine_internal.h"
#include "xineutils.h"
#include "demux.h"
#include "buffer.h"
#include "bswap.h"
#include "group_audio.h"

typedef struct {
  demux_plugin_t       demux_plugin;

  xine_stream_t       *stream;
  fifo_buffer_t       *video_fifo;
  fifo_buffer_t       *audio_fifo;
  input_plugin_t      *input;
  int                  status;

  off_t                data_start;
  off_t                data_size;

  int                  seek_flag;  /* this is set when a seek just occurred */
} demux_aac_t;

typedef struct {
  demux_class_t     demux_class;
} demux_aac_class_t;


static int open_aac_file(demux_aac_t *this) {
  int i;
  uint8_t peak[MAX_PREVIEW_SIZE];
  uint16_t syncword = 0;

  /* Check for an ADIF header - should be at the start of the file */
  if (_x_demux_read_header(this->input, peak, 4) != 4)
      return 0;

  if ((peak[0] == 'A') && (peak[1] == 'D') &&
      (peak[2] == 'I') && (peak[3] == 'F')) {
    lprintf("found ADIF header\n");
    return 1;
  }

  /* Look for an ADTS header - might not be at the start of the file */
  if (_x_demux_read_header(this->input, peak, MAX_PREVIEW_SIZE) != 
      MAX_PREVIEW_SIZE)
    return 0;

  for (i=0; i<MAX_PREVIEW_SIZE; i++) {
    if ((syncword & 0xfff6) == 0xfff0) {
      this->data_start = i - 2;
      lprintf("found ADTS header at offset %d\n", i-2);
      break;
    }

    syncword = (syncword << 8) | peak[i];
  }

  /* Look for second ADTS header to confirm it's really aac */
  if (this->data_start + 5 < MAX_PREVIEW_SIZE) {
    int frame_size = ((peak[this->data_start+3] & 0x03) << 11) |
                      (peak[this->data_start+4] << 3) |
                     ((peak[this->data_start+5] & 0xe0) >> 5);

    lprintf("first frame size %d\n", frame_size);

    if ((frame_size > 0) &&
        (this->data_start+frame_size < MAX_PREVIEW_SIZE-1) &&
        /* first 28 bits must be identical */
        (peak[this->data_start  ]   ==peak[this->data_start+frame_size  ]) &&
        (peak[this->data_start+1]   ==peak[this->data_start+frame_size+1]) &&
        (peak[this->data_start+2]   ==peak[this->data_start+frame_size+2]) &&
        (peak[this->data_start+3]>>4==peak[this->data_start+frame_size+3]>>4))
    {
      lprintf("found second ADTS header\n");

      _x_stream_info_set(this->stream, XINE_STREAM_INFO_HAS_VIDEO, 0);
      _x_stream_info_set(this->stream, XINE_STREAM_INFO_HAS_AUDIO, 1);

      this->input->seek(this->input, this->data_start, SEEK_SET);
      return 1;
    }
  }

  return 0;
}

static int demux_aac_send_chunk(demux_plugin_t *this_gen) {
  demux_aac_t *this = (demux_aac_t *) this_gen;
  int bytes_read;
  off_t current_pos, length;
  uint32_t bitrate;

  buf_element_t *buf = NULL;

  /* just load an entire buffer from wherever the audio happens to be */
  buf = this->audio_fifo->buffer_pool_alloc (this->audio_fifo);
  buf->type = BUF_AUDIO_AAC;
  buf->pts = 0;

  length = this->input->get_length(this->input);
  current_pos = this->input->get_current_pos(this->input);
  bitrate = _x_stream_info_get(this->stream, XINE_STREAM_INFO_AUDIO_BITRATE);

  if (length)
    buf->extra_info->input_normpos = (int)((double) current_pos * 65535/length);

  if (bitrate)
    buf->extra_info->input_time = (8*current_pos) / (bitrate/1000);

  bytes_read = this->input->read(this->input, buf->content, buf->max_size);
  if (bytes_read == 0) {
    buf->free_buffer(buf);
    this->status = DEMUX_FINISHED;
    return this->status;
  } else 
    buf->size = bytes_read;

  /* each buffer stands on its own */
  buf->decoder_flags |= BUF_FLAG_FRAME_END;

  this->audio_fifo->put (this->audio_fifo, buf);

  return this->status;
}

static void demux_aac_send_headers(demux_plugin_t *this_gen) {
  demux_aac_t *this = (demux_aac_t *) this_gen;
  buf_element_t *buf;

  this->video_fifo  = this->stream->video_fifo;
  this->audio_fifo  = this->stream->audio_fifo;

  this->status = DEMUX_OK;

  /* send start buffers */
  _x_demux_control_start(this->stream);

  /* send init info to decoders */
  if (this->audio_fifo) {
    buf = this->audio_fifo->buffer_pool_alloc (this->audio_fifo);
    buf->type = BUF_AUDIO_AAC;
    buf->decoder_flags = BUF_FLAG_HEADER|BUF_FLAG_FRAME_END;
    buf->content = NULL;
    buf->size = 0;
    this->audio_fifo->put (this->audio_fifo, buf);
  }
}

static int demux_aac_seek (demux_plugin_t *this_gen,
                           off_t start_pos, int start_time, int playing) {
  demux_aac_t *this = (demux_aac_t *) this_gen;

  /* if thread is not running, initialize demuxer */
  if( !playing ) {

    /* send new pts */
    _x_demux_control_newpts(this->stream, 0, 0);

    this->status = DEMUX_OK;
  }

  return this->status;
}

static void demux_aac_dispose (demux_plugin_t *this_gen) {
  demux_aac_t *this = (demux_aac_t *) this_gen;

  free(this);
}

static int demux_aac_get_status (demux_plugin_t *this_gen) {
  demux_aac_t *this = (demux_aac_t *) this_gen;

  return this->status;
}

static int demux_aac_get_stream_length (demux_plugin_t *this_gen) {
//  demux_aac_t *this = (demux_aac_t *) this_gen;

  return 0;
}

static uint32_t demux_aac_get_capabilities(demux_plugin_t *this_gen) {
  return DEMUX_CAP_NOCAP;
}

static int demux_aac_get_optional_data(demux_plugin_t *this_gen,
                                       void *data, int data_type) {
  return DEMUX_OPTIONAL_UNSUPPORTED;
}

static demux_plugin_t *open_plugin (demux_class_t *class_gen, xine_stream_t *stream,
                                    input_plugin_t *input) {

  demux_aac_t    *this;

  this         = xine_xmalloc (sizeof (demux_aac_t));
  this->stream = stream;
  this->input  = input;

  this->demux_plugin.send_headers      = demux_aac_send_headers;
  this->demux_plugin.send_chunk        = demux_aac_send_chunk;
  this->demux_plugin.seek              = demux_aac_seek;
  this->demux_plugin.dispose           = demux_aac_dispose;
  this->demux_plugin.get_status        = demux_aac_get_status;
  this->demux_plugin.get_stream_length = demux_aac_get_stream_length;
  this->demux_plugin.get_capabilities  = demux_aac_get_capabilities;
  this->demux_plugin.get_optional_data = demux_aac_get_optional_data;
  this->demux_plugin.demux_class       = class_gen;

  this->status = DEMUX_FINISHED;
  switch (stream->content_detection_method) {

  case METHOD_BY_EXTENSION: {
    char *extensions, *mrl;

    mrl = input->get_mrl (input);
    extensions = class_gen->get_extensions (class_gen);

    if (!_x_demux_check_extension (mrl, extensions)) {
      free (this);
      return NULL;
    }
  }
  /* Falling through is intended */

  case METHOD_BY_CONTENT:
  case METHOD_EXPLICIT:
    if (!open_aac_file(this)) {
      free (this);
      return NULL;
    }
    break;

  default:
    free (this);
    return NULL;
  }

  return &this->demux_plugin;
}

static char *get_description (demux_class_t *this_gen) {
  return "ADIF/ADTS AAC demux plugin";
}

static char *get_identifier (demux_class_t *this_gen) {
  return "AAC";
}

static char *get_extensions (demux_class_t *this_gen) {
  return "aac";
}

static char *get_mimetypes (demux_class_t *this_gen) {
  return NULL;
}

static void class_dispose (demux_class_t *this_gen) {
  demux_aac_class_t *this = (demux_aac_class_t *) this_gen;

  free (this);
}

void *demux_aac_init_plugin (xine_t *xine, void *data) {
  demux_aac_class_t     *this;

  this = xine_xmalloc (sizeof (demux_aac_class_t));

  this->demux_class.open_plugin     = open_plugin;
  this->demux_class.get_description = get_description;
  this->demux_class.get_identifier  = get_identifier;
  this->demux_class.get_mimetypes   = get_mimetypes;
  this->demux_class.get_extensions  = get_extensions;
  this->demux_class.dispose         = class_dispose;

  return this;
}

