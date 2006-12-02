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
 * MOD File "demuxer" by Paul Eggleton (bluelightning@bluelightning.org)
 * This is really just a loader for Amiga MOD (and similar) music files
 * which reads an entire MOD file and passes it over to the ModPlug library
 * for playback.
 *
 * This file was based on demux_nsf.c by Mike Melanson.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#ifdef HAVE_MODPLUG

#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>

/********** logging **********/
#define LOG_MODULE "demux_mod"
/* #define LOG_VERBOSE */
/* #define LOG */

#include "xine_internal.h"
#include "xineutils.h"
#include "compat.h"
#include "demux.h"
#include "group_audio.h"
#include "modplug.h"

#define MOD_SAMPLERATE 44100
#define MOD_BITS 16
#define MOD_CHANNELS 2

#define OUT_BYTES_PER_SECOND (MOD_SAMPLERATE * MOD_CHANNELS * (MOD_BITS >> 3))

#define BLOCK_SIZE 4096


typedef struct {
  demux_plugin_t       demux_plugin;

  xine_stream_t       *stream;
  fifo_buffer_t       *video_fifo;
  fifo_buffer_t       *audio_fifo;
  input_plugin_t      *input;
  int                  status;

  char                *title;
  char                *artist;
  char                *copyright;
  off_t                filesize;
  
  char                *buffer;

  int64_t              current_pts;
  
  ModPlug_Settings     settings;
  ModPlugFile         *mpfile;
  int                  mod_length;
  int                  seek_flag;  /* this is set when a seek just occurred */
  
} demux_mod_t;

typedef struct {
  demux_class_t     demux_class;
} demux_mod_class_t;

/* returns 1 if the MOD file was opened successfully, 0 otherwise */
static int open_mod_file(demux_mod_t *this) {
  int total_read;
  
  /* Get size and create buffer */
  this->filesize = this->input->get_length(this->input);
  this->buffer = (char *)malloc(this->filesize);
  
  /* Seek to beginning */
  this->input->seek(this->input, 0, SEEK_SET);
  
  /* Read data */
  total_read = this->input->read(this->input, this->buffer, this->filesize);
  
  if(total_read != this->filesize) {
    xine_log(this->stream->xine, XINE_LOG_PLUGIN, "modplug - filesize error\n");
    free(this->buffer);
    return 0;
  }
      
  this->mpfile = ModPlug_Load(this->buffer, this->filesize);
  if (this->mpfile==NULL) {
    xine_log(this->stream->xine, XINE_LOG_PLUGIN, "modplug - load error\n");
    free(this->buffer);
    return 0;
  }
  
  /* Set up modplug engine */
  ModPlug_GetSettings(&this->settings);
  this->settings.mResamplingMode = MODPLUG_RESAMPLE_FIR; /* RESAMP */
  this->settings.mChannels = MOD_CHANNELS;
  this->settings.mBits = MOD_BITS;
  this->settings.mFrequency = MOD_SAMPLERATE;
  ModPlug_SetSettings(&this->settings);
  
  this->title = strdup(ModPlug_GetName(this->mpfile));
  this->artist = strdup("");
  this->copyright = strdup("");
  
  this->mod_length = ModPlug_GetLength(this->mpfile);
    
  return 1;
}

static int demux_mod_send_chunk(demux_plugin_t *this_gen) {
  demux_mod_t *this = (demux_mod_t *) this_gen;
  buf_element_t *buf;
  int mlen;

  buf = this->audio_fifo->buffer_pool_alloc (this->audio_fifo);
  buf->type = BUF_AUDIO_LPCM_LE;

  mlen = ModPlug_Read(this->mpfile, buf->content, buf->max_size);
  if (mlen == 0) {
    this->status = DEMUX_FINISHED;
    buf->free_buffer(buf);
  }
  else {
    buf->size = mlen;
    buf->pts = this->current_pts;
    buf->extra_info->input_time = buf->pts / 90;
    
    buf->extra_info->input_normpos = buf->extra_info->input_time * 65535 / this->mod_length;
    buf->decoder_flags = BUF_FLAG_FRAME_END;
    
    if (this->seek_flag) {
      _x_demux_control_newpts(this->stream, buf->pts, BUF_FLAG_SEEK);
      this->seek_flag = 0;
    }

    this->audio_fifo->put (this->audio_fifo, buf);
    
    this->current_pts += 90000 * mlen / OUT_BYTES_PER_SECOND;
  }
  
  return this->status;
}

static void demux_mod_send_headers(demux_plugin_t *this_gen) {
  demux_mod_t *this = (demux_mod_t *) this_gen;
  buf_element_t *buf;
  char copyright[100];

  this->video_fifo = this->stream->video_fifo;
  this->audio_fifo = this->stream->audio_fifo;

  this->status = DEMUX_OK;

  /* load stream information */
  _x_stream_info_set(this->stream, XINE_STREAM_INFO_HAS_VIDEO, 0);
  _x_stream_info_set(this->stream, XINE_STREAM_INFO_HAS_AUDIO, 1);
  _x_stream_info_set(this->stream, XINE_STREAM_INFO_AUDIO_CHANNELS, MOD_CHANNELS);
  _x_stream_info_set(this->stream, XINE_STREAM_INFO_AUDIO_SAMPLERATE, MOD_SAMPLERATE);
  _x_stream_info_set(this->stream, XINE_STREAM_INFO_AUDIO_BITS, MOD_BITS);

  _x_meta_info_set(this->stream, XINE_META_INFO_TITLE, this->title);
  _x_meta_info_set(this->stream, XINE_META_INFO_ARTIST, this->artist);
  snprintf(copyright, 100, "(C) %s", this->copyright);
  _x_meta_info_set(this->stream, XINE_META_INFO_COMMENT, copyright);

  /* send start buffers */
  _x_demux_control_start(this->stream);

  /* send init info to the audio decoder */
  buf = this->audio_fifo->buffer_pool_alloc (this->audio_fifo);
  buf->type = BUF_AUDIO_LPCM_LE;
  buf->decoder_flags = BUF_FLAG_HEADER|BUF_FLAG_STDHEADER|BUF_FLAG_FRAME_END;
  buf->decoder_info[0] = 0;
  buf->decoder_info[1] = MOD_SAMPLERATE;
  buf->decoder_info[2] = MOD_BITS;
  buf->decoder_info[3] = MOD_CHANNELS;
  buf->size = 0;
  this->audio_fifo->put (this->audio_fifo, buf);
}

static int demux_mod_seek (demux_plugin_t *this_gen,
                           off_t start_pos, int start_time, int playing) {

  demux_mod_t *this = (demux_mod_t *) this_gen;
  int64_t seek_millis;
  
  if (start_pos) {
    seek_millis = this->mod_length; 
    seek_millis *= start_pos;
    seek_millis /= 65535;
  } else {
    seek_millis = start_time;
  }

  _x_demux_flush_engine(this->stream);
  ModPlug_Seek(this->mpfile, seek_millis);
  this->current_pts = seek_millis * 90;
  
  this->seek_flag = 1;
  return this->status;
}

static void demux_mod_dispose (demux_plugin_t *this_gen) {
  demux_mod_t *this = (demux_mod_t *) this_gen;
  
  ModPlug_Unload(this->mpfile);
  free(this->buffer);
  free(this->title);
  free(this->artist);
  free(this->copyright);
  free(this);
}

static int demux_mod_get_status (demux_plugin_t *this_gen) {
  demux_mod_t *this = (demux_mod_t *) this_gen;
  return this->status;
}

/* return the approximate length in miliseconds */
static int demux_mod_get_stream_length (demux_plugin_t *this_gen) {
  demux_mod_t *this = (demux_mod_t *) this_gen;
  return ModPlug_GetLength(this->mpfile);
}

static uint32_t demux_mod_get_capabilities(demux_plugin_t *this_gen) {
  return DEMUX_CAP_NOCAP;
}

static int demux_mod_get_optional_data(demux_plugin_t *this_gen,
                                       void *data, int data_type) {
  return DEMUX_OPTIONAL_UNSUPPORTED;
}

static demux_plugin_t *open_plugin (demux_class_t *class_gen, xine_stream_t *stream,
                                    input_plugin_t *input) {

  demux_mod_t   *this;

  if (!INPUT_IS_SEEKABLE(input)) {
    xprintf(stream->xine, XINE_VERBOSITY_DEBUG, "input not seekable, can not handle!\n");
    return NULL;
  }

  this         = xine_xmalloc (sizeof (demux_mod_t));
  this->stream = stream;
  this->input  = input;

  this->demux_plugin.send_headers      = demux_mod_send_headers;
  this->demux_plugin.send_chunk        = demux_mod_send_chunk;
  this->demux_plugin.seek              = demux_mod_seek;
  this->demux_plugin.dispose           = demux_mod_dispose;
  this->demux_plugin.get_status        = demux_mod_get_status;
  this->demux_plugin.get_stream_length = demux_mod_get_stream_length;
  this->demux_plugin.get_capabilities  = demux_mod_get_capabilities;
  this->demux_plugin.get_optional_data = demux_mod_get_optional_data;
  this->demux_plugin.demux_class       = class_gen;

  this->status = DEMUX_FINISHED;

  xprintf(stream->xine, XINE_VERBOSITY_DEBUG, "TEST mod decode\n");
  
  switch (stream->content_detection_method) {

  case METHOD_EXPLICIT:
  case METHOD_BY_EXTENSION: {
    char *extensions, *mrl;

    mrl = input->get_mrl (input);
    extensions = class_gen->get_extensions (class_gen);

    if (!_x_demux_check_extension (mrl, extensions)) {
      free (this);
      return NULL;
    }
    if (!open_mod_file(this)) {
      free (this);
      return NULL;
    }
  }
  break;

  case METHOD_BY_CONTENT:
  default:
    free (this);
    return NULL;
  }

  return &this->demux_plugin;
}

static char *get_description (demux_class_t *this_gen) {
  return "ModPlug Amiga MOD Music file demux plugin";
}

static char *get_identifier (demux_class_t *this_gen) {
  return "mod";
}

static char *get_extensions (demux_class_t *this_gen) {
  return "mod it stm s3m 669 amf med mdl xm";
}

static char *get_mimetypes (demux_class_t *this_gen) {
  return NULL;
}

static void class_dispose (demux_class_t *this_gen) {
  demux_mod_class_t *this = (demux_mod_class_t *) this_gen;

  free (this);
}

void *demux_mod_init_plugin (xine_t *xine, void *data) {
  demux_mod_class_t     *this;

  this = xine_xmalloc (sizeof (demux_mod_class_t));

  this->demux_class.open_plugin     = open_plugin;
  this->demux_class.get_description = get_description;
  this->demux_class.get_identifier  = get_identifier;
  this->demux_class.get_mimetypes   = get_mimetypes;
  this->demux_class.get_extensions  = get_extensions;
  this->demux_class.dispose         = class_dispose;

  return this;
}

#endif  /* HAVE_MODPLUG */
