/*
 * Copyright (C) 2000-2002 the xine project
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
 *
 * $Id: video_out_none.c,v 1.6 2002/12/21 12:56:49 miguelfreitas Exp $
 *
 * Was originally part of toxine frontend.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>

#include "xine.h"

#include "xine.h"
#include "video_out.h"
#include "xine_internal.h"
#include "xineutils.h"
#include "vo_scale.h"

typedef struct {
  vo_frame_t           vo_frame;
  int                  width;
  int                  height;
  int                  ratio_code;
  int                  format;
} none_frame_t;

typedef struct {
  vo_driver_t          vo_driver;
  config_values_t     *config;
  int                   ratio;
} none_driver_t;

typedef struct {
  video_driver_class_t  driver_class;
  config_values_t      *config;
} none_class_t;


static void free_framedata(none_frame_t* frame) {
  if(frame->vo_frame.base[0]) {
    free(frame->vo_frame.base[0]);
    frame->vo_frame.base[0] = NULL;
  }
  
  if(frame->vo_frame.base[1]) {
    free(frame->vo_frame.base[1]);
    frame->vo_frame.base[1] = NULL;
  }
  
  if(frame->vo_frame.base[2]) {
    free(frame->vo_frame.base[2]);
    frame->vo_frame.base[2] = NULL;
  }
}

static void none_frame_dispose(vo_frame_t *vo_frame) {
  none_frame_t *frame = (none_frame_t *)vo_frame;
  free_framedata(frame);  
  free (frame);
}

static void none_frame_field(vo_frame_t *vo_frame, int which_field) {
  /* do nothing */
}

static uint32_t none_get_capabilities(vo_driver_t *vo_driver) {
  return VO_CAP_YV12 | VO_CAP_YUY2;
}

static vo_frame_t *none_alloc_frame(vo_driver_t *vo_driver) {
  none_frame_t  *frame;
  
  frame = (none_frame_t *) malloc(sizeof(none_frame_t));
  memset(frame, 0, sizeof(none_frame_t));
  
  if(frame == NULL) {
    printf ("%s()@%d: out of memory\n", __XINE_FUNCTION__, __LINE__);
    abort();
  }
  
  frame->vo_frame.base[0] = NULL;
  frame->vo_frame.base[1] = NULL;
  frame->vo_frame.base[2] = NULL;
  
  frame->vo_frame.copy    = NULL;
  frame->vo_frame.field   = none_frame_field;
  frame->vo_frame.dispose = none_frame_dispose;
  frame->vo_frame.driver  = vo_driver;
  
  return (vo_frame_t *)frame;
}

static void none_update_frame_format(vo_driver_t *vo_driver, vo_frame_t *vo_frame,
				     uint32_t width, uint32_t height, 
				     int ratio_code, int format, int flags) {
  none_frame_t  *frame = (none_frame_t *)vo_frame;
  uint32_t      frame_size = (width * height);
  
  if((frame->width != width) || (frame->height != height) || (frame->format != format)) {
    free_framedata(frame);
    
    frame->width  = width;
    frame->height = height;
    frame->format = format;
    
    switch(format) {

    case XINE_IMGFMT_YV12:
      frame->vo_frame.base[0] = malloc(frame_size);
      frame->vo_frame.base[1] = malloc((frame_size / 4));
      frame->vo_frame.base[2] = malloc((frame_size / 4));
      break;

    case XINE_IMGFMT_YUY2:
      frame->vo_frame.base[0] = malloc((frame_size * 2));
      frame->vo_frame.base[1] = NULL;
      frame->vo_frame.base[2] = NULL;
      break;

    default:
      printf("%s()@%d: unknown frame format %04x)\n", __XINE_FUNCTION__, __LINE__, format);
      break;

    }
    
    if((format == XINE_IMGFMT_YV12 
	&& (frame->vo_frame.base[0] == NULL 
	    || frame->vo_frame.base[1] == NULL 
	    || frame->vo_frame.base[2] == NULL))
       || (format == XINE_IMGFMT_YUY2 && frame->vo_frame.base[0] == NULL)) {
      printf("%s()@%d: error. (framedata allocation failed: out of memory)\n", 
	     __XINE_FUNCTION__, __LINE__);
      free_framedata(frame);
    }
  }

  frame->ratio_code = ratio_code;
}

static void none_display_frame(vo_driver_t *vo_driver, vo_frame_t *vo_frame) {
  /* none_driver_t  *driver = (none_driver_t *)vo_driver; */
  none_frame_t   *frame = (none_frame_t *)vo_frame;
  
  frame->vo_frame.displayed(&frame->vo_frame);
}

static int none_get_property(vo_driver_t *vo_driver, int property) {
  none_driver_t  *driver = (none_driver_t *)vo_driver;
  
  switch(property) {

  case VO_PROP_ASPECT_RATIO:
    return driver->ratio;
    break;
    
  default:
    break;
  }

  return 0;
}

static int none_set_property(vo_driver_t *vo_driver, int property, int value) {
  none_driver_t  *driver = (none_driver_t *)vo_driver;
  
  switch(property) {

  case VO_PROP_ASPECT_RATIO:
    if(value >= NUM_ASPECT_RATIOS)
      value = ASPECT_AUTO;

    driver->ratio = value;
    break;

  default:
    break;
  }
  return value;
}

static void none_get_property_min_max(vo_driver_t *vo_driver, 
				      int property, int *min, int *max) {
  *min = 0;
  *max = 0;
}

static int none_gui_data_exchange(vo_driver_t *vo_driver, int data_type, void *data) {
/*   none_driver_t     *this = (none_driver_t *) vo_driver; */

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
static void none_dispose(vo_driver_t *vo_driver) {
  none_driver_t *this = (none_driver_t *) vo_driver;

  free(this);
}

static int none_redraw_needed(vo_driver_t *vo_driver) {
  return 0;
}

static vo_driver_t *open_plugin(video_driver_class_t *driver_class, const void *visual) {
  none_class_t    *class = (none_class_t *) driver_class;
  none_driver_t   *driver;
  
  driver = (none_driver_t *) malloc(sizeof(none_driver_t));
  
  driver->config = class->config;
  driver->ratio  = ASPECT_AUTO;
  
  driver->vo_driver.get_capabilities     = none_get_capabilities;
  driver->vo_driver.alloc_frame          = none_alloc_frame ;
  driver->vo_driver.update_frame_format  = none_update_frame_format;
  driver->vo_driver.overlay_begin        = NULL;
  driver->vo_driver.overlay_blend        = NULL;
  driver->vo_driver.overlay_end          = NULL;
  driver->vo_driver.display_frame        = none_display_frame;
  driver->vo_driver.get_property         = none_get_property;
  driver->vo_driver.set_property         = none_set_property;
  driver->vo_driver.get_property_min_max = none_get_property_min_max;
  driver->vo_driver.gui_data_exchange    = none_gui_data_exchange;
  driver->vo_driver.dispose              = none_dispose;
  driver->vo_driver.redraw_needed        = none_redraw_needed;
  
  return &driver->vo_driver;
}    

/*
 * Class related functions.
 */
static char* get_identifier (video_driver_class_t *driver_class) {
  return "None";
}

static char* get_description (video_driver_class_t *driver_class) {
  return _("xine video output plugin which display nothing");
}

static void dispose_class (video_driver_class_t *driver_class) {
  none_class_t    *this = (none_class_t *) driver_class;
  
  free (this);
}

static void *init_class (xine_t *xine, void *visual) {
  none_class_t        *this;
  
  this = (none_class_t *) malloc(sizeof(none_class_t));

  this->driver_class.open_plugin     = open_plugin;
  this->driver_class.get_identifier  = get_identifier;
  this->driver_class.get_description = get_description;
  this->driver_class.dispose         = dispose_class;

  this->config                       = xine->config;

  return this;
}

static vo_info_t vo_info_none = {
  5,                        /* Priority    */
  XINE_VISUAL_TYPE_NONE     /* Visual type */
};

plugin_info_t xine_plugin_info[] = {
  /* type, API, "name", version, special_info, init_function */  
  { PLUGIN_VIDEO_OUT, 14, "none", XINE_VERSION_CODE, &vo_info_none, init_class },
  { PLUGIN_NONE, 0, "", 0, NULL, NULL }
};
