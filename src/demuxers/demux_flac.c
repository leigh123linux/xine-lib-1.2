/*
 * Copyright (C) 2000-2004 the xine project
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
 * FLAC File Demuxer by Mike Melanson (melanson@pcisys.net)
 * For more information on the FLAC file format, visit:
 *   http://flac.sourceforge.net/
 *
 * $Id: demux_flac.c,v 1.3 2004/06/14 13:40:57 mroi Exp $
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>

#define LOG_MODULE "demux_flac"
#define LOG_VERBOSE
/*
#define LOG
*/

#include "xine_internal.h"
#include "xineutils.h"
#include "compat.h"
#include "demux.h"
#include "bswap.h"
#include "group_audio.h"

typedef struct {
  off_t offset;
  int64_t sample_number;
  int64_t pts;
  int size;
} flac_seekpoint_t;

#define FLAC_SIGNATURE_SIZE 4
#define FLAC_STREAMINFO_SIZE 34
#define FLAC_SEEKPOINT_SIZE 18

typedef struct {
  demux_plugin_t       demux_plugin;

  xine_stream_t       *stream;
  fifo_buffer_t       *audio_fifo;
  input_plugin_t      *input;
  int                  status;

  int                  sample_rate;
  int                  bits_per_sample;
  int                  channels;
  int64_t              total_samples;
  off_t                data_start;
  off_t                data_size;

  unsigned char        streaminfo[sizeof(xine_waveformatex) + FLAC_STREAMINFO_SIZE];
  flac_seekpoint_t    *seekpoints;
  int                  seekpoint_count;

} demux_flac_t;

typedef struct {
  demux_class_t     demux_class;
} demux_flac_class_t;

/* Open a flac file
 * This function is called from the _open() function of this demuxer.
 * It returns 1 if flac file was opened successfully. */
static int open_flac_file(demux_flac_t *flac) {

  unsigned char preamble[4];
  unsigned int block_length;
  unsigned char buffer[16];
  unsigned char *streaminfo = flac->streaminfo + sizeof(xine_waveformatex);
  int i;

  flac->seekpoints = NULL;

  /* fetch the file signature */
  if (_x_demux_read_header(flac->input, preamble, 4) != 4)
    return 0;

  /* validate signature */
  if ((preamble[0] != 'f') ||
      (preamble[1] != 'L') ||
      (preamble[2] != 'a') ||
      (preamble[3] != 'C'))
    return 0;

  /* file is qualified; skip over the signature bytes in the stream */
  flac->input->seek(flac->input, 4, SEEK_SET);

  /* loop through the metadata blocks; use a do-while construct since there
   * will always be 1 metadata block */
  do {

    if (flac->input->read(flac->input, preamble, FLAC_SIGNATURE_SIZE) != 
        FLAC_SIGNATURE_SIZE)
      return 0;

    block_length = (preamble[1] << 16) |
                   (preamble[2] <<  8) |
                   (preamble[3] <<  0);

    switch (preamble[0] & 0x7F) {

    /* STREAMINFO */
    case 0:
      lprintf ("STREAMINFO metadata\n");
      if (block_length != FLAC_STREAMINFO_SIZE) {
        lprintf ("expected STREAMINFO chunk of %d bytes\n", 
          FLAC_STREAMINFO_SIZE);
        return 0;
      }
      if (flac->input->read(flac->input, 
        flac->streaminfo + sizeof(xine_waveformatex),
        FLAC_STREAMINFO_SIZE) != FLAC_STREAMINFO_SIZE)
        return 0;
      flac->sample_rate = BE_32(&streaminfo[10]);
      flac->channels = ((flac->sample_rate >> 9) & 0x07) + 1;
      flac->bits_per_sample = ((flac->sample_rate >> 4) & 0x1F) + 1;
      flac->sample_rate >>= 12;
      flac->total_samples = BE_64(&streaminfo[10]) & 0x0FFFFFFFFFLL;  /* 36 bits */
      lprintf ("%d Hz, %d bits, %d channels, %lld total samples\n", 
        flac->sample_rate, flac->bits_per_sample, 
        flac->channels, flac->total_samples);
      break;

    /* PADDING */
    case 1:
      lprintf ("PADDING metadata\n");
      flac->input->seek(flac->input, block_length, SEEK_CUR);
      break;

    /* APPLICATION */
    case 2:
      lprintf ("APPLICATION metadata\n");
      flac->input->seek(flac->input, block_length, SEEK_CUR);
      break;

    /* SEEKTABLE */
    case 3:
      lprintf ("SEEKTABLE metadata, %d bytes\n", block_length);
      flac->seekpoint_count = block_length / FLAC_SEEKPOINT_SIZE;
      flac->seekpoints = xine_xmalloc(flac->seekpoint_count * 
        sizeof(flac_seekpoint_t));
      for (i = 0; i < flac->seekpoint_count; i++) {
        if (flac->input->read(flac->input, buffer, FLAC_SEEKPOINT_SIZE) !=
            FLAC_SEEKPOINT_SIZE)
          return 0;
        flac->seekpoints[i].sample_number = BE_64(&buffer[0]);
        lprintf (" %d: sample %lld, ", i, flac->seekpoints[i].sample_number);
        flac->seekpoints[i].offset = BE_64(&buffer[8]);
        flac->seekpoints[i].size = BE_16(&buffer[16]);
        lprintf ("@ 0x%llX, size = %d bytes, ", 
          flac->seekpoints[i].offset, flac->seekpoints[i].size);
        flac->seekpoints[i].pts = flac->seekpoints[i].sample_number;
        flac->seekpoints[i].pts *= 90000;
        flac->seekpoints[i].pts /= flac->sample_rate;
        lprintf ("pts = %lld\n", flac->seekpoints[i].pts);
      }
      break;

    /* VORBIS_COMMENT */
    case 4:
      lprintf ("VORBIS_COMMENT metadata\n");
      flac->input->seek(flac->input, block_length, SEEK_CUR);
      break;

    /* CUESHEET */
    case 5:
      lprintf ("CUESHEET metadata\n");
      flac->input->seek(flac->input, block_length, SEEK_CUR);
      break;

    /* 6-127 are presently reserved */
    default:
      lprintf ("unknown metadata chunk: %d\n", preamble[0] & 0x7F);
      flac->input->seek(flac->input, block_length, SEEK_CUR);
      break;

    }

  } while ((preamble[0] & 0x80) == 0);

  flac->data_start = flac->input->get_current_pos(flac->input);
  flac->data_size = flac->input->get_length(flac->input) - flac->data_start;

  /* now at the beginning of the audio, adjust the seekpoint offsets */
  for (i = 0; i < flac->seekpoint_count; i++) {
    flac->seekpoints[i].offset += flac->data_start;
  }

  return 1;
}

static int demux_flac_send_chunk(demux_plugin_t *this_gen) {
  demux_flac_t *this = (demux_flac_t *) this_gen;
  buf_element_t *buf = NULL;
  int64_t input_time_guess;

  /* just send a buffer-sized chunk; let the decoder figure out the
   * boundaries and let the engine figure out the pts */
  buf = this->audio_fifo->buffer_pool_alloc (this->audio_fifo);
  buf->type = BUF_AUDIO_FLAC;
  if( this->data_size )
    buf->extra_info->input_normpos = (int) ( (double) (this->input->get_current_pos(this->input) -
                                     this->data_start) * 65535 / this->data_size );
  buf->pts = 0;
  buf->size = buf->max_size;

  /*
   * Estimate the input_time field based on file position:
   *
   *   current_pos     input time
   *   -----------  =  ----------
   *    total_pos      total time
   *
   *  total time = total samples / sample rate * 1000
   */

  /* do this one step at a time to make sure all the numbers stay safe */
  input_time_guess = this->total_samples;
  input_time_guess /= this->sample_rate;
  input_time_guess *= 1000;
  input_time_guess *= buf->extra_info->input_normpos;
  input_time_guess /= 65535;
  buf->extra_info->input_time = input_time_guess;

  if (this->input->read(this->input, buf->content, buf->size) !=
    buf->size) {
    buf->free_buffer(buf);
    this->status = DEMUX_FINISHED;
    return this->status;
  }
  buf->decoder_flags |= BUF_FLAG_FRAME_END;
  this->audio_fifo->put(this->audio_fifo, buf);

  return this->status;
}

static void demux_flac_send_headers(demux_plugin_t *this_gen) {
  demux_flac_t *this = (demux_flac_t *) this_gen;
  buf_element_t *buf;
  xine_waveformatex wave;

  this->audio_fifo  = this->stream->audio_fifo;

  /* send start buffers */
  _x_demux_control_start(this->stream);

  if (this->audio_fifo) {
    buf = this->audio_fifo->buffer_pool_alloc (this->audio_fifo);
    buf->type = BUF_AUDIO_FLAC;
    buf->decoder_flags = BUF_FLAG_HEADER|BUF_FLAG_STDHEADER|BUF_FLAG_FRAME_END;
    buf->decoder_info[0] = 0;
    buf->decoder_info[1] = this->sample_rate;
    buf->decoder_info[2] = this->bits_per_sample;
    buf->decoder_info[3] = this->channels;
    /* copy the faux WAV header */
    buf->size = sizeof(xine_waveformatex) + FLAC_STREAMINFO_SIZE;
    memcpy(buf->content, this->streaminfo, buf->size);
    /* forge a WAV header with the proper length */
    wave.cbSize = FLAC_STREAMINFO_SIZE;
    memcpy(buf->content, &wave, sizeof(xine_waveformatex));
    this->audio_fifo->put (this->audio_fifo, buf);
  }

  this->status = DEMUX_OK;
}

static int demux_flac_seek (demux_plugin_t *this_gen,
                            off_t start_pos, int start_time, int playing) {
  demux_flac_t *this = (demux_flac_t *) this_gen;
  start_pos = (off_t) ( (double) start_pos / 65535 *
              this->data_size );
  int seekpoint_index = 0;
  int64_t start_pts;

  /* if thread is not running, initialize demuxer */
  if( !playing ) {

    /* send new pts */
    _x_demux_control_newpts(this->stream, 0, 0);

    this->status = DEMUX_OK;
  } else {

    /* do a lazy, linear seek based on the assumption that there are not
     * that many seek points */
    if (start_pos) {
      /* offset-based seek */
      if (start_pos < this->seekpoints[0].offset)
        seekpoint_index = 0;
      else {
        for (seekpoint_index = 0; seekpoint_index < this->seekpoint_count - 1;
          seekpoint_index++) {
          if (start_pos < this->seekpoints[seekpoint_index + 1].offset) {
            break;
          }
        }
      }
    } else {
      /* time-based seek */
      start_pts = start_time;
      start_pts *= 90;
      if (start_pts < this->seekpoints[0].pts)
        seekpoint_index = 0;
      else {
        for (seekpoint_index = 0; seekpoint_index < this->seekpoint_count - 1;
          seekpoint_index++) {
          if (start_pts < this->seekpoints[seekpoint_index + 1].pts) {
            break;
          }
        }
      }
    }

    _x_demux_flush_engine(this->stream);
    this->input->seek(this->input, this->seekpoints[seekpoint_index].offset, 
      SEEK_SET);
    _x_demux_control_newpts(this->stream, 
      this->seekpoints[seekpoint_index].pts, BUF_FLAG_SEEK);
  }

  return this->status;
}

static void demux_flac_dispose (demux_plugin_t *this_gen) {
  demux_flac_t *this = (demux_flac_t *) this_gen;

  free(this->seekpoints);
}

static int demux_flac_get_status (demux_plugin_t *this_gen) {
  demux_flac_t *this = (demux_flac_t *) this_gen;

  return this->status;
}

static int demux_flac_get_stream_length (demux_plugin_t *this_gen) {
  demux_flac_t *this = (demux_flac_t *) this_gen;
  int64_t length = this->total_samples;

  length *= 1000;
  length /= this->sample_rate;

  return length;
}

static uint32_t demux_flac_get_capabilities(demux_plugin_t *this_gen) {
  return DEMUX_CAP_NOCAP;
}

static int demux_flac_get_optional_data(demux_plugin_t *this_gen,
                                        void *data, int data_type) {
  return DEMUX_OPTIONAL_UNSUPPORTED;
}

static demux_plugin_t *open_plugin (demux_class_t *class_gen, xine_stream_t *stream,
                                    input_plugin_t *input) {

  demux_flac_t    *this;

  /* this should change eventually... */
  if (!INPUT_IS_SEEKABLE(input)) {
    xprintf(stream->xine, XINE_VERBOSITY_DEBUG, "input not seekable, can not handle!\n");
    return NULL;
  }

  this         = xine_xmalloc (sizeof (demux_flac_t));
  this->stream = stream;
  this->input  = input;

  this->demux_plugin.send_headers      = demux_flac_send_headers;
  this->demux_plugin.send_chunk        = demux_flac_send_chunk;
  this->demux_plugin.seek              = demux_flac_seek;
  this->demux_plugin.dispose           = demux_flac_dispose;
  this->demux_plugin.get_status        = demux_flac_get_status;
  this->demux_plugin.get_stream_length = demux_flac_get_stream_length;
  this->demux_plugin.get_capabilities  = demux_flac_get_capabilities;
  this->demux_plugin.get_optional_data = demux_flac_get_optional_data;
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

    if (!open_flac_file(this)) {
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
  return "Free Lossless Audio Codec (flac) demux plugin";
}

static char *get_identifier (demux_class_t *this_gen) {
  return "FLAC";
}

static char *get_extensions (demux_class_t *this_gen) {
  return "flac";
}

static char *get_mimetypes (demux_class_t *this_gen) {
  return NULL;
}

static void class_dispose (demux_class_t *this_gen) {
  demux_flac_class_t *this = (demux_flac_class_t *) this_gen;

  free (this);
}

void *demux_flac_init_plugin (xine_t *xine, void *data) {
  demux_flac_class_t     *this;

  this = xine_xmalloc (sizeof (demux_flac_class_t));

  this->demux_class.open_plugin     = open_plugin;
  this->demux_class.get_description = get_description;
  this->demux_class.get_identifier  = get_identifier;
  this->demux_class.get_mimetypes   = get_mimetypes;
  this->demux_class.get_extensions  = get_extensions;
  this->demux_class.dispose         = class_dispose;

  return this;
}
