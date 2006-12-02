/*
 * Copyright (C) 2001-2003 the xine project
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
 */

/*
 * CDDA "Demuxer" by Mike Melanson (melanson@pcisys.net)
 * All this demuxer does is read raw CD frames and shovel them to the
 * linear PCM "decoder" (which in turn sends them directly to the audio
 * output target; this is a really fancy CD-playing architecture).
 *
 * $Id: demux_cdda.c,v 1.19 2004/06/13 21:28:52 miguelfreitas Exp $
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

/********** logging **********/
#define LOG_MODULE "demux_cdda"
/* #define LOG_VERBOSE */
/* #define LOG */

#include "xine_internal.h"
#include "xineutils.h"
#include "demux.h"
#include "buffer.h"
#include "bswap.h"
#include "group_audio.h"

/* 44100 samples/sec * 2 bytes/samples * 2 channels */
#define CD_BYTES_PER_SECOND (44100 * 2 * 2)

typedef struct {
  demux_plugin_t       demux_plugin;

  xine_stream_t       *stream;
  fifo_buffer_t       *video_fifo;
  fifo_buffer_t       *audio_fifo;
  input_plugin_t      *input;
  int                  status;

  int                  seek_flag;  /* this is set when a seek just occurred */
} demux_cdda_t;

typedef struct {
  demux_class_t     demux_class;
} demux_cdda_class_t;

static int demux_cdda_send_chunk (demux_plugin_t *this_gen) {
  demux_cdda_t *this = (demux_cdda_t *) this_gen;
  buf_element_t *buf = NULL;
  uint32_t blocksize;

  blocksize = this->input->get_blocksize(this->input);
  if (!blocksize) blocksize = 2352;
  buf = this->input->read_block(this->input, this->audio_fifo,
    blocksize);
  if (!buf) {
    this->status = DEMUX_FINISHED;
    return this->status;
  }

  buf->type = BUF_AUDIO_LPCM_LE;
  if( this->input->get_length (this->input) )
    buf->extra_info->input_normpos = (int)( (double) this->input->get_current_pos (this->input) * 
                                     65535 / this->input->get_length (this->input) );
  buf->pts = this->input->get_current_pos(this->input);
  buf->pts *= 90000;
  buf->pts /= CD_BYTES_PER_SECOND;
  buf->extra_info->input_time = buf->pts / 90;
  buf->decoder_flags |= BUF_FLAG_FRAME_END;

  if (this->seek_flag) {
    _x_demux_control_newpts(this->stream, buf->pts, BUF_FLAG_SEEK);
    this->seek_flag = 0;
  }

  this->audio_fifo->put (this->audio_fifo, buf);

  return this->status;
}

static void demux_cdda_send_headers(demux_plugin_t *this_gen) {
  demux_cdda_t *this = (demux_cdda_t *) this_gen;
  buf_element_t *buf;

  this->video_fifo  = this->stream->video_fifo;
  this->audio_fifo  = this->stream->audio_fifo;

  this->status = DEMUX_OK;

  /* load stream information */
  _x_stream_info_set(this->stream, XINE_STREAM_INFO_SEEKABLE,
                       INPUT_IS_SEEKABLE(this->input));
  _x_stream_info_set(this->stream, XINE_STREAM_INFO_HAS_VIDEO, 0);
  _x_stream_info_set(this->stream, XINE_STREAM_INFO_HAS_AUDIO, 1);
  _x_stream_info_set(this->stream, XINE_STREAM_INFO_AUDIO_CHANNELS, 2);
  _x_stream_info_set(this->stream, XINE_STREAM_INFO_AUDIO_SAMPLERATE, 44100);
  _x_stream_info_set(this->stream, XINE_STREAM_INFO_AUDIO_BITS, 16);

  /* send start buffers */
  _x_demux_control_start(this->stream);

  /* send init info to decoders */
  if (this->audio_fifo) {
    buf = this->audio_fifo->buffer_pool_alloc (this->audio_fifo);
    buf->type = BUF_AUDIO_LPCM_LE;
    buf->decoder_flags = BUF_FLAG_HEADER|BUF_FLAG_STDHEADER|BUF_FLAG_FRAME_END;
    buf->decoder_info[0] = 0;
    buf->decoder_info[1] = 44100;
    buf->decoder_info[2] = 16;
    buf->decoder_info[3] = 2;
    buf->size = 0;
    this->audio_fifo->put (this->audio_fifo, buf);
  }
}

static int demux_cdda_seek (demux_plugin_t *this_gen, off_t start_pos, int start_time, int playing) {
  demux_cdda_t *this = (demux_cdda_t *) this_gen;
  start_pos = (off_t) ( (double) start_pos / 65535 *
              this->input->get_length (this->input) );
  start_time /= 1000;

  if (start_pos)
    this->input->seek(this->input, start_pos & ~3, SEEK_SET);
  else
    this->input->seek(this->input, start_time * CD_BYTES_PER_SECOND, SEEK_SET);
  this->seek_flag = 1;
  this->status = DEMUX_OK;
  _x_demux_flush_engine (this->stream);

  return this->status;
}

static void demux_cdda_dispose (demux_plugin_t *this_gen) {
  demux_cdda_t *this = (demux_cdda_t *) this_gen;

  free(this);
}

static int demux_cdda_get_status (demux_plugin_t *this_gen) {
  demux_cdda_t *this = (demux_cdda_t *) this_gen;

  return this->status;
}

/* return the approximate length in miliseconds */
static int demux_cdda_get_stream_length (demux_plugin_t *this_gen) {
  demux_cdda_t *this = (demux_cdda_t *) this_gen;

  return (int)((int64_t) this->input->get_length(this->input)
                * 1000 / CD_BYTES_PER_SECOND);
}

static uint32_t demux_cdda_get_capabilities(demux_plugin_t *this_gen){
  return DEMUX_CAP_NOCAP;
}

static int demux_cdda_get_optional_data(demux_plugin_t *this_gen,
                                        void *data, int data_type){
  return DEMUX_OPTIONAL_UNSUPPORTED;
}

static demux_plugin_t *open_plugin (demux_class_t *class_gen, xine_stream_t *stream,
                                    input_plugin_t *input) {

  demux_cdda_t   *this;

  this         = xine_xmalloc (sizeof (demux_cdda_t));
  this->stream = stream;
  this->input  = input;

  this->demux_plugin.send_headers      = demux_cdda_send_headers;
  this->demux_plugin.send_chunk        = demux_cdda_send_chunk;
  this->demux_plugin.seek              = demux_cdda_seek;
  this->demux_plugin.dispose           = demux_cdda_dispose;
  this->demux_plugin.get_status        = demux_cdda_get_status;
  this->demux_plugin.get_stream_length = demux_cdda_get_stream_length;
  this->demux_plugin.get_capabilities  = demux_cdda_get_capabilities;
  this->demux_plugin.get_optional_data = demux_cdda_get_optional_data;
  this->demux_plugin.demux_class       = class_gen;

  this->status = DEMUX_FINISHED;

  switch (stream->content_detection_method) {

  case METHOD_BY_CONTENT:
  case METHOD_BY_EXTENSION:
    if (strncasecmp (input->get_mrl (input), "cdda:", 5)) {
      free (this);
      return NULL;
    }

  break;

  case METHOD_EXPLICIT:
  break;

  default:
    free (this);
    return NULL;
  }

  return &this->demux_plugin;
}

static char *get_description (demux_class_t *this_gen) {
  return "CD Digital Audio demux plugin";
}

static char *get_identifier (demux_class_t *this_gen) {
  return "CDDA";
}

static char *get_extensions (demux_class_t *this_gen) {
  return NULL;
}

static char *get_mimetypes (demux_class_t *this_gen) {
  return NULL;
}

static void class_dispose (demux_class_t *this_gen) {
  demux_cdda_class_t *this = (demux_cdda_class_t *) this_gen;

  free (this);
}

void *demux_cdda_init_plugin (xine_t *xine, void *data) {
  demux_cdda_class_t     *this;

  this = xine_xmalloc (sizeof (demux_cdda_class_t));

  this->demux_class.open_plugin     = open_plugin;
  this->demux_class.get_description = get_description;
  this->demux_class.get_identifier  = get_identifier;
  this->demux_class.get_mimetypes   = get_mimetypes;
  this->demux_class.get_extensions  = get_extensions;
  this->demux_class.dispose         = class_dispose;

  return this;
}
