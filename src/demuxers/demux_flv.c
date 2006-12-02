/*
 * Copyright (C) 2004 the xine project
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
 * Flash Video (.flv) File Demuxer
 *   by Mike Melanson (melanson@pcisys.net)
 * For more information on the FLV file format, visit:
 * http://download.macromedia.com/pub/flash/flash_file_format_specification.pdf
 *
 * $Id: demux_flv.c,v 1.9 2006/07/10 22:08:13 dgp85 Exp $
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>

#define LOG_MODULE "demux_flv"
#define LOG_VERBOSE
/*
#define LOG
*/

#include "xine_internal.h"
#include "xineutils.h"
#include "compat.h"
#include "demux.h"
#include "bswap.h"
#include "group_games.h"

typedef struct {
  demux_plugin_t       demux_plugin;

  xine_stream_t       *stream;
  fifo_buffer_t       *video_fifo;
  fifo_buffer_t       *audio_fifo;
  input_plugin_t      *input;
  int                  status;

  unsigned int         video_type;
  unsigned int         audio_type;

  off_t                data_start;
  off_t                data_size;

  unsigned char        bih[sizeof(xine_bmiheader)];
  xine_waveformatex    wave;

} demux_flv_t ;

typedef struct {
  demux_class_t     demux_class;
} demux_flv_class_t;

/* returns 1 if the FLV file was opened successfully, 0 otherwise */
static int open_flv_file(demux_flv_t *this) {

  unsigned char buffer[4];
  off_t first_offset;

  if (_x_demux_read_header(this->input, buffer, 4) != 4)
    return 0;

  if ((buffer[0] != 'F') || (buffer[1] != 'L') || (buffer[2] != 'V'))
    return 0;

  this->video_type = this->audio_type = 0;
  if (buffer[3] & 0x1)
    this->video_type = BUF_VIDEO_FLV1;
/* buffer[3] * 0x4 indicates audio, possibly always MP3; deal with
   that later */

  /* file is qualified at this point; position to start of first packet */
  this->input->seek(this->input, 5, SEEK_SET);
  if (this->input->read(this->input, buffer, 4) != 4)
    return 0;

  first_offset = BE_32(buffer);
  this->input->seek(this->input, first_offset, SEEK_SET);
  lprintf("  qualified FLV file, repositioned @ offset 0x%" PRIxMAX "\n", 
          (intmax_t)first_offset);

  return 1;
}

static int demux_flv_send_chunk(demux_plugin_t *this_gen) {

  demux_flv_t *this = (demux_flv_t *) this_gen;
  buf_element_t *buf = NULL;
  unsigned int remaining_bytes;
  unsigned char chunk_type, sub_type;
  int64_t pts;

  unsigned char buffer[12];

  lprintf ("  sending FLV chunk...\n");
  this->input->seek(this->input, 4, SEEK_CUR);
  if (this->input->read(this->input, buffer, 12) != 12) {
    this->status = DEMUX_FINISHED;
    return this->status;
  }

  chunk_type = buffer[0];
  remaining_bytes = BE_32(&buffer[0]);
  remaining_bytes &= 0x00FFFFFF;
  pts = BE_32(&buffer[3]);
  pts &= 0x00FFFFFF;
  sub_type = buffer[11];

  /* Flash timestamps are in milliseconds; multiply by 90 to get xine pts */
  pts *= 90;

  lprintf ("  chunk_type = %X, 0x%X -1 bytes, pts %lld, sub-type = %X\n",
           chunk_type, remaining_bytes, pts, sub_type);

  /* only handle the chunk right now if chunk type is 9 and lower nibble
   * of sub-type is 2 */
  if ((chunk_type != 9) || ((sub_type & 0x0F) != 2)) {
    this->input->seek(this->input, remaining_bytes - 1, SEEK_CUR);
  } else {
    /* send the chunk off to the video demuxer */
    remaining_bytes--;  /* sub-type byte does not count */
    while (remaining_bytes) {
      buf = this->video_fifo->buffer_pool_alloc (this->video_fifo);
      buf->type = BUF_VIDEO_FLV1;
      if( this->input->get_length (this->input) )
        buf->extra_info->input_normpos = (int)( (double) this->input->get_current_pos (this->input) * 
                                         65535 / this->input->get_length (this->input) );

      if (remaining_bytes > buf->max_size)
        buf->size = buf->max_size;
      else
        buf->size = remaining_bytes;
      remaining_bytes -= buf->size;

      if (!remaining_bytes)
        buf->decoder_flags |= BUF_FLAG_FRAME_END;

      if (this->input->read(this->input, buf->content, buf->size) !=
        buf->size) {
        buf->free_buffer(buf);
        this->status = DEMUX_FINISHED;
        break;
      }

      buf->pts = pts;
      buf->extra_info->input_time = buf->pts / 90;
      this->video_fifo->put(this->video_fifo, buf);
    }
  }

  return this->status;
}

static void demux_flv_send_headers(demux_plugin_t *this_gen) {
  demux_flv_t *this = (demux_flv_t *) this_gen;
  buf_element_t *buf;

  this->video_fifo  = this->stream->video_fifo;
  this->audio_fifo  = this->stream->audio_fifo;

  this->status = DEMUX_OK;

  /* load stream information */
  _x_stream_info_set(this->stream, XINE_STREAM_INFO_HAS_VIDEO, 
    (this->video_type ? 1 : 0));
  _x_stream_info_set(this->stream, XINE_STREAM_INFO_HAS_AUDIO,
    (this->audio_type ? 1 : 0));

  /* send start buffers */
  _x_demux_control_start(this->stream);

  /* send init info to decoders; send the bitmapinfo header to the decoder
   * primarily as a formality since there is no real data inside */
  buf = this->video_fifo->buffer_pool_alloc (this->video_fifo);
  buf->decoder_flags = BUF_FLAG_HEADER|BUF_FLAG_STDHEADER|BUF_FLAG_FRAMERATE|
                       BUF_FLAG_FRAME_END;
  buf->decoder_info[0] = 7470;  /* initial duration */
  memcpy(buf->content, this->bih, sizeof(xine_bmiheader));
  buf->size = sizeof(xine_bmiheader);
  buf->type = BUF_VIDEO_FLV1;
  this->video_fifo->put (this->video_fifo, buf);

}

static int demux_flv_seek (demux_plugin_t *this_gen,
                           off_t start_pos, int start_time, int playing) {

  demux_flv_t *this = (demux_flv_t *) this_gen;

  /* if thread is not running, initialize demuxer */
  if( !playing ) {
    this->status = DEMUX_OK;
  }

  return this->status;
}

static void demux_flv_dispose (demux_plugin_t *this_gen) {
  demux_flv_t *this = (demux_flv_t *) this_gen;

  free(this);
}

static int demux_flv_get_status (demux_plugin_t *this_gen) {
  demux_flv_t *this = (demux_flv_t *) this_gen;

  return this->status;
}

static int demux_flv_get_stream_length (demux_plugin_t *this_gen) {
/*  demux_flv_t *this = (demux_flv_t *) this_gen;*/

  return 0;
}

static uint32_t demux_flv_get_capabilities(demux_plugin_t *this_gen) {
  return DEMUX_CAP_NOCAP;
}

static int demux_flv_get_optional_data(demux_plugin_t *this_gen,
                                       void *data, int data_type) {
  return DEMUX_OPTIONAL_UNSUPPORTED;
}

static demux_plugin_t *open_plugin (demux_class_t *class_gen, xine_stream_t *stream,
                                    input_plugin_t *input) {

  demux_flv_t    *this;

  this         = xine_xmalloc (sizeof (demux_flv_t));
  this->stream = stream;
  this->input  = input;

  this->demux_plugin.send_headers      = demux_flv_send_headers;
  this->demux_plugin.send_chunk        = demux_flv_send_chunk;
  this->demux_plugin.seek              = demux_flv_seek;
  this->demux_plugin.dispose           = demux_flv_dispose;
  this->demux_plugin.get_status        = demux_flv_get_status;
  this->demux_plugin.get_stream_length = demux_flv_get_stream_length;
  this->demux_plugin.get_capabilities  = demux_flv_get_capabilities;
  this->demux_plugin.get_optional_data = demux_flv_get_optional_data;
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
  /* falling through is intended */

  case METHOD_BY_CONTENT:
  case METHOD_EXPLICIT:

    if (!open_flv_file(this)) {
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
  return "Flash Video file demux plugin";
}

static char *get_identifier (demux_class_t *this_gen) {
  return "FLV";
}

static char *get_extensions (demux_class_t *this_gen) {
  return "flv";
}

static char *get_mimetypes (demux_class_t *this_gen) {
  return NULL;
}

static void class_dispose (demux_class_t *this_gen) {
  demux_flv_class_t *this = (demux_flv_class_t *) this_gen;

  free (this);
}

static void *init_plugin (xine_t *xine, void *data) {
  demux_flv_class_t     *this;

  this = xine_xmalloc (sizeof (demux_flv_class_t));

  this->demux_class.open_plugin     = open_plugin;
  this->demux_class.get_description = get_description;
  this->demux_class.get_identifier  = get_identifier;
  this->demux_class.get_mimetypes   = get_mimetypes;
  this->demux_class.get_extensions  = get_extensions;
  this->demux_class.dispose         = class_dispose;

  return this;
}

/*
 * exported plugin catalog entry
 */
static const demuxer_info_t demux_info_flv = {
  10                       /* priority */
};

const plugin_info_t xine_plugin_info[] EXPORTED = {
  /* type, API, "name", version, special_info, init_function */
  { PLUGIN_DEMUX, 26, "flashvideo", XINE_VERSION_CODE, &demux_info_flv, init_plugin },
  { PLUGIN_NONE, 0, "", 0, NULL, NULL }
};
