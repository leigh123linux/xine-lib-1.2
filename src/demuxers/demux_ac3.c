/*
 * Copyright (C) 2001-2018 the xine project
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
 */

/*
 * AC3 File Demuxer by Mike Melanson (melanson@pcisys.net)
 * This demuxer detects raw AC3 data in a file and shovels AC3 data
 * directly to the AC3 decoder.
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
#ifdef HAVE_ALLOCA_H
#include <alloca.h>
#endif
#ifdef HAVE_MALLOC_H
#include <malloc.h>
#endif

#define LOG_MODULE "demux_ac3"
#define LOG_VERBOSE
/*
#define LOG
*/

#include <xine/xine_internal.h>
#include <xine/xineutils.h>
#include <xine/demux.h>
#include <xine/buffer.h>
#include "bswap.h"
#include "group_audio.h"

#define DATA_TAG 0x61746164

typedef struct {
  demux_plugin_t       demux_plugin;

  xine_stream_t       *stream;
  fifo_buffer_t       *video_fifo;
  fifo_buffer_t       *audio_fifo;
  input_plugin_t      *input;
  int                  status;

  int                  seek_flag;
  int                  sample_rate;
  int                  frame_size;
  int                  running_time;

  uint32_t             buf_type;

} demux_ac3_t;

/* returns 1 if the AC3 file was opened successfully, 0 otherwise */
static int open_ac3_file(demux_ac3_t *this) {
  buf_element_t *buf = NULL;
  uint32_t offset = 0;
  int ret = 0;

  do {
    uint32_t bsize = 0;
    int spdif_mode = 0;
    uint8_t hb[MAX_PREVIEW_SIZE];
    const uint8_t *b;

    do {
      if (!INPUT_IS_SEEKABLE (this->input))
        break;
      bsize = this->input->get_blocksize (this->input);
      if (!bsize)
        break;
      if (this->input->seek (this->input, 0, SEEK_SET) != 0)
        break;
      buf = this->input->read_block (this->input, this->stream->audio_fifo, bsize);
      if (!buf)
        break;
      if (this->input->seek (this->input, 0, SEEK_SET) != 0) { /* should not happen */
        buf->free_buffer (buf);
        buf = NULL;
        break;
      }
      b = buf->content;
      bsize = buf->size;
    } while (0);
    if (!buf) {
      int s;
      s = _x_demux_read_header (this->input, hb, MAX_PREVIEW_SIZE);
      if (s <= 0)
        break;
      bsize = s;
      b = hb;
    }
    lprintf ("peek size: %u\n", (unsigned int)bsize);
    if (bsize < 16)
      break;

    do {
      /* Check for wav header, as we'll handle AC3 with a wav header shoved
       * on the front for CD burning.
       * FIXME: This is risky. Real LPCM may contain anything, even sync words. */
      unsigned int audio_type;
      xine_waveformatex *wave;
      uint32_t o;

      /* Check this looks like a cd audio wav */
      if (bsize < 20 + sizeof (xine_waveformatex))
        break;
      if (memcmp (b, "RIFF", 4) && memcmp (b + 8, "WAVEfmt ", 8))
        break;
      wave = (xine_waveformatex *)(b + 20);
      _x_waveformatex_le2me (wave);
      audio_type = _x_formattag_to_buf_audio (wave->wFormatTag);
      if ((audio_type != BUF_AUDIO_LPCM_LE) || (wave->nChannels != 2) ||
          (wave->nSamplesPerSec != 44100) || (wave->wBitsPerSample != 16))
        break;
      lprintf ("looks like a cd audio wav file\n");

      /* Find the data chunk */
      o = _X_LE_32 (b + 16);
      if (o > bsize - 20 - 8)
        break;
      o += 20;
      do {
        uint32_t chunk_tag = _X_LE_32 (b + o);
        uint32_t chunk_size = _X_LE_32 (b + o + 4);
        o += 8;
        if (chunk_tag == DATA_TAG) {
          offset = o;
          lprintf ("found the start of the data at offset %d\n", offset);
          break;
        }
        if (o >= bsize)
          break;
        if (chunk_size > bsize - o)
          break;
        o += chunk_size;
      } while (o <= bsize - 8);
    } while (0);

    /* Look for a valid AC3 sync word */
    {
      uint32_t syncword = 0;
      while (offset < bsize) {
        if ((syncword & 0xffff) == 0x0b77) {
          offset -= 2;
          lprintf ("found AC3 syncword at offset %u\n", (unsigned int)offset);
          break;
        }
        if ((syncword == 0x72f81f4e) && (b[offset] == 0x01)) {
          spdif_mode = 1;
          lprintf ("found AC3 SPDIF header at offset %u\n", (unsigned int)offset - 4);
          offset += 4;
          break;
        }
        syncword = (syncword << 8) | b[offset];
        offset++;
      }
    }
    if (offset >= bsize - 2)
      break;

    if (spdif_mode) {
      this->sample_rate = 44100;
      this->frame_size = 256 * 6 * 4;
      this->buf_type = BUF_AUDIO_DNET;
      ret = 1;
      break;
    }

    {
      static const uint8_t byterates[19] = {
         4,   5,   6,   7,   8,  10,  12,  14,
        16,  20,  24,  28,  32,  40,  48,  56,
        64,  72,  80
      };
      uint32_t idx, rate;
      if (offset >= bsize - 4)
        break;
      idx = b[offset + 4];
      if ((idx & 0x3f) > 37)
        break;
      rate = (uint32_t)byterates[(idx >> 1) & 0x1f];
      if (idx & 0x80) {
        if (idx & 0x40) {
          break;
        } else {
          this->frame_size = rate * 24 * 2;
          this->sample_rate = 32000;
        }
      } else {
        if (idx & 0x40) {
          this->frame_size = ((rate * 16 * 48000 / 44100) + (idx & 0x01)) * 2;
          this->sample_rate = 44100;
        } else {
          this->frame_size = rate * 16 * 2;
          this->sample_rate = 48000;
        }
      }
    }

    /* Look for a second sync word */
    if ((offset + this->frame_size + 1 >= bsize) ||
        (b[offset + this->frame_size] != 0x0b) ||
        (b[offset + this->frame_size + 1] != 0x77)) {
      break;
    }
    lprintf ("found second AC3 sync word\n");
    this->buf_type = BUF_AUDIO_A52;
    ret = 1;
  } while (0);

  if (buf)
    buf->free_buffer (buf);

  if (ret) {
    this->running_time = this->input->get_length (this->input) - offset;
    this->running_time /= this->frame_size;
    this->running_time *= (90000 / 1000) * (256 * 6);
    this->running_time /= this->sample_rate;

    lprintf ("sample rate: %d\n", this->sample_rate);
    lprintf ("frame size: %d\n", this->frame_size);
    lprintf("running time: %d\n", this->running_time);
  }
  return ret;
}

static int demux_ac3_send_chunk (demux_plugin_t *this_gen) {
  demux_ac3_t *this = (demux_ac3_t *) this_gen;

  buf_element_t *buf = NULL;
  off_t current_stream_pos;
  int64_t audio_pts;
  int frame_number;
  uint32_t blocksize;

  current_stream_pos = this->input->get_current_pos(this->input);
  frame_number = current_stream_pos / this->frame_size;

  /*
   * Each frame represents 256*6 new audio samples according to the a52 spec.
   * Thus, the pts computation should work something like:
   *
   *   pts = frame #  *  256*6 samples        1 sec        90000 pts
   *                     -------------  *  -----------  *  ---------
   *                        1 frame        sample rate       1 sec
   */
  audio_pts = frame_number;
  audio_pts *= 90000;
  audio_pts *= 256 * 6;
  audio_pts /= this->sample_rate;

  if (this->seek_flag) {
    _x_demux_control_newpts(this->stream, audio_pts, BUF_FLAG_SEEK);
    this->seek_flag = 0;
  }

  blocksize = this->input->get_blocksize(this->input);
  if (blocksize) {
    buf = this->input->read_block(this->input, this->audio_fifo,
                                  blocksize);
    if (!buf) {
      this->status = DEMUX_FINISHED;
      return this->status;
    }
  } else {
    buf = this->audio_fifo->buffer_pool_alloc (this->audio_fifo);
    buf->size = this->input->read(this->input, buf->content,
                                  this->frame_size);
  }

  if (buf->size <= 0) {
    buf->free_buffer(buf);
    this->status = DEMUX_FINISHED;
    return this->status;
  }

  buf->type = this->buf_type;
  if( this->input->get_length (this->input) )
    buf->extra_info->input_normpos = (int)( (double) current_stream_pos *
                                     65535 / this->input->get_length (this->input) );
  buf->extra_info->input_time = audio_pts / 90;
  buf->pts = audio_pts;
  buf->decoder_flags |= BUF_FLAG_FRAME_END;

  this->audio_fifo->put (this->audio_fifo, buf);

  return this->status;
}

static void demux_ac3_send_headers(demux_plugin_t *this_gen) {
  demux_ac3_t *this = (demux_ac3_t *) this_gen;
  buf_element_t *buf;

  this->video_fifo  = this->stream->video_fifo;
  this->audio_fifo  = this->stream->audio_fifo;

  this->status = DEMUX_OK;

  /* load stream information */
  _x_stream_info_set(this->stream, XINE_STREAM_INFO_HAS_VIDEO, 0);
  _x_stream_info_set(this->stream, XINE_STREAM_INFO_HAS_AUDIO, 1);

  /* send start buffers */
  _x_demux_control_start(this->stream);

  /* send init info to decoders */
  if (this->audio_fifo) {
    buf = this->audio_fifo->buffer_pool_alloc (this->audio_fifo);
    buf->type = this->buf_type;
    buf->decoder_flags = BUF_FLAG_HEADER|BUF_FLAG_FRAME_END;
    buf->size = 0;
    this->audio_fifo->put (this->audio_fifo, buf);
  }
}

static int demux_ac3_seek (demux_plugin_t *this_gen,
                           off_t start_pos, int start_time, int playing) {

  demux_ac3_t *this = (demux_ac3_t *) this_gen;
  start_pos = (off_t) ( (double) start_pos / 65535 *
              this->input->get_length (this->input) );

  (void)start_time;
  (void)playing;

  this->seek_flag = 1;
  this->status = DEMUX_OK;
  _x_demux_flush_engine (this->stream);

  /* if input is non-seekable, do not proceed with the rest of this
   * seek function */
  if (!INPUT_IS_SEEKABLE(this->input))
    return this->status;

  /* divide the requested offset integer-wise by the frame alignment and
   * multiply by the frame alignment to determine the new starting block */
  start_pos /= this->frame_size;
  start_pos *= this->frame_size;
  this->input->seek(this->input, start_pos, SEEK_SET);

  return this->status;
}

static int demux_ac3_get_status (demux_plugin_t *this_gen) {
  demux_ac3_t *this = (demux_ac3_t *) this_gen;

  return this->status;
}

/* return the approximate length in milliseconds */
static int demux_ac3_get_stream_length (demux_plugin_t *this_gen) {
  demux_ac3_t *this = (demux_ac3_t *) this_gen;

  return this->running_time;
}

static uint32_t demux_ac3_get_capabilities(demux_plugin_t *this_gen) {
  (void)this_gen;
  return DEMUX_CAP_NOCAP;
}

static int demux_ac3_get_optional_data(demux_plugin_t *this_gen,
                                       void *data, int data_type) {
  (void)this_gen;
  (void)data;
  (void)data_type;
  return DEMUX_OPTIONAL_UNSUPPORTED;
}

static demux_plugin_t *open_plugin (demux_class_t *class_gen, xine_stream_t *stream,
                                   input_plugin_t *input) {

  demux_ac3_t   *this;

  this = calloc(1, sizeof(demux_ac3_t));
  if (!this)
    return NULL;

  this->stream = stream;
  this->input  = input;

  this->demux_plugin.send_headers      = demux_ac3_send_headers;
  this->demux_plugin.send_chunk        = demux_ac3_send_chunk;
  this->demux_plugin.seek              = demux_ac3_seek;
  this->demux_plugin.dispose           = default_demux_plugin_dispose;
  this->demux_plugin.get_status        = demux_ac3_get_status;
  this->demux_plugin.get_stream_length = demux_ac3_get_stream_length;
  this->demux_plugin.get_capabilities  = demux_ac3_get_capabilities;
  this->demux_plugin.get_optional_data = demux_ac3_get_optional_data;
  this->demux_plugin.demux_class       = class_gen;

  this->status = DEMUX_FINISHED;

  switch (stream->content_detection_method) {

  case METHOD_BY_MRL:
  case METHOD_BY_CONTENT:
  case METHOD_EXPLICIT:

    if (!open_ac3_file(this)) {
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

void *demux_ac3_init_plugin (xine_t *xine, const void *data) {

  (void)xine;
  (void)data;

  static const demux_class_t demux_ac3_class = {
    .open_plugin     = open_plugin,
    .description     = N_("Raw AC3 demux plugin"),
    .identifier      = "AC3",
    .mimetypes       = "audio/ac3: ac3: Dolby Digital audio;",
    .extensions      = "ac3",
    .dispose         = NULL,
  };

  return (void *)&demux_ac3_class;
}
