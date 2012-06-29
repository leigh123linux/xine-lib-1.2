/*
 * Copyright (C) 2012 the xine project
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

  This file must be included after declaration of xxxx_driver_t,
  and #define'ing CM_DRIVER_T to it.
  That struct must contain the integer values cm_state and cm_new.

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

  BTW. VDPAU already has its own matrix switch based on stream info.
  Rumour has it that proprietory ATI drivers auto switch their xv ports
  based on video size. Both not user configurable, and both not tested...
*/

#ifdef CM_DRIVER_T

/* cm_update () modes */
#define CM_UPDATE_INIT   0
#define CM_UPDATE_CONFIG 1
#define CM_UPDATE_STREAM 2
#define CM_UPDATE_HEIGHT 3
#define CM_UPDATE_CLOSE  4

/* user configuration settings */
#define CM_CONFIG_SIGNAL 0
#define CM_CONFIG_SIZE   1
#define CM_CONFIG_SD     2
#define CM_CONFIG_HD     3

static void cm_update (CM_DRIVER_T *this, int mode, int value);

/* the names thereof */
static const char * const cm_conf_labels[] = {
  "Signal", "Signal+Size", "SD", "HD", NULL
};

#endif /* defined CM_DRIVER_T */

static const char * const cm_names[] = {
  "ITU-R 470 BG / SDTV",
  "ITU-R 709 / HDTV",
  "ITU-R 470 BG / SDTV",
  "ITU-R 470 BG / SDTV",
  "FCC",
  "ITU-R 470 BG / SDTV",
  "SMPTE 170M",
  "SMPTE 240M"
};

#ifdef CM_DRIVER_T

/* callback when user changes them */
static void cm_cb_config (void *this_gen, xine_cfg_entry_t *entry) {
  CM_DRIVER_T *this = (CM_DRIVER_T *)this_gen;
  cm_update (this, CM_UPDATE_CONFIG, entry->num_value);
}

/* handle generic changes */
static void cm_update (CM_DRIVER_T *this, int mode, int value) {
  /* get state */
  int conf = this->cm_state & 7;
  int stream = (this->cm_state >> 3) & 7;
  int hd = (this->cm_state >> 6) & 1;
  /* update it */
  switch (mode) {
    case CM_UPDATE_INIT:
      /* register configuration */
      conf = this->xine->config->register_enum (
        this->xine->config,
        "video.output.color_matrix",
        CM_CONFIG_SD,
        (char **)cm_conf_labels,
        _("Output color matrix to use"),
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
      );
      /* lock to legacy matrix until someone sends CM_UPDATE_STREAM.
         This way still images and audio visualisation stay correct */
      stream = 0;
      hd = 0;
    break;
    case CM_UPDATE_CONFIG:
      conf = value;
    break;
    case CM_UPDATE_STREAM:
      stream = value;
      if ((stream < 1) || (stream > 7)) stream = 2; /* undefined */
    break;
    case CM_UPDATE_HEIGHT:
      hd = value >= 720 ? 1 : 0;
    break;
    case CM_UPDATE_CLOSE:
      /* dont know whether this is really necessary */
      this->xine->config->unregister_callback (this->xine->config,
        "video.output.color_matrix");
    break;
  }
  /* check user configuration */
  switch (conf) {
    case CM_CONFIG_SIZE:
      if (stream == 2) {
        /* no stream info, decide based on video size */
        this->yuv2rgb_cm = hd ? 1 : 2;
        break;
      }
      /* fall through */
    case CM_CONFIG_SIGNAL:
      /* follow stream info */
      this->yuv2rgb_cm = stream ? stream : 2;
    break;
    case CM_CONFIG_SD:
      this->yuv2rgb_cm = 5; /* ITU-R 601/SDTV */
    break;
    case CM_CONFIG_HD:
      this->yuv2rgb_cm = 1; /* ITU-R 709/HDTV */
    break;
  }
  /* save state */
  this->cm_state = (hd << 6) | (stream << 3) | conf;
}

#endif /* defined CM_DRIVER_T */
