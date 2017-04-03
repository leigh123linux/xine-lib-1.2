/*
 * Copyright (C) 2000-2017 the xine project
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

#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include "latm.c"

#define LOG_MODULE "libfaad"
#define LOG_VERBOSE
/*
#define LOG
*/

#include <xine/xine_internal.h>
#include <xine/audio_out.h>
#include <xine/buffer.h>
#include <xine/xineutils.h>
#ifdef HAVE_NEAACDEC_H
#include <neaacdec.h>
#else
#include "common.h"
#include "structs.h"
#include "syntax.h"
#endif

#define FAAD_MIN_STREAMSIZE 768 /* 6144 bits/channel */

typedef struct faad_class_s {
  audio_decoder_class_t   decoder_class;

  xine_t                 *xine;

  /* provide a single configuration for both "faad" and "faad-latm" */
  struct faad_class_s    *master;
  int                     refs;

  int                     gain_db;
  /* fixed point scalers (v * gain?_i) >> 32 */
  int32_t                 gain_i, gain3_i, gain6_i, gain9_i, gain12_i;
  /* floating point scalers */
  float                   gain_f, gain3_f, gain6_f, gain9_f, gain12_f;
  /* look here for fixed or float mode */
  int                     caps;
} faad_class_t;

typedef struct faad_decoder_s {
  audio_decoder_t  audio_decoder;

  faad_class_t     *class, *master;

  xine_stream_t    *stream;

  /* faad2 stuff */
  NeAACDecHandle           faac_dec;
  NeAACDecConfigurationPtr faac_cfg;
  NeAACDecFrameInfo        faac_finfo;
  int                     faac_failed;

  unsigned char   *buf;
  int              size;
  int              rec_audio_src_size;
  int              max_audio_src_size;
  int64_t          pts0, pts1;

  unsigned char   *dec_config;
  int              dec_config_size;

  unsigned long    rate;
  int              bits_per_sample;
  unsigned char    num_channels;
  int              sbr;

  /* 1 (OK), 0 (closed), < 0 (# failed attempts) */
  int              output_open;

  unsigned long    total_time;
  unsigned long    total_data;

  int              in_channels, out_channels, out_used;
  int              in_mode, out_mode, out_flags;

  bebf_latm_t      latm;
  bebf_latm_parser_status_t latm_mode;

  uint32_t         adts_fake;
  uint8_t          adts_lasthead[2];

} faad_decoder_t;


static int faad_map_channels (faad_decoder_t *this) {
  static const char *input_names[4] = { "mono", "stereo", "5.1", "7.1" };
  static const uint8_t input_modes[16] = {
    255, 0, 1, 255, 255, 255, 2, 255, 3, 255, 255, 255, 255, 255, 255, 255
  };
  /* the audio out modes we use. */
  static const char *out_names[6] = { "mono", "stereo", "4.0", "4.1", "5.0", "5.1" };
  static const int  out_modes[6] = {
    AO_CAP_MODE_MONO, AO_CAP_MODE_STEREO,
    AO_CAP_MODE_4CHANNEL, AO_CAP_MODE_4_1CHANNEL,
    AO_CAP_MODE_5CHANNEL, AO_CAP_MODE_5_1CHANNEL
  };
  /* how many channels are sent to audio out. */
  static const uint8_t out_chan[6] = { 1, 2, 4, 6, 6, 6 };
  /* how many of them contain audible data. */
  static const uint8_t out_used[6] = { 1, 2, 4, 5, 5, 6 };
  /* what out modes we prefer in what order for a given input. */
  static const uint8_t wishlist[4 * 6] = {
    0, 1, 2, 3, 4, 5,
    1, 2, 3, 4, 5, 0,
    5, 4, 3, 2, 1, 0,
    5, 4, 3, 2, 1, 0
  };
  /**/
  int out_caps = 0;
  int in_mode = input_modes[this->num_channels & 15];
  const uint8_t *p;
  int i;
  if (!this->stream->audio_out)
    return 0;
  if (in_mode == 255)
    return 0;
  this->in_mode = in_mode;
  out_caps = this->stream->audio_out->get_capabilities (this->stream->audio_out);
  p = wishlist + in_mode * 6;
  for (i = 0; i < 6; i++) {
    int out_flags = out_modes[p[i]];
    if (out_caps & out_flags) {
      this->out_flags = out_flags;
      this->out_mode = p[i];
      break;
    }
  }
  if (i == 6)
    return 0;
  this->in_channels  = this->num_channels;
  this->out_channels = out_chan[this->out_mode];
  this->out_used     = out_used[this->out_mode];
  xprintf (this->class->xine, XINE_VERBOSITY_DEBUG,
    "faad_audio_decoder: channel layout: %s -> %s\n",
    input_names[this->in_mode], out_names[this->out_mode]);
  return 1;
};


static void faad_reset (audio_decoder_t *this_gen) {

  faad_decoder_t *this = (faad_decoder_t *) this_gen;
  this->size = 0;
#if 0
  bebf_latm_close (&this->latm);
  bebf_latm_open (&this->latm);
  this->latm_mode = BEBF_LATM_NEED_MORE_DATA;
  memset (&this->adts_lasthead, 0, 2);
  this->adts_fake = 0;
#endif
}

static void faad_meta_info_set ( faad_decoder_t *this ) {
  switch (this->num_channels) {
    case 1:
      if (this->faac_finfo.sbr == SBR_UPSAMPLED)
        _x_meta_info_set_utf8(this->stream, XINE_META_INFO_AUDIOCODEC,
                              "HE-AAC 1.0 (libfaad)");
      else
        _x_meta_info_set_utf8(this->stream, XINE_META_INFO_AUDIOCODEC,
                              "AAC 1.0 (libfaad)");
      break;
    case 2:
      /* check if this is downmixed 5.1 */
      if (!this->faac_cfg || !this->faac_cfg->downMatrix) {
        if (this->faac_finfo.sbr == SBR_UPSAMPLED)
          _x_meta_info_set_utf8(this->stream, XINE_META_INFO_AUDIOCODEC,
                                "HE-AAC 2.0 (libfaad)");
        else
          _x_meta_info_set_utf8(this->stream, XINE_META_INFO_AUDIOCODEC,
                                "AAC 2.0 (libfaad)");
        break;
      }
    case 6:
      if (this->faac_finfo.sbr == SBR_UPSAMPLED)
        _x_meta_info_set_utf8(this->stream, XINE_META_INFO_AUDIOCODEC,
                              "HE-AAC 5.1 (libfaad)");
      else
        _x_meta_info_set_utf8(this->stream, XINE_META_INFO_AUDIOCODEC,
                              "AAC 5.1 (libfaad)");
      break;
  }
}

static void faad_close_output (faad_decoder_t *this) {
  if (this->output_open > 0)
    this->stream->audio_out->close (this->stream->audio_out, this->stream);
  this->output_open = 0;
}

static int faad_open_output( faad_decoder_t *this ) {
#if 0
  int ao_cap_mode;
#endif

  this->rec_audio_src_size = this->num_channels * FAAD_MIN_STREAMSIZE;

  /* maybe not needed again here */
  this->faac_cfg = NeAACDecGetCurrentConfiguration (this->faac_dec);
  if (this->faac_cfg) {
    this->faac_cfg->outputFormat = (this->class->caps & FIXED_POINT_CAP) ? FAAD_FMT_FIXED : FAAD_FMT_FLOAT;
    NeAACDecSetConfiguration (this->faac_dec, this->faac_cfg);
  }
#if 0
  switch( this->num_channels ) {
    case 1:
      ao_cap_mode=AO_CAP_MODE_MONO;
      break;
    case 6:
      if(this->stream->audio_out->get_capabilities(this->stream->audio_out) &
         AO_CAP_MODE_5_1CHANNEL) {
        ao_cap_mode = AO_CAP_MODE_5_1CHANNEL;
        break;
      } else {
        this->faac_cfg = NeAACDecGetCurrentConfiguration(this->faac_dec);
        this->faac_cfg->downMatrix = 1;
        NeAACDecSetConfiguration(this->faac_dec, this->faac_cfg);
        this->num_channels = 2;
      }
    case 2:
      ao_cap_mode=AO_CAP_MODE_STEREO;
      break;
  default:
    return 0;
  }
#endif
  if (!faad_map_channels (this))
    return 0;

  {
    int ret = this->stream->audio_out->open (
      this->stream->audio_out, this->stream, this->bits_per_sample, this->rate, this->out_flags);
    this->output_open = ret ? 1 : (this->output_open - 1);
    return ret;
  }
}

static int faad_reopen_dec (faad_decoder_t *this) {

  if (this->faac_dec)
    NeAACDecClose (this->faac_dec);

  this->faac_dec = NeAACDecOpen ();

  if (!this->faac_dec) {
    xprintf (this->stream->xine, XINE_VERBOSITY_LOG, _("libfaad: libfaad NeAACDecOpen() failed.\n"));
    this->faac_failed++;
    return -1;
  }

  this->class->caps = NeAACDecGetCapabilities ();
  this->faac_cfg = NeAACDecGetCurrentConfiguration (this->faac_dec);
  if (this->faac_cfg) {
    /* We want float or fixed point data. */
    this->faac_cfg->outputFormat = (this->class->caps & FIXED_POINT_CAP) ? FAAD_FMT_FIXED : FAAD_FMT_FLOAT;
    NeAACDecSetConfiguration (this->faac_dec, this->faac_cfg);
  }
  return 0;
}

static int faad_apply_conf (faad_decoder_t *this, uint8_t *conf, int len) {
  unsigned long rate = 0;
  uint8_t num_channels = 0;
  int res;

  if (faad_reopen_dec (this) < 0)
    return -1;

  res = NeAACDecInit2 (this->faac_dec, conf, len, &rate, &num_channels);
  /* HACK: At least internal libfaad does not understand
   *  AOT_PS / n Hz / Mono / n*2 Hz / AOT_AAC_LC.
   * But it does understand (and find that PS in)
   *  AOT_SBR / n Hz / Mono / n*2 Hz / AOT_AAC_LC.
   */
  if (res < 0) do {
    static const uint8_t double_samplerates[16] = {~0, ~0, ~0, ~0, ~0, ~0, 3, 4, 5, 6, 7, 8, ~0, ~0, ~0, ~0};
    uint32_t bits;
    uint8_t save = conf[0];
    if (len < 3)
      break;
    memcpy (&bits, conf, 4);
    bits = bebf_ADJ32 (bits);
    if ((bits & 0xf8787c00) != ((AOT_PS << (32 - 5)) | (1 << (32 - 5 - 4 - 4)) | (AOT_AAC_LC << (32 - 5 - 4 - 4 - 4 - 5))))
      break;
    if (double_samplerates[(bits >> (32 - 5 - 4)) & 15] != ((bits >> (32 - 5 - 4 - 4 - 4)) & 15))
      break;
    conf[0] = (conf[0] & 7) | (AOT_SBR << 3);
    xprintf (this->class->xine, XINE_VERBOSITY_DEBUG, "faad_audio_decoder: using AOT_PS -> AOT_SBR hack\n");
    res = NeAACDecInit2 (this->faac_dec, conf, len, &rate, &num_channels);
    conf[0] = save;
  } while (0);
  /* more HACK s here */
  /* are we fine? */
  if (res >= 0) {
    if ((rate != this->rate) || (num_channels != this->num_channels)) {
      this->rate = rate;
      this->num_channels = num_channels;
      faad_close_output (this);
    }
    if (this->output_open <= 0)
      faad_open_output (this);
    faad_meta_info_set (this);
    return res;
  }
  /* no, its not working */
  xprintf (this->stream->xine, XINE_VERBOSITY_LOG, _("libfaad: libfaad NeAACDecInit2 failed.\n"));
  return res;
}

static int faad_apply_frame (faad_decoder_t *this, uint8_t *frame, int len) {
  unsigned long rate = 0;
  uint8_t num_channels = 0;
  int res;

  if (faad_reopen_dec (this) < 0)
    return -1;

  res = NeAACDecInit (this->faac_dec, frame, len, &rate, &num_channels);

  /* are we fine? */
  if (res >= 0) {
    if ((rate != this->rate) || (num_channels != this->num_channels)) {
      this->rate = rate;
      this->num_channels = num_channels;
      faad_close_output (this);
    }
    if (this->output_open <= 0)
      faad_open_output (this);
    faad_meta_info_set (this);
    return res;
  }
  /* no, its not working */
  xprintf (this->stream->xine, XINE_VERBOSITY_LOG, _("libfaad: libfaad NeAACDecInit failed.\n"));
  return res;
}

static int faad_open_dec (faad_decoder_t *this) {
  int res;

  if (this->dec_config)
    res = faad_apply_conf (this, this->dec_config, this->dec_config_size);
  else
    res = faad_apply_frame (this, this->buf, this->size);

  if (res < 0) {
    if (this->faac_dec) {
      NeAACDecClose (this->faac_dec);
      this->faac_dec = NULL;
    }
    _x_stream_info_set (this->stream, XINE_STREAM_INFO_AUDIO_HANDLED, 0);
    return 1;
  }

  lprintf ("NeAACDecInit() returned rate=%lu channels=%d (used=%d)\n", this->rate, this->num_channels, used);
  return 0;
}

static void faad_decode_audio ( faad_decoder_t *this, int end_frame ) {
  int used, decoded, framecount;
  const uint8_t *sample_buffer;
  uint8_t *inbuf;

#define ADTS_FAKE_CFG

  inbuf = this->buf;
  framecount = 0;
  while (1) {

    if (this->latm_mode == BEBF_LATM_IS_ADTS) {
      if (framecount == 0) {
        /* quick and dirty ADTS parser. */
        /* It is faster than libfaad's own, and it is safe with 0xff padding bytes. */
        uint8_t *q = inbuf;
        int headsize, s;
        memcpy (q + this->size, "\xf0\xff\xf0\x00", 4); /* overread stop */
        while (1) {
          if (q[0] == 0xff) {
            if ((q[1] & 0xf6) == 0xf0)
              break;
          }
          q++;
        }
        this->size -= q - inbuf;
        if (this->size < 0) { /* no sync */
          this->size = 0;
          break;
        }
        inbuf = q;
        headsize = (q[1] & 1) ? 7 : 9; /* optional 2 bytes crc */
        if (this->size < headsize) /* incomplete head */
          break;
        s = (((uint32_t)q[3] << 11) | ((uint32_t)q[4] << 3) | (q[5] >> 5)) & 0x1fff;
        if (s < headsize) { /* invalid head */
          inbuf++;
          this->size--;
          continue;
        }
        if (this->size < s) /* incomplete frame */
          break;
        framecount = (q[6] & 3) + 1;
        /* seen this head before? */
        if (((q[2] ^ this->adts_lasthead[0]) & 0xfd) | ((q[3] ^ this->adts_lasthead[1]) & 0xc0)) {
          /* Safety: require this new head to appear at least got7 I mean twice ;-) */
          if (this->size < s + 4)
            break;
          if ((q[s] != 0xff) || ((q[s + 1] & 0xf6) != 0xf0)
            || ((q[2] ^ q[s + 2]) & 0xfd) || ((q[3] ^ q[s + 3]) & 0xc0)) {
            framecount = 0;
            inbuf++;
            this->size--;
            continue;
          }
          xprintf (this->stream->xine, XINE_VERBOSITY_DEBUG, "faad_audio_decoder: got new AAC config from ADTS\n");
          memcpy (this->adts_lasthead, q + 2, 2);
          /* TJ. A note on that nasty SBR hack below.
           * SBR (Spectral Band Replication) is a more efficient algorithm for encoding high pitched sound.
           * It is common to do the base as plain LC (Low Complexity) at half samplerate (eg 22050), then
           * add treble as SBR at full rate (eg 44100). The actual SBR data hides inside "filler" blocks,
           * so old decoders can still play in AM radio quality.
           * SBR need not show up in every frame, and ADTS heads have no room for announcing SBR initially.
           * So output may open at half rate. If SBR appears later, it cannot play properly, and will be dropped.
           * This hack now always prepares for SBR when it might be useful. If there is really no SBR used,
           * we will just upsample.
           */
          this->adts_fake = 0;
#ifdef ADTS_FAKE_CFG
          do {
            static const uint8_t double_samplerates[16] = {0, 0, 0, 0, 0, 0, 3, 4, 5, 6, 7, 8, 0, 0, 0, 0};
            uint32_t sr1, sr2, chancfg;
            if ((q[2] >> 6) != AOT_AAC_LC - 1)
              break;
            sr1 = (q[2] >> 2) & 15;
            sr2 = double_samplerates[sr1];
            if (!sr2)
              break;
            chancfg = ((q[2] << 2) | (q[3] >> 6)) & 7;
            this->adts_fake  = AOT_SBR    << (32 - 5);
            this->adts_fake |= sr1        << (32 - 5 - 4);
            this->adts_fake |= chancfg    << (32 - 5 - 4 - 4);
            this->adts_fake |= sr2        << (32 - 5 - 4 - 4 - 4);
            this->adts_fake |= AOT_AAC_LC << (32 - 5 - 4 - 4 - 4 - 5);
            /* + 3 more 0 bits: frameLength, dependsOnCoreCoder, extensionFlag1 = 25 bits = 4 bytes */
            this->adts_fake = bebf_ADJ32 (this->adts_fake);
            xprintf (this->stream->xine, XINE_VERBOSITY_DEBUG, "faad_audio_decoder: trying fake AAC config to enable SBR\n");
            if (faad_apply_conf (this, (uint8_t *)&this->adts_fake, 4) >= 0)
              break;
            this->adts_fake = 0;
          } while (0);
#endif
          if (!this->adts_fake) {
            if (faad_apply_frame (this, inbuf, s) < 0) {
              framecount = 0;
              inbuf++;
              this->size--;
              continue;
            }
          }
        }
        if (this->adts_fake) {
          inbuf += headsize;
          this->size -= headsize;
        } else {
          framecount = 0;
        }
      }
      if (framecount)
        framecount--;
      sample_buffer = NeAACDecDecode (this->faac_dec, &this->faac_finfo, inbuf, this->size);
      if (!sample_buffer) {
        xprintf (this->stream->xine, XINE_VERBOSITY_DEBUG, "libfaad: %s\n", NeAACDecGetErrorMessage (this->faac_finfo.error));
        used = 1;
      } else {
        used = this->faac_finfo.bytesconsumed;
      }

    } else if (this->latm_mode == BEBF_LATM_IS_LATM) {
      int l = this->size;
      int latm_state = bebf_latm_parse (&this->latm, (const uint8_t *)inbuf, &l);
      if (l <= 0)
        break;
      used = l;
      if (latm_state & BEBF_LATM_GOT_CONF) {
        xprintf (this->stream->xine, XINE_VERBOSITY_DEBUG, "faad_audio_decoder: got new AAC config from LATM\n");
        if (faad_apply_conf (this, this->latm.config, this->latm.conflen) < 0) {
          inbuf++;
          this->size--;
          continue;
        }
      }
      sample_buffer = NULL;
      if (latm_state & BEBF_LATM_GOT_FRAME) {
        sample_buffer = NeAACDecDecode (this->faac_dec, &this->faac_finfo, this->latm.frame, this->latm.framelen);
        if (!sample_buffer) {
          xprintf (this->stream->xine, XINE_VERBOSITY_DEBUG, "libfaad: %s\n", NeAACDecGetErrorMessage (this->faac_finfo.error));
        }
      }

    } else {
      if (!this->faac_dec) {
        if (faad_open_dec (this)) {
          this->size = 0;
          return;
        }
      }
      if (this->latm_mode == BEBF_LATM_IS_RAW) {
        if (!end_frame || this->size < 10)
          break;
      } else {
        if (this->size < this->rec_audio_src_size)
          break;
      }
      sample_buffer = NeAACDecDecode (this->faac_dec, &this->faac_finfo, inbuf, this->size);
      if (!sample_buffer) {
        xprintf (this->stream->xine, XINE_VERBOSITY_DEBUG, "libfaad: %s\n", NeAACDecGetErrorMessage (this->faac_finfo.error));
        used = 1;
      } else {
        used = this->faac_finfo.bytesconsumed;
      }
    }

    if (sample_buffer) {

      /* need a different output? */
      if ((this->num_channels != this->faac_finfo.channels) || (this->rate != this->faac_finfo.samplerate)) {

        this->num_channels = this->faac_finfo.channels;
        this->rate = this->faac_finfo.samplerate;

        lprintf("NeAACDecDecode() returned rate=%lu channels=%d used=%d\n",
                this->rate, this->num_channels, used);

        faad_close_output (this);
      }

      if (this->output_open <= 0) {
        if (this->output_open <= -5) { /* too many fails for same settings */
          this->size = 0;
          break;
        }
        if (!faad_open_output (this)) {
          this->size = 0;
          break;
        }
        faad_meta_info_set( this );
      }

      /* faad doesn't tell us about sbr until after the first frame */
      if (this->sbr != this->faac_finfo.sbr) {
        this->sbr = this->faac_finfo.sbr;
        faad_meta_info_set( this );
      }

      /* estimate bitrate */
      this->total_time += (1000*this->faac_finfo.samples/(this->rate*this->num_channels));
      this->total_data += 8*used;

      if ((this->total_time > LONG_MAX) || (this->total_data > LONG_MAX)) {
        this->total_time >>= 2;
        this->total_data >>= 2;
      }

      if (this->total_time)
        _x_stream_info_set(this->stream, XINE_STREAM_INFO_AUDIO_BITRATE,
                           1000*(this->total_data/this->total_time));

      decoded = this->faac_finfo.samples / this->num_channels;

      lprintf("decoded %d/%d output %ld\n",
              used, this->size, this->faac_finfo.samples );

      /* Performing necessary channel reordering because aac uses a different
       * layout than alsa:
       *
       *  aac 5.1 channel layout: c l r ls rs lfe
       * alsa 5.1 channel layout: l r ls rs c lfe
       *
       * Reordering is only necessary for 5.0 and above. Currently only 5.0
       * and 5.1 is being taken care of, the rest will stay in the wrong order
       * for now.
       */

#define sat16(v) (((v + 0x8000) & ~0xffff) ? ((v) >> 31) ^ 0x7fff : (v))

      /* Maximum frame size is 1024 samples * 8 channels * 2 bytes = 16k.
       * Thus we probably dont need to while this.
       */
      while (decoded) {
        audio_buffer_t *audio_buffer = this->stream->audio_out->get_buffer (this->stream->audio_out);
        int done = this->out_channels;
        done = audio_buffer->mem_size / done / 2;
        if (done > decoded)
          done = decoded;
        audio_buffer->num_frames = done;
        audio_buffer->vpts = this->pts0;
        this->pts0 = 0;

        if ((this->in_channels <= 2) && (this->out_channels > this->in_channels))
          memset (audio_buffer->mem, 0, this->out_channels * done * 2);

        this->master = this->class->master;

        if (this->class->caps & FIXED_POINT_CAP) {
          /* hint compiler to use 32 to 64 bit multiply instruction where available. */
          /* also, that shift optimizes to almost nothing at 32bit system.           */
#define GET1(i,j)    v = ((int64_t)g1 * (int64_t)p[i]) >> 32; q[j] = sat16 (v)
#define GET2(i,j)    v = ((int64_t)g1 * (int64_t)(p[i] + p[i + 1])) >> 32; q[j] = sat16 (v)
#define GET1M(i,j)   v = (m + (int64_t)g1 * (int64_t)p[i]) >> 32; q[j] = sat16 (v)
#define GET2M(i,j,l) v = ((int64_t)g1 * (int64_t)p[i] + (int64_t)g2 * (int64_t)(m + p[j])) >> 32; q[l] = sat16 (v)
          const int32_t *p = (const int32_t *)sample_buffer;
          int16_t *q = (int16_t *)audio_buffer->mem;
          int n = done;
          if (this->in_channels < 6) {
            if (this->in_channels < 2) {
              if (this->out_used <= 1) { /* M -> M */
                int32_t g1 = this->master->gain_i;
                do { int32_t v; GET1 (0, 0); p++; q++; } while (--n);
              } else { /* M -> M M ... */
                int32_t g1 = this->master->gain_i;
                do { int32_t v; GET1 (0, 0); q[1] = q[0]; p++; q += this->out_channels; } while (--n);
              }
            } else {
              if (this->out_used < 2) { /* L R -> M */
                int32_t g1 = this->master->gain6_i;
                do { int32_t v; GET2 (0, 0); p += 2; q++; } while (--n);
              } else { /* L R -> L R ... */
                int32_t g1 = this->master->gain_i;
                do { int32_t v; GET1 (0, 0); GET1 (1, 1); p += 2; q += this->out_channels; } while (--n);
              }
            }
          } else {
            /* if (this->in_channels < 8) */ {
              switch (this->out_mode) {
                case 0: /* C L R SL SR B -> M */
                  do {
                    int32_t v;
                    v = ((int64_t)this->master->gain9_i * (int64_t)(p[1] + p[2])
                      +  (int64_t)this->master->gain6_i * (int64_t)(p[0] + p[5])
                      +  (int64_t)this->master->gain12_i * (int64_t)(p[3] + p[4])) >> 32;
                    *q++ = sat16 (v); p += this->in_channels;
                  } while (--n);
                break;
                case 1: { /* C L R SL SR B -> L R */
                  int32_t g1 = this->master->gain3_i;
                  int32_t g2 = this->master->gain6_i;
                  do {
                    int32_t m = p[0] + p[5];
                    int32_t v;
                    GET2M (1, 3, 0); GET2M (2, 4, 1);
                    p += this->in_channels; q += 2;
                  } while (--n);
                } break;
                case 2: { /* C L R SL SR B -> L R SL SR */
                  int32_t g1 = this->master->gain3_i;
                  do {
                    int64_t m = (int64_t)this->master->gain6_i * (int64_t)(p[0] + p[5]);
                    int32_t v;
                    GET1M (1, 0); GET1M (2, 1); GET1 (3, 2); GET1 (4, 3);
                    p += this->in_channels; q += 4;
                  } while (--n);
                } break;
                case 3: { /* C L R SL SR B -> L R SL SR 0 B */
                  int32_t g1 = this->master->gain3_i;
                  do {
                    int64_t m = (int64_t)this->master->gain6_i * (int64_t)p[0];
                    int32_t v;
                    GET1M (1, 0); GET1M (2, 1); GET1 (3, 2); GET1 (4, 3); q[4] = 0; GET1 (5, 5);
                    p += this->in_channels; q += 6;
                  } while (--n);
                } break;
                case 4: { /* C L R SL SR B -> L R SL SR C 0 */
                  int32_t g1 = this->master->gain3_i;
                  do {
                    int64_t m = (int64_t)this->master->gain6_i * (int64_t)p[5];
                    int32_t v;
                    GET1M (1, 0); GET1M (2, 1); GET1 (3, 2); GET1 (4, 3); GET1 (0, 4); q[5] = 0;
                    p += this->in_channels; q += 6;
                  } while (--n);
                } break;
                case 5: { /* C L R SL SR B -> L R SL SR C B */
                  int32_t g1 = this->master->gain_i;
                  do {
                    int32_t v;
                    GET1 (1, 0); GET1 (2, 1); GET1 (3, 2); GET1 (4, 3); GET1 (0, 4); GET1 (5, 5);
                    p += this->in_channels; q += 6;
                  } while (--n);
                } break;
              }
            }
          }
          sample_buffer = (const uint8_t *)p;
#undef GET1
#undef GET2
#undef GET1M
#undef GET2M
        } else {
#define GET1(i,j)    v = g1 * p[i]; q[j] = sat16 (v)
#define GET2(i,j)    v = g1 * (p[i] + p[i + 1]); q[j] = sat16 (v)
#define GET1M(i,j)   v = m + g1 * p[i]; q[j] = sat16 (v)
#define GET2M(i,j,l) v = g1 * p[i] + g2 * (m + p[j]); q[l] = sat16 (v)
          const float *p = (const float *)sample_buffer;
          int16_t *q = (int16_t *)audio_buffer->mem;
          int n = done;
          if (this->in_channels < 6) {
            if (this->in_channels < 2) {
              if (this->out_used <= 1) { /* M -> M */
                float g1 = this->master->gain_f;
                do { int32_t v; GET1 (0, 0); p++; q++; } while (--n);
              } else { /* M -> M M ... */
                float g1 = this->master->gain_f;
                do { int32_t v; GET1 (0, 0); q[1] = q[0]; p++; q += this->out_channels; } while (--n);
              }
            } else {
              if (this->out_used < 2) { /* L R -> M */
                float g1 = this->master->gain6_f;
                do { int32_t v; GET2 (0, 0); p += 2; q++; } while (--n);
              } else { /* L R -> L R ... */
                float g1 = this->master->gain_f;
                do { int32_t v; GET1 (0, 0); GET1 (1, 1); p += 2; q += this->out_channels; } while (--n);
              }
            }
          } else {
            /* if (this->in_channels < 8) */ {
              switch (this->out_mode) {
                case 0: /* C L R SL SR B -> M */
                  do {
                    int32_t v;
                    v = this->master->gain9_f * (p[1] + p[2])
                      + this->master->gain6_f * (p[0] + p[5])
                      + this->master->gain12_f * (p[3] + p[4]);
                    *q++ = sat16 (v); p += this->in_channels;
                  } while (--n);
                break;
                case 1: { /* C L R SL SR B -> L R */
                  float g1 = this->master->gain3_f;
                  float g2 = this->master->gain6_f;
                  do {
                    float m = p[0] + p[5];
                    int32_t v;
                    GET2M (1, 3, 0); GET2M (2, 4, 1);
                    p += this->in_channels; q += 2;
                  } while (--n);
                } break;
                case 2: { /* C L R SL SR B -> L R SL SR */
                  float g1 = this->master->gain3_f;
                  do {
                    float m = this->master->gain6_f * (p[0] + p[5]);
                    int32_t v;
                    GET1M (1, 0); GET1M (2, 1); GET1 (3, 2); GET1 (4, 3);
                    p += this->in_channels; q += 4;
                  } while (--n);
                } break;
                case 3: { /* C L R SL SR B -> L R SL SR 0 B */
                  float g1 = this->master->gain3_f;
                  do {
                    float m = this->master->gain6_f * p[0];
                    int32_t v;
                    GET1M (1, 0); GET1M (2, 1); GET1 (3, 2); GET1 (4, 3); q[4] = 0; GET1 (5, 5);
                    p += this->in_channels; q += 6;
                  } while (--n);
                } break;
                case 4: { /* C L R SL SR B -> L R SL SR C 0 */
                  float g1 = this->master->gain3_f;
                  do {
                    float m = this->master->gain6_f * p[5];
                    int32_t v;
                    GET1M (1, 0); GET1M (2, 1); GET1 (3, 2); GET1 (4, 3); GET1 (0, 4); q[5] = 0;
                    p += this->in_channels; q += 6;
                  } while (--n);
                } break;
                case 5: { /* C L R SL SR B -> L R SL SR C B */
                  float g1 = this->master->gain_f;
                  do {
                    int32_t v;
                    GET1 (1, 0); GET1 (2, 1); GET1 (3, 2); GET1 (4, 3); GET1 (0, 4); GET1 (5, 5);
                    p += this->in_channels; q += 6;
                  } while (--n);
                } break;
              }
            }
          }
          sample_buffer = (const uint8_t *)p;
#undef GET1
#undef GET2
#undef GET1M
#undef GET2M
        }

        this->stream->audio_out->put_buffer (this->stream->audio_out, audio_buffer, this->stream);

        decoded -= done;
      }
    }

    this->pts0 = this->pts1;
    this->pts1 = 0;

    if ((used >= this->size) || (this->latm_mode == BEBF_LATM_IS_RAW)) {
      this->size = 0;
      break;
    }
    this->size -= used;
    inbuf += used;
  }

  if( this->size )
    memmove( this->buf, inbuf, this->size);

}

static void faad_get_conf (faad_decoder_t *this, const uint8_t *d, int len) {
  uint8_t *b;

  if (!d || (len <= 0))
    return;

  b = this->dec_config;
  if (b) {
    if ((this->dec_config_size == len) && !memcmp (b, d, len))
      return;
    if (this->dec_config_size < len) {
      free (b);
      this->dec_config = b = NULL;
      this->dec_config_size = 0;
    }
  }

  if (!b) {
    b = malloc (len + 8);
    if (!b)
      return;
  }

  memcpy (b, d, len);
  memset (b + len, 0, 8);
  this->dec_config = b;
  this->dec_config_size = len;
  this->latm_mode = BEBF_LATM_IS_RAW;
  xprintf (this->class->xine, XINE_VERBOSITY_DEBUG, "faad_audio_decoder: got new AAC config from demuxer\n");

  if (!this->faac_dec)
    return;
  NeAACDecClose (this->faac_dec);
  this->faac_dec = NULL;
}

static void faad_decode_data (audio_decoder_t *this_gen, buf_element_t *buf) {

  faad_decoder_t *this = (faad_decoder_t *) this_gen;

  if (buf->decoder_flags & BUF_FLAG_PREVIEW)
    return;

  /* store config information from ESDS mp4/qt atom */
  if ((buf->decoder_flags & BUF_FLAG_SPECIAL) &&
      (buf->decoder_info[1] == BUF_SPECIAL_DECODER_CONFIG))
    faad_get_conf (this, buf->decoder_info_ptr[2], buf->decoder_info[2]);

  /* get audio parameters from file header
     (may be overwritten by libfaad returned parameters) */
  if (buf->decoder_flags & BUF_FLAG_STDHEADER) {
    xine_waveformatex *wavex = (xine_waveformatex *)buf->content;
    this->rate            = buf->decoder_info[1];
    this->bits_per_sample = buf->decoder_info[2];
    this->num_channels    = buf->decoder_info[3];
    if (wavex) {
      int len = (int)buf->size - sizeof (xine_waveformatex);
      if (len > 0) {
        if (len > wavex->cbSize)
          len = wavex->cbSize;
        faad_get_conf (this, buf->content + sizeof (xine_waveformatex), len);
      }
    }
  } else {

    lprintf ("decoding %d data bytes...\n", buf->size);

    if ((int)buf->size <= 0)
      return;

    /* Queue up to 2 pts values as frames may overlap buffer boundaries (mpeg-ts). */
    if (!this->size || !this->pts0) {
      this->pts0 = buf->pts;
      this->pts1 = 0;
    } else if (!this->pts1) {
      this->pts1 = buf->pts;
    }

    if (this->size + buf->size + 8 > this->max_audio_src_size) {
      size_t s = this->size + 2 * buf->size + 8;
      uint8_t *b = realloc (this->buf, s);
      if (!b)
        return;
      this->buf = b;
      this->max_audio_src_size = s;
    }

    memcpy (&this->buf[this->size], buf->content, buf->size);
    this->size += buf->size;

    /* Instream header formats like ADTS do support midstream config changes,
     * eg a 5.1 movie following the stereo news show.
     * Unfortunately, libfaad by itself does detect such changes, but it does
     * not follow them, and bails out instead. It probably does so to prevent
     * glitches from damaged streams.
     * However, this leaves us to decide what a real change is, and call
     * NeAACDecInit () again on the new data. Even worse, switching from 5.1
     * to stereo needs a full reopen.
     * Making that sane requires parsing anyway, so maybe it is a good idea
     * to always parse here.
     */
#ifdef PARSE_ONLY_LATM
    if ((buf->type & (BUF_MAJOR_MASK | BUF_DECODER_MASK)) == BUF_AUDIO_AAC_LATM)
#endif
    {
      if (this->latm_mode == BEBF_LATM_NEED_MORE_DATA) {
        this->latm_mode = bebf_latm_test ((const uint8_t *)this->buf, this->size);
        if (this->latm_mode == BEBF_LATM_NEED_MORE_DATA)
          return;
        if (this->latm_mode == BEBF_LATM_IS_ADTS) {
#ifndef PARSE_ONLY_LATM
          if ((buf->type & (BUF_MAJOR_MASK | BUF_DECODER_MASK)) == BUF_AUDIO_AAC_LATM)
#endif
            xprintf (this->class->xine, XINE_VERBOSITY_DEBUG,
              "faad_audio_decoder: stream says LATM but is ADTS\n");
        }
      }
    }

    faad_decode_audio(this, buf->decoder_flags & BUF_FLAG_FRAME_END );
  }
}

static void faad_discontinuity (audio_decoder_t *this_gen) {
}

static void faad_dispose (audio_decoder_t *this_gen) {

  faad_decoder_t *this = (faad_decoder_t *) this_gen;

  bebf_latm_close (&this->latm);

  faad_close_output (this);

  if( this->buf )
    free(this->buf);
  this->buf = NULL;
  this->size = 0;
  this->max_audio_src_size = 0;

  if( this->dec_config )
    free(this->dec_config);
  this->dec_config = NULL;
  this->dec_config_size = 0;

  if( this->faac_dec )
    NeAACDecClose(this->faac_dec);
  this->faac_dec = NULL;
  this->faac_failed = 0;

  free (this);
}


static audio_decoder_t *open_plugin (audio_decoder_class_t *class_gen, xine_stream_t *stream) {

  faad_decoder_t *this ;

  this = calloc(1, sizeof (faad_decoder_t));
  if (!this)
    return NULL;

  this->class = (faad_class_t *)class_gen;

  this->audio_decoder.decode_data         = faad_decode_data;
  this->audio_decoder.reset               = faad_reset;
  this->audio_decoder.discontinuity       = faad_discontinuity;
  this->audio_decoder.dispose             = faad_dispose;

  this->stream             = stream;
  this->output_open        = 0;
  this->faac_dec           = NULL;
  this->faac_failed        = 0;
  this->buf                = NULL;
  this->size               = 0;
  this->max_audio_src_size = 0;
  this->dec_config         = NULL;
  this->dec_config_size    = 0;
  this->total_time         = 0;
  this->total_data         = 0;

  this->bits_per_sample    = 16;
  this->rate               = 0;

  bebf_latm_open (&this->latm);
  this->latm_mode          = BEBF_LATM_NEED_MORE_DATA;

  return &this->audio_decoder;
}

/*
 * TJ. The default -3dB preamp gain fixes stereo streams done with
 * buggy encoders. FFmpeg used to have that bug, and even today (2016)
 * certain people keep publishing such material.
 * Multichannel downmixing stategy:
 * - Downmix center and bass to front left and right. As that doubles
 *   their power (1 -> 2 playback channels), compensate by -3dB.
 * - Downmix sides to front by -3dB as well, this time to reduce fx.
 * - Downmix to mono by simply averaging (l + r -6dB).
 * To be absolutely clip safe, we would need to attenuate the result
 * by up to 1 / (1 + 3 * sqrt (1 / 2)), or -9.9 dB. However, this is
 * hardly ever needed. So lets use another -3dB instead, and - hey -
 * we got gain control now ;-)
 * Note these are binary (6 * ld (v)) not decimal (20 * log (v)) dB,
 * as they are easier to handle in fixed point mode.
 */
static void gain_update (faad_class_t *this) {
  /* XXX: assuming 30bit fixed point math. If it is not, simply adjust your gain ;-) */
  static const int32_t gi[6] = { 262144, 294247, 330281, 370728, 416128, 467088 };
  static const float   gf[6] = { 32768.0000, 36780.8364, 41285.0930, 46340.9500, 52015.9577, 58385.9384 };
  int exp = this->gain_db, mant = exp;
  /* main channel scaling */
  if (exp < 0) {
    exp = (5 - exp) / 6;
    mant = (6000 + mant) % 6;
    this->gain_i  = gi[mant] >> exp;
    this->gain_f  = gf[mant] / (float)(1 << exp);
  } else {
    exp = exp / 6;
    mant = mant % 6;
    this->gain_i = gi[mant] << exp;
    this->gain_f = gf[mant] * (float)(1 << exp);
  }
  /* sub channel downmixing */
  this->gain3_i  = (this->gain_i * 11) >> 4;
  this->gain9_i  = this->gain3_i >> 1;
  this->gain6_i  = this->gain_i >> 1;
  this->gain12_i = this->gain_i >> 2;
  this->gain3_f  = this->gain_f * 0.7071;
  this->gain6_f  = this->gain_f * 0.5;
  this->gain9_f  = this->gain_f * 0.3535;
  this->gain12_f = this->gain_f * 0.25;
}

static void gain_cb (void *user_data, xine_cfg_entry_t *entry) {
  faad_class_t *class = (faad_class_t *)user_data;
  class->gain_db = entry->num_value;
  gain_update (class);
}

/* class management */

static void faad_class_ref (faad_class_t *this) {
  if (this)
    (this->refs)++;
}

static void faad_class_unref (audio_decoder_class_t *this_gen) {
  faad_class_t *this = (faad_class_t *)this_gen, *master;
  xine_t *xine;
  if (!this)
    return;
  xine = this->xine;
  master = this->master;
  (this->refs)--;
  if (!this->refs && (master != this))
    free (this);
  if (--(master->refs))
    return;
  xine->config->unregister_callback (xine->config, "audio.processing.faad_gain_dB");
  free (master);
}

static void *faad_init_plugin (xine_t *xine, void *data, const char *id) {

  faad_class_t *this, *master;
  struct cfg_entry_s *entry;

  this = calloc(1, sizeof (faad_class_t));
  if (!this)
    return NULL;

  faad_class_ref (this);
  this->decoder_class.open_plugin     = open_plugin;
  this->decoder_class.identifier      = id;
  this->decoder_class.description     = N_("Freeware Advanced Audio Decoder");
  this->decoder_class.dispose         = faad_class_unref;

  this->xine = xine;

  master = NULL;
  entry = xine->config->lookup_entry (xine->config, "audio.processing.faad_gain_dB");
  if (entry && (entry->callback == gain_cb))
    master = (faad_class_t *)entry->callback_data;
  if (master) {
    faad_class_ref (master);
    this->master = master;
  } else {
    faad_class_ref (this);
    this->master = this;
    this->gain_db = xine->config->register_num (xine->config,
      "audio.processing.faad_gain_dB", -3,
      _("FAAD audio gain (dB)"),
      _("Some AAC tracks are encoded too loud and thus play distorted.\n"
      "This cannot be fixed by volume control, but by this setting."),
      10, gain_cb, this);
    gain_update (this);
  }

  return this;
}

static void *latm_init_class (xine_t *xine, void *data) {
  return faad_init_plugin (xine, data, "FAAD-LATM");
}

static void *faad_init_class (xine_t *xine, void *data) {
  return faad_init_plugin (xine, data, "FAAD");
}

static const uint32_t audio_types[] = {
  BUF_AUDIO_AAC, 0
 };

static const decoder_info_t dec_info_audio = {
  audio_types,         /* supported types */
  8                    /* priority        */
};

static const uint32_t latm_audio_types[] = {
  BUF_AUDIO_AAC_LATM, 0
 };

static const decoder_info_t dec_info_latm_audio = {
  latm_audio_types,    /* supported types */
  1                    /* priority        */
};

const plugin_info_t xine_plugin_info[] EXPORTED = {
  /* type, API, "name", version, special_info, init_function */
  { PLUGIN_AUDIO_DECODER, 16, "faad", XINE_VERSION_CODE, &dec_info_audio, faad_init_class },
  { PLUGIN_AUDIO_DECODER, 16, "faad-latm", XINE_VERSION_CODE, &dec_info_latm_audio, latm_init_class },
  { PLUGIN_NONE, 0, "", 0, NULL, NULL }
};

