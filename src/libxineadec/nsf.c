/*
 * Copyright (C) 2000-2001 the xine project
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
 * NSF Audio "Decoder" using the Nosefart NSF engine by Matt Conte
 *   http://www.baisoku.org/
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>

#include "xine_internal.h"
#include "audio_out.h"
#include "buffer.h"
#include "xineutils.h"
#include "bswap.h"

/* Nosefart includes */
#include "nosefart/types.h"
#include "nosefart/nsf.h"

typedef struct {
  audio_decoder_class_t   decoder_class;
} nsf_class_t;

typedef struct nsf_decoder_s {
  audio_decoder_t  audio_decoder;

  xine_stream_t    *stream;

  int              sample_rate;       /* audio sample rate */
  int              bits_per_sample;   /* bits/sample, usually 8 or 16 */
  int              channels;          /* 1 or 2, usually */

  int              output_open;       /* flag to indicate audio is ready */

  int              nsf_size;
  unsigned char   *nsf_file;
  int              nsf_index;
  int              song_number;

  /* nsf-specific variables */
  int64_t           last_pts;
  unsigned int      iteration;

  nsf_t            *nsf;
} nsf_decoder_t;

/**************************************************************************
 * xine audio plugin functions
 *************************************************************************/

static void nsf_decode_data (audio_decoder_t *this_gen, buf_element_t *buf) {

  nsf_decoder_t *this = (nsf_decoder_t *) this_gen;
  audio_buffer_t *audio_buffer;

  if (buf->decoder_flags & BUF_FLAG_HEADER) {

    /* When the engine sends a BUF_FLAG_HEADER flag, it is time to initialize
     * the decoder. The buffer element type has 4 decoder_info fields,
     * 0..3. Field 1 is the sample rate. Field 2 is the bits/sample. Field
     * 3 is the number of channels. */
    this->sample_rate = buf->decoder_info[1];
    this->bits_per_sample = buf->decoder_info[2];
    this->channels = buf->decoder_info[3];

    /* take this opportunity to initialize stream/meta information */
    _x_meta_info_set_utf8(this->stream, XINE_META_INFO_AUDIOCODEC,
      "NES Music (Nosefart)");

    this->song_number = buf->content[4];
    /* allocate a buffer for the file */
    this->nsf_size = _X_BE_32(&buf->content[0]);
    this->nsf_file = calloc(1, this->nsf_size);
    this->nsf_index = 0;

    /* peform any other required initialization */
    this->last_pts = -1;
    this->iteration = 0;

    return;
  }

  /* accumulate chunks from the NSF file until whole file is received */
  if (this->nsf_index < this->nsf_size) {
    xine_fast_memcpy(&this->nsf_file[this->nsf_index], buf->content,
      buf->size);
    this->nsf_index += buf->size;

    if (this->nsf_index == this->nsf_size) {
      /* file has been received, proceed to initialize engine */
      nsf_init();
      this->nsf = nsf_load(NULL, this->nsf_file, this->nsf_size);
      if (!this->nsf) {
        xprintf (this->stream->xine, XINE_VERBOSITY_DEBUG, "nsf: could not initialize NSF\n");
        /* make the decoder return on every subsequent buffer */
        this->nsf_index = 0;
	return;
      }
      this->nsf->current_song = this->song_number;
      nsf_playtrack(this->nsf, this->nsf->current_song, this->sample_rate,
        this->bits_per_sample, this->channels);
    }
    return;
  }

  /* if the audio output is not open yet, open the audio output */
  if (!this->output_open) {
    this->output_open = (this->stream->audio_out->open) (
      this->stream->audio_out,
      this->stream,
      this->bits_per_sample,
      this->sample_rate,
      _x_ao_channels2mode(this->channels));
  }

  /* if the audio still isn't open, do not go any further with the decode */
  if (!this->output_open)
    return;

  /* check if a song change was requested */
  if (buf->decoder_info[1]) {
    this->nsf->current_song = buf->decoder_info[1];
    nsf_playtrack(this->nsf, this->nsf->current_song, this->sample_rate,
      this->bits_per_sample, this->channels);
  }

  /* time to decode a frame */
  if (this->last_pts != -1) {

    /* process a frame */
    nsf_frame(this->nsf);

    /* get an audio buffer */
    audio_buffer = this->stream->audio_out->get_buffer (this->stream->audio_out);
    if (audio_buffer->mem_size == 0) {
       xprintf (this->stream->xine, XINE_VERBOSITY_DEBUG, "nsf: Help! Allocated audio buffer with nothing in it!\n");
       return;
    }

    apu_process(audio_buffer->mem, this->sample_rate / this->nsf->playback_rate);
    audio_buffer->vpts = buf->pts;
    audio_buffer->num_frames = this->sample_rate / this->nsf->playback_rate;
    this->stream->audio_out->put_buffer (this->stream->audio_out, audio_buffer, this->stream);
  }
  this->last_pts = buf->pts;
}

/* This function resets the state of the audio decoder. This usually
 * entails resetting the data accumulation buffer. */
static void nsf_reset (audio_decoder_t *this_gen) {

  nsf_decoder_t *this = (nsf_decoder_t *) this_gen;

  this->last_pts = -1;
}

/* This function resets the last pts value of the audio decoder. */
static void nsf_discontinuity (audio_decoder_t *this_gen) {

  nsf_decoder_t *this = (nsf_decoder_t *) this_gen;

  this->last_pts = -1;
}

/* This function closes the audio output and frees the private audio decoder
 * structure. */
static void nsf_dispose (audio_decoder_t *this_gen) {

  nsf_decoder_t *this = (nsf_decoder_t *) this_gen;

  /* close the audio output */
  if (this->output_open)
    this->stream->audio_out->close (this->stream->audio_out, this->stream);
  this->output_open = 0;

  /* free anything that was allocated during operation */
  nsf_free(&this->nsf);
  free(this->nsf_file);
  free(this);
}

/* This function allocates, initializes, and returns a private audio
 * decoder structure. */
static audio_decoder_t *open_plugin (audio_decoder_class_t *class_gen, xine_stream_t *stream) {

  nsf_decoder_t *this ;

  this = (nsf_decoder_t *) calloc(1, sizeof(nsf_decoder_t));

  /* connect the member functions */
  this->audio_decoder.decode_data         = nsf_decode_data;
  this->audio_decoder.reset               = nsf_reset;
  this->audio_decoder.discontinuity       = nsf_discontinuity;
  this->audio_decoder.dispose             = nsf_dispose;

  /* connect the stream */
  this->stream = stream;

  /* audio output is not open at the start */
  this->output_open = 0;

  /* initialize the basic audio parameters */
  this->channels = 0;
  this->sample_rate = 0;
  this->bits_per_sample = 0;

  /* return the newly-initialized audio decoder */
  return &this->audio_decoder;
}

/* This function returns a brief string that describes (usually with the
 * decoder's most basic name) the audio decoder plugin. */
static char *get_identifier (audio_decoder_class_t *this) {
  return "NSF";
}

/* This function returns a slightly longer string describing the audio
 * decoder plugin. */
static char *get_description (audio_decoder_class_t *this) {
  return "NES Music audio decoder plugin";
}

/* This function frees the audio decoder class and any other memory that was
 * allocated. */
static void dispose_class (audio_decoder_class_t *this_gen) {

  nsf_class_t *this = (nsf_class_t *)this_gen;

  free (this);
}

/* This function allocates a private audio decoder class and initializes 
 * the class's member functions. */
static void *init_plugin (xine_t *xine, void *data) {

  nsf_class_t *this ;

  this = (nsf_class_t *) calloc(1, sizeof(nsf_class_t));

  this->decoder_class.open_plugin     = open_plugin;
  this->decoder_class.get_identifier  = get_identifier;
  this->decoder_class.get_description = get_description;
  this->decoder_class.dispose         = dispose_class;

  return this;
}

/* This is a list of all of the internal xine audio buffer types that 
 * this decoder is able to handle. Check src/xine-engine/buffer.h for a
 * list of valid buffer types (and add a new one if the one you need does
 * not exist). Terminate the list with a 0. */
static uint32_t audio_types[] = { 
  BUF_AUDIO_NSF,
  0
};

/* This data structure combines the list of supported xine buffer types and
 * the priority that the plugin should be given with respect to other
 * plugins that handle the same buffer type. A plugin with priority (n+1) 
 * will be used instead of a plugin with priority (n). */
static const decoder_info_t dec_info_audio = {
  audio_types,         /* supported types */
  5                    /* priority        */
};

/* The plugin catalog entry. This is the only information that this plugin
 * will export to the public. */
const plugin_info_t xine_plugin_info[] EXPORTED = {
  /* { type, API version, "name", version, special_info, init_function }, */
  { PLUGIN_AUDIO_DECODER, 15, "nsf", XINE_VERSION_CODE, &dec_info_audio, &init_plugin },
  { PLUGIN_NONE, 0, "", 0, NULL, NULL }
};

