/*
 * Copyright (C) 2000-2018 the xine project
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
 * config object (was: file) management - implementation
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <errno.h>
#include <time.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>

#include <xine/configfile.h>
#include "bswap.h"

#define LOG_MODULE "configfile"
#define LOG_VERBOSE
/*
#define LOG
*/
/* #define DEBUG_CONFIG_FIND */

/* XXX: does this break some strange applictions?? */
#define SINGLE_CHUNK_ENUMS

#include <xine/xineutils.h>
#include <xine/xine_internal.h>
#include "xine_private.h"

/* FIXME: static data, no expiry ?! */
static const xine_config_entry_translation_t *config_entry_translation_user = NULL;

static const char *config_xlate_old (const char *s) {
  static const char * const tab[] = {
  /*"audio.a52_pass_through",			NULL,*/
    "audio.alsa_a52_device",			"audio.device.alsa_passthrough_device",
    "audio.alsa_default_device",		"audio.device.alsa_default_device",
    "audio.alsa_front_device",			"audio.device.alsa_front_device",
    "audio.alsa_mixer_name",			"audio.device.alsa_mixer_name",
    "audio.alsa_mmap_enable",			"audio.device.alsa_mmap_enable",
    "audio.alsa_surround40_device",		"audio.device.alsa_surround40_device",
    "audio.alsa_surround51_device",		"audio.device.alsa_surround51_device",
    "audio.av_sync_method",			"audio.synchronization.av_sync_method",
  /*"audio.directx_device",			NULL,*/
    "audio.esd_latency",			"audio.device.esd_latency",
  /*"audio.five_channel",			NULL,*/
  /*"audio.five_lfe_channel",			NULL,*/
    "audio.force_rate",				"audio.synchronization.force_rate",
  /*"audio.four_channel",			NULL,*/
  /*"audio.four_lfe_channel",			NULL,*/
    "audio.irixal_gap_tolerance",		"audio.device.irixal_gap_tolerance",
  /*"audio.mixer_name",				NULL,*/
    "audio.mixer_number",			"audio.device.oss_mixer_number",
    "audio.mixer_volume",			"audio.volume.mixer_volume",
    "audio.num_buffers",			"engine.buffers.audio_num_buffers",
    "audio.oss_device_name",			"audio.device.oss_device_name",
  /*"audio.oss_device_num",			NULL,*/
    "audio.oss_device_number",			"audio.device.oss_device_number",
  /*"audio.oss_pass_through_bug",		NULL,*/
    "audio.passthrough_offset",			"audio.synchronization.passthrough_offset",
    "audio.remember_volume",			"audio.volume.remember_volume",
    "audio.resample_mode",			"audio.synchronization.resample_mode",
    "audio.speaker_arrangement",		"audio.output.speaker_arrangement",
    "audio.sun_audio_device",			"audio.device.sun_audio_device",
    "codec.a52_dynrng",				"audio.a52.dynamic_range",
    "codec.a52_level",				"audio.a52.level",
    "codec.a52_surround_downmix",		"audio.a52.surround_downmix",
    "codec.ffmpeg_pp_quality",			"video.processing.ffmpeg_pp_quality",
    "codec.real_codecs_path",			"decoder.external.real_codecs_path",
    "codec.win32_path",				"decoder.external.win32_codecs_path",
    "dxr3.alt_play_mode",			"dxr3.playback.alt_play_mode",
    "dxr3.color_interval",			"dxr3.output.keycolor_interval",
    "dxr3.correct_durations",			"dxr3.playback.correct_durations",
  /*"dxr3.devicename",				NULL,*/
    "dxr3.enc_add_bars",			"dxr3.encoding.add_bars",
    "dxr3.enc_alt_play_mode",			"dxr3.encoding.alt_play_mode",
    "dxr3.enc_swap_fields",			"dxr3.encoding.swap_fields",
    "dxr3.encoder",				"dxr3.encoding.encoder",
    "dxr3.fame_quality",			"dxr3.encoding.fame_quality",
    "dxr3.keycolor",				"dxr3.output.keycolor",
    "dxr3.lavc_bitrate",			"dxr3.encoding.lavc_bitrate",
    "dxr3.lavc_qmax",				"dxr3.encoding.lavc_qmax",
    "dxr3.lavc_qmin",				"dxr3.encoding.lavc_qmin",
    "dxr3.lavc_quantizer",			"dxr3.encoding.lavc_quantizer",
    "dxr3.preferred_tvmode",			"dxr3.output.tvmode",
    "dxr3.rte_bitrate",				"dxr3.encoding.rte_bitrate",
    "dxr3.shrink_overlay_area",			"dxr3.output.shrink_overlay_area",
    "dxr3.sync_every_frame",			"dxr3.playback.sync_every_frame",
    "dxr3.videoout_mode",			"dxr3.output.mode",
    "input.cdda_cddb_cachedir",			"media.audio_cd.cddb_cachedir",
    "input.cdda_cddb_port",			"media.audio_cd.cddb_port",
    "input.cdda_cddb_server",			"media.audio_cd.cddb_server",
    "input.cdda_device",			"media.audio_cd.device",
    "input.cdda_use_cddb",			"media.audio_cd.use_cddb",
    "input.css_cache_path",			"media.dvd.css_cache_path",
    "input.css_decryption_method",		"media.dvd.css_decryption_method",
    "input.drive_slowdown",			"media.audio_cd.drive_slowdown",
    "input.dvb_last_channel_enable",		"media.dvb.remember_channel",
    "input.dvb_last_channel_watched",		"media.dvb.last_channel",
    "input.dvbdisplaychan",			"media.dvb.display_channel",
    "input.dvbzoom",				"media.dvb.zoom",
    "input.dvb_adapternum",			"media.dvb.adapter",
    "input.dvd_device",				"media.dvd.device",
    "input.dvd_language",			"media.dvd.language",
    "input.dvd_raw_device",			"media.dvd.raw_device",
    "input.dvd_region",				"media.dvd.region",
    "input.dvd_seek_behaviour",			"media.dvd.seek_behaviour",
    "input.dvd_skip_behaviour",			"media.dvd.skip_behaviour",
    "input.dvd_use_readahead",			"media.dvd.readahead",
    "input.file_hidden_files",			"media.files.show_hidden_files",
    "input.file_origin_path",			"media.files.origin_path",
    "input.http_no_proxy",			"media.network.http_no_proxy",
    "input.http_proxy_host",			"media.network.http_proxy_host",
    "input.http_proxy_password",		"media.network.http_proxy_password",
    "input.http_proxy_port",			"media.network.http_proxy_port",
    "input.http_proxy_user",			"media.network.http_proxy_user",
    "input.mms_network_bandwidth",		"media.network.bandwidth",
    "input.mms_protocol",			"media.network.mms_protocol",
    "input.pvr_device",				"media.wintv_pvr.device",
    "input.v4l_radio_device_path",		"media.video4linux.radio_device",
    "input.v4l_video_device_path",		"media.video4linux.video_device",
    "input.vcd_device",				"media.vcd.device",
    "misc.cc_center",				"subtitles.closedcaption.center",
    "misc.cc_enabled",				"subtitles.closedcaption.enabled",
    "misc.cc_font",				"subtitles.closedcaption.font",
    "misc.cc_font_size",			"subtitles.closedcaption.font_size",
    "misc.cc_italic_font",			"subtitles.closedcaption.italic_font",
    "misc.cc_scheme",				"subtitles.closedcaption.scheme",
    "misc.demux_strategy",			"engine.demux.strategy",
    "misc.memcpy_method",			"engine.performance.memcpy_method",
    "misc.osd_text_palette",			"ui.osd.text_palette",
    "misc.save_dir",				"media.capture.save_dir",
    "misc.spu_font",				"subtitles.separate.font",
    "misc.spu_src_encoding",			"subtitles.separate.src_encoding",
    "misc.spu_subtitle_size",			"subtitles.separate.subtitle_size",
    "misc.spu_use_unscaled_osd",		"subtitles.separate.use_unscaled_osd",
    "misc.spu_vertical_offset",			"subtitles.separate.vertical_offset",
    "misc.sub_timeout",				"subtitles.separate.timeout",
    "post.goom_csc_method",			"effects.goom.csc_method",
    "post.goom_fps",				"effects.goom.fps",
    "post.goom_height",				"effects.goom.height",
    "post.goom_width",				"effects.goom.width",
    "vcd.autoadvance",				"media.vcd.autoadvance",
    "vcd.autoplay",				"media.vcd.autoplay",
    "vcd.comment_format",			"media.vcd.comment_format",
    "vcd.debug",				"media.vcd.debug",
    "vcd.default_device",			"media.vcd.device",
    "vcd.length_reporting",			"media.vcd.length_reporting",
    "vcd.show_rejected",			"media.vcd.show_rejected",
    "vcd.title_format",				"media.vcd.title_format",
    "video.XV_DOUBLE_BUFFER",			"video.device.xv_double_buffer",
    "video.XV_FILTER",				"video.device.xv_filter",
    "video.deinterlace_method",			"video.output.xv_deinterlace_method",
    "video.disable_exact_osd_alpha_blending",	"video.output.disable_exact_alphablend",
    "video.disable_scaling",			"video.output.disable_scaling",
    "video.fb_device",				"video.device.fb_device",
    "video.fb_gamma",				"video.output.fb_gamma",
    "video.horizontal_position",		"video.output.horizontal_position",
    "video.num_buffers",			"engine.buffers.video_num_buffers",
    "video.opengl_double_buffer",		"video.device.opengl_double_buffer",
    "video.opengl_gamma",			"video.output.opengl_gamma",
    "video.opengl_min_fps",			"video.output.opengl_min_fps",
    "video.opengl_renderer",			"video.output.opengl_renderer",
    "video.pgx32_device",			"video.device.pgx32_device",
    "video.pgx64_brightness",			"video.output.pgx64_brightness",
    "video.pgx64_chromakey_en",			"video.device.pgx64_chromakey_en",
    "video.pgx64_colour_key",			"video.device.pgx64_colour_key",
  /*"video.pgx64_device",			NULL,*/
    "video.pgx64_multibuf_en",			"video.device.pgx64_multibuf_en",
  /*"video.pgx64_overlay_mode",			NULL,*/
    "video.pgx64_saturation",			"video.output.pgx64_saturation",
    "video.sdl_hw_accel",			"video.device.sdl_hw_accel",
    "video.unichrome_cpu_save",			"video.device.unichrome_cpu_save",
    "video.vertical_position",			"video.output.vertical_position",
    "video.vidix_blue_intensity",		"video.output.vidix_blue_intensity",
    "video.vidix_colour_key_blue",		"video.device.vidix_colour_key_blue",
    "video.vidix_colour_key_green",		"video.device.vidix_colour_key_green",
    "video.vidix_colour_key_red",		"video.device.vidix_colour_key_red",
    "video.vidix_green_intensity",		"video.output.vidix_green_intensity",
    "video.vidix_red_intensity",		"video.output.vidix_red_intensity",
    "video.vidix_use_double_buffer",		"video.device.vidix_double_buffer",
    "video.vidixfb_device",			"video.device.vidixfb_device",
    "video.warn_discarded_threshold",		"engine.performance.warn_discarded_threshold",
    "video.warn_skipped_threshold",		"engine.performance.warn_skipped_threshold",
    "video.xshm_gamma",				"video.output.xshm_gamma",
    "video.xv_autopaint_colorkey",		"video.device.xv_autopaint_colorkey",
    "video.xv_colorkey",			"video.device.xv_colorkey",
    "video.xv_pitch_alignment",			"video.device.xv_pitch_alignment",
    "video.xvmc_more_frames",			"video.device.xvmc_more_frames",
    "video.xvmc_nvidia_color_fix",		"video.device.xvmc_nvidia_color_fix"
  };
  int b = 0, e = sizeof (tab) / sizeof (tab[0]) / 2, m = e >> 1;
  do {
    int d = strcmp (s, tab[m << 1]);
    if (d == 0)
      return tab[(m << 1) + 1];
    if (d < 0)
      e = m;
    else
      b = m + 1;
    m = (b + e) >> 1;
  } while (b != e);
  return NULL;
}


static int config_section_enum(const char *sect) {
  static const char * const known_section[] = {
    "\x03""audio",
    "\x08""codec",
    "\x09""decoder",
    "\x05""dxr3",
    "\x0c""effects",
    "\x0d""engine",
    "\x01""gui",
    "\x06""input",
    "\x07""media",
    "\x0e""misc",
    "\x0b""post",
    "\x0a""subtitles",
    "\x02""ui",
    "\x04""video"
  };
  int b = 0, e = sizeof (known_section) / sizeof (known_section[0]), m = e >> 1;
  do {
    int d = strcmp (sect, known_section[m] + 1);
    if (d == 0)
      return known_section[m][0];
    if (d < 0)
      e = m;
    else
      b = m + 1;
    m = (b + e) >> 1;
  } while (b != e);
  return 0;
}

#define MAX_SORT_KEY 320
static void config_make_sort_key (char *dest, const char *key, int exp_level) {
  char *q = dest, *e = dest + MAX_SORT_KEY - 7;
  const char *p;
  int n;
  p = key;
  /* section name */
  while (q < e) {
    char z = *p;
    if (z == 0) break;
    p++;
    if (z == '.') break;
    *q++ = z;
  }
  *q = 0;
  n = config_section_enum (dest);
  if (n) {
    q = dest;
    *q++ = n;
  }
  *q++ = 0x1f;
  /* subsection name */
  while (q < e) {
    char z = *p;
    if (z == 0) break;
    p++;
    if (z == '.') break;
    *q++ = z;
  }
  *q++ = 0x1f;
  /* TJ. original code did sort by section/subsection/exp/name.
   * We can do that here as well but that means inefficient
   * adding (2 passes) and finding (linear scanning).
   * 2 entries with same key but different exp level never
   * were supported anyway, and frontends group entries by
   * section only. So lets leave level out, and benefit from
   * binary searches instead.
   */
#if 0
  /* experience level */
  n = exp_level;
  q[2] = (n % 10) + '0';
  n /= 10;
  q[1] = (n % 10) + '0';
  n /= 10;
  q[0] = (n % 10) + '0';
  q += 3;
  *q++ = 0x1f;
#else
  (void)exp_level;
#endif
  /* entry name */
  while (q < e) {
    char z = *p;
    if (z == 0) break;
    p++;
    *q++ = z;
  }
  *q = 0;
}

/* Ugly: rebuild index every time. Maybe we could cache it somewhere? */
static cfg_entry_t **config_array (config_values_t *this, int *n) {
  cfg_entry_t **tab, *e;
  int m = 256, i;
  tab = malloc (m * sizeof (*tab));
  if (!tab) {
    *n = 0;
    return NULL;
  }
  for (i = 0, e = this->first; e; e = e->next) {
    tab[i++] = e;
    if (i >= m) {
      cfg_entry_t **t2;
      m += 256;
      t2 = realloc (tab, m * sizeof (*tab));
      if (!t2)
        break;
      tab = t2;
    }
  }
  *n = i;
  return tab;
}

#define FIND_ONLY 0x7fffffff
static cfg_entry_t *config_insert (config_values_t *this, const char *key, int exp_level) {
  char new_sortkey[MAX_SORT_KEY];
  char cur_sortkey[MAX_SORT_KEY];
  cfg_entry_t *entry;

  if (!this->first || !this->last) {
    if (exp_level == FIND_ONLY)
      return NULL;
    entry = calloc (1, sizeof (cfg_entry_t));
    if (!entry)
      return NULL;
    this->first          = entry;
    this->last           = entry;
#ifndef HAVE_ZERO_SAFE_MEM
    entry->next          = NULL;
    entry->unknown_value = NULL;
    entry->str_value     = NULL;
    entry->str_default   = NULL;
    entry->help          = NULL;
    entry->description   = NULL;
#endif
    entry->config        = this;
    entry->key           = strdup(key);
    entry->type          = XINE_CONFIG_TYPE_UNKNOWN;
    entry->exp_level     = exp_level;
#ifdef DEBUG_CONFIG_FIND
    printf ("config_insert (\"%s\", %d) = new (0).\n", key, exp_level);
#endif
    return entry;
  }

  config_make_sort_key (new_sortkey, key, exp_level);

  /* Most frequent case is loading a config file entry.
   * Unless edited by user, these come in already sorted.
   * Thus try last pos first.
   */
  if (exp_level != FIND_ONLY) {
    config_make_sort_key (cur_sortkey, this->last->key, this->last->exp_level);
    if (strcmp (new_sortkey, cur_sortkey) > 0) {
      entry = calloc (1, sizeof (cfg_entry_t));
      if (!entry)
        return NULL;
      this->last->next     = entry;
      this->last           = entry;
#ifndef HAVE_ZERO_SAFE_MEM
      entry->next          = NULL;
      entry->unknown_value = NULL;
      entry->str_value     = NULL;
      entry->str_default   = NULL;
      entry->help          = NULL;
      entry->description   = NULL;
#endif
      entry->config        = this;
      entry->key           = strdup(key);
      entry->type          = XINE_CONFIG_TYPE_UNKNOWN;
      entry->exp_level     = exp_level;
#ifdef DEBUG_CONFIG_FIND
      printf ("config_insert (\"%s\", %d) = new (last).\n", key, exp_level);
#endif
      return entry;
    }
  }

  {
    cfg_entry_t **entries;
    int n, b, m, e, d;
    entries = config_array (this, &n);
    if (!entries)
      return NULL;

    b = 0; e = n; m = n >> 1;
    do {
      config_make_sort_key (cur_sortkey, entries[m]->key, entries[m]->exp_level);
      d = strcmp (new_sortkey, cur_sortkey);
      if (d == 0)
        break;
      if (d < 0)
        e = m;
      else
        b = m + 1;
      m = (b + e) >> 1;
    } while (b != e);

    if (d == 0) {
      entry = entries[m];
      free (entries);
#ifdef DEBUG_CONFIG_FIND
      printf ("config_insert (\"%s\", %d) = found (%d/%d).\n", key, exp_level, m, n);
#endif
      return entry;
    }

    if (exp_level == FIND_ONLY) {
      free (entries);
#ifdef DEBUG_CONFIG_FIND
      printf ("config_insert (\"%s\", %d) = not found.\n", key, exp_level);
#endif
      return NULL;
    }

    entry = calloc (1, sizeof (cfg_entry_t));
    if (!entry) {
      free (entries);
      return NULL;
    }
    if (m == 0) {
      entry->next = this->first;
      this->first = entry;
    } else {
      entries[m - 1]->next = entry;
      if (m < n) {
        entry->next = entries[m];
      } else {
        entry->next = NULL;
        this->last = entry;
      }
    }
    free (entries);
    entry->config        = this;
    entry->key           = strdup(key);
    entry->type          = XINE_CONFIG_TYPE_UNKNOWN;
#ifndef HAVE_ZERO_SAFE_MEM
    entry->unknown_value = NULL;
    entry->str_value     = NULL;
    entry->str_default   = NULL;
    entry->help          = NULL;
    entry->description   = NULL;
#endif
    entry->exp_level     = exp_level;
#ifdef DEBUG_CONFIG_FIND
    printf ("config_insert (\"%s\", %d) = new (%d/%d).\n", key, exp_level, m, n);
#endif
    return entry;
  }
}

static const char *config_xlate_internal (const char *key, const xine_config_entry_translation_t *trans)
{
  --trans;
  while ((++trans)->old_name)
    if (trans->new_name[0] && strcmp(key, trans->old_name) == 0)
      return trans->new_name;
  return NULL;
}

static const char *config_translate_key (const char *key, char **tmp) {
  /* Returns translated key or, if no translation found, NULL.
   * Translated key may be in a static buffer allocated within this function.
   * NOT re-entrant; assumes that config_lock is held.
   */
  unsigned trans;
  const char *newkey = NULL;

  /* first, special-case the decoder entries (so that new ones can be added
   * without requiring modification of the translation table)
   */
  *tmp = NULL;
  if (!strncmp (key, "decoder.", 8) &&
      !strcmp (key + (trans = strlen (key)) - 9, "_priority")) {
    *tmp = _x_asprintf ("engine.decoder_priorities.%.*s", trans - 17, key + 8);
    return *tmp;
  }

  /* search the translation table... */
  newkey = config_xlate_old (key);
  if (!newkey && config_entry_translation_user)
    newkey = config_xlate_internal (key, config_entry_translation_user);

  return newkey;
}

static cfg_entry_t *config_lookup_entry_int (config_values_t *this, const char *key) {
  cfg_entry_t *entry;
  char *tmp = NULL;

  /* try twice at most (second time with translation from old key name) */
  entry = config_insert (this, key, FIND_ONLY);
  if (entry)
    return entry;
  /* we did not find a match, maybe this is an old config entry name
   * trying to translate */
  key = config_translate_key (key, &tmp);
  if (!key) {
    free (tmp);
    return NULL;
  }
  entry = config_insert (this, key, FIND_ONLY);
  free (tmp);
  return entry;
}

static char **str_array_dup (const char **from, uint32_t *n) {
#ifdef SINGLE_CHUNK_ENUMS
  uint32_t sizes[257], *sitem, all;
  const char **fitem;
  char **to, **titem, *q;

  *n = 0;
  if (!from)
    return NULL;

  fitem = from;
  sitem = sizes;
  all = sizeof (char *);
  while (*fitem && (sitem < sizes + 256))
    all += (*sitem++ = strlen (*fitem++) + 1) + sizeof (char *);
  *sitem = 0;
  to = malloc (all);
  if (!to)
    return NULL;

  *n = sitem - sizes;
  q = (char *)to + (*n + 1) * sizeof (char *);
  fitem = from;
  sitem = sizes;
  titem = to;
  while (*sitem) {
    *titem++ = q;
    memcpy (q, *fitem++, *sitem);
    q += *sitem++;
  }
  *titem = NULL;
  return to;
#else
  const char **fitem;
  char **to, **titem;

  *n = 0;
  if (!from)
    return NULL;

  for (fitem = from; *fitem; fitem++) ;
  to = malloc ((fitem - from + 1) * sizeof (char *));
  if (!to)
    return NULL;

  *n = fitem - from;
  fitem = from;
  titem = to;
  while (*fitem)
    *titem++ = strdup (*fitem++);
  *titem = NULL;
  return to;
#endif
}

static void str_array_free (char **a) {
  char **i;
  if (!a)
    return;
  for (i = a; *i; i++) ;
  if (a[0] != (char *)(i + 1)) {
    for (i = a; *i; i++)
      free (*i);
  }
  free (a);
}

/*
 * external interface
 */

static cfg_entry_t *config_lookup_entry(config_values_t *this, const char *key) {
  cfg_entry_t *entry;

  pthread_mutex_lock(&this->config_lock);
  entry = config_lookup_entry_int (this, key);
  pthread_mutex_unlock(&this->config_lock);

  return entry;
}

/* 1 private */
static cfg_entry_t *config_lookup_entry_safe (config_values_t *this, const char *key) {
  pthread_mutex_lock (&this->config_lock);
  if (this->lookup_entry == config_lookup_entry)
    return config_lookup_entry_int (this, key);
  return this->lookup_entry (this, key);
}

static void config_reset_value(cfg_entry_t *entry) {

  _x_freep (&entry->str_value);
  _x_freep (&entry->str_default);
  _x_freep (&entry->description);
  _x_freep (&entry->help);

  str_array_free (entry->enum_values);
  entry->enum_values = NULL;
  entry->num_value = 0;
}

static void config_shallow_copy(xine_cfg_entry_t *dest, const cfg_entry_t *src);

static cfg_entry_t *config_register_key (config_values_t *this,
					 const char *key,
					 int exp_level,
					 xine_config_cb_t changed_cb,
					 void *cb_data) {
  cfg_entry_t *entry;

  _x_assert(this);
  _x_assert(key);

  lprintf ("registering %s\n", key);

  entry = config_insert (this, key, exp_level);
  if (!entry)
    return NULL;

  /* new entry */
  entry->exp_level = exp_level != FIND_ONLY ? exp_level : 0;

  /* override callback */
  if (changed_cb) {
    if (entry->callback && (entry->callback != changed_cb)) {
      lprintf("overriding callback\n");
    }
    entry->callback = changed_cb;
    entry->callback_data = cb_data;
  }

  /* we created a new entry, call the callback */
  if (this->new_entry_cb) {
    xine_cfg_entry_t cb_entry;
    /* thread safe extension, private to _x_scan_plugins ()
     * (.cur is otherwise unused). */
    this->cur = entry;
    config_shallow_copy(&cb_entry, entry);
    this->new_entry_cb(this->new_entry_cbdata, &cb_entry);
    this->cur = NULL;
  }

  return entry;
}

static cfg_entry_t *config_register_string_internal (config_values_t *this,
						     const char *key,
						     const char *def_value,
						     int num_value,
						     const char *description,
						     const char *help,
						     int exp_level,
						     xine_config_cb_t changed_cb,
						     void *cb_data) {
  cfg_entry_t *entry;

  _x_assert(this);
  _x_assert(key);
  _x_assert(def_value);

  pthread_mutex_lock(&this->config_lock);

  entry = config_register_key(this, key, exp_level, changed_cb, cb_data);
  if (!entry) {
    pthread_mutex_unlock (&this->config_lock);
    return NULL;
  }

  if (entry->type != XINE_CONFIG_TYPE_UNKNOWN) {
    lprintf("config entry already registered: %s\n", key);
    pthread_mutex_unlock(&this->config_lock);
    return entry;
  }

  config_reset_value(entry);

  /* set string */
  entry->type = XINE_CONFIG_TYPE_STRING;

  if (entry->unknown_value) {
    entry->str_value = entry->unknown_value;
    entry->unknown_value = NULL;
  } else {
    entry->str_value = strdup(def_value);
  }

  entry->num_value = num_value;

  /* fill out rest of struct */
  entry->str_default    = strdup(def_value);
  entry->description    = (description) ? strdup(description) : NULL;
  entry->help           = (help) ? strdup(help) : NULL;

  pthread_mutex_unlock(&this->config_lock);
  return entry;
}

static char *config_register_string (config_values_t *this,
				     const char *key,
				     const char *def_value,
				     const char *description,
				     const char *help,
				     int exp_level,
				     xine_config_cb_t changed_cb,
				     void *cb_data) {
  return config_register_string_internal (this, key, def_value, 0, description,
					  help, exp_level, changed_cb, cb_data)->str_value;
}

static char *config_register_filename (config_values_t *this,
				       const char *key,
				       const char *def_value,
				       int req_type,
				       const char *description,
				       const char *help,
				       int exp_level,
				       xine_config_cb_t changed_cb,
				       void *cb_data) {
  return config_register_string_internal (this, key, def_value, req_type, description,
					  help, exp_level, changed_cb, cb_data)->str_value;
}

static int config_register_num (config_values_t *this,
				const char *key,
				int def_value,
				const char *description,
				const char *help,
				int exp_level,
				xine_config_cb_t changed_cb,
				void *cb_data) {

  cfg_entry_t *entry;
  _x_assert(this);
  _x_assert(key);

  pthread_mutex_lock(&this->config_lock);

  entry = config_register_key(this, key, exp_level, changed_cb, cb_data);
  if (!entry) {
    pthread_mutex_unlock (&this->config_lock);
    return 0;
  }

  if (entry->type != XINE_CONFIG_TYPE_UNKNOWN) {
    lprintf("config entry already registered: %s\n", key);
    pthread_mutex_unlock(&this->config_lock);
    return entry->num_value;
  }

  config_reset_value(entry);
  entry->type = XINE_CONFIG_TYPE_NUM;

  {
    const char *val = entry->unknown_value;
    entry->num_value = val ? xine_str2int32 (&val) : def_value;
  }

  /* fill out rest of struct */
  entry->num_default    = def_value;
  entry->description    = (description) ? strdup(description) : NULL;
  entry->help           = (help) ? strdup(help) : NULL;

  pthread_mutex_unlock(&this->config_lock);

  return entry->num_value;
}

static int config_register_bool (config_values_t *this,
				 const char *key,
				 int def_value,
				 const char *description,
				 const char *help,
				 int exp_level,
				 xine_config_cb_t changed_cb,
				 void *cb_data) {

  cfg_entry_t *entry;
  _x_assert(this);
  _x_assert(key);

  pthread_mutex_lock(&this->config_lock);

  entry = config_register_key(this, key, exp_level, changed_cb, cb_data);
  if (!entry) {
    pthread_mutex_unlock (&this->config_lock);
    return 0;
  }

  if (entry->type != XINE_CONFIG_TYPE_UNKNOWN) {
    lprintf("config entry already registered: %s\n", key);
    pthread_mutex_unlock(&this->config_lock);
    return entry->num_value;
  }

  config_reset_value(entry);
  entry->type = XINE_CONFIG_TYPE_BOOL;

  {
    const char *val = entry->unknown_value;
    entry->num_value = val ? xine_str2int32 (&val) : def_value;
  }

  /* fill out rest of struct */
  entry->num_default    = def_value;
  entry->description    = (description) ? strdup(description) : NULL;
  entry->help           = (help) ? strdup(help) : NULL;

  pthread_mutex_unlock(&this->config_lock);

  return entry->num_value;
}

static int config_register_range (config_values_t *this,
				  const char *key,
				  int def_value,
				  int min, int max,
				  const char *description,
				  const char *help,
				  int exp_level,
				  xine_config_cb_t changed_cb,
				  void *cb_data) {

  cfg_entry_t *entry;
  _x_assert(this);
  _x_assert(key);

  pthread_mutex_lock(&this->config_lock);

  entry = config_register_key(this, key, exp_level, changed_cb, cb_data);
  if (!entry) {
    pthread_mutex_unlock (&this->config_lock);
    return 0;
  }

  if (entry->type != XINE_CONFIG_TYPE_UNKNOWN) {
    lprintf("config entry already registered: %s\n", key);
    pthread_mutex_unlock(&this->config_lock);
    return entry->num_value;
  }

  config_reset_value(entry);
  entry->type = XINE_CONFIG_TYPE_RANGE;

  {
    const char *val = entry->unknown_value;
    entry->num_value = val ? xine_str2int32 (&val) : def_value;
  }

  /* validate value (config files can be edited ...) */
  /* Allow default to be out of range. xine-ui uses this for brightness etc. */
  if (entry->num_value != def_value) {
    if (entry->num_value > max) {
      printf ("configfile: WARNING: value %d for %s is larger than max (%d)\n",
        entry->num_value, key, max);
      entry->num_value = max;
    }
    if (entry->num_value < min) {
      printf ("configfile: WARNING: value %d for %s is smaller than min (%d)\n",
        entry->num_value, key, min);
      entry->num_value = min;
    }
  }

  /* fill out rest of struct */

  entry->num_default   = def_value;
  entry->range_min     = min;
  entry->range_max     = max;
  entry->description    = (description) ? strdup(description) : NULL;
  entry->help           = (help) ? strdup(help) : NULL;

  pthread_mutex_unlock(&this->config_lock);

  return entry->num_value;
}

static int config_parse_enum (const char *str, const char **values) {

  const char **value;
  int    i;


  value = values;
  i = 0;

  while (*value) {

    lprintf ("parse enum, >%s< ?= >%s<\n", *value, str);

    if (!strcmp (*value, str))
      return i;

    value++;
    i++;
  }

  lprintf ("warning, >%s< is not a valid enum here, using 0\n", str);

  return 0;
}

static int config_register_enum (config_values_t *this,
				 const char *key,
				 int def_value,
				 char **values,
				 const char *description,
				 const char *help,
				 int exp_level,
				 xine_config_cb_t changed_cb,
				 void *cb_data) {

  cfg_entry_t *entry;
  uint32_t value_count;


  _x_assert(this);
  _x_assert(key);
  _x_assert(values);

  pthread_mutex_lock(&this->config_lock);

  entry = config_register_key(this, key, exp_level, changed_cb, cb_data);
  if (!entry) {
    pthread_mutex_unlock (&this->config_lock);
    return 0;
  }

  if (entry->type != XINE_CONFIG_TYPE_UNKNOWN) {
    lprintf("config entry already registered: %s\n", key);
    pthread_mutex_unlock(&this->config_lock);
    return entry->num_value;
  }

  config_reset_value(entry);
  entry->type = XINE_CONFIG_TYPE_ENUM;

  if (entry->unknown_value)
    entry->num_value = config_parse_enum (entry->unknown_value, (const char **)values);
  else
    entry->num_value = def_value;

  /* fill out rest of struct */
  entry->num_default = def_value;

  /* allocate and copy the enum values */
  entry->enum_values = str_array_dup ((const char **)values, &value_count);

  entry->description   = (description) ? strdup(description) : NULL;
  entry->help          = (help) ? strdup(help) : NULL;

  entry->range_min = 0;
  entry->range_max = value_count;

  if (entry->num_value < 0)
    entry->num_value = 0;
  if (entry->num_value >= (int)value_count)
    entry->num_value = value_count;

  pthread_mutex_unlock(&this->config_lock);

  return entry->num_value;
}

static void config_shallow_copy(xine_cfg_entry_t *dest, const cfg_entry_t *src)
{
  dest->key           = src->key;
  dest->type          = src->type;
  dest->unknown_value = src->unknown_value;
  dest->str_value     = src->str_value;
  dest->str_default   = src->str_default;
  dest->num_value     = src->num_value;
  dest->num_default   = src->num_default;
  dest->range_min     = src->range_min;
  dest->range_max     = src->range_max;
  dest->enum_values   = src->enum_values;
  dest->description   = src->description;
  dest->help          = src->help;
  dest->exp_level     = src->exp_level;
  dest->callback      = src->callback;
  dest->callback_data = src->callback_data;
}

static void config_update_num_e (cfg_entry_t *entry, int value) {

  switch (entry->type) {
    case XINE_CONFIG_TYPE_RANGE:
      if (value != entry->num_default) {
        if (value < entry->range_min)
          value = entry->range_min;
        else if (value > entry->range_max)
          value = entry->range_max;
      }
      entry->num_value = value;
      break;
    case XINE_CONFIG_TYPE_BOOL:
      entry->num_value = value ? 1 : 0;
      break;
    case XINE_CONFIG_TYPE_ENUM:
      if (value != entry->num_default) {
        if (value < 0)
          value = 0;
        else if (value >= entry->range_max)
          value = entry->range_max - 1;
      }
      /* fall through */
    case XINE_CONFIG_TYPE_NUM:
      entry->num_value = value;
      break;
    default: ;
  }

  if (entry->callback) {
    xine_cfg_entry_t cb_entry;
    config_shallow_copy (&cb_entry, entry);
    /* it is safe to enter the callback from within a locked context
     * because we use a recursive mutex.
     */
    entry->callback (entry->callback_data, &cb_entry);
  }
}

static void config_update_num (config_values_t *this, const char *key, int value) {
  cfg_entry_t *entry;
  lprintf ("updating %s to %d\n", key, value);
  entry = config_lookup_entry_safe (this, key);
  if (!entry) {
    lprintf ("WARNING! tried to update unknown key %s (to %d)\n", key, value);
    pthread_mutex_unlock (&this->config_lock);
    return;
  }
  if ((entry->type == XINE_CONFIG_TYPE_UNKNOWN)
      || (entry->type == XINE_CONFIG_TYPE_STRING)) {
    printf ("configfile: error - tried to update non-num type %d (key %s, value %d)\n",
	    entry->type, entry->key, value);
    pthread_mutex_unlock (&this->config_lock);
    return;
  }
  config_update_num_e (entry, value);
  pthread_mutex_unlock (&this->config_lock);
}

/* Anything can be updated with a string. Config file load uses this. */
static void config_update_string_e (cfg_entry_t *entry, const char *value) {
  char *str_free = NULL;
  int min, max;

  switch (entry->type) {
    case XINE_CONFIG_TYPE_RANGE:
      min = entry->range_min;
      max = entry->range_max;
      goto set_snum;
    case XINE_CONFIG_TYPE_NUM:
      min = -2147483647 - 1;
      max = 2147483647;
      goto set_snum;
    case XINE_CONFIG_TYPE_BOOL:
      min = 0;
      max = 1;
    set_snum: {
      const char *val = value;
      int v = xine_str2int32 (&val);
      if (v != entry->num_default) {
        if (v < min)
          v = min;
        else if (v > max)
          v = max;
      }
      entry->num_value = v;
      break;
    }
    case XINE_CONFIG_TYPE_ENUM:
      entry->num_value = config_parse_enum (value, (const char **)entry->enum_values);
      break;
    case XINE_CONFIG_TYPE_STRING:
      /* Client may be using the string directly, not a private duplicate of it.
       * Idea #1: Let both the old and new strings be valid during the change callback
       * as sort of a safe switch protocol there. However, to be really safe, client
       * needs to lock the config object during any use of the string as well.
       * Even worse: no callback...
       * Idea #2: keep a full backlog of outdated strings.
       * Might be misused for flooding the heap.
       * Idea #3: with callback, park previous string at entry.unknown_value.
       * without, park initial string there. */
      if (value != entry->str_value) {
        if (entry->str_value) {
          if (entry->callback || !entry->unknown_value) {
            str_free = entry->unknown_value;
            entry->unknown_value = entry->str_value;
          } else {
            str_free = entry->str_value;
          }
        }
        entry->str_value = strdup (value);
      }
      break;
    default:
      if (value != entry->unknown_value) {
        str_free = entry->unknown_value;
        entry->unknown_value = strdup (value);
      }
  }

  if (entry->callback) {
    xine_cfg_entry_t cb_entry;
    config_shallow_copy (&cb_entry, entry);
    /* it is safe to enter the callback from within a locked context
     * because we use a recursive mutex.
     */
    entry->callback (entry->callback_data, &cb_entry);
  }
  free (str_free);
}

static void config_update_string (config_values_t *this, const char *key, const char *value) {
  cfg_entry_t *entry;
  lprintf ("updating %s to %s\n", key, value);
  entry = config_lookup_entry_safe (this, key);
  if (!entry) {
    printf ("configfile: error - tried to update unknown key %s (to %s)\n", key, value);
    pthread_mutex_unlock (&this->config_lock);
    return;
  }
  config_update_string_e (entry, value);
  pthread_mutex_unlock (&this->config_lock);
}

/*
 * front end config translation handling
 */
void xine_config_set_translation_user (const xine_config_entry_translation_t *xlate)
{
  config_entry_translation_user = xlate;
}

/*
 * load/save config data from/to afile (e.g. $HOME/.xine/config)
 */
void xine_config_load (xine_t *xine, const char *filename) {

  config_values_t *this = xine->config;

  lprintf ("reading from file '%s'\n", filename);

  do {
    FILE *f_config;
    int version;
    char *buf, *line, *nextline;

    f_config = fopen (filename, "rb");
    if (!f_config)
      break;
    {
      long int flen;
      fseek (f_config, 0, SEEK_END);
      flen = ftell (f_config);
      if (flen < 0) {
        fclose (f_config);
        break;
      }
      /* TJ. I got far less than 32k, so > 2M is probably insane. */
      if (flen > (2 << 20))
        flen = 2 << 20;
      buf = malloc (flen + 2);
      if (!buf) {
        fclose (f_config);
        break;
      }
      fseek (f_config, 0, SEEK_SET);
      flen = fread (buf, 1, flen, f_config);
      buf[flen] = '\n';
      buf[flen + 1] = 0;
    }
    fclose (f_config);

    pthread_mutex_lock (&this->config_lock);
    version = this->current_version;
    pthread_mutex_unlock (&this->config_lock);

    for (line = buf; line[0]; line = nextline) {
      char *value;
      /* make string from line */
      char *lend = strchr (line, '\n');
      if (!lend) /* should not happen */
        break;
      nextline = lend + 1;
      if ((lend > line) && (lend[-1] == '\r'))
        lend--;
      lend[0] = 0;

      /* skip comments */
      if (line[0] == '#')
        continue;

      if (line[0] == '.') {
        if (!strncmp (line, ".version:", 9)) {
          const char *val = line + 9;
          version = xine_str2int32 (&val);
          if (version > CONFIG_FILE_VERSION) {
            xine_log (xine, XINE_LOG_MSG,
              _("The current config file has been modified by a newer version of xine."));
          }
          pthread_mutex_lock (&this->config_lock);
          this->current_version = version;
          pthread_mutex_unlock (&this->config_lock);
          continue;
        }
      }
      
      value = strchr (line, ':');
      if (value) {
        cfg_entry_t *entry;

        *value++ = 0;

        pthread_mutex_lock (&this->config_lock);

        if (version < CONFIG_FILE_VERSION) {
          /* old config file -> let's see if we have to rename this one */
          entry = config_insert (this, line, FIND_ONLY);
          if (!entry) {
            char *tmp = NULL;
            const char *key = config_translate_key (line, &tmp);
            if (!key)
              key = line; /* no translation? fall back on untranslated key */
            entry = config_insert (this, key, 50);
            free (tmp);
          }
        } else {
          entry = config_insert (this, line, 50);
        }

        if (entry)
          config_update_string_e (entry, value);

        pthread_mutex_unlock (&this->config_lock);
      }
    }
    free (buf);
    xine_log (xine, XINE_LOG_MSG,
      _("Loaded configuration from file '%s'\n"), filename);
    return;
  } while (0);

  if (errno != ENOENT)
    xine_log (xine, XINE_LOG_MSG,
      _("Failed to load configuration from file '%s': %s\n"), filename, strerror (errno));
}

void xine_config_save (xine_t *xine, const char *filename) {

  config_values_t *this = xine->config;
  char             temp[XINE_PATH_MAX];
  int              backup = 0;
  struct stat      backup_stat, config_stat;
  FILE            *f_config, *f_backup;

  snprintf(temp, XINE_PATH_MAX, "%s~", filename);
  unlink (temp);

  if (stat(temp, &backup_stat) != 0) {

    lprintf("backing up configfile to %s\n", temp);

    f_backup = fopen(temp, "wb");
    f_config = fopen(filename, "rb");

    if (f_config && f_backup && (stat(filename, &config_stat) == 0)) {
      char    *buf = NULL;
      size_t   rlen;

      buf = (char *) malloc(config_stat.st_size + 1);
      if((rlen = fread(buf, 1, config_stat.st_size, f_config)) && ((off_t)rlen == config_stat.st_size)) {
	if (rlen != fwrite(buf, 1, rlen, f_backup)) {
	  lprintf("backing up configfile to %s failed\n", temp);
	}
      }
      free(buf);

      fclose(f_config);
      fclose(f_backup);

      if (stat(temp, &backup_stat) == 0 && config_stat.st_size == backup_stat.st_size)
	backup = 1;
      else
	unlink(temp);

    }
    else {

      if (f_config)
        fclose(f_config);
      else
	backup = 1;

      if (f_backup)
        fclose(f_backup);

    }
  }

  if (!backup && (stat(filename, &config_stat) == 0)) {
    xprintf(xine, XINE_VERBOSITY_LOG, _("configfile: WARNING: backing up configfile to %s failed\n"), temp);
    xprintf(xine, XINE_VERBOSITY_LOG, _("configfile: WARNING: your configuration will not be saved\n"));
    return;
  }

  lprintf ("writing config file to %s\n", filename);

  f_config = fopen (filename, "wb");

  if (f_config) {

#define XCS_BUF_SIZE 4096
    char buf[XCS_BUF_SIZE], *q, *e = buf + XCS_BUF_SIZE - 28 - 4 * XINE_MAX_INT32_STR;
    cfg_entry_t *entry;

    q = buf;
    memcpy (q, "#\n# xine config file\n#\n.version:", 32); q += 32;
    xine_uint32_2str (&q, CONFIG_FILE_VERSION);
    memcpy (q,
      "\n"
      "\n"
      "# Entries which are still set to their default values are commented out.\n"
      "# Remove the \'#\' at the beginning of the line, if you want to change them.\n"
      "\n",
      151); q += 151;
    fwrite (buf, 1, q - buf, f_config);

    pthread_mutex_lock(&this->config_lock);

    for (entry = this->first; entry; entry = entry->next) {

      if (!entry->key[0]) /* deleted key */
        continue;

      lprintf ("saving key '%s'\n", entry->key);

      q = buf;
      if (entry->description) {
        memcpy (q, "# ", 2); q += 2;
        q += strlcpy (q, entry->description, e - q);
        if (q >= e)
          q = e - 1;
        *q++ = '\n';
      }

      switch (entry->type) {

      case XINE_CONFIG_TYPE_UNKNOWN: {
#if 0
        /* discard unclaimed values.
         * better dont do that because
         * a) multiple frontends may share the same config file, and
         * b) we dont always load all plugins for performance. */
#else
        q += strlcpy (q, entry->key, e - q);
        if (q >= e)
          q = e - 1;
        *q++ = ':';
        q += strlcpy (q, entry->unknown_value, e - q);
        if (q >= e)
          q = e - 1;
        memcpy (q, "\n\n", 2); q += 2;
#endif
	break;
      }

      case XINE_CONFIG_TYPE_RANGE:
        memcpy (q, "# [ ", 4); q += 3;
        xine_int32_2str (&q, entry->range_min);
        memcpy (q, "..", 2); q += 2;
        xine_int32_2str (&q, entry->range_max);
        *q++ = ']';
      tail_num:
        memcpy (q, ", default:  ", 12); q += 11;
        xine_int32_2str (&q, entry->num_default);
        *q++ = '\n';
        if (entry->num_value == entry->num_default)
          *q++ = '#';
        q += strlcpy (q, entry->key, e - q);
        if (q >= e)
          q = e - 1;
        *q++ = ':';
        xine_int32_2str (&q, entry->num_value);
        memcpy (q, "\n\n", 2); q += 2;
	break;

      case XINE_CONFIG_TYPE_STRING:
        memcpy (q, "# string, default:  ", 20); q += 19;
        q += strlcpy (q, entry->str_default, e - q);
        if (q >= e)
          q = e - 1;
        *q++ = '\n';
        if (!strcmp (entry->str_value, entry->str_default))
          *q++ = '#';
        q += strlcpy (q, entry->key, e - q);
        if (q >= e)
          q = e - 1;
        *q++ = ':';
        q += strlcpy (q, entry->str_value, e - q);
        if (q >= e)
          q = e - 1;
        memcpy (q, "\n\n", 2); q += 2;
	break;

      case XINE_CONFIG_TYPE_ENUM:
        do {
          char **value;
          memcpy (q, "# { ", 4); q += 3;
          value = entry->enum_values;
          while (*value) {
            *q++ = ' ';
            q += strlcpy (q, *value, e - q);
            if (q >= e) {
              q = e - 1;
              break;
            }
            *q++ = ' ';
            value++;
          }
          memcpy (q, "}, default: ", 12); q += 12;
          xine_uint32_2str (&q, entry->num_default);
          *q++ = '\n';
          if ((entry->num_value >= 0) &&
              (entry->num_value < entry->range_max) &&
              entry->enum_values[entry->num_value]) {
            if (entry->num_value == entry->num_default)
              *q++ = '#';
            q += strlcpy (q, entry->key, e - q);
            if (q >= e)
              q = e - 1;
            *q++ = ':';
            q += strlcpy (q, entry->enum_values[entry->num_value], e - q);
            if (q >= e)
              q = e - 1;
            *q++ = '\n';
          }
        } while (0);
        *q++ = '\n';
	break;

      case XINE_CONFIG_TYPE_NUM:
        memcpy (q, "# numeric", 9); q += 9;
        goto tail_num;

      case XINE_CONFIG_TYPE_BOOL:
        memcpy (q, "# bool", 6); q += 6;
        goto tail_num;
      }
      fwrite (buf, 1, q - buf, f_config);

    }
    pthread_mutex_unlock(&this->config_lock);

    if (fclose(f_config) != 0) {
      xprintf(xine, XINE_VERBOSITY_LOG, _("configfile: WARNING: writing configuration to %s failed\n"), filename);
      xprintf(xine, XINE_VERBOSITY_LOG, _("configfile: WARNING: removing possibly broken config file %s\n"), filename);
      xprintf(xine, XINE_VERBOSITY_LOG, _("configfile: WARNING: you should check the backup file %s\n"), temp);
      /* writing config failed -> remove file, it might be broken ... */
      unlink(filename);
      /* ... but keep the backup */
      backup = 0;
    }
  }

  if (backup)
    unlink(temp);
}

static void config_dispose (config_values_t *this) {
  cfg_entry_t *entry, *last;

  pthread_mutex_lock (&this->config_lock);
  entry = this->first;

  lprintf ("dispose\n");

  while (entry) {
    last = entry;
    entry = entry->next;

    _x_freep (&last->key);
    _x_freep (&last->unknown_value);

    config_reset_value(last);

    free (last);
  }

  pthread_mutex_unlock (&this->config_lock);

  pthread_mutex_destroy (&this->config_lock);
  free (this);
}


static void config_unregister_cb (config_values_t *this, const char *key) {

  cfg_entry_t *entry;

  _x_assert(key);
  _x_assert(this);

  entry = config_lookup_entry_safe (this, key);
  if (entry) {
    entry->callback = NULL;
    entry->callback_data = NULL;
  }
  pthread_mutex_unlock (&this->config_lock);
}

void _x_config_unregister_cb_class_d (config_values_t *this, void *callback_data) {

  cfg_entry_t *entry;

  _x_assert(this);
  _x_assert(callback_data);

  pthread_mutex_lock(&this->config_lock);

  entry = this->first;
  while (entry) {
    if (entry->callback && entry->callback_data == callback_data) {
      entry->callback = NULL;
      entry->callback_data = NULL;
    }
    entry = entry->next;
  }

  pthread_mutex_unlock(&this->config_lock);
}

void _x_config_unregister_cb_class_p (config_values_t *this, xine_config_cb_t callback) {

  cfg_entry_t *entry;

  _x_assert(this);
  _x_assert(callback);

  pthread_mutex_lock (&this->config_lock);

  entry = this->first;
  while (entry) {
    if (entry->callback && entry->callback == callback) {
      entry->callback = NULL;
      entry->callback_data = NULL;
    }
    entry = entry->next;
  }

  pthread_mutex_unlock (&this->config_lock);
}

static void config_set_new_entry_callback (config_values_t *this, xine_config_cb_t new_entry_cb, void* cbdata) {
  pthread_mutex_lock(&this->config_lock);
  this->new_entry_cb = new_entry_cb;
  this->new_entry_cbdata = cbdata;
  pthread_mutex_unlock(&this->config_lock);
}

static void config_unset_new_entry_callback (config_values_t *this) {
  pthread_mutex_lock(&this->config_lock);
  this->new_entry_cb = NULL;
  this->new_entry_cbdata = NULL;
  pthread_mutex_unlock(&this->config_lock);
}

static void put_int (uint8_t **dest, int value) {
  int32_t value_int32 = (int32_t)value;
#if (defined(__GNUC__) && (__GNUC__ >= 4)) || defined(__clang__)
#  ifdef WORDS_BIGENDIAN
  value_int32 = __builtin_bswap32 (value_int32);
#  endif
  __builtin_memcpy (*dest, &value_int32, 4);
#else
  (*dest)[0] = value_int32 & 0xFF;
  (*dest)[1] = (value_int32 >> 8) & 0xFF;
  (*dest)[2] = (value_int32 >> 16) & 0xFF;
  (*dest)[3] = (value_int32 >> 24) & 0xFF;
#endif
  *dest += 4;
}

static void put_string (uint8_t **dest, const char *value, uint32_t value_len) {
  put_int (dest, value_len);
  memcpy (*dest, value, value_len);
  *dest += value_len;
}

static char* config_get_serialized_entry (config_values_t *this, const char *key) {
  cfg_entry_t *entry;

  /* thread safe extension, private to _x_scan_plugins ()
   * (.cur is otherwise unused). */
  pthread_mutex_lock (&this->config_lock);
  if (!key) {
    entry = this->cur;
    this->cur = NULL;
  } else if (this->lookup_entry == config_lookup_entry) {
    entry = config_lookup_entry_int (this, key);
  } else {
    entry = this->lookup_entry (this, key);
  }
  if (!entry) {
    pthread_mutex_unlock (&this->config_lock);
    return NULL;
  }

  {
    /* now serialize this stuff
      fields to serialize :
          int              type;
          int              range_min;
          int              range_max;
          int              exp_level;
          int              num_default;
          int              num_value;
          char            *key;
          char            *str_default;
          char            *description;
          char            *help;
          char           **enum_values;
    */

    uint8_t *buf1, *buf2, *q;

    uint32_t total_len, buf_len;
    uint32_t key_len         = entry->key         ? strlen (entry->key) : 0;
    uint32_t str_default_len = entry->str_default ? strlen (entry->str_default) : 0;
    uint32_t description_len = entry->description ? strlen (entry->description) : 0;
    uint32_t help_len        = entry->help        ? strlen (entry->help) : 0;
    uint32_t value_len[256];
    uint32_t value_count, i;
    char   **cur_value;

    /* 6 integers: value (4 bytes)
     * 4 strings:  len (4 bytes) + string (len bytes)
     *   enums:    count (4 bytes) + count * (len (4 bytes) + string (len bytes)) */
    total_len = 11 * 4 + key_len + str_default_len + description_len + help_len;

    value_count = 0;
    cur_value = entry->enum_values;
    if (cur_value) {
      while (*cur_value && (value_count < (sizeof (value_len) / sizeof (value_len[0])))) {
        value_len[value_count] = strlen (*cur_value);
        total_len += 4 + value_len[value_count];
        value_count++;
        cur_value++;
      }
    }

    /* Now we have the length needed to serialize the entry and the length of each string */
    buf_len = (total_len * 4 + 2) / 3 + 8;
    buf2 = malloc (buf_len);
    if (!buf2) {
      pthread_mutex_unlock(&this->config_lock);
      return NULL;
    }
    buf1 = buf2 + (buf_len - total_len - 4);

    /* Let's go */
    q = buf1;

    /* the integers */
    put_int (&q, entry->type);
    put_int (&q, entry->range_min);
    put_int (&q, entry->range_max);
    put_int (&q, entry->exp_level);
    put_int (&q, entry->num_default);
    put_int (&q, entry->num_value);

    /* the strings */
    put_string (&q, entry->key, key_len);
    put_string (&q, entry->str_default, str_default_len);
    put_string (&q, entry->description, description_len);
    put_string (&q, entry->help, help_len);

    /* the enum stuff */
    put_int (&q, value_count);
    cur_value = entry->enum_values;
    for (i = 0; i < value_count; i++) {
      put_string (&q, *cur_value, value_len[i]);
      cur_value++;
    }
    pthread_mutex_unlock (&this->config_lock);

    /* and now the output encoding */
    /* We're going to encode total_len bytes in base64 */
    xine_base64_encode (buf1, (char *)buf2, total_len);
    return (char *)buf2;
  }
}

static char* config_register_serialized_entry (config_values_t *this, const char *value) {
  /*
      fields serialized :
          int              type;
          int              range_min;
          int              range_max;
          int              exp_level;
          int              num_default;
          int              num_value;
          char            *key;
          char            *str_default;
          char            *description;
          char            *help;
          char           **enum_values;
  */

  uint8_t *output;

  if (!value)
    return NULL;
  output = malloc ((strlen (value) * 3 + 3) / 4 + 1);

  if (output) do {
    char    *enum_values[257];
    int      type, range_min, range_max, exp_level, num_default, num_value;
    char    *key, *str_default, *description, *help;
    uint8_t *p;
    uint32_t left, value_count, i;

    left = xine_base64_decode (value, output);
    /* we need at least 7 ints and 4 string lengths */
    if (left < 11 * 4)
      break;
    left -= 11 * 4;

    p = output;
    type        = (int32_t)_X_LE_32 (p); p += 4;
    range_min   = (int32_t)_X_LE_32 (p); p += 4;
    range_max   = (int32_t)_X_LE_32 (p); p += 4;
    exp_level   = (int32_t)_X_LE_32 (p); p += 4;
    num_default = (int32_t)_X_LE_32 (p); p += 4;
    num_value   = (int32_t)_X_LE_32 (p); p += 4;

#define get_string(s) { \
  uint32_t len = _X_LE_32 (p); p[0] = 0; p += 4; \
  if (len > left) \
    break; \
  left -= len; \
  s = (char *)p; p += len; \
}

    get_string (key);
    get_string (str_default);
    get_string (description);
    get_string (help);

    value_count = _X_LE_32 (p); p[0] = 0; p += 4;
    if (value_count > 256)
      break;
    if (left < value_count * 4)
      break;
    left -= value_count * 4;
    for (i = 0; i < value_count; i++) {
      get_string (enum_values[i]);
    }
    if (i < value_count)
      break;
    /* yes we have that byte. */
    p[0] = 0;
    enum_values[value_count] = NULL;

#undef get_string

    if (exp_level == FIND_ONLY)
      exp_level = 0;
#ifdef LOG
    printf ("config entry deserialization:\n");
    printf ("  key        : %s\n", key);
    printf ("  type       : %d\n", type);
    printf ("  exp_level  : %d\n", exp_level);
    printf ("  num_default: %d\n", num_default);
    printf ("  num_value  : %d\n", num_value);
    printf ("  str_default: %s\n", str_default);
    printf ("  range_min  : %d\n", range_min);
    printf ("  range_max  : %d\n", range_max);
    printf ("  description: %s\n", description);
    printf ("  help       : %s\n", help);
    printf ("  enum       : %d values\n", value_count);
    for (i = 0; i < value_count; i++)
      printf ("    enum[%2d]: %s\n", i, enum_values[i]);
    printf ("\n");
#endif

    switch (type) {
      case XINE_CONFIG_TYPE_STRING:
        switch (num_value) {
          case 0:
            this->register_string (this, key, str_default, description, help, exp_level, NULL, NULL);
            break;
          default:
            this->register_filename (this, key, str_default, num_value, description, help, exp_level, NULL, NULL);
            break;
        }
        break;
      case XINE_CONFIG_TYPE_RANGE:
        this->register_range (this, key, num_default, range_min, range_max, description, help, exp_level, NULL, NULL);
        break;
      case XINE_CONFIG_TYPE_ENUM:
        this->register_enum (this, key, num_default, enum_values, description, help, exp_level, NULL, NULL);
        break;
      case XINE_CONFIG_TYPE_NUM:
        this->register_num (this, key, num_default, description, help, exp_level, NULL, NULL);
        break;
      case XINE_CONFIG_TYPE_BOOL:
        this->register_bool (this, key, num_default, description, help, exp_level, NULL, NULL);
        break;
      default: ;
    }

    key = strdup (key);
    free (output);
    return key;
  } while (0);

  /* cleanup */
  free (output);
  return NULL;
}

config_values_t *_x_config_init (void) {

#ifdef HAVE_IRIXAL
  volatile /* is this a (old, 2.91.66) irix gcc bug?!? */
#endif
  config_values_t *this;
  pthread_mutexattr_t attr;

  this = calloc (1, sizeof (*this));
  if (!this) {
    fprintf (stderr, "configfile: could not allocate config object\n");
    return NULL;
  }

#ifndef HAVE_ZERO_SAFE_MEM
  this->first           = NULL;
  this->last            = NULL;
  this->current_version = 0;
#endif

  /* warning: config_lock is a recursive mutex. it must NOT be
   * used with neither pthread_cond_wait() or pthread_cond_timedwait()
   */
  pthread_mutexattr_init(&attr);
  pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_RECURSIVE);
  pthread_mutex_init(&this->config_lock, &attr);
  pthread_mutexattr_destroy(&attr);

  this->register_string           = config_register_string;
  this->register_filename         = config_register_filename;
  this->register_range            = config_register_range;
  this->register_enum             = config_register_enum;
  this->register_num              = config_register_num;
  this->register_bool             = config_register_bool;
  this->register_serialized_entry = config_register_serialized_entry;
  this->update_num                = config_update_num;
  this->update_string             = config_update_string;
  this->parse_enum                = config_parse_enum;
  this->lookup_entry              = config_lookup_entry;
  this->unregister_callback       = config_unregister_cb;
  this->dispose                   = config_dispose;
  this->set_new_entry_callback    = config_set_new_entry_callback;
  this->unset_new_entry_callback  = config_unset_new_entry_callback;
  this->get_serialized_entry      = config_get_serialized_entry;

  return this;
}

int _x_config_change_opt(config_values_t *config, const char *opt) {
  cfg_entry_t *entry;
  int          handled = 0;
  char        *key, *value;

  /* If the configuration is missing, return now rather than trying
   * to dereference it and then check it. */
  if ( ! config || ! opt ) return -1;

  if ((entry = config->lookup_entry(config, "misc.implicit_config")) &&
      entry->type == XINE_CONFIG_TYPE_BOOL) {
    if (!entry->num_value)
      /* changing config entries implicitly is denied */
      return -1;
  } else
    /* someone messed with the config entry */
    return -1;

  if ( *opt == '\0' ) return 0;

  key = strdup(opt);
  if ( !key ) return 0;

  value = strrchr(key, ':');
  if ( !value || *value == '\0' ) {
    free(key);
    return 0;
  }

  *value++ = '\0';

  handled = -1;
  entry = config_lookup_entry_safe (config, key);
  if (entry) {
    if (entry->exp_level >= XINE_CONFIG_SECURITY) {
      printf (_("configfile: entry '%s' mustn't be modified from MRL\n"), key);
    } else {
      switch (entry->type) {
        case XINE_CONFIG_TYPE_STRING:
        case XINE_CONFIG_TYPE_RANGE:
        case XINE_CONFIG_TYPE_ENUM:
        case XINE_CONFIG_TYPE_NUM:
        case XINE_CONFIG_TYPE_BOOL:
        case XINE_CONFIG_TYPE_UNKNOWN:
          config_update_string_e (entry, value);
          handled = 1;
          break;
        default: ;
      }
    }
  }
  pthread_mutex_unlock (&config->config_lock);
  free(key);
  return handled;
}
