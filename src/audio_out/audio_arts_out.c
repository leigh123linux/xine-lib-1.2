/*
 * Copyright (C) 2000-2003 the xine project
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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include <fcntl.h>
#include <math.h>
#include <unistd.h>
#include <inttypes.h>
#include <artsc.h>

#include "xine_internal.h"
#include "xineutils.h"
#include "audio_out.h"
#include "bswap.h"

#define AO_OUT_ARTS_IFACE_VERSION 8

#define AUDIO_NUM_FRAGMENTS     15
#define AUDIO_FRAGMENT_SIZE   8192

#define GAP_TOLERANCE        AO_MAX_GAP

typedef struct arts_driver_s {

  ao_driver_t    ao_driver;

  xine_t        *xine;

  arts_stream_t  audio_stream;
  int            capabilities;
  int            mode;

  int32_t        sample_rate;
  uint32_t       num_channels;
  uint32_t       bits_per_sample;
  uint32_t       bytes_per_frame;

  uint32_t       latency;

  struct {
	int     volume;
	int     mute;
	int     vol_scale;
	int     v_mixer;
  } mixer;

} arts_driver_t;

typedef struct {
  audio_driver_class_t  driver_class;

  xine_t               *xine;
  int                   inited;
} arts_class_t;

/*
 * Software stereo volume control.....
 * Igor Mokrushin <igor@avtomir.ru>
 */
static void ao_arts_volume(void *buffer, int length, int volume) {
  int v;
  short *data = (short *)buffer;

  while (length--) {
    v=(int) ((*(data) * volume) / 100);
    *(data)=(v>32767) ? 32767 : ((v<-32768) ? -32768 : v);
    *(data)=_X_LE_16(data);
    data++;
  }
}
/* End volume control */

/*
 * open the audio device for writing to
 */
static int ao_arts_open(ao_driver_t *this_gen,
		   uint32_t bits, uint32_t rate, int mode)
{
  arts_driver_t *this = (arts_driver_t *) this_gen;

  xprintf (this->xine, XINE_VERBOSITY_DEBUG,
	   "audio_arts_out: ao_open bits=%d rate=%d, mode=%d\n", bits, rate, mode);

  if ( (mode & this->capabilities) == 0 ) {
    xprintf (this->xine, XINE_VERBOSITY_DEBUG, "audio_arts_out: unsupported mode %08x\n", mode);
    return 0;
  }

  if (this->audio_stream) {

    if ( (mode == this->mode) && (rate == this->sample_rate) )
      return this->sample_rate;

    sleep(2); /* arts might segfault if we are still playing */
    arts_close_stream(this->audio_stream);
  }

  this->mode                   = mode;
  this->sample_rate            = rate;
  this->bits_per_sample        = bits;

  switch (mode) {
  case AO_CAP_MODE_MONO:
    this->num_channels = 1;
    break;
  case AO_CAP_MODE_STEREO:
    this->num_channels = 2;
    break;
  }

  this->bytes_per_frame=(this->bits_per_sample*this->num_channels)/8;

  xprintf (this->xine, XINE_VERBOSITY_DEBUG, "audio_arts_out: %d channels output\n", this->num_channels);

  this->audio_stream=arts_play_stream(this->sample_rate, bits, this->num_channels, "xine");

  this->latency = arts_stream_get (this->audio_stream, ARTS_P_TOTAL_LATENCY);

  /* try to keep latency low, if we don't do this we might end
     with very high latencies for low quality sound and audio_out will
     try to fill gaps every time...(values in ms) */
  if( this->latency > 800 )
  {
     this->latency = 800 - arts_stream_get (this->audio_stream, ARTS_P_SERVER_LATENCY);
     if( this->latency < 100 )
       this->latency = 100;
     arts_stream_set( this->audio_stream, ARTS_P_BUFFER_TIME, this->latency );
     this->latency = arts_stream_get (this->audio_stream, ARTS_P_TOTAL_LATENCY);
  }

  xprintf (this->xine, XINE_VERBOSITY_DEBUG, "audio_arts_out : latency %d ms\n", this->latency);

  return this->sample_rate;
}


static int ao_arts_num_channels(ao_driver_t *this_gen)
{
  arts_driver_t *this = (arts_driver_t *) this_gen;
    return this->num_channels;
}

static int ao_arts_bytes_per_frame(ao_driver_t *this_gen)
{
  arts_driver_t *this = (arts_driver_t *) this_gen;
  return this->bytes_per_frame;
}

static int ao_arts_get_gap_tolerance (ao_driver_t *this_gen)
{
  return GAP_TOLERANCE;
}

static int ao_arts_write(ao_driver_t *this_gen, int16_t *data,
                         uint32_t num_frames)
{
  arts_driver_t *this = (arts_driver_t *) this_gen;
  int size = num_frames * this->bytes_per_frame;

  ao_arts_volume(data, num_frames * this->num_channels, this->mixer.vol_scale );
  arts_write(this->audio_stream, data, size );

  return 1;
}


static int ao_arts_delay (ao_driver_t *this_gen)
{
  arts_driver_t *this = (arts_driver_t *) this_gen;

  /* Just convert latency (ms) to frame units.
     please note that there is no function in aRts C API to
     get the current buffer utilization. This is, at best,
     a very roughly aproximation.
  */

  return this->latency * this->sample_rate / 1000;
}

static void ao_arts_close(ao_driver_t *this_gen)
{
  arts_driver_t *this = (arts_driver_t *) this_gen;

  if (this->audio_stream) {
    sleep(2); /* arts might segfault if we are still playing */
    arts_close_stream(this->audio_stream);
    this->audio_stream = NULL;
  }
}

static uint32_t ao_arts_get_capabilities (ao_driver_t *this_gen) {
  arts_driver_t *this = (arts_driver_t *) this_gen;
  return this->capabilities;
}

static void ao_arts_exit(ao_driver_t *this_gen)
{
  arts_driver_t *this = (arts_driver_t *) this_gen;

  ao_arts_close(this_gen);
  /* FIXME: arts_free() freezes on BSD, so don't use it there */
#if !defined(__OpenBSD__) && !defined (__FreeBSD__) && !defined(__NetBSD__)
  arts_free();
#endif

  free (this);
}

static int ao_arts_get_property (ao_driver_t *this_gen, int property) {

  arts_driver_t *this = (arts_driver_t *) this_gen;

  switch(property) {
  case AO_PROP_PCM_VOL:
  case AO_PROP_MIXER_VOL:
    if(!this->mixer.mute)
        this->mixer.volume = this->mixer.vol_scale;
        return this->mixer.volume;
    break;
  case AO_PROP_MUTE_VOL:
	return this->mixer.mute;
    break;
  }
  return 0;
}

static int ao_arts_set_property (ao_driver_t *this_gen, int property, int value) {

  arts_driver_t *this = (arts_driver_t *) this_gen;
  int mute = (value) ? 1 : 0;

  switch(property) {
  case AO_PROP_PCM_VOL:
  case AO_PROP_MIXER_VOL:
    if(!this->mixer.mute)
	this->mixer.volume = value;
	this->mixer.vol_scale = this->mixer.volume;
	return this->mixer.volume;
    break;
  case AO_PROP_MUTE_VOL:
    if(mute) {
        this->mixer.v_mixer = this->mixer.volume;
        this->mixer.volume = 0;
        this->mixer.vol_scale = this->mixer.volume;
    } else {
        this->mixer.volume = this->mixer.v_mixer;
        this->mixer.vol_scale = this->mixer.volume;
    }
	this->mixer.mute = mute;
	return value;
	break;
  }

  return ~value;
}

static int ao_arts_ctrl(ao_driver_t *this_gen, int cmd, ...) {
  /*arts_driver_t *this = (arts_driver_t *) this_gen;*/

  switch (cmd) {

  case AO_CTRL_PLAY_PAUSE:
    break;

  case AO_CTRL_PLAY_RESUME:
    break;

  case AO_CTRL_FLUSH_BUFFERS:
    break;
  }

  return 0;
}

static ao_driver_t *open_plugin (audio_driver_class_t *class_gen, const void *data) {
  arts_class_t   *class = (arts_class_t *) class_gen;
  arts_driver_t  *this;
  int            rc;

  lprintf ("audio_arts_out: open_plugin called\n");

  this = calloc(1, sizeof (arts_driver_t));
  if (!this)
    return NULL;

  this->xine = class->xine;

  if (class->inited == 0) {
    rc = arts_init();
    class->inited++;
  } else {
    xprintf (this->xine, XINE_VERBOSITY_LOG, "audio_arts_out: not trying to initialise a second time\n");
    free(this);
    return NULL;
  }

  if (rc < 0) {
    xprintf (this->xine, XINE_VERBOSITY_DEBUG,"audio_arts_out: arts_init failed: %s\n", arts_error_text(rc));
    free(this);
    return NULL;
  }

  /*
   * set volume control
   */
  this->mixer.mute      = 0;
  this->mixer.vol_scale = 60;
  this->mixer.v_mixer   = 0;
  /*
   * set capabilities
   */
  this->capabilities = 0;
  xprintf (this->xine, XINE_VERBOSITY_DEBUG, "audio_arts_out : supported modes are ");
  this->capabilities |= AO_CAP_MODE_MONO | AO_CAP_MIXER_VOL | AO_CAP_PCM_VOL | AO_CAP_MUTE_VOL;
  xprintf (this->xine, XINE_VERBOSITY_DEBUG, "mono ");
  this->capabilities |= AO_CAP_MODE_STEREO | AO_CAP_MIXER_VOL | AO_CAP_PCM_VOL | AO_CAP_MUTE_VOL;
  xprintf (this->xine, XINE_VERBOSITY_DEBUG, "stereo ");

  this->sample_rate  = 0;
  this->audio_stream = NULL;

  this->ao_driver.get_capabilities    = ao_arts_get_capabilities;
  this->ao_driver.get_property        = ao_arts_get_property;
  this->ao_driver.set_property        = ao_arts_set_property;
  this->ao_driver.open                = ao_arts_open;
  this->ao_driver.num_channels        = ao_arts_num_channels;
  this->ao_driver.bytes_per_frame     = ao_arts_bytes_per_frame;
  this->ao_driver.delay               = ao_arts_delay;
  this->ao_driver.write               = ao_arts_write;
  this->ao_driver.close               = ao_arts_close;
  this->ao_driver.exit                = ao_arts_exit;
  this->ao_driver.get_gap_tolerance   = ao_arts_get_gap_tolerance;
  this->ao_driver.control	      = ao_arts_ctrl;

  return &this->ao_driver;
}

/*
 * class functions
 */

static char* get_identifier (audio_driver_class_t *this_gen) {
  return "arts";
}

static char* get_description (audio_driver_class_t *this_gen) {
  return _("xine audio output plugin using kde artsd");
}

static void dispose_class (audio_driver_class_t *this_gen) {

  arts_class_t *this = (arts_class_t *) this_gen;

  free (this);
}

static void *init_class (xine_t *xine, void *data) {

  arts_class_t        *this;

  lprintf ("audio_arts_out: init class\n");

  this = calloc(1, sizeof (arts_class_t));
  if (!this)
    return NULL;

  this->inited = 0;

  this->driver_class.open_plugin     = open_plugin;
  this->driver_class.get_identifier  = get_identifier;
  this->driver_class.get_description = get_description;
  this->driver_class.dispose         = dispose_class;

  this->xine                         = xine;

  return this;
}

static ao_info_t ao_info_arts = {
  5
};

/*
 * exported plugin catalog entry
 */

const plugin_info_t xine_plugin_info[] EXPORTED = {
  /* type, API, "name", version, special_info, init_function */
  { PLUGIN_AUDIO_OUT, AO_OUT_ARTS_IFACE_VERSION, "arts", XINE_VERSION_CODE, &ao_info_arts, init_class },
  { PLUGIN_NONE, 0, "", 0, NULL, NULL }
};

