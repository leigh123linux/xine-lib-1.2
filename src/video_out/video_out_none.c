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
 *
 * Was originally part of toxine frontend.
 * ...but has now been adapted to xine coding style standards ;)
 * ......what changes, impressive!
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>

#include "xine.h"

#include <xine/video_out.h>
#include <xine/xine_internal.h>
#include <xine/xineutils.h>
#include <xine/vo_scale.h>

typedef struct {
  vo_frame_t           vo_frame;
  int                  width;
  int                  height;
  double               ratio;
  int                  format;
  xine_t               *xine;
} vo_none_frame_t;

typedef struct {
  vo_driver_t          vo_driver;
  config_values_t     *config;
  int                  ratio;
  xine_t               *xine;
} vo_none_driver_t;

typedef struct {
  video_driver_class_t  driver_class;
  config_values_t      *config;
  xine_t               *xine;
} vo_none_class_t;


static void vo_none_free_framedata(vo_none_frame_t* frame) {
  if(frame->vo_frame.base[0]) {
    xine_free_aligned(frame->vo_frame.base[0]);
    frame->vo_frame.base[0] = NULL;
    frame->vo_frame.base[1] = NULL;
    frame->vo_frame.base[2] = NULL;
  }
}

static void vo_none_frame_dispose(vo_frame_t *vo_frame) {
  vo_none_frame_t *frame = (vo_none_frame_t *)vo_frame;
  vo_none_free_framedata(frame);
  pthread_mutex_destroy (&frame->vo_frame.mutex);
  free (frame);
}

static void vo_none_frame_field(vo_frame_t *vo_frame, int which_field) {
  /* do nothing */
}

static uint32_t vo_none_get_capabilities(vo_driver_t *vo_driver) {
  /* No, we dont crop. Neither do we interpret color matrix or range. */
  /* But we also dont ask decoders to convert data just for the trash ;-) */
  return VO_CAP_YV12 | VO_CAP_YUY2 | VO_CAP_CROP | VO_CAP_COLOR_MATRIX | VO_CAP_FULLRANGE;
}

static vo_frame_t *vo_none_alloc_frame(vo_driver_t *vo_driver) {
  /* none_driver_t *this = (none_driver_t *) vo_driver; */
  vo_none_frame_t  *frame;

  frame = calloc(1, sizeof(vo_none_frame_t));
  if(!frame)
    return NULL;

  pthread_mutex_init(&frame->vo_frame.mutex, NULL);

  frame->vo_frame.base[0] = NULL;
  frame->vo_frame.base[1] = NULL;
  frame->vo_frame.base[2] = NULL;

  frame->vo_frame.proc_slice = NULL;
  frame->vo_frame.proc_frame = NULL;
  frame->vo_frame.field      = vo_none_frame_field;
  frame->vo_frame.dispose    = vo_none_frame_dispose;
  frame->vo_frame.driver     = vo_driver;

  return (vo_frame_t *)frame;
}

static void vo_none_update_frame_format(vo_driver_t *vo_driver, vo_frame_t *vo_frame,
				     uint32_t width, uint32_t height,
				     double ratio, int format, int flags) {
  vo_none_driver_t *this = (vo_none_driver_t *) vo_driver;
  vo_none_frame_t  *frame = (vo_none_frame_t *) vo_frame;

  if((frame->width != width) || (frame->height != height) || (frame->format != format)) {

    vo_none_free_framedata(frame);

    frame->width  = width;
    frame->height = height;
    frame->format = format;

    switch(format) {

    case XINE_IMGFMT_YV12:
      {
	int y_size, uv_size;

	frame->vo_frame.pitches[0] = 8*((width + 7) / 8);
	frame->vo_frame.pitches[1] = 8*((width + 15) / 16);
	frame->vo_frame.pitches[2] = 8*((width + 15) / 16);

	y_size  = frame->vo_frame.pitches[0] * height;
	uv_size = frame->vo_frame.pitches[1] * ((height+1)/2);

	frame->vo_frame.base[0] = xine_malloc_aligned (y_size + 2*uv_size);
        if (frame->vo_frame.base[0]) {
          frame->vo_frame.base[1] = frame->vo_frame.base[0] + y_size;
          frame->vo_frame.base[2] = frame->vo_frame.base[0] + y_size + uv_size;
        } else {
          frame->vo_frame.base[1] = NULL;
          frame->vo_frame.base[2] = NULL;
          xprintf (this->xine, XINE_VERBOSITY_DEBUG,
            "video_out_none: error. (framedata allocation failed: out of memory)\n");
          frame->width = 0;
          frame->vo_frame.width = 0;
        }
      }
      break;

    case XINE_IMGFMT_YUY2:
      frame->vo_frame.pitches[0] = 8*((width + 3) / 4);
      frame->vo_frame.base[0] = xine_malloc_aligned(frame->vo_frame.pitches[0] * height);
      frame->vo_frame.base[1] = NULL;
      frame->vo_frame.base[2] = NULL;
      if (!frame->vo_frame.base[0]) {
        xprintf (this->xine, XINE_VERBOSITY_DEBUG,
          "video_out_none: error. (framedata allocation failed: out of memory)\n");
        frame->width = 0;
        frame->vo_frame.width = 0;
      }
      break;

    default:
      xprintf (this->xine, XINE_VERBOSITY_DEBUG, "video_out_none: unknown frame format %04x)\n", format);
      break;

    }
  }

  frame->ratio = ratio;
}

static void vo_none_display_frame(vo_driver_t *vo_driver, vo_frame_t *vo_frame) {
  /* vo_none_driver_t  *driver = (vo_none_driver_t *)vo_driver; */
  vo_none_frame_t   *frame = (vo_none_frame_t *)vo_frame;

  frame->vo_frame.free(&frame->vo_frame);
}

static int vo_none_get_property(vo_driver_t *vo_driver, int property) {
  vo_none_driver_t  *driver = (vo_none_driver_t *)vo_driver;

  switch(property) {

  case VO_PROP_ASPECT_RATIO:
    return driver->ratio;
    break;

  default:
    break;
  }

  return 0;
}

static int vo_none_set_property(vo_driver_t *vo_driver, int property, int value) {
  vo_none_driver_t  *driver = (vo_none_driver_t *)vo_driver;

  switch(property) {

  case VO_PROP_ASPECT_RATIO:
    if(value >= XINE_VO_ASPECT_NUM_RATIOS)
      value = XINE_VO_ASPECT_AUTO;

    driver->ratio = value;
    break;

  default:
    break;
  }
  return value;
}

static void vo_none_get_property_min_max(vo_driver_t *vo_driver,
				      int property, int *min, int *max) {
  *min = 0;
  *max = 0;
}

static int vo_none_gui_data_exchange(vo_driver_t *vo_driver, int data_type, void *data) {
/*   vo_none_driver_t     *this = (vo_none_driver_t *) vo_driver; */

  switch (data_type) {
  case XINE_GUI_SEND_COMPLETION_EVENT:
  case XINE_GUI_SEND_DRAWABLE_CHANGED:
  case XINE_GUI_SEND_EXPOSE_EVENT:
  case XINE_GUI_SEND_TRANSLATE_GUI_TO_VIDEO:
  case XINE_GUI_SEND_VIDEOWIN_VISIBLE:
  case XINE_GUI_SEND_SELECT_VISUAL:
    break;
  }

  return 0;
}
static void vo_none_dispose(vo_driver_t *vo_driver) {
  vo_none_driver_t *this = (vo_none_driver_t *) vo_driver;

  free(this);
}

static int vo_none_redraw_needed(vo_driver_t *vo_driver) {
  return 0;
}

static vo_driver_t *vo_none_open_plugin(video_driver_class_t *driver_class, const void *visual) {
  vo_none_class_t    *class = (vo_none_class_t *) driver_class;
  vo_none_driver_t   *driver;

  driver = calloc(1, sizeof(vo_none_driver_t));

  driver->config = class->config;
  driver->xine   = class->xine;
  driver->ratio  = XINE_VO_ASPECT_AUTO;

  driver->vo_driver.get_capabilities     = vo_none_get_capabilities;
  driver->vo_driver.alloc_frame          = vo_none_alloc_frame ;
  driver->vo_driver.update_frame_format  = vo_none_update_frame_format;
  driver->vo_driver.overlay_begin        = NULL;
  driver->vo_driver.overlay_blend        = NULL;
  driver->vo_driver.overlay_end          = NULL;
  driver->vo_driver.display_frame        = vo_none_display_frame;
  driver->vo_driver.get_property         = vo_none_get_property;
  driver->vo_driver.set_property         = vo_none_set_property;
  driver->vo_driver.get_property_min_max = vo_none_get_property_min_max;
  driver->vo_driver.gui_data_exchange    = vo_none_gui_data_exchange;
  driver->vo_driver.dispose              = vo_none_dispose;
  driver->vo_driver.redraw_needed        = vo_none_redraw_needed;

  return &driver->vo_driver;
}

/*
 * Class related functions.
 */
static void *vo_none_init_class (xine_t *xine, void *visual) {
  vo_none_class_t        *this;

  this = calloc(1, sizeof(vo_none_class_t));

  this->driver_class.open_plugin     = vo_none_open_plugin;
  this->driver_class.identifier      = "none";
  this->driver_class.description     = N_("xine video output plugin which displays nothing");
  this->driver_class.dispose         = default_video_driver_class_dispose;

  this->config                       = xine->config;
  this->xine                         = xine;

  return this;
}

static const vo_info_t vo_info_none = {
  5,                        /* Priority    */
  XINE_VISUAL_TYPE_NONE     /* Visual type */
};

#define VO_NONE_CATALOG { PLUGIN_VIDEO_OUT, 22, "none", XINE_VERSION_CODE, &vo_info_none, vo_none_init_class }

#ifndef XINE_MAKE_BUILTINS
const plugin_info_t xine_plugin_info[] EXPORTED = {
  /* type, API, "name", version, special_info, init_function */
  VO_NONE_CATALOG,
  { PLUGIN_NONE, 0, "", 0, NULL, NULL }
};
#endif

