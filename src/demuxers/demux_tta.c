/*
 * Copyright (C) 2006 the xine project
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
 * True Audio demuxer by Diego Pettenò <flameeyes@gentoo.org>
 * Inspired by tta libavformat demuxer by Alex Beregszaszi
 */

#define LOG_MODULE "demux_tta"
#define LOG_VERBOSE

#include "xine_internal.h"
#include "xineutils.h"
#include "demux.h"
#include "buffer.h"
#include "bswap.h"
#include "group_audio.h"
#include "attributes.h"

typedef struct {
  demux_plugin_t       demux_plugin;

  xine_stream_t       *stream;
  fifo_buffer_t       *video_fifo;
  fifo_buffer_t       *audio_fifo;
  input_plugin_t      *input;

  uint32_t            *seektable;
  uint32_t             totalframes;
  uint32_t             currentframe;

  int                  status;

  union {
    struct tta_header {
      uint32_t signature; /* TTA1 */
      uint16_t flags;     /* Skipped */
      uint16_t channels;
      uint16_t bits_per_sample;
      uint32_t samplerate;
      uint32_t data_length;
      uint32_t crc32;
    } XINE_PACKED tta;
    uint8_t buffer[22]; /* This is the size of the header */
  } header;
} demux_tta_t;

typedef struct {
  demux_class_t     demux_class;
} demux_tta_class_t;

static int open_tta_file(demux_tta_t *this) {
  uint32_t peek;
  uint32_t framelen;

  if (_x_demux_read_header(this->input, &peek, 4) != 4)
      return 0;

  if ( peek != ME_FOURCC('T', 'T', 'A', '1') )
    return 0;

  if ( this->input->read(this->input, this->header.buffer, sizeof(this->header)) != sizeof(this->header) )
    return 0;

  framelen = 1.04489795918367346939 * le2me_32(this->header.tta.samplerate);
  this->totalframes = le2me_32(this->header.tta.data_length) / framelen + ((le2me_32(this->header.tta.data_length) % framelen) ? 1 : 0);
  this->currentframe = 0;

  if(this->totalframes >= UINT_MAX/sizeof(uint32_t)) {
    xprintf(this->stream->xine, XINE_VERBOSITY_DEBUG, _("demux_tta: total frames count too high\n"));
    return 0;
  }

  this->seektable = xine_xcalloc(this->totalframes, sizeof(uint32_t));
  this->input->read(this->input, this->seektable, sizeof(uint32_t)*this->totalframes);

  /* Skip the CRC32 */
  this->input->seek(this->input, 4, SEEK_CUR);

  return 1;
}

static int demux_tta_send_chunk(demux_plugin_t *this_gen) {
  demux_tta_t *this = (demux_tta_t *) this_gen;
  uint32_t bytes_to_read;

  if ( this->currentframe > this->totalframes ) {
    this->status = DEMUX_FINISHED;
    return this->status;
  }

  bytes_to_read = le2me_32(this->seektable[this->currentframe]);

  while(bytes_to_read) {
    off_t bytes_read = 0;
    buf_element_t *buf = NULL;

    /* Get a buffer */
    buf = this->audio_fifo->buffer_pool_alloc(this->audio_fifo);
    buf->type = BUF_AUDIO_TTA;
    buf->pts = 0;
    buf->extra_info->total_time = this->totalframes;
    buf->decoder_flags = 0;

    /* Set normalised position */
    buf->extra_info->input_normpos =
      (int) ((double) this->currentframe * 65535 / this->totalframes);

    /* Set time */
    /* buf->extra_info->input_time = this->current_sample / this->samplerate; */

    bytes_read = this->input->read(this->input, buf->content, ( bytes_to_read > buf->max_size ) ? buf->max_size : bytes_to_read);

    buf->size = bytes_read;

    bytes_to_read -= bytes_read;

    if ( bytes_to_read <= 0 )
      buf->decoder_flags |= BUF_FLAG_FRAME_END;
    
    this->audio_fifo->put(this->audio_fifo, buf);
  }

  this->currentframe++;

  return this->status;
}

static void demux_tta_send_headers(demux_plugin_t *this_gen) {
  demux_tta_t *this = (demux_tta_t *) this_gen;
  buf_element_t *buf;

  this->audio_fifo  = this->stream->audio_fifo;

  this->status = DEMUX_OK;

  _x_stream_info_set(this->stream, XINE_STREAM_INFO_HAS_VIDEO, 0);
  _x_stream_info_set(this->stream, XINE_STREAM_INFO_HAS_AUDIO, 1);
  _x_stream_info_set(this->stream, XINE_STREAM_INFO_AUDIO_CHANNELS,
		     le2me_16(this->header.tta.channels));
  _x_stream_info_set(this->stream, XINE_STREAM_INFO_AUDIO_SAMPLERATE,
		     le2me_32(this->header.tta.samplerate));
  _x_stream_info_set(this->stream, XINE_STREAM_INFO_AUDIO_BITS,
		     le2me_16(this->header.tta.bits_per_sample));

  /* send start buffers */
  _x_demux_control_start(this->stream);

  /* send init info to decoders */
  if (this->audio_fifo) {
    xine_waveformatex wave;

    buf = this->audio_fifo->buffer_pool_alloc (this->audio_fifo);
    buf->type = BUF_AUDIO_TTA;
    buf->decoder_flags = BUF_FLAG_HEADER|BUF_FLAG_STDHEADER|BUF_FLAG_FRAME_END;
    buf->decoder_info[0] = 0;
    buf->decoder_info[1] = le2me_32(this->header.tta.samplerate);
    buf->decoder_info[2] = le2me_16(this->header.tta.bits_per_sample);
    buf->decoder_info[3] = le2me_16(this->header.tta.channels);

    buf->size = sizeof(xine_waveformatex) + sizeof(this->header) + sizeof(uint32_t)*this->totalframes;
    memcpy(buf->content+sizeof(xine_waveformatex), this->header.buffer, sizeof(this->header));
    memcpy(buf->content+sizeof(xine_waveformatex)+sizeof(this->header), this->seektable, sizeof(uint32_t)*this->totalframes);

    wave.cbSize = buf->size - sizeof(xine_waveformatex);
    memcpy(buf->content, &wave, sizeof(wave));

    this->audio_fifo->put (this->audio_fifo, buf);
  }
}

static int demux_tta_seek (demux_plugin_t *this_gen,
                           off_t start_pos, int start_time, int playing) {
  demux_tta_t *this = (demux_tta_t *) this_gen;

  /* if thread is not running, initialize demuxer */
  if( !playing ) {

    /* send new pts */
    _x_demux_control_newpts(this->stream, 0, 0);

    this->status = DEMUX_OK;
  }

  return this->status;
}

static int demux_tta_get_status (demux_plugin_t *this_gen) {
  demux_tta_t *this = (demux_tta_t *) this_gen;

  return this->status;
}

static int demux_tta_get_stream_length (demux_plugin_t *this_gen) {
//  demux_tta_t *this = (demux_tta_t *) this_gen;

  return 0;
}

static uint32_t demux_tta_get_capabilities(demux_plugin_t *this_gen) {
  return DEMUX_CAP_NOCAP;
}

static int demux_tta_get_optional_data(demux_plugin_t *this_gen,
                                       void *data, int data_type) {
  return DEMUX_OPTIONAL_UNSUPPORTED;
}

static demux_plugin_t *open_plugin (demux_class_t *class_gen, xine_stream_t *stream,
                                    input_plugin_t *input) {

  demux_tta_t    *this;

  this         = xine_xmalloc (sizeof (demux_tta_t));
  this->stream = stream;
  this->input  = input;

  this->demux_plugin.send_headers      = demux_tta_send_headers;
  this->demux_plugin.send_chunk        = demux_tta_send_chunk;
  this->demux_plugin.seek              = demux_tta_seek;
  this->demux_plugin.dispose           = default_demux_plugin_dispose;
  this->demux_plugin.get_status        = demux_tta_get_status;
  this->demux_plugin.get_stream_length = demux_tta_get_stream_length;
  this->demux_plugin.get_capabilities  = demux_tta_get_capabilities;
  this->demux_plugin.get_optional_data = demux_tta_get_optional_data;
  this->demux_plugin.demux_class       = class_gen;

  this->status = DEMUX_FINISHED;

  this->seektable = NULL;

  switch (stream->content_detection_method) {

  case METHOD_BY_EXTENSION: {
    const char *extensions, *mrl;

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
    if (!open_tta_file(this)) {
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

void *demux_tta_init_plugin (xine_t *xine, void *data) {
  demux_tta_class_t     *this;

  this = xine_xmalloc (sizeof (demux_tta_class_t));

  this->demux_class.open_plugin     = open_plugin;
  this->demux_class.description     = N_("True Audio demux plugin");
  this->demux_class.identifier      = "True Audio";
  this->demux_class.mimetypes       = NULL;
  this->demux_class.extensions      = "tta";
  this->demux_class.dispose         = default_demux_class_dispose;

  return this;
}
