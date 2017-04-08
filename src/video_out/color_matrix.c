/*
 * Copyright (C) 2012-2017 the xine project
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
 */


/*
  TJ. the output color matrix selection feature.
  Example use:
*/
#if 0
  #define CM_LUT /* recommended optimization */

  typedef struct {
    ...
    int     cm_state;
  #ifdef CM_LUT
    uint8_t cm_lut[32];
  #endif
    ...
  } xxxx_driver_t;

  #define CM_HAVE_YCGCO_SUPPORT /* if you already handle that */
  #define CM_DRIVER_T xxxx_driver_t
  #include "color_matrix.c"
#endif
/*
  cm_from_frame () returns current (color_matrix << 1) | color_range control value.
  Having only 1 var simplifies change event handling and avoids unecessary vo
  reconfiguration. In the libyuv2rgb case, they are even handled by same code.

  In theory, HD video uses a different YUV->RGB matrix than the rest.
  It shall come closer to the human eye's brightness feel, and give
  more shades of green even without higher bit depth.

  I discussed this topic with local TV engineers earlier.
  They say their studio equipment throws around uncompressed YUV with no
  extra info attached to it. Anything smaller than 720p is assumed to be
  ITU-R 601, otherwise ITU-R 709. A rematrix filter applies whenever
  video is scaled across the above mentioned HD threshold.

  However, the weak point of their argumentation is potentially non-standard
  input material. Those machines obviously dont verify input data, and
  ocasionally they dont even respect stream info (tested by comparing TV
  against retail DVD version of same movie).

  Consumer TV sets handle this fairly inconsistent - stream info, video size,
  hard-wired matrix, user choice or so-called intelligent picture enhancers
  that effectively go way off standards.
  So I decided to provide functionality, and let the user decide if and how
  to actually use it.
*/

/* eveybody gets these */

/* user configuration settings */
#define CM_CONFIG_NAME   "video.output.color_matrix"
#define CM_CONFIG_SIGNAL 0
#define CM_CONFIG_SIZE   1
#define CM_CONFIG_SD     2
#define CM_CONFIG_HD     3

#define CR_CONFIG_NAME   "video.output.color_range"
#define CR_CONFIG_AUTO   0
#define CR_CONFIG_MPEG   1
#define CR_CONFIG_FULL   2

static const char * const cm_names[] = {
  "RGB",
  "RGB",
  "ITU-R 709 / HDTV",
  "full range ITU-R 709 / HDTV",
  "undefined",
  "full range, undefined",
  "ITU-R 470 BG / SDTV",
  "full range ITU-R 470 BG / SDTV",
  "FCC",
  "full range FCC",
  "ITU-R 470 BG / SDTV",
  "full range ITU-R 470 BG / SDTV",
  "SMPTE 170M",
  "full range SMPTE 170M",
  "SMPTE 240M",
  "full range SMPTE 240M"
#ifdef CM_HAVE_YCGCO_SUPPORT
  ,
  "YCgCo",
  "YCgCo", /* this is always fullrange */
  "#9",
  "fullrange #9",
  "#10",
  "fullrange #10",
  "#11",
  "fullrange #11",
  "#12",
  "fullrange #12",
  "#13",
  "fullrange #13",
  "#14",
  "fullrange #14",
  "#15",
  "fullrange #15"
#endif
};

#ifdef CM_DRIVER_T

/* this is for vo plugins only */

/* the option names */
static const char * const cm_conf_labels[] = {
  "Signal", "Signal+Size", "SD", "HD", NULL
};

static const char * const cr_conf_labels[] = {
  "Auto", "MPEG", "FULL", NULL
};

#ifdef CM_HAVE_YCGCO_SUPPORT
#  define CM_G 16
#else
#  define CM_G 10
#endif

static
#ifdef CM_LUT
const
#endif
uint8_t cm_m[] = {
  10, 2,10, 6, 8,10,12,14,CM_G,10,10,10,10,10,10,10, /* SIGNAL */
  10, 2, 0, 6, 8,10,12,14,CM_G,10,10,10,10,10,10,10, /* SIZE */
  10,10,10,10,10,10,10,10,CM_G,10,10,10,10,10,10,10, /* SD */
  10, 2, 2, 2, 2, 2, 2, 2,CM_G, 2, 2, 2, 2, 2, 2, 2  /* HD */
};

static void cm_lut_setup (CM_DRIVER_T *this) {
#ifdef CM_LUT
  {
    const uint8_t *a = cm_m + ((this->cm_state >> 2) << 4);
    uint8_t *d = this->cm_lut, *e = d + 32;
    while (d < e) {
      d[0] = d[1] = *a++;
      d += 2;
    }
  }
  if ((this->cm_state & 3) == CR_CONFIG_AUTO) {
    /* keep range */
    int i;
    for (i = 1; i < 32; i += 2)
      this->cm_lut[i] |= 1;
  } else if ((this->cm_state & 3) == CR_CONFIG_FULL) {
    /* force full range */
    int i;
    for (i = 0; i < 32; i += 1)
      this->cm_lut[i] |= 1;
  }
#endif
}

/* callback when user changes them */
static void cm_cb_config (void *this_gen, xine_cfg_entry_t *entry) {
  CM_DRIVER_T *this = (CM_DRIVER_T *)this_gen;
  this->cm_state = (this->cm_state & 3) | (entry->num_value << 2);
  cm_lut_setup (this);
}

static void cr_cb_config (void *this_gen, xine_cfg_entry_t *entry) {
  CM_DRIVER_T *this = (CM_DRIVER_T *)this_gen;
  this->cm_state = (this->cm_state & 0x1c) | entry->num_value;
  cm_lut_setup (this);
}

static void cm_init (CM_DRIVER_T *this) {
  /* register configuration */
  this->cm_state = this->xine->config->register_enum (
    this->xine->config,
    CM_CONFIG_NAME,
    CM_CONFIG_SIZE,
    (char **)cm_conf_labels,
    _("Output color matrix"),
    _("Tell how output colors should be calculated.\n\n"
      "Signal: Do as current stream suggests.\n"
      "        This may be wrong sometimes.\n\n"
      "Signal+Size: Same as above,\n"
      "        but assume HD color for unmarked HD streams.\n\n"
      "SD:     Force SD video standard ITU-R 470/601.\n"
      "        Try this if you get too little green.\n\n"
      "HD:     Force HD video standard ITU-R 709.\n"
      "        Try when there is too much green coming out.\n\n"),
    10,
    cm_cb_config,
    this
  ) << 2;
  this->cm_state |= this->xine->config->register_enum (
    this->xine->config,
    CR_CONFIG_NAME,
    CR_CONFIG_AUTO,
    (char **)cr_conf_labels,
    _("Output color range"),
    _("Tell how output colors should be ranged.\n\n"
      "Auto: Do as current stream suggests.\n"
      "      This may be wrong sometimes.\n\n"
      "MPEG: Force MPEG color range (16..235) / studio swing / video mode.\n"
      "      Try if image looks dull (no real black or white in it).\n\n"
      "FULL: Force FULL color range (0..255) / full swing / PC mode.\n"
      "      Try when flat black and white spots appear.\n\n"),
    10,
    cr_cb_config,
    this
  );
  cm_lut_setup (this);
}

static int cm_from_frame (vo_frame_t *frame) {
  CM_DRIVER_T *this = (CM_DRIVER_T *)frame->driver;
  int cm = VO_GET_FLAGS_CM (frame->flags);
#ifdef CM_LUT
  cm = this->cm_lut[cm & 31];
  if (cm & ~1)
    return cm;
  return cm | ((frame->height - frame->crop_top - frame->crop_bottom >= 720) ||
               (frame->width - frame->crop_left - frame->crop_right >= 1280) ? 2 : 10);
#else
  static uint8_t cm_r[] = {0, 0, 1, 0}; /* AUTO, MPEG, FULL, safety */
  int cf = this->cm_state;
  cm_m[18] = (frame->height - frame->crop_top - frame->crop_bottom >= 720) ||
             (frame->width - frame->crop_left - frame->crop_right >= 1280) ? 2 : 10;
  cm_r[0] = cm & 1;
  return cm_m[((cf >> 2) << 4) | (cm >> 1)] | cm_r[cf & 3];
#endif
}

static void cm_close (CM_DRIVER_T *this) {
  /* dont know whether this is really necessary */
  this->xine->config->unregister_callback (this->xine->config, CR_CONFIG_NAME);
  this->xine->config->unregister_callback (this->xine->config, CM_CONFIG_NAME);
}

#endif /* defined CM_DRIVER_T */

