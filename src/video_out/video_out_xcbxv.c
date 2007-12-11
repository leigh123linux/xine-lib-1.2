/*
 * Copyright (C) 2000-2004, 2007 the xine project
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
 * video_out_xcbxv.c, X11 video extension interface for xine
 *
 * based on mpeg2dec code from
 * Aaron Holtzman <aholtzma@ess.engr.uvic.ca>
 *
 * Xv image support by Gerd Knorr <kraxel@goldbach.in-berlin.de>
 *
 * xine-specific code by Guenter Bartsch <bartscgr@studbox.uni-stuttgart.de>
 *
 * overlay support by James Courtier-Dutton <James@superbug.demon.co.uk> - July 2001
 * X11 unscaled overlay support by Miguel Freitas - Nov 2003
 * ported to xcb by Christoph Pfister - Feb 2007
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <math.h>

#include <sys/types.h>
#if defined(__FreeBSD__)
#include <machine/param.h>
#endif
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/time.h>

#include <xcb/xv.h>

#define LOG_MODULE "video_out_xcbxv"
#define LOG_VERBOSE
/*
#define LOG
*/

#include "xine.h"
#include "video_out.h"
#include "xine_internal.h"
/* #include "overlay.h" */
#include "xineutils.h"
#include "vo_scale.h"
#include "xcbosd.h"

typedef struct xv_driver_s xv_driver_t;

typedef struct {
  int                value;
  int                min;
  int                max;
  xcb_atom_t         atom;

  cfg_entry_t       *entry;

  xv_driver_t       *this;
} xv_property_t;

typedef struct {
  char              *name;
  int                value;
} xv_portattribute_t;

typedef struct {
  vo_frame_t         vo_frame;

  int                width, height, format;
  double             ratio;

  uint8_t           *image;
  xcb_shm_seg_t      shmseg;
  unsigned int       xv_format;
  unsigned int       xv_data_size;
  unsigned int       xv_width;
  unsigned int       xv_height;
  unsigned int       xv_pitches[3];
  unsigned int       xv_offsets[3];

} xv_frame_t;


struct xv_driver_s {

  vo_driver_t        vo_driver;

  config_values_t   *config;

  /* xcb / xv related stuff */
  xcb_connection_t  *connection;
  xcb_screen_t      *screen;
  xcb_window_t       window;
  unsigned int       xv_format_yv12;
  unsigned int       xv_format_yuy2;
  xcb_gc_t           gc;
  xcb_xv_port_t      xv_port;

  int                use_shm;
  int                use_pitch_alignment;
  uint32_t           capabilities;
  xv_property_t      props[VO_NUM_PROPERTIES];

  xv_frame_t        *recent_frames[VO_NUM_RECENT_FRAMES];
  xv_frame_t        *cur_frame;
  xcbosd            *xoverlay;
  int                ovl_changed;

  /* all scaling information goes here */
  vo_scale_t         sc;

  int                use_colorkey;
  uint32_t           colorkey;

  /* hold initial port attributes values to restore on exit */
  xine_list_t       *port_attributes;

  xine_t            *xine;

  alphablend_t       alphablend_extra_data;

  pthread_mutex_t    main_mutex;

};

typedef struct {
  video_driver_class_t driver_class;

  config_values_t     *config;
  xine_t              *xine;
} xv_class_t;

static uint32_t xv_get_capabilities (vo_driver_t *this_gen) {
  xv_driver_t *this = (xv_driver_t *) this_gen;

  return this->capabilities;
}

static void xv_frame_field (vo_frame_t *vo_img, int which_field) {
  /* not needed for Xv */
}

static void xv_frame_dispose (vo_frame_t *vo_img) {
  xv_frame_t  *frame = (xv_frame_t *) vo_img ;
  xv_driver_t *this  = (xv_driver_t *) vo_img->driver;

  if (frame->shmseg) {
    pthread_mutex_lock(&this->main_mutex);
    xcb_shm_detach(this->connection, frame->shmseg);
    frame->shmseg = 0;
    pthread_mutex_unlock(&this->main_mutex);
    shmdt(frame->image);
  }
  else
    free(frame->image);

  free (frame);
}

static vo_frame_t *xv_alloc_frame (vo_driver_t *this_gen) {
  /* xv_driver_t  *this = (xv_driver_t *) this_gen; */
  xv_frame_t   *frame ;

  frame = (xv_frame_t *) xine_xmalloc (sizeof (xv_frame_t));
  if (!frame)
    return NULL;
  
  pthread_mutex_init (&frame->vo_frame.mutex, NULL);

  /*
   * supply required functions
   */
  frame->vo_frame.proc_slice = NULL;
  frame->vo_frame.proc_frame = NULL;
  frame->vo_frame.field      = xv_frame_field;
  frame->vo_frame.dispose    = xv_frame_dispose;
  frame->vo_frame.driver     = this_gen;

  return (vo_frame_t *) frame;
}

static void create_ximage(xv_driver_t *this, xv_frame_t *frame, int width, int height, int format)
{
  xcb_xv_query_image_attributes_cookie_t query_attributes_cookie;
  xcb_xv_query_image_attributes_reply_t *query_attributes_reply;

  unsigned int length;

  if (this->use_pitch_alignment) {
    width = (width + 7) & ~0x7;
  }

  switch (format) {
  case XINE_IMGFMT_YV12:
    frame->xv_format = this->xv_format_yv12;
    break;
  case XINE_IMGFMT_YUY2:
    frame->xv_format = this->xv_format_yuy2;
    break;
  default:
    xprintf (this->xine, XINE_VERBOSITY_DEBUG, "create_ximage: unknown format %08x\n",format);
    _x_abort();
  }

  query_attributes_cookie = xcb_xv_query_image_attributes(this->connection, this->xv_port, frame->xv_format, width, height);
  query_attributes_reply = xcb_xv_query_image_attributes_reply(this->connection, query_attributes_cookie, NULL);

  if (query_attributes_reply == NULL)
    return;

  frame->xv_data_size = query_attributes_reply->data_size;
  frame->xv_width = query_attributes_reply->width;
  frame->xv_height = query_attributes_reply->height;

  length = xcb_xv_query_image_attributes_pitches_length(query_attributes_reply);
  if (length > 3)
    length = 3;
  memcpy(frame->xv_pitches, xcb_xv_query_image_attributes_pitches(query_attributes_reply), length * sizeof(frame->xv_pitches[0]));

  length = xcb_xv_query_image_attributes_offsets_length(query_attributes_reply);
  if (length > 3)
    length = 3;
  memcpy(frame->xv_offsets, xcb_xv_query_image_attributes_offsets(query_attributes_reply), length * sizeof(frame->xv_offsets[0]));

  free(query_attributes_reply);

  if (this->use_shm) {
    int shmid;
    xcb_void_cookie_t shm_attach_cookie;
    xcb_generic_error_t *generic_error;

    /*
     * try shm
     */

    if (frame->xv_data_size == 0) {
      xprintf(this->xine, XINE_VERBOSITY_LOG,
	      _("%s: XvShmCreateImage returned a zero size\n"), LOG_MODULE);
      xprintf(this->xine, XINE_VERBOSITY_LOG,
	      _("%s: => not using MIT Shared Memory extension.\n"), LOG_MODULE);
      goto shm_fail1;
    }

    shmid = shmget(IPC_PRIVATE, frame->xv_data_size, IPC_CREAT | 0777);

    if (shmid < 0 ) {
      xprintf(this->xine, XINE_VERBOSITY_LOG,
	      _("%s: shared memory error in shmget: %s\n"), LOG_MODULE, strerror(errno));
      xprintf(this->xine, XINE_VERBOSITY_LOG,
	      _("%s: => not using MIT Shared Memory extension.\n"), LOG_MODULE);
      goto shm_fail1;
    }

    frame->image = shmat(shmid, 0, 0);

    if (frame->image == ((void *) -1)) {
      xprintf(this->xine, XINE_VERBOSITY_DEBUG,
	      _("%s: shared memory error (address error)\n"), LOG_MODULE);
      xprintf(this->xine, XINE_VERBOSITY_LOG,
	      _("%s: => not using MIT Shared Memory extension.\n"), LOG_MODULE);
      goto shm_fail2;
    }

    frame->shmseg = xcb_generate_id(this->connection);
    shm_attach_cookie = xcb_shm_attach_checked(this->connection, frame->shmseg, shmid, 0);
    generic_error = xcb_request_check(this->connection, shm_attach_cookie);

    if (generic_error != NULL) {
      xprintf(this->xine, XINE_VERBOSITY_LOG,
	      _("%s: x11 error during shared memory XImage creation\n"), LOG_MODULE);
      xprintf(this->xine, XINE_VERBOSITY_LOG,
	      _("%s: => not using MIT Shared Memory extension.\n"), LOG_MODULE);
      free(generic_error);
      goto shm_fail3;
    }

    /*
     * Now that the Xserver has learned about and attached to the
     * shared memory segment,  delete it.  It's actually deleted by
     * the kernel when all users of that segment have detached from
     * it.  Gives an automatic shared memory cleanup in case we crash.
     */

    shmctl(shmid, IPC_RMID, 0);

    return;

  shm_fail3:
    frame->shmseg = 0;
    shmdt(frame->image);
  shm_fail2:
    shmctl(shmid, IPC_RMID, 0);
  shm_fail1:
    this->use_shm = 0;
  }

  /*
   * fall back to plain Xv if necessary
   */

  switch (format) {
  case XINE_IMGFMT_YV12:
    frame->image = malloc(width * height * 3/2);
    break;
  case XINE_IMGFMT_YUY2:
    frame->image = malloc(width * height * 2);
    break;
  default:
    xprintf (this->xine, XINE_VERBOSITY_DEBUG, "create_ximage: unknown format %08x\n",format);
    _x_abort();
  }
}

static void dispose_ximage(xv_driver_t *this, xv_frame_t *frame)
{
  if (frame->shmseg) {
    xcb_shm_detach(this->connection, frame->shmseg);
    frame->shmseg = 0;
    shmdt(frame->image);
  } else
    free(frame->image);
  frame->image = NULL;
}

static void xv_update_frame_format (vo_driver_t *this_gen,
				    vo_frame_t *frame_gen,
				    uint32_t width, uint32_t height,
				    double ratio, int format, int flags) {
  xv_driver_t  *this  = (xv_driver_t *) this_gen;
  xv_frame_t   *frame = (xv_frame_t *) frame_gen;

  if (this->use_pitch_alignment) {
    width = (width + 7) & ~0x7;
  }

  if ((frame->width != width)
      || (frame->height != height)
      || (frame->format != format)) {

    /* printf (LOG_MODULE ": updating frame to %d x %d (ratio=%d, format=%08x)\n",width,height,ratio_code,format); */

    pthread_mutex_lock(&this->main_mutex);

    /*
     * (re-) allocate xvimage
     */

    if (frame->image)
      dispose_ximage(this, frame);

    create_ximage(this, frame, width, height, format);

    if(format == XINE_IMGFMT_YUY2) {
      frame->vo_frame.pitches[0] = frame->xv_pitches[0];
      frame->vo_frame.base[0] = frame->image + frame->xv_offsets[0];
    } 
    else {
      frame->vo_frame.pitches[0] = frame->xv_pitches[0];
      frame->vo_frame.pitches[1] = frame->xv_pitches[2];
      frame->vo_frame.pitches[2] = frame->xv_pitches[1];
      frame->vo_frame.base[0] = frame->image + frame->xv_offsets[0];
      frame->vo_frame.base[1] = frame->image + frame->xv_offsets[2];
      frame->vo_frame.base[2] = frame->image + frame->xv_offsets[1];
    }

    frame->width  = width;
    frame->height = height;
    frame->format = format;

    pthread_mutex_unlock(&this->main_mutex);
  }

  frame->ratio = ratio;
}

static void xv_clean_output_area (xv_driver_t *this) {
  int i;
  xcb_rectangle_t rects[4];
  int rects_count = 0;

  pthread_mutex_lock(&this->main_mutex);

  xcb_change_gc(this->connection, this->gc, XCB_GC_FOREGROUND, &this->screen->black_pixel);

  for( i = 0; i < 4; i++ ) {
    if( this->sc.border[i].w && this->sc.border[i].h ) {
      rects[rects_count].x = this->sc.border[i].x;
      rects[rects_count].y = this->sc.border[i].y;
      rects[rects_count].width = this->sc.border[i].w;
      rects[rects_count].height = this->sc.border[i].h;
      rects_count++;
    }
  }

  if (rects_count > 0)
    xcb_poly_fill_rectangle(this->connection, this->window, this->gc, rects_count, rects);

  if (this->use_colorkey) {
    xcb_change_gc(this->connection, this->gc, XCB_GC_FOREGROUND, &this->colorkey);
    xcb_rectangle_t rectangle = { this->sc.output_xoffset, this->sc.output_yoffset,
				  this->sc.output_width, this->sc.output_height };
    xcb_poly_fill_rectangle(this->connection, this->window, this->gc, 1, &rectangle);
  }
  
  if (this->xoverlay) {
    xcbosd_resize(this->xoverlay, this->sc.gui_width, this->sc.gui_height);
    this->ovl_changed = 1;
  }
  
  pthread_mutex_unlock(&this->main_mutex);
}

/*
 * convert delivered height/width to ideal width/height
 * taking into account aspect ratio and zoom factor
 */

static void xv_compute_ideal_size (xv_driver_t *this) {
  _x_vo_scale_compute_ideal_size( &this->sc );
}


/*
 * make ideal width/height "fit" into the gui
 */

static void xv_compute_output_size (xv_driver_t *this) {

  _x_vo_scale_compute_output_size( &this->sc );
}

static void xv_overlay_begin (vo_driver_t *this_gen, 
			      vo_frame_t *frame_gen, int changed) {
  xv_driver_t  *this = (xv_driver_t *) this_gen;

  this->ovl_changed += changed;

  if( this->ovl_changed && this->xoverlay ) {
    pthread_mutex_lock(&this->main_mutex);
    xcbosd_clear(this->xoverlay);
    pthread_mutex_unlock(&this->main_mutex);
  }
  
  this->alphablend_extra_data.offset_x = frame_gen->overlay_offset_x;
  this->alphablend_extra_data.offset_y = frame_gen->overlay_offset_y;
}

static void xv_overlay_end (vo_driver_t *this_gen, vo_frame_t *vo_img) {
  xv_driver_t  *this = (xv_driver_t *) this_gen;

  if( this->ovl_changed && this->xoverlay ) {
    pthread_mutex_lock(&this->main_mutex);
    xcbosd_expose(this->xoverlay);
    pthread_mutex_unlock(&this->main_mutex);
  }

  this->ovl_changed = 0;
}

static void xv_overlay_blend (vo_driver_t *this_gen, 
			      vo_frame_t *frame_gen, vo_overlay_t *overlay) {
  xv_driver_t  *this = (xv_driver_t *) this_gen;
  xv_frame_t   *frame = (xv_frame_t *) frame_gen;

  if (overlay->rle) {
    if( overlay->unscaled ) {
      if( this->ovl_changed && this->xoverlay ) {
        pthread_mutex_lock(&this->main_mutex);
        xcbosd_blend(this->xoverlay, overlay);
        pthread_mutex_unlock(&this->main_mutex);
      }
    } else {
      if (frame->format == XINE_IMGFMT_YV12)
        _x_blend_yuv(frame->vo_frame.base, overlay, 
		  frame->width, frame->height, frame->vo_frame.pitches,
                  &this->alphablend_extra_data);
      else
        _x_blend_yuy2(frame->vo_frame.base[0], overlay, 
		   frame->width, frame->height, frame->vo_frame.pitches[0],
                   &this->alphablend_extra_data);
    }
  }
}

static void xv_add_recent_frame (xv_driver_t *this, xv_frame_t *frame) {
  int i;

  i = VO_NUM_RECENT_FRAMES-1;
  if( this->recent_frames[i] )
    this->recent_frames[i]->vo_frame.free
       (&this->recent_frames[i]->vo_frame);

  for( ; i ; i-- )
    this->recent_frames[i] = this->recent_frames[i-1];

  this->recent_frames[0] = frame;
}

/* currently not used - we could have a method to call this from video loop */
#if 0
static void xv_flush_recent_frames (xv_driver_t *this) {
  int i;

  for( i=0; i < VO_NUM_RECENT_FRAMES; i++ ) {
    if( this->recent_frames[i] )
      this->recent_frames[i]->vo_frame.free
         (&this->recent_frames[i]->vo_frame);
    this->recent_frames[i] = NULL;
  }
}
#endif

static int xv_redraw_needed (vo_driver_t *this_gen) {
  xv_driver_t  *this = (xv_driver_t *) this_gen;
  int           ret  = 0;

  if( this->cur_frame ) {

    this->sc.delivered_height = this->cur_frame->height;
    this->sc.delivered_width  = this->cur_frame->width;
    this->sc.delivered_ratio  = this->cur_frame->ratio;
    
    this->sc.crop_left        = this->cur_frame->vo_frame.crop_left;
    this->sc.crop_right       = this->cur_frame->vo_frame.crop_right;
    this->sc.crop_top         = this->cur_frame->vo_frame.crop_top;
    this->sc.crop_bottom      = this->cur_frame->vo_frame.crop_bottom;

    xv_compute_ideal_size(this);

    if( _x_vo_scale_redraw_needed( &this->sc ) ) {

      xv_compute_output_size (this);

      xv_clean_output_area (this);

      ret = 1;
    }
  }
  else
    ret = 1;

  return ret;
}

static void xv_display_frame (vo_driver_t *this_gen, vo_frame_t *frame_gen) {
  xv_driver_t  *this  = (xv_driver_t *) this_gen;
  xv_frame_t   *frame = (xv_frame_t *) frame_gen;
  /*
  printf (LOG_MODULE ": xv_display_frame...\n");
  */
  
  /*
   * queue frames (deinterlacing)
   * free old frames
   */

  xv_add_recent_frame (this, frame); /* deinterlacing */

  this->cur_frame = frame;

  /*
   * let's see if this frame is different in size / aspect
   * ratio from the previous one
   */
  if ( (frame->width != this->sc.delivered_width)
       || (frame->height != this->sc.delivered_height)
       || (frame->ratio != this->sc.delivered_ratio) ) {
    lprintf("frame format changed\n");
    this->sc.force_redraw = 1;    /* trigger re-calc of output size */
  }

  /*
   * tell gui that we are about to display a frame,
   * ask for offset and output size
   */
  xv_redraw_needed (this_gen);

  pthread_mutex_lock(&this->main_mutex);

  if (this->cur_frame->shmseg) {
    xcb_xv_shm_put_image(this->connection, this->xv_port, this->window, this->gc,
                         this->cur_frame->shmseg, this->cur_frame->xv_format, 0,
                         this->sc.displayed_xoffset, this->sc.displayed_yoffset,
                         this->sc.displayed_width, this->sc.displayed_height,
                         this->sc.output_xoffset, this->sc.output_yoffset,
                         this->sc.output_width, this->sc.output_height,
                         this->cur_frame->xv_width, this->cur_frame->xv_height, 0);

  } else {
    xcb_xv_put_image(this->connection, this->xv_port, this->window, this->gc,
                     this->cur_frame->xv_format,
                     this->sc.displayed_xoffset, this->sc.displayed_yoffset,
                     this->sc.displayed_width, this->sc.displayed_height,
                     this->sc.output_xoffset, this->sc.output_yoffset,
                     this->sc.output_width, this->sc.output_height,
                     this->cur_frame->xv_width, this->cur_frame->xv_height,
                     this->cur_frame->xv_data_size, this->cur_frame->image);
  }

  xcb_flush(this->connection);

  pthread_mutex_unlock(&this->main_mutex);

  /*
  printf (LOG_MODULE ": xv_display_frame... done\n");
  */
}

static int xv_get_property (vo_driver_t *this_gen, int property) {
  xv_driver_t *this = (xv_driver_t *) this_gen;

  switch (property) {
    case VO_PROP_WINDOW_WIDTH:
      this->props[property].value = this->sc.gui_width;
      break;
    case VO_PROP_WINDOW_HEIGHT:
      this->props[property].value = this->sc.gui_height;
      break;
  }

  lprintf(LOG_MODULE ": property #%d = %d\n", property, this->props[property].value);

  return this->props[property].value;
}

static void xv_property_callback (void *property_gen, xine_cfg_entry_t *entry) {
  xv_property_t *property = (xv_property_t *) property_gen;
  xv_driver_t   *this = property->this;
  
  pthread_mutex_lock(&this->main_mutex);
  xcb_xv_set_port_attribute(this->connection, this->xv_port,
			    property->atom, entry->num_value);
  pthread_mutex_unlock(&this->main_mutex);
}

static int xv_set_property (vo_driver_t *this_gen,
			    int property, int value) {
  xv_driver_t *this = (xv_driver_t *) this_gen;

  if (this->props[property].atom != XCB_NONE) {
    xcb_xv_get_port_attribute_cookie_t get_attribute_cookie;
    xcb_xv_get_port_attribute_reply_t *get_attribute_reply;

    /* value is out of bound */
    if((value < this->props[property].min) || (value > this->props[property].max))
      value = (this->props[property].min + this->props[property].max) >> 1;

    pthread_mutex_lock(&this->main_mutex);
    xcb_xv_set_port_attribute(this->connection, this->xv_port,
			      this->props[property].atom, value);

    get_attribute_cookie = xcb_xv_get_port_attribute(this->connection, this->xv_port, this->props[property].atom);
    get_attribute_reply = xcb_xv_get_port_attribute_reply(this->connection, get_attribute_cookie, NULL);
    this->props[property].value = get_attribute_reply->value;
    free(get_attribute_reply);

    pthread_mutex_unlock(&this->main_mutex);

    if (this->props[property].entry)
      this->props[property].entry->num_value = this->props[property].value;

    return this->props[property].value;
  } 
  else {
    switch (property) {
    case VO_PROP_ASPECT_RATIO:
      if (value>=XINE_VO_ASPECT_NUM_RATIOS)
	value = XINE_VO_ASPECT_AUTO;

      this->props[property].value = value;
      xprintf(this->xine, XINE_VERBOSITY_LOG, 
	      LOG_MODULE ": VO_PROP_ASPECT_RATIO(%d)\n", this->props[property].value);
      this->sc.user_ratio = value;

      xv_compute_ideal_size (this);

      this->sc.force_redraw = 1;    /* trigger re-calc of output size */
      break;

    case VO_PROP_ZOOM_X:
      if ((value >= XINE_VO_ZOOM_MIN) && (value <= XINE_VO_ZOOM_MAX)) {
        this->props[property].value = value;
	xprintf(this->xine, XINE_VERBOSITY_LOG,
		LOG_MODULE ": VO_PROP_ZOOM_X = %d\n", this->props[property].value);
	
	this->sc.zoom_factor_x = (double)value / (double)XINE_VO_ZOOM_STEP;

	xv_compute_ideal_size (this);

	this->sc.force_redraw = 1;    /* trigger re-calc of output size */
      }
      break;

    case VO_PROP_ZOOM_Y:
      if ((value >= XINE_VO_ZOOM_MIN) && (value <= XINE_VO_ZOOM_MAX)) {
        this->props[property].value = value;
	xprintf(this->xine, XINE_VERBOSITY_LOG,
		LOG_MODULE ": VO_PROP_ZOOM_Y = %d\n", this->props[property].value);

	this->sc.zoom_factor_y = (double)value / (double)XINE_VO_ZOOM_STEP;

	xv_compute_ideal_size (this);

	this->sc.force_redraw = 1;    /* trigger re-calc of output size */
      }
      break;
    }
  }

  return value;
}

static void xv_get_property_min_max (vo_driver_t *this_gen,
				     int property, int *min, int *max) {
  xv_driver_t *this = (xv_driver_t *) this_gen;

  *min = this->props[property].min;
  *max = this->props[property].max;
}

static int xv_gui_data_exchange (vo_driver_t *this_gen,
				 int data_type, void *data) {
  xv_driver_t     *this = (xv_driver_t *) this_gen;

  switch (data_type) {
#ifndef XINE_DISABLE_DEPRECATED_FEATURES
  case XINE_GUI_SEND_COMPLETION_EVENT:
    break;
#endif

  case XINE_GUI_SEND_EXPOSE_EVENT: {
    /* XExposeEvent * xev = (XExposeEvent *) data; */

    if (this->cur_frame) {
      int i;
      xcb_rectangle_t rects[4];
      int rects_count = 0;

      pthread_mutex_lock(&this->main_mutex);

      if (this->cur_frame->shmseg) {
	xcb_xv_shm_put_image(this->connection, this->xv_port, this->window, this->gc,
			     this->cur_frame->shmseg, this->cur_frame->xv_format, 0,
			     this->sc.displayed_xoffset, this->sc.displayed_yoffset,
			     this->sc.displayed_width, this->sc.displayed_height,
			     this->sc.output_xoffset, this->sc.output_yoffset,
			     this->sc.output_width, this->sc.output_height,
			     this->cur_frame->xv_width, this->cur_frame->xv_height, 0);
      } else {
	xcb_xv_put_image(this->connection, this->xv_port, this->window, this->gc,
			 this->cur_frame->xv_format,
			 this->sc.displayed_xoffset, this->sc.displayed_yoffset,
			 this->sc.displayed_width, this->sc.displayed_height,
			 this->sc.output_xoffset, this->sc.output_yoffset,
			 this->sc.output_width, this->sc.output_height,
			 this->cur_frame->xv_width, this->cur_frame->xv_height,
			 this->cur_frame->xv_data_size, this->cur_frame->image);
      }

      xcb_change_gc(this->connection, this->gc, XCB_GC_FOREGROUND, &this->screen->black_pixel);

      for( i = 0; i < 4; i++ ) {
	if( this->sc.border[i].w && this->sc.border[i].h ) {
	  rects[rects_count].x = this->sc.border[i].x;
	  rects[rects_count].y = this->sc.border[i].y;
	  rects[rects_count].width = this->sc.border[i].w;
	  rects[rects_count].height = this->sc.border[i].h;
	  rects_count++;
	}
      }

      if (rects_count > 0)
	xcb_poly_fill_rectangle(this->connection, this->window, this->gc, rects_count, rects);

      if(this->xoverlay)
	xcbosd_expose(this->xoverlay);
      
      xcb_flush(this->connection);
      pthread_mutex_unlock(&this->main_mutex);
    }
  }
  break;

  case XINE_GUI_SEND_DRAWABLE_CHANGED:
    pthread_mutex_lock(&this->main_mutex);
    this->window = (xcb_window_t) data;
    xcb_free_gc(this->connection, this->gc);
    this->gc = xcb_generate_id(this->connection);
    xcb_create_gc(this->connection, this->gc, this->window, 0, NULL);
    if(this->xoverlay)
      xcbosd_drawable_changed(this->xoverlay, this->window);
    this->ovl_changed = 1;
    pthread_mutex_unlock(&this->main_mutex);
    this->sc.force_redraw = 1;
    break;

  case XINE_GUI_SEND_TRANSLATE_GUI_TO_VIDEO:
    {
      int x1, y1, x2, y2;
      x11_rectangle_t *rect = data;

      _x_vo_scale_translate_gui2video(&this->sc, rect->x, rect->y,
				   &x1, &y1);
      _x_vo_scale_translate_gui2video(&this->sc, rect->x + rect->w, rect->y + rect->h,
				   &x2, &y2);
      rect->x = x1;
      rect->y = y1;
      rect->w = x2-x1;
      rect->h = y2-y1;
    }
    break;

  default:
    return -1;
  }

  return 0;
}

static void xv_store_port_attribute(xv_driver_t *this, char *name) {
  xcb_intern_atom_cookie_t atom_cookie;
  xcb_intern_atom_reply_t *atom_reply;

  xcb_xv_get_port_attribute_cookie_t get_attribute_cookie;
  xcb_xv_get_port_attribute_reply_t *get_attribute_reply;
  
  pthread_mutex_lock(&this->main_mutex);
  atom_cookie = xcb_intern_atom(this->connection, 0, strlen(name), name);
  atom_reply = xcb_intern_atom_reply(this->connection, atom_cookie, NULL);
  get_attribute_cookie = xcb_xv_get_port_attribute(this->connection, this->xv_port, atom_reply->atom);
  get_attribute_reply = xcb_xv_get_port_attribute_reply(this->connection, get_attribute_cookie, NULL);
  free(atom_reply);
  pthread_mutex_unlock(&this->main_mutex);
  
  if (get_attribute_reply != NULL) {
    xv_portattribute_t *attr;

    attr = (xv_portattribute_t *) malloc(sizeof(xv_portattribute_t));
    attr->name = strdup(name);
    attr->value = get_attribute_reply->value;
    free(get_attribute_reply);

    xine_list_push_back(this->port_attributes, attr);
  }
}

static void xv_restore_port_attributes(xv_driver_t *this) {
  xine_list_iterator_t ite;

  xcb_intern_atom_cookie_t atom_cookie;
  xcb_intern_atom_reply_t *atom_reply;
  
  while ((ite = xine_list_front(this->port_attributes)) != NULL) {
    xv_portattribute_t *attr = xine_list_get_value(this->port_attributes, ite);
    xine_list_remove (this->port_attributes, ite);
  
    pthread_mutex_lock(&this->main_mutex);
    atom_cookie = xcb_intern_atom(this->connection, 0, strlen(attr->name), attr->name);
    atom_reply = xcb_intern_atom_reply(this->connection, atom_cookie, NULL);
    xcb_xv_set_port_attribute(this->connection, this->xv_port, atom_reply->atom, attr->value);
    free(atom_reply);
    pthread_mutex_unlock(&this->main_mutex);
        
    free( attr->name );
    free( attr );
  }
  
  pthread_mutex_lock(&this->main_mutex);
  xcb_flush(this->connection);
  pthread_mutex_unlock(&this->main_mutex);
 
  xine_list_delete( this->port_attributes );
}

static void xv_dispose (vo_driver_t *this_gen) {
  xv_driver_t *this = (xv_driver_t *) this_gen;
  int          i;

  /* restore port attributes to their initial values */
  xv_restore_port_attributes(this);

  pthread_mutex_lock(&this->main_mutex);
  xcb_xv_ungrab_port(this->connection, this->xv_port, XCB_CURRENT_TIME);
  xcb_free_gc(this->connection, this->gc);
  pthread_mutex_unlock(&this->main_mutex);

  for( i=0; i < VO_NUM_RECENT_FRAMES; i++ ) {
    if( this->recent_frames[i] )
      this->recent_frames[i]->vo_frame.dispose
         (&this->recent_frames[i]->vo_frame);
    this->recent_frames[i] = NULL;
  }

  if( this->xoverlay ) {
    pthread_mutex_lock(&this->main_mutex);
    xcbosd_destroy(this->xoverlay);
    pthread_mutex_unlock(&this->main_mutex);
  }

  pthread_mutex_destroy(&this->main_mutex);

  _x_alphablend_free(&this->alphablend_extra_data);
  
  free (this);
}

static int xv_check_yv12(xcb_connection_t *connection, xcb_xv_port_t port) {
  xcb_xv_list_image_formats_cookie_t list_formats_cookie;
  xcb_xv_list_image_formats_reply_t *list_formats_reply;

  xcb_xv_image_format_info_iterator_t format_it;

  list_formats_cookie = xcb_xv_list_image_formats(connection, port);
  list_formats_reply = xcb_xv_list_image_formats_reply(connection, list_formats_cookie, NULL);
  format_it = xcb_xv_list_image_formats_format_iterator(list_formats_reply);
  
  for (; format_it.rem; xcb_xv_image_format_info_next(&format_it))
    if ((format_it.data->id == XINE_IMGFMT_YV12) &&
	(! (strcmp ((char *) format_it.data->guid, "YV12")))) {
      free(list_formats_reply);
      return 0;
    }

  free(list_formats_reply);
  return 1;
}

static void xv_check_capability (xv_driver_t *this,
				 int property, xcb_xv_attribute_info_t *attr,
				 int base_id,
				 char *config_name,
				 char *config_desc,
				 char *config_help) {
  int          int_default;
  cfg_entry_t *entry;
  char        *str_prop = xcb_xv_attribute_info_name(attr);

  xcb_xv_get_port_attribute_cookie_t get_attribute_cookie;
  xcb_xv_get_port_attribute_reply_t *get_attribute_reply;

  xcb_intern_atom_cookie_t atom_cookie;
  xcb_intern_atom_reply_t *atom_reply;
   
  /*
   * some Xv drivers (Gatos ATI) report some ~0 as max values, this is confusing.
   */
  if (VO_PROP_COLORKEY && (attr->max == ~0))
    attr->max = 2147483615;

  atom_cookie = xcb_intern_atom(this->connection, 0, strlen(str_prop), str_prop);
  atom_reply = xcb_intern_atom_reply(this->connection, atom_cookie, NULL);

  this->props[property].min  = attr->min;
  this->props[property].max  = attr->max;
  this->props[property].atom = atom_reply->atom;

  free(atom_reply);

  get_attribute_cookie = xcb_xv_get_port_attribute(this->connection, this->xv_port, this->props[property].atom);
  get_attribute_reply = xcb_xv_get_port_attribute_reply(this->connection, get_attribute_cookie, NULL);

  int_default = get_attribute_reply->value;

  free(get_attribute_reply);

  xprintf(this->xine, XINE_VERBOSITY_DEBUG,
	  LOG_MODULE ": port attribute %s (%d) value is %d\n", str_prop, property, int_default);

  /* disable autopaint colorkey by default */
  /* might be overridden using config entry */
  if(strcmp(str_prop, "XV_AUTOPAINT_COLORKEY") == 0)
    int_default = 0;
    
  if (config_name) {
    /* is this a boolean property ? */
    if ((attr->min == 0) && (attr->max == 1)) {
      this->config->register_bool (this->config, config_name, int_default,
				   config_desc,
				   config_help, 20, xv_property_callback, &this->props[property]);

    } else {
      this->config->register_range (this->config, config_name, int_default,
				    this->props[property].min, this->props[property].max,
				    config_desc,
				    config_help, 20, xv_property_callback, &this->props[property]);
    }

    entry = this->config->lookup_entry (this->config, config_name);

    if((entry->num_value < this->props[property].min) || 
       (entry->num_value > this->props[property].max)) {

      this->config->update_num(this->config, config_name, 
			       ((this->props[property].min + this->props[property].max) >> 1));
      
      entry = this->config->lookup_entry (this->config, config_name);
    }

    this->props[property].entry = entry;

    xv_set_property (&this->vo_driver, property, entry->num_value);

    if (strcmp(str_prop, "XV_COLORKEY") == 0) {
      this->use_colorkey |= 1;
      this->colorkey = entry->num_value;
    } else if(strcmp(str_prop, "XV_AUTOPAINT_COLORKEY") == 0) {
      if(entry->num_value==1)
        this->use_colorkey |= 2;	/* colorkey is autopainted */
    }
  } else
    this->props[property].value  = int_default;
}

static void xv_update_XV_FILTER(void *this_gen, xine_cfg_entry_t *entry) {
  xv_driver_t *this = (xv_driver_t *) this_gen;
  int xv_filter;

  xcb_intern_atom_cookie_t atom_cookie;
  xcb_intern_atom_reply_t *atom_reply;

  xv_filter = entry->num_value;

  pthread_mutex_lock(&this->main_mutex);
  atom_cookie = xcb_intern_atom(this->connection, 0, sizeof("XV_FILTER"), "XV_FILTER");
  atom_reply = xcb_intern_atom_reply(this->connection, atom_cookie, NULL);
  xcb_xv_set_port_attribute(this->connection, this->xv_port, atom_reply->atom, xv_filter);
  free(atom_reply);
  pthread_mutex_unlock(&this->main_mutex);

  xprintf(this->xine, XINE_VERBOSITY_DEBUG,
	  LOG_MODULE ": bilinear scaling mode (XV_FILTER) = %d\n",xv_filter);
}

static void xv_update_XV_DOUBLE_BUFFER(void *this_gen, xine_cfg_entry_t *entry) {
  xv_driver_t *this = (xv_driver_t *) this_gen;
  int          xv_double_buffer;

  xcb_intern_atom_cookie_t atom_cookie;
  xcb_intern_atom_reply_t *atom_reply;

  xv_double_buffer = entry->num_value;

  pthread_mutex_lock(&this->main_mutex);
  atom_cookie = xcb_intern_atom(this->connection, 0, sizeof("XV_DOUBLE_BUFFER"), "XV_DOUBLE_BUFFER");
  atom_reply = xcb_intern_atom_reply(this->connection, atom_cookie, NULL);
  xcb_xv_set_port_attribute(this->connection, this->xv_port, atom_reply->atom, xv_double_buffer);
  free(atom_reply);
  pthread_mutex_unlock(&this->main_mutex);

  xprintf(this->xine, XINE_VERBOSITY_DEBUG,
	  LOG_MODULE ": double buffering mode = %d\n", xv_double_buffer);
}

static void xv_update_xv_pitch_alignment(void *this_gen, xine_cfg_entry_t *entry) {
  xv_driver_t *this = (xv_driver_t *) this_gen;

  this->use_pitch_alignment = entry->num_value;
}

static vo_driver_t *open_plugin(video_driver_class_t *class_gen, const void *visual_gen) {
  xv_class_t           *class = (xv_class_t *) class_gen;
  config_values_t      *config = class->config;
  xv_driver_t          *this;
  int                   i;
  xcb_visual_t         *visual = (xcb_visual_t *) visual_gen;
  unsigned int          j;
  xcb_xv_port_t         xv_port;

  const xcb_query_extension_reply_t *query_extension_reply;

  xcb_xv_query_adaptors_cookie_t query_adaptors_cookie;
  xcb_xv_query_adaptors_reply_t *query_adaptors_reply;
  xcb_xv_query_port_attributes_cookie_t query_attributes_cookie;
  xcb_xv_query_port_attributes_reply_t *query_attributes_reply;
  xcb_xv_list_image_formats_cookie_t list_formats_cookie;
  xcb_xv_list_image_formats_reply_t *list_formats_reply;

  xcb_xv_adaptor_info_iterator_t adaptor_it;
  xcb_xv_image_format_info_iterator_t format_it;

  this = (xv_driver_t *) xine_xmalloc (sizeof (xv_driver_t));
  if (!this)
    return NULL;

  pthread_mutex_init(&this->main_mutex, NULL);

  _x_alphablend_init(&this->alphablend_extra_data, class->xine);
  
  this->connection        = visual->connection;
  this->screen            = visual->screen;
  this->window            = visual->window;
  this->config            = config;

  /*
   * check for Xvideo support
   */

  query_extension_reply = xcb_get_extension_data(this->connection, &xcb_xv_id);
  if (!query_extension_reply || !query_extension_reply->present) {
    xprintf (class->xine, XINE_VERBOSITY_LOG, _("%s: Xv extension not present.\n"), LOG_MODULE);
    return NULL;
  }

  /*
   * check adaptors, search for one that supports (at least) yuv12
   */

  query_adaptors_cookie = xcb_xv_query_adaptors(this->connection, this->window);
  query_adaptors_reply = xcb_xv_query_adaptors_reply(this->connection, query_adaptors_cookie, NULL);

  if (!query_adaptors_reply) {
    xprintf(class->xine, XINE_VERBOSITY_DEBUG, LOG_MODULE ": XvQueryAdaptors failed.\n");
    return NULL;
  }

  adaptor_it = xcb_xv_query_adaptors_info_iterator(query_adaptors_reply);

  xv_port = 0;

  for (; adaptor_it.rem && !xv_port; xcb_xv_adaptor_info_next(&adaptor_it)) {

    if (adaptor_it.data->type & XCB_XV_TYPE_IMAGE_MASK) {

      for (j = 0; j < adaptor_it.data->num_ports; j++)
        if (!xv_check_yv12(this->connection, adaptor_it.data->base_id + j)) {
          xcb_xv_grab_port_cookie_t grab_port_cookie;
          xcb_xv_grab_port_reply_t *grab_port_reply;
          grab_port_cookie = xcb_xv_grab_port(this->connection, adaptor_it.data->base_id + j, XCB_CURRENT_TIME);
          grab_port_reply = xcb_xv_grab_port_reply(this->connection, grab_port_cookie, NULL);
          if (grab_port_reply && (grab_port_reply->result == XCB_GRAB_STATUS_SUCCESS)) {
            free(grab_port_reply);
            xv_port = adaptor_it.data->base_id + j;
            break;
          }
          free(grab_port_reply);
        }
    }
  }

  if (!xv_port) {
    xprintf(class->xine, XINE_VERBOSITY_LOG,
	    _("%s: Xv extension is present but I couldn't find a usable yuv12 port.\n"
	      "\tLooks like your graphics hardware driver doesn't support Xv?!\n"),
	    LOG_MODULE);
    
    /* XvFreeAdaptorInfo (adaptor_info); this crashed on me (gb)*/
    return NULL;
  } 
  else
    xprintf(class->xine, XINE_VERBOSITY_LOG,
	    _("%s: using Xv port %d from adaptor %s for hardware "
	      "colorspace conversion and scaling.\n"), LOG_MODULE, xv_port,
            xcb_xv_adaptor_info_name(adaptor_it.data));
  
  this->xv_port           = xv_port;

  _x_vo_scale_init (&this->sc, 1, 0, config );
  this->sc.frame_output_cb   = visual->frame_output_cb;
  this->sc.user_data         = visual->user_data;

  this->gc                      = xcb_generate_id(this->connection);
  xcb_create_gc(this->connection, this->gc, this->window, 0, NULL);
  this->capabilities            = VO_CAP_CROP;
  this->use_shm                 = 1;
  this->use_colorkey            = 0;
  this->colorkey                = 0;
  this->xoverlay                = NULL;
  this->ovl_changed             = 0;
  this->xine                    = class->xine;

  this->vo_driver.get_capabilities     = xv_get_capabilities;
  this->vo_driver.alloc_frame          = xv_alloc_frame;
  this->vo_driver.update_frame_format  = xv_update_frame_format;
  this->vo_driver.overlay_begin        = xv_overlay_begin;
  this->vo_driver.overlay_blend        = xv_overlay_blend;
  this->vo_driver.overlay_end          = xv_overlay_end;
  this->vo_driver.display_frame        = xv_display_frame;
  this->vo_driver.get_property         = xv_get_property;
  this->vo_driver.set_property         = xv_set_property;
  this->vo_driver.get_property_min_max = xv_get_property_min_max;
  this->vo_driver.gui_data_exchange    = xv_gui_data_exchange;
  this->vo_driver.dispose              = xv_dispose;
  this->vo_driver.redraw_needed        = xv_redraw_needed;

  /*
   * init properties
   */

  for (i = 0; i < VO_NUM_PROPERTIES; i++) {
    this->props[i].value = 0;
    this->props[i].min   = 0;
    this->props[i].max   = 0;
    this->props[i].atom  = XCB_NONE;
    this->props[i].entry = NULL;
    this->props[i].this  = this;
  }

  this->sc.user_ratio                        =
    this->props[VO_PROP_ASPECT_RATIO].value  = XINE_VO_ASPECT_AUTO;
  this->props[VO_PROP_ZOOM_X].value          = 100;
  this->props[VO_PROP_ZOOM_Y].value          = 100;

  /*
   * check this adaptor's capabilities
   */
  this->port_attributes = xine_list_new();

  query_attributes_cookie = xcb_xv_query_port_attributes(this->connection, xv_port);
  query_attributes_reply = xcb_xv_query_port_attributes_reply(this->connection, query_attributes_cookie, NULL);
  if(query_attributes_reply) {
    xcb_xv_attribute_info_iterator_t attribute_it;
    attribute_it = xcb_xv_query_port_attributes_attributes_iterator(query_attributes_reply);

    for (; attribute_it.rem; xcb_xv_attribute_info_next(&attribute_it)) {
      if ((attribute_it.data->flags & XCB_XV_ATTRIBUTE_FLAG_SETTABLE) && (attribute_it.data->flags & XCB_XV_ATTRIBUTE_FLAG_GETTABLE)) {
	/* store initial port attribute value */
	xv_store_port_attribute(this, xcb_xv_attribute_info_name(attribute_it.data));
	
	if(!strcmp(xcb_xv_attribute_info_name(attribute_it.data), "XV_HUE")) {
	  if (!strncmp(xcb_xv_adaptor_info_name(adaptor_it.data), "NV", 2)) {
            xprintf (this->xine, XINE_VERBOSITY_NONE, LOG_MODULE ": ignoring broken XV_HUE settings on NVidia cards\n");
	  } else {
	    xv_check_capability (this, VO_PROP_HUE, attribute_it.data,
			         adaptor_it.data->base_id,
			         NULL, NULL, NULL);
	  }
	} else if(!strcmp(xcb_xv_attribute_info_name(attribute_it.data), "XV_SATURATION")) {
	  xv_check_capability (this, VO_PROP_SATURATION, attribute_it.data,
			       adaptor_it.data->base_id,
			       NULL, NULL, NULL);

	} else if(!strcmp(xcb_xv_attribute_info_name(attribute_it.data), "XV_BRIGHTNESS")) {
	  xv_check_capability (this, VO_PROP_BRIGHTNESS, attribute_it.data,
			       adaptor_it.data->base_id,
			       NULL, NULL, NULL);

	} else if(!strcmp(xcb_xv_attribute_info_name(attribute_it.data), "XV_CONTRAST")) {
	  xv_check_capability (this, VO_PROP_CONTRAST, attribute_it.data,
			       adaptor_it.data->base_id,
			       NULL, NULL, NULL);

	} else if(!strcmp(xcb_xv_attribute_info_name(attribute_it.data), "XV_COLORKEY")) {
	  xv_check_capability (this, VO_PROP_COLORKEY, attribute_it.data,
			       adaptor_it.data->base_id,
			       "video.device.xv_colorkey",
			       _("video overlay colour key"),
			       _("The colour key is used to tell the graphics card where to "
				 "overlay the video image. Try different values, if you experience "
				 "windows becoming transparent."));

	} else if(!strcmp(xcb_xv_attribute_info_name(attribute_it.data), "XV_AUTOPAINT_COLORKEY")) {
	  xv_check_capability (this, VO_PROP_AUTOPAINT_COLORKEY, attribute_it.data,
			       adaptor_it.data->base_id,
			       "video.device.xv_autopaint_colorkey",
			       _("autopaint colour key"),
			       _("Make Xv autopaint its colorkey."));

	} else if(!strcmp(xcb_xv_attribute_info_name(attribute_it.data), "XV_FILTER")) {
	  int xv_filter;
	  /* This setting is specific to Permedia 2/3 cards. */
	  xv_filter = config->register_range (config, "video.device.xv_filter", 0,
					      attribute_it.data->min, attribute_it.data->max,
					      _("bilinear scaling mode"),
					      _("Selects the bilinear scaling mode for Permedia cards. "
						"The individual values are:\n\n"
						"Permedia 2\n"
						"0 - disable bilinear filtering\n"
						"1 - enable bilinear filtering\n\n"
						"Permedia 3\n"
						"0 - disable bilinear filtering\n"
						"1 - horizontal linear filtering\n"
						"2 - enable full bilinear filtering"),
					      20, xv_update_XV_FILTER, this);
	  config->update_num(config,"video.device.xv_filter",xv_filter);
	} else if(!strcmp(xcb_xv_attribute_info_name(attribute_it.data), "XV_DOUBLE_BUFFER")) {
	  int xv_double_buffer;
	  xv_double_buffer = 
	    config->register_bool (config, "video.device.xv_double_buffer", 1,
	      _("enable double buffering"),
	      _("Double buffering will synchronize the update of the video image to the "
		"repainting of the entire screen (\"vertical retrace\"). This eliminates "
		"flickering and tearing artifacts, but will use more graphics memory."),
	      20, xv_update_XV_DOUBLE_BUFFER, this);
	  config->update_num(config,"video.device.xv_double_buffer",xv_double_buffer);
	}
      }
    }
    free(query_attributes_reply);
  }
  else
    xprintf(this->xine, XINE_VERBOSITY_DEBUG, LOG_MODULE ": no port attributes defined.\n");
  free(query_adaptors_reply);

  /*
   * check supported image formats
   */

  list_formats_cookie = xcb_xv_list_image_formats(this->connection, xv_port);
  list_formats_reply = xcb_xv_list_image_formats_reply(this->connection, list_formats_cookie, NULL);

  format_it = xcb_xv_list_image_formats_format_iterator(list_formats_reply);
  
  this->xv_format_yv12 = 0;
  this->xv_format_yuy2 = 0;
  
  for (; format_it.rem; xcb_xv_image_format_info_next(&format_it)) {
    lprintf ("Xv image format: 0x%x (%4.4s) %s\n",
	     format_it.data->id, (char*)&format_it.data->id,
	     (format_it.data->format == XCB_XV_IMAGE_FORMAT_INFO_FORMAT_PACKED)
	       ? "packed" : "planar");

    if (format_it.data->id == XINE_IMGFMT_YV12)  {
      this->xv_format_yv12 = format_it.data->id;
      this->capabilities |= VO_CAP_YV12;
      xprintf(this->xine, XINE_VERBOSITY_LOG,
	      _("%s: this adaptor supports the yv12 format.\n"), LOG_MODULE);
    } else if (format_it.data->id == XINE_IMGFMT_YUY2) {
      this->xv_format_yuy2 = format_it.data->id;
      this->capabilities |= VO_CAP_YUY2;
      xprintf(this->xine, XINE_VERBOSITY_LOG, 
	      _("%s: this adaptor supports the yuy2 format.\n"), LOG_MODULE);
    }
  }

  free(list_formats_reply);

  this->use_pitch_alignment = 
    config->register_bool (config, "video.device.xv_pitch_alignment", 0,
			   _("pitch alignment workaround"),
			   _("Some buggy video drivers need a workaround to function properly."),
			   10, xv_update_xv_pitch_alignment, this);

  if(this->use_colorkey==1) {
    this->xoverlay = xcbosd_create(this->xine, this->connection, this->screen,
                                   this->window, XCBOSD_COLORKEY);
    if(this->xoverlay)
      xcbosd_colorkey(this->xoverlay, this->colorkey, &this->sc);
  } else {
    this->xoverlay = xcbosd_create(this->xine, this->connection, this->screen,
                                   this->window, XCBOSD_SHAPED);
  }

  if( this->xoverlay )
    this->capabilities |= VO_CAP_UNSCALED_OVERLAY;

  return &this->vo_driver;
}

/*
 * class functions
 */

static void dispose_class (video_driver_class_t *this_gen) {
  xv_class_t        *this = (xv_class_t *) this_gen;
  
  free (this);
}

static void *init_class (xine_t *xine, void *visual_gen) {
  xv_class_t        *this = (xv_class_t *) xine_xmalloc (sizeof (xv_class_t));

  this->driver_class.open_plugin     = open_plugin;
  this->driver_class.identifier      = "Xv";
  this->driver_class.description     = _("xine video output plugin using the MIT X video extension");
  this->driver_class.dispose         = dispose_class;

  this->config                       = xine->config;
  this->xine                         = xine;

  return this;
}

static const vo_info_t vo_info_xv = {
  9,                      /* priority    */
  XINE_VISUAL_TYPE_XCB    /* visual type */
};

/*
 * exported plugin catalog entry
 */

const plugin_info_t xine_plugin_info[] EXPORTED = {
  /* type, API, "name", version, special_info, init_function */
  { PLUGIN_VIDEO_OUT, 21, "xv", XINE_VERSION_CODE, &vo_info_xv, init_class },
  { PLUGIN_NONE, 0, "", 0, NULL, NULL }
};
