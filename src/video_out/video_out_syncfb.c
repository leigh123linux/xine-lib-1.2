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
 *
 * video_out_syncfb.c, SyncFB (for Matrox G200/G400 cards) interface for xine
 * 
 * based on video_out_xv.c     by (see file for original authors)
 * 
 * with lot's of code from:
 *          video_out_syncfb.c by Joachim Koenig   <joachim.koenig@gmx.net>
 *                         and by Matthias Oelmann <mao@well.com>
 *          video_out_mga      by Aaron Holtzman   <aholtzma@ess.engr.uvic.ca>
 * 
 * glued together for xine     by Matthias Dahl    <matthew2k@web.de>
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#ifdef __sun
#include <sys/ioccom.h>
#endif

#include <sys/ioctl.h>
#if defined (__FreeBSD__)
#include <sys/types.h>
#endif
#include <sys/mman.h>
#include <math.h>
#include <fcntl.h>
#include <unistd.h>

#include <X11/Xlib.h>
#include <X11/Xutil.h>

#include "video_out_syncfb.h"

#include "xine.h"
#include "video_out.h"
#include "xine_internal.h"
#include "xineutils.h"
#include "vo_scale.h"

/*#define DEBUG_OUTPUT*/

typedef struct syncfb_driver_s syncfb_driver_t;

typedef struct {
  int                value;
  int                min;
  int                max;
} syncfb_property_t;

typedef struct {
  vo_frame_t         vo_frame;
/*  uint8_t*           data_mem[3];*/
  int                width, height, format;
  double             ratio;
} syncfb_frame_t;

struct syncfb_driver_s {

  vo_driver_t        vo_driver;

  config_values_t   *config;

  /* X11 related stuff */
  Display           *display;
  int                screen;
  Drawable           drawable;
  XVisualInfo        vinfo;
  GC                 gc;
  XColor             black;

  vo_scale_t         sc;

  int                virtual_screen_width;
  int                virtual_screen_height;
  int                screen_depth;

  syncfb_property_t      props[VO_NUM_PROPERTIES];

  syncfb_frame_t*        cur_frame;
  vo_overlay_t*          overlay;

  /* syncfb module related stuff */
  int               fd;              /* file descriptor of the syncfb device */
  int               yuv_format;      /* either YUV420P3, YUV420P2 or YUV422  */
  int               overlay_state;   /* 0 = off, 1 = on                      */
  uint8_t*          video_mem;       /* mmapped video memory                 */
  int               default_repeat;  /* how many times a frame will be repeatedly displayed */
  uint32_t          supported_capabilities;

  syncfb_config_t      syncfb_config;
  syncfb_capability_t  capabilities;
  syncfb_buffer_info_t bufinfo;
  syncfb_param_t       params;

  int                  video_win_visibility;
  xine_t              *xine;

  alphablend_t         alphablend_extra_data;
};

typedef struct {
  video_driver_class_t  driver_class;

  config_values_t      *config;
  char                 *device_name;
  xine_t               *xine;
} syncfb_class_t;

/*
 * internal video_out_syncfb functions
 */

/* returns boolean value (1 success, 0 failure) */
static int syncfb_overlay_on(syncfb_driver_t* this)
{
   if(ioctl(this->fd, SYNCFB_ON)) {	
      xprintf(this->xine, XINE_VERBOSITY_DEBUG,
	      "video_out_syncfb: error. (on ioctl failed)\n");
      return 0;
   } else {
      this->overlay_state = 1;
      return 1;
   }
}

/* returns boolean value (1 success, 0 failure) */
static int syncfb_overlay_off(syncfb_driver_t* this)
{  
   if(ioctl(this->fd, SYNCFB_OFF)) {
      xprintf(this->xine, XINE_VERBOSITY_DEBUG,
	      "video_out_syncfb: error. (off ioctl failed)\n");
      return 0;
   } else {
      this->overlay_state = 0;
      return 1;
   }
}

static void write_frame_YUV422(syncfb_driver_t* this, syncfb_frame_t* frame)
{
   uint8_t*  y  = (uint_8 *)frame->vo_frame.base[0];
   uint8_t*  cb = (uint_8 *)frame->vo_frame.base[1];
   uint8_t*  cr = (uint_8 *)frame->vo_frame.base[2];
   uint8_t*  crp;
   uint8_t*  cbp;
   uint32_t* dst32 = (uint32_t *)(this->video_mem + this->bufinfo.offset);
   int h,w;

   for(h = 0; h < (frame->height / 2); h++) {
      cbp = cb;
      crp = cr;
      
      for(w = 0; w < (frame->width / 2); w++) {
	 *dst32++ = (*y) + ((*cb)<<8) + ((*(y+1))<<16) + ((*cr)<<24);
	 y++; y++; cb++; cr++;
      }

      dst32 += (this->syncfb_config.src_pitch - frame->width) / 2;

      for(w=0; w < (frame->width / 2); w++) {
	 *dst32++ = (*y) + ((*cbp)<<8) + ((*(y+1))<<16) + ((*crp)<<24);
	 y++; y++; cbp++; crp++;
      }
      
      dst32 += (this->syncfb_config.src_pitch - frame->width) / 2;
   }
}

static void write_frame_YUV420P2(syncfb_driver_t* this, syncfb_frame_t* frame)
{   
   uint8_t* y    = (uint8_t *)frame->vo_frame.base[0];
   uint8_t* cb   = (uint8_t *)frame->vo_frame.base[1];
   uint8_t* cr   = (uint8_t *)frame->vo_frame.base[2];
   uint8_t* dst8 = this->video_mem + this->bufinfo.offset_p2;
   int h, w; 
   
   register uint32_t* tmp32;
   register uint8_t*  rcr;
   register uint8_t*  rcb;

   rcr = cr;
   rcb = cb;
	
   for(h = 0; h < (frame->height / 2); h++) {
      tmp32 = (uint32_t *)dst8;
      w = (frame->width / 8) * 2;
      
      while(w--) {
	 register uint32_t temp;
	 
	 temp = (*rcb) | (*rcr << 8);
	 rcr++;
	 rcb++;
	 temp |= (*rcb << 16) | (*rcr << 24);
	 rcr++;
	 rcb++;
	 *tmp32 = temp;
	 tmp32++;
      }
      
      dst8 += this->syncfb_config.src_pitch;
   }

   dst8 = this->video_mem + this->bufinfo.offset;
   for(h = 0; h < frame->height; h++) {
      xine_fast_memcpy(dst8, y, frame->width);
      y    += frame->width;
      dst8 += this->syncfb_config.src_pitch;
   }
}

static void write_frame_YUV420P3(syncfb_driver_t* this, syncfb_frame_t* frame)
{   
   uint8_t* y    = (uint8_t *)frame->vo_frame.base[0];
   uint8_t* cb   = (uint8_t *)frame->vo_frame.base[1];
   uint8_t* cr   = (uint8_t *)frame->vo_frame.base[2];
   uint8_t* dst8 = this->video_mem + this->bufinfo.offset;
   int h, half_width = (frame->width/2);
   
   for(h = 0; h < frame->height; h++) {
      xine_fast_memcpy(dst8, y, frame->width);
      y    += frame->width;
      dst8 += this->syncfb_config.src_pitch;
   }

   dst8 = this->video_mem;
   for(h = 0; h < (frame->height / 2); h++) {
      xine_fast_memcpy((dst8 + this->bufinfo.offset_p2), cb, half_width);
      xine_fast_memcpy((dst8 + this->bufinfo.offset_p3), cr, half_width);
      
      cb   += half_width;
      cr   += half_width;
      
      dst8 += (this->syncfb_config.src_pitch / 2);
   }
}

static void write_frame_YUY2(syncfb_driver_t* this, syncfb_frame_t* frame)
{   
   uint8_t* src8 = (uint8_t *)frame->vo_frame.base[0];
   uint8_t* dst8 = (uint8_t *)(this->video_mem + this->bufinfo.offset);
   int h, double_width = (frame->width * 2);

   for(h = 0; h < frame->height; h++) {
      xine_fast_memcpy(dst8, src8, double_width);

      dst8 += (this->syncfb_config.src_pitch * 2);
      src8 += double_width;
   }
}

static void write_frame_sfb(syncfb_driver_t* this, syncfb_frame_t* frame)
{
   switch(frame->format) {   
   case XINE_IMGFMT_YUY2:
      if(this->capabilities.palettes & (1<<VIDEO_PALETTE_YUV422))
	write_frame_YUY2(this, frame);
      else
	xprintf(this->xine, XINE_VERBOSITY_LOG,
		_("video_out_syncfb: error. (YUY2 not supported by your graphic card)\n"));
      break;
      
   case XINE_IMGFMT_YV12:
      switch(this->yuv_format) {
       case VIDEO_PALETTE_YUV422:
	 write_frame_YUV422(this, frame);
	 break;
       case VIDEO_PALETTE_YUV420P2:
	 write_frame_YUV420P2(this, frame);
	 break;
       case VIDEO_PALETTE_YUV420P3:
	 write_frame_YUV420P3(this, frame);
	 break;
       default:
	 xprintf(this->xine, XINE_VERBOSITY_LOG,
		 _("video_out_syncfb: error. (YV12 not supported by your graphic card)\n"));
      }	   
      break;
      
    default:
      xprintf(this->xine, XINE_VERBOSITY_DEBUG, "video_out_syncfb: error. (unknown frame format)\n");
      break;
   }
   
   frame->vo_frame.free(&frame->vo_frame);
}

static void free_framedata(syncfb_frame_t* frame)
{
/*   if(frame->data_mem[0]) {
      free(frame->data_mem[0]);
      frame->data_mem[0] = NULL;
   }

   if(frame->data_mem[1]) {
      free(frame->data_mem[1]);
      frame->data_mem[1] = NULL;
   }

   if(frame->data_mem[2]) {
      free(frame->data_mem[2]);
      frame->data_mem[2] = NULL;
   }*/
   
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

static void syncfb_clean_output_area(syncfb_driver_t* this)
{
  XLockDisplay (this->display);

  XSetForeground (this->display, this->gc, this->black.pixel);

  XFillRectangle(this->display, this->drawable, this->gc,
		 this->sc.gui_x, this->sc.gui_y, this->sc.gui_width, this->sc.gui_height);

  XUnlockDisplay (this->display);
}


static void syncfb_compute_ideal_size (syncfb_driver_t *this)
{
  _x_vo_scale_compute_ideal_size( &this->sc );
}

/* make ideal width/height "fit" into the gui */
static void syncfb_compute_output_size(syncfb_driver_t *this)
{
  _x_vo_scale_compute_output_size( &this->sc );

#ifdef DEBUG_OUTPUT
  printf("video_out_syncfb: debug. (frame source %d x %d, screen output %d x %d)\n",
	 this->sc.delivered_width, this->sc.delivered_height,
	 this->sc.output_width, this->sc.output_height);
#endif

   /*
    * configuring SyncFB module from this point on.
    */
   syncfb_overlay_off(this);
   
   /* sanity checking - certain situations *may* crash the SyncFB module, so
    * take care that we always have valid numbers.
    */
   if(this->sc.output_xoffset >= 0 && this->sc.output_yoffset >= 0 && 
      this->cur_frame->width > 0 && this->cur_frame->height > 0 && 
      this->sc.output_width > 0 && this->sc.output_height > 0 && 
      this->cur_frame->format > 0 && this->video_win_visibility) {
	
      if(ioctl(this->fd, SYNCFB_GET_CONFIG, &this->syncfb_config))
	xprintf(this->xine, XINE_VERBOSITY_DEBUG, 
		"video_out_syncfb: error. (get_config ioctl failed)\n");
	
      this->syncfb_config.syncfb_mode = SYNCFB_FEATURE_SCALE | SYNCFB_FEATURE_CROP;
	
      if(this->props[VO_PROP_INTERLACED].value)
	this->syncfb_config.syncfb_mode |= SYNCFB_FEATURE_DEINTERLACE;
	
      switch(this->cur_frame->format) {
      case XINE_IMGFMT_YV12:
	 this->syncfb_config.src_palette = this->yuv_format;
	 break;
      case XINE_IMGFMT_YUY2:
	 this->syncfb_config.src_palette = VIDEO_PALETTE_YUV422;
	 break;
      default:
	 xprintf(this->xine, XINE_VERBOSITY_DEBUG, 
		 "video_out_syncfb: error. (unknown frame format)\n");
	 this->syncfb_config.src_palette = 0;
	 break;
      }
	
      this->syncfb_config.fb_screen_size = this->virtual_screen_width * this->virtual_screen_height * (this->screen_depth / 8) * 2;
      this->syncfb_config.src_width      = this->cur_frame->width;
      this->syncfb_config.src_height     = this->cur_frame->height;
	
      this->syncfb_config.image_width    = this->sc.output_width;
      this->syncfb_config.image_height   = this->sc.output_height;
	
      this->syncfb_config.image_xorg     = this->sc.output_xoffset + this->sc.gui_win_x;
      this->syncfb_config.image_yorg     = this->sc.output_yoffset + this->sc.gui_win_y;
	
      this->syncfb_config.src_crop_top   = this->sc.displayed_yoffset;
      this->syncfb_config.src_crop_bot   = (this->props[VO_PROP_INTERLACED].value && this->sc.displayed_yoffset == 0) ? 1 : this->sc.displayed_yoffset;
      this->syncfb_config.src_crop_left  = this->sc.displayed_xoffset;
      this->syncfb_config.src_crop_right = this->sc.displayed_xoffset;
	
      this->syncfb_config.default_repeat  = (this->props[VO_PROP_INTERLACED].value) ? 1 : this->default_repeat;
	
      if(this->capabilities.palettes & (1<<this->syncfb_config.src_palette)) {
	 if(ioctl(this->fd,SYNCFB_SET_CONFIG,&this->syncfb_config))
	   xprintf(this->xine, XINE_VERBOSITY_DEBUG, "video_out_syncfb: error. (set_config ioctl failed)\n");
	  
	 syncfb_overlay_on(this);
      }
   }
}

/*
 * public functions defined and used by the xine interface
 */

static int syncfb_redraw_needed(vo_driver_t* this_gen)
{
  syncfb_driver_t* this = (syncfb_driver_t *) this_gen;

  int ret = 0;
  
  if( _x_vo_scale_redraw_needed( &this->sc ) ) {

    syncfb_compute_output_size (this);

    syncfb_clean_output_area (this);
    
    ret = 1;
  }
  
  return ret;
}

static uint32_t syncfb_get_capabilities (vo_driver_t *this_gen)
{
  syncfb_driver_t *this = (syncfb_driver_t *) this_gen;

  return this->supported_capabilities;
}

static void syncfb_frame_field (vo_frame_t *vo_img, int which_field)
{
  /* not needed for SyncFB */
}

static void syncfb_frame_dispose(vo_frame_t* vo_img)
{
  syncfb_frame_t* frame = (syncfb_frame_t *) vo_img;
  
  if(frame) {
    free_framedata(frame);
    free(frame);
  }
}

static vo_frame_t* syncfb_alloc_frame(vo_driver_t* this_gen)
{
  /* syncfb_driver_t *this = (syncfb_driver_t *) this_gen; */
  syncfb_frame_t  *frame;
  
  frame = (syncfb_frame_t *) xine_xmalloc(sizeof(syncfb_frame_t));
  if(!frame)
    return NULL;
  
  pthread_mutex_init(&frame->vo_frame.mutex, NULL);
  
  frame->vo_frame.base[0] = NULL;
  frame->vo_frame.base[1] = NULL;
  frame->vo_frame.base[2] = NULL;
  
  /*
   * supply required functions
   */
  frame->vo_frame.proc_slice = NULL;
  frame->vo_frame.proc_frame = NULL;
  frame->vo_frame.field      = syncfb_frame_field;
  frame->vo_frame.dispose    = syncfb_frame_dispose;
  
  frame->vo_frame.driver  = this_gen;
  
  return (vo_frame_t *) frame;
}

static void syncfb_update_frame_format(vo_driver_t* this_gen,
				       vo_frame_t* frame_gen,
				       uint32_t width, uint32_t height,
				       double ratio, int format, int flags)
{
   syncfb_driver_t *this = (syncfb_driver_t *) this_gen;
   syncfb_frame_t  *frame = (syncfb_frame_t *) frame_gen;
   /* uint32_t frame_size   = width*height; */
   
   if((frame->width != width)
      || (frame->height != height)
      || (frame->format != format)) {

#ifdef DEBUG_OUTPUT
      printf("video_out_syncfb: debug. (update frame format: old values [width=%d, height=%d, format=%04x], new values [width=%d, height=%d, format=%04x])\n", frame->width, frame->height, frame->format, width, height, format);
#endif
      free_framedata(frame);
      
      frame->width  = width;
      frame->height = height;
      frame->format = format;

      switch(format) {
      case XINE_IMGFMT_YV12:
/*	 frame->vo_frame.base[0] = xine_xmalloc_aligned(16, frame_size, (void **)&frame->data_mem[0]);
         frame->vo_frame.base[1] = xine_xmalloc_aligned(16, frame_size/4, (void **)&frame->data_mem[1]);
	 frame->vo_frame.base[2] = xine_xmalloc_aligned(16, frame_size/4, (void **)&frame->data_mem[2]);*/
	 frame->vo_frame.pitches[0] = 8*((width + 7) / 8);
	 frame->vo_frame.pitches[1] = 8*((width + 15) / 16);
	 frame->vo_frame.pitches[2] = 8*((width + 15) / 16);
	 frame->vo_frame.base[0] = malloc(frame->vo_frame.pitches[0] * height);
         frame->vo_frame.base[1] = malloc(frame->vo_frame.pitches[1] * ((height+1)/2));
	 frame->vo_frame.base[2] = malloc(frame->vo_frame.pitches[2] * ((height+1)/2));
	 break;
      case XINE_IMGFMT_YUY2:
/*	 frame->vo_frame.base[0] = xine_xmalloc_aligned(16, (frame_size*2), (void **)&frame->data_mem[0]);*/
	 frame->vo_frame.pitches[0] = 8*((width + 3) / 4);
	 frame->vo_frame.base[0] = malloc(frame->vo_frame.pitches[0] * height);
	 frame->vo_frame.base[1] = NULL;
	 frame->vo_frame.base[2] = NULL;
	 break;
      default:
	 xprintf(this->xine, XINE_VERBOSITY_DEBUG,
		 "video_out_syncfb: error. (unable to allocate "
		 "framedata because of unknown frame format: %04x)\n", format);
      }
      
/*      if((format == IMGFMT_YV12 && (frame->data_mem[0] == NULL || frame->data_mem[1] == NULL || frame->data_mem[2] == NULL))
	 || (format == IMGFMT_YUY2 && frame->data_mem[0] == NULL)) {*/
      if((format == XINE_IMGFMT_YV12 && (frame->vo_frame.base[0] == NULL || frame->vo_frame.base[1] == NULL || frame->vo_frame.base[2] == NULL))
	 || (format == XINE_IMGFMT_YUY2 && frame->vo_frame.base[0] == NULL)) {
	 xprintf(this->xine, XINE_VERBOSITY_DEBUG,
		 "video_out_syncfb: error. (framedata allocation failed: out of memory)\n");
	 
	 free_framedata(frame);
      }
   }

  frame->ratio = ratio;
}

static void syncfb_overlay_blend(vo_driver_t* this_gen, vo_frame_t* frame_gen, vo_overlay_t* overlay)
{
  syncfb_frame_t* frame = (syncfb_frame_t *) frame_gen;
  syncfb_driver_t* this = (syncfb_driver_t *) this_gen;
   
  this->alphablend_extra_data.offset_x = frame_gen->overlay_offset_x;
  this->alphablend_extra_data.offset_y = frame_gen->overlay_offset_y;

  /* alpha blend here */
  if (overlay->rle) {
    if (frame->format == XINE_IMGFMT_YV12)
      _x_blend_yuv(frame->vo_frame.base, overlay, frame->width, frame->height, frame->vo_frame.pitches, &this->alphablend_extra_data);
    else
      _x_blend_yuy2(frame->vo_frame.base[0], overlay, frame->width, frame->height, frame->vo_frame.pitches[0], &this->alphablend_extra_data);
  }
}

static void syncfb_display_frame(vo_driver_t* this_gen, vo_frame_t* frame_gen)
{
  syncfb_driver_t* this  = (syncfb_driver_t *) this_gen;
  syncfb_frame_t*  frame = (syncfb_frame_t *) frame_gen;

  this->cur_frame = frame;

   /*
    * let's see if this frame is different in size / aspect
    * ratio from the previous one
    */
   if((frame->width != this->sc.delivered_width)
      || (frame->height != this->sc.delivered_height)
      || (frame->ratio != this->sc.delivered_ratio)) {
#ifdef DEBUG_OUTPUT
      printf("video_out_syncfb: debug. (frame format changed)\n");
#endif

      this->sc.delivered_width  = frame->width;
      this->sc.delivered_height = frame->height;
      this->sc.delivered_ratio  = frame->ratio;
    
      this->sc.crop_left        = frame->vo_frame.crop_left;
      this->sc.crop_right       = frame->vo_frame.crop_right;
      this->sc.crop_top         = frame->vo_frame.crop_top;
      this->sc.crop_bottom      = frame->vo_frame.crop_bottom;

      syncfb_compute_ideal_size(this);
      
      this->sc.force_redraw = 1;
   }

   /* 
    * tell gui that we are about to display a frame,
    * ask for offset and output size
    */   
   syncfb_redraw_needed(this_gen);
    
   /* the rest is only successful and safe, if the overlay is really on */
   if(this->overlay_state) {
      if(this->bufinfo.id != -1) {
	 xprintf(this->xine, XINE_VERBOSITY_DEBUG,
		 "video_out_syncfb: error. (invalid syncfb image buffer state)\n");
	 frame->vo_frame.free(&frame->vo_frame);

	 return;
      }
      
      if(ioctl(this->fd, SYNCFB_REQUEST_BUFFER, &this->bufinfo))
	xprintf(this->xine, XINE_VERBOSITY_DEBUG, "video_out_syncfb: error. (request ioctl failed)\n");
   
      if(this->bufinfo.id == -1) {
	 xprintf(this->xine, XINE_VERBOSITY_DEBUG, 
		 "video_out_syncfb: error. (syncfb module couldn't allocate image buffer)\n");
	 frame->vo_frame.free(&frame->vo_frame);
	 
	 /* 
	  * there are several "fixable" situations when this request will fail.
	  * for example when the screen resolution changes, the kernel module
	  * will get confused - reinitializing everything will fix things for
	  * the next frame in that case.
	  */
	 syncfb_compute_ideal_size(this);
	 syncfb_compute_output_size(this);
	 syncfb_clean_output_area(this);     
  	
	 return;
      }
      
      write_frame_sfb(this, frame);
      
      if(ioctl(this->fd, SYNCFB_COMMIT_BUFFER, &this->bufinfo))
	xprintf(this->xine, XINE_VERBOSITY_DEBUG, "video_out_syncfb: error. (commit ioctl failed)\n");
   }
   else
     frame->vo_frame.free(&frame->vo_frame);
   
   this->bufinfo.id = -1;   
}

static int syncfb_get_property(vo_driver_t* this_gen, int property)
{
  syncfb_driver_t* this = (syncfb_driver_t *) this_gen;
  
  switch (property) {
    case VO_PROP_WINDOW_WIDTH:
      this->props[property].value = this->sc.gui_width;
      break;
    case VO_PROP_WINDOW_HEIGHT:
      this->props[property].value = this->sc.gui_height;
      break;
  }
  
  return this->props[property].value;
}

static int syncfb_set_property(vo_driver_t* this_gen, int property, int value)
{
  syncfb_driver_t* this = (syncfb_driver_t *) this_gen;
  
  switch (property) {
    case VO_PROP_INTERLACED:
      this->props[property].value = value;

#ifdef DEBUG_OUTPUT
      printf("video_out_syncfb: debug. (VO_PROP_INTERLACED(%d))\n", 
	     this->props[property].value);
#endif
     
      syncfb_compute_ideal_size(this);
      syncfb_compute_output_size(this);
      syncfb_clean_output_area(this);     
      break;
     
    case VO_PROP_ASPECT_RATIO:
      if(value >= XINE_VO_ASPECT_NUM_RATIOS)
	value = XINE_VO_ASPECT_AUTO;

      this->props[property].value = value;
      this->sc.user_ratio = value;

#ifdef DEBUG_OUTPUT
      printf("video_out_syncfb: debug. (VO_PROP_ASPECT_RATIO(%d))\n",
	     this->props[property].value);
#endif

      syncfb_compute_ideal_size(this);
      syncfb_compute_output_size(this);
      syncfb_clean_output_area(this);
      break;
     
    case VO_PROP_ZOOM_X:
      if ((value >= XINE_VO_ZOOM_MIN) && (value <= XINE_VO_ZOOM_MAX)) {
        this->props[property].value = value;
	this->sc.zoom_factor_x = (double)value / (double)XINE_VO_ZOOM_STEP;
	           
	syncfb_compute_ideal_size (this);
      
	this->sc.force_redraw = 1;
      }
/*
      printf("video_out_syncfb: info. (the zooming feature is not supported at the moment because of a bug with the SyncFB kernel driver, please refer to README.syncfb)\n");
*/
      break;

    case VO_PROP_ZOOM_Y:
      if ((value >= XINE_VO_ZOOM_MIN) && (value <= XINE_VO_ZOOM_MAX)) {
        this->props[property].value = value;
	this->sc.zoom_factor_y = (double)value / (double)XINE_VO_ZOOM_STEP;
	           
	syncfb_compute_ideal_size (this);
      
	this->sc.force_redraw = 1;
      }
/*
      printf("video_out_syncfb: info. (the zooming feature is not supported at the moment because of a bug with the SyncFB kernel driver, please refer to README.syncfb)\n");
*/
      break;
     
    case VO_PROP_CONTRAST:
      this->props[property].value = value;

#ifdef DEBUG_OUTPUT
      printf("video_out_syncfb: debug. (VO_PROP_CONTRAST(%d))\n",
             this->props[property].value);
#endif

      this->params.contrast     = value;
      this->params.brightness   = this->props[VO_PROP_BRIGHTNESS].value;
      this->params.image_width  = this->syncfb_config.image_width;       /* FIXME */
      this->params.image_height = this->syncfb_config.image_height;
      this->params.image_xorg   = this->syncfb_config.image_xorg;
      this->params.image_yorg   = this->syncfb_config.image_yorg;
      
      if(ioctl(this->fd,SYNCFB_SET_PARAMS,&this->params))
	 xprintf(this->xine, XINE_VERBOSITY_DEBUG,
		 "video_out_syncfb: error. (setting of contrast value failed)\n");

      break;

    case VO_PROP_BRIGHTNESS:
      this->props[property].value = value;

#ifdef DEBUG_OUTPUT
      printf("video_out_syncfb: debug. (VO_PROP_BRIGHTNESS(%d))\n",
             this->props[property].value);
#endif
     
      this->params.brightness   = value;
      this->params.contrast     = this->props[VO_PROP_CONTRAST].value;
      this->params.image_width  = this->syncfb_config.image_width;       /* FIXME */
      this->params.image_height = this->syncfb_config.image_height;
      this->params.image_xorg   = this->syncfb_config.image_xorg;
      this->params.image_yorg   = this->syncfb_config.image_yorg;
    
      if(ioctl(this->fd,SYNCFB_SET_PARAMS,&this->params))
	 xprintf(this->xine, XINE_VERBOSITY_DEBUG,
		 "video_out_syncfb: error. (setting of brightness value failed)\n");
     
      break;
  } 

  return value;
}

static void syncfb_get_property_min_max(vo_driver_t *this_gen, 
					int property, int *min, int *max)
{
  syncfb_driver_t* this = (syncfb_driver_t *) this_gen;

  *min = this->props[property].min;
  *max = this->props[property].max;
}

static int syncfb_gui_data_exchange(vo_driver_t* this_gen, int data_type,
				    void *data)
{
  syncfb_driver_t* this = (syncfb_driver_t *) this_gen;
   
  switch (data_type) {
   case XINE_GUI_SEND_DRAWABLE_CHANGED:
     this->drawable = (Drawable) data;

     XLockDisplay (this->display);
     XFreeGC(this->display, this->gc);
     this->gc       = XCreateGC (this->display, this->drawable, 0, NULL);
     XUnlockDisplay (this->display);
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
   /*
   case XINE_GUI_DATA_EX_VIDEOWIN_VISIBLE:
     this->video_win_visibility = (int)(int *)data;
     syncfb_compute_output_size(this);
     break;
   */  
  
   default:
     return -1;
  }

  return 0;
}

static void syncfb_dispose(vo_driver_t *this_gen)
{
  syncfb_driver_t *this = (syncfb_driver_t *) this_gen;

  /* get it off the screen - I wanna see my desktop again :-) */
  syncfb_overlay_off(this);
  
  /* don't know if it is necessary are even right, but anyway...?! */
  munmap(0, this->capabilities.memory_size);   
  
  close(this->fd);

  XLockDisplay (this->display);
  XFreeGC(this->display, this->gc);
  XUnlockDisplay (this->display);
  
  _x_alphablend_free(&this->alphablend_extra_data);
  
  free(this);
}

static vo_driver_t *open_plugin (video_driver_class_t *class_gen, const void *visual_gen) {
   
   syncfb_class_t   *class = (syncfb_class_t *) class_gen;
   config_values_t  *config = class->config;
   syncfb_driver_t*  this;
   Display*          display = NULL; 
   unsigned int      i;
   x11_visual_t*     visual = (x11_visual_t *) visual_gen;
   XColor            dummy;
   XWindowAttributes attr;

   display     = visual->display;
   
   if(!(this = xine_xmalloc(sizeof (syncfb_driver_t))))
     return NULL;
 
   _x_alphablend_init(&this->alphablend_extra_data, class->xine);
  
   /* check for syncfb device */
   if((this->fd = open(class->device_name, O_RDWR)) < 0) {
      xprintf(class->xine, XINE_VERBOSITY_DEBUG,
	      "video_out_syncfb: aborting. (unable to open syncfb device \"%s\")\n", class->device_name);
      free(this);
      return NULL;
   }
   
   this->xine = class->xine;

   /* get capabilities from the syncfb module */
   if(ioctl(this->fd, SYNCFB_GET_CAPS, &this->capabilities)) {
      xprintf(this->xine, XINE_VERBOSITY_DEBUG,
	      "video_out_syncfb: aborting. (syncfb_get_caps ioctl failed)\n");
      
      close(this->fd);
      free(this);
      
      return NULL;
   }

   /* mmap whole video memory */
   this->video_mem = (uint8_t *) mmap(0, this->capabilities.memory_size, PROT_WRITE, MAP_SHARED, this->fd, 0);

   if(this->video_mem == MAP_FAILED) {
      xprintf(this->xine, XINE_VERBOSITY_DEBUG,
	      "video_out_syncfb: aborting. (mmap of video memory failed)\n");
      
      close(this->fd);
      free(this);
      
      return NULL;
   }
	
   /*
    * init properties and capabilities
    */   
   for (i = 0; i<VO_NUM_PROPERTIES; i++) {
      this->props[i].value = 0;
      this->props[i].min   = 0;
      this->props[i].max   = 0;
   }

   this->props[VO_PROP_INTERLACED].value     = 0;
   this->sc.user_ratio = this->props[VO_PROP_ASPECT_RATIO].value   = XINE_VO_ASPECT_AUTO;
   this->props[VO_PROP_ZOOM_X].value    = 100;
   this->props[VO_PROP_ZOOM_Y].value    = 100;

   /* check for formats we need... */
   this->supported_capabilities = VO_CAP_CROP;
   this->yuv_format = 0;

   /*
    * simple fallback mechanism - we want YUV 4:2:0 (3 plane) but we can also
    * convert YV12 material to YUV 4:2:0 (2 plane) and YUV 4:2:2 ...
    */
   if(this->capabilities.palettes & (1<<VIDEO_PALETTE_YUV420P3)) {
      this->supported_capabilities |= VO_CAP_YV12;
      this->yuv_format = VIDEO_PALETTE_YUV420P3;
      xprintf(this->xine, XINE_VERBOSITY_LOG,
	      _("video_out_syncfb: info. (SyncFB module supports YUV 4:2:0 (3 plane))\n"));
   } else if(this->capabilities.palettes & (1<<VIDEO_PALETTE_YUV420P2)) {
      this->supported_capabilities |= VO_CAP_YV12;
      this->yuv_format = VIDEO_PALETTE_YUV420P2;
      xprintf(this->xine, XINE_VERBOSITY_LOG,
	      _("video_out_syncfb: info. (SyncFB module supports YUV 4:2:0 (2 plane))\n"));
   } else if(this->capabilities.palettes & (1<<VIDEO_PALETTE_YUV422)) {	   
      this->supported_capabilities |= VO_CAP_YV12;
      this->yuv_format = VIDEO_PALETTE_YUV422;
      xprintf(this->xine, XINE_VERBOSITY_DEBUG,
	      _("video_out_syncfb: info. (SyncFB module supports YUV 4:2:2)\n"));
   }
   
   if(this->capabilities.palettes & (1<<VIDEO_PALETTE_YUV422)) {
      this->supported_capabilities |= VO_CAP_YUY2;
      xprintf(this->xine, XINE_VERBOSITY_DEBUG,
	      _("video_out_syncfb: info. (SyncFB module supports YUY2)\n"));
   }
   if(this->capabilities.palettes & (1<<VIDEO_PALETTE_RGB565)) {
     /* FIXME: no RGB support yet
      *      this->supported_capabilities |= VO_CAP_RGB;
      */
      xprintf(this->xine, XINE_VERBOSITY_DEBUG,
	      _("video_out_syncfb: info. (SyncFB module supports RGB565)\n"));
   }

   if(!this->supported_capabilities) {
      xprintf(this->xine, XINE_VERBOSITY_DEBUG,
	      _("video_out_syncfb: aborting. (SyncFB module does not support YV12, YUY2 nor RGB565)\n"));
      
      munmap(0, this->capabilities.memory_size);   
      close(this->fd);
      free(this);
      
      return NULL;
   }

   if(ioctl(this->fd,SYNCFB_GET_PARAMS,&this->params) == 0) {
      this->props[VO_PROP_CONTRAST].value = this->params.contrast;
      this->props[VO_PROP_CONTRAST].min   = 0;
      this->props[VO_PROP_CONTRAST].max   = 255;

      this->props[VO_PROP_BRIGHTNESS].value = this->params.brightness;
      this->props[VO_PROP_BRIGHTNESS].min   = -128;
      this->props[VO_PROP_BRIGHTNESS].max   = 127;
   } else {
     xprintf(this->xine, XINE_VERBOSITY_LOG,
	     _("video_out_syncfb: info. (brightness/contrast control won\'t be available because "
	       "your SyncFB kernel module seems to be outdated. Please refer to README."
	       "syncfb for informations on how to update it.)\n"));
   }

  /* check for virtual screen size and screen depth - this is rather important
     because that data is later used for free memory calculation */
  XGetWindowAttributes(visual->display, DefaultRootWindow(visual->display), &attr);
   
  this->virtual_screen_height = attr.height;
  this->virtual_screen_width  = attr.width;
  this->screen_depth          = attr.depth;
  
  /* initialize the rest of the variables now with default values */
  this->bufinfo.id           = -1;
  this->config               = config;
  this->cur_frame            = NULL;
   
  /* FIXME: setting the default_repeat to anything higher than 1 will result
            in a distorted video, so for now, set this manually to 0 until
            the kernel driver is fixed... */
#if 0
  this->default_repeat       = config->register_range(config, 
						      "video.device.syncfb_default_repeat", 3, 1, 4,
						      _("default number of frame repetitions"),
						      _("This specifies how many times a single video "
						        "frame will be displayed consecutively."),
						      10, NULL, NULL);
#endif
  this->default_repeat       = 0;
 
  this->display              = visual->display;
  this->drawable             = visual->d;
  this->gc                   = XCreateGC (this->display, this->drawable, 0, NULL);

  _x_vo_scale_init (&this->sc, 1, 0, config );
  this->sc.frame_output_cb   = visual->frame_output_cb;
  this->sc.user_data         = visual->user_data;

  this->overlay              = NULL;
  this->screen               = visual->screen;   
  this->video_win_visibility = 1;
   
  XAllocNamedColor(this->display,
		   DefaultColormap(this->display, this->screen),
		   "black", &this->black, &dummy);

  this->vo_driver.get_capabilities     = syncfb_get_capabilities;
  this->vo_driver.alloc_frame          = syncfb_alloc_frame;
  this->vo_driver.update_frame_format  = syncfb_update_frame_format;
  this->vo_driver.overlay_begin        = NULL; /* not used */
  this->vo_driver.overlay_blend        = syncfb_overlay_blend;
  this->vo_driver.overlay_end          = NULL; /* not used */
  this->vo_driver.display_frame        = syncfb_display_frame;
  this->vo_driver.get_property         = syncfb_get_property;
  this->vo_driver.set_property         = syncfb_set_property;
  this->vo_driver.get_property_min_max = syncfb_get_property_min_max;
  this->vo_driver.gui_data_exchange    = syncfb_gui_data_exchange;
  this->vo_driver.dispose              = syncfb_dispose;
  this->vo_driver.redraw_needed        = syncfb_redraw_needed;

  return &this->vo_driver;
}

/*
 * class functions
 */

static void dispose_class (video_driver_class_t *this_gen) {

  syncfb_class_t        *this = (syncfb_class_t *) this_gen;

  free (this);
}

static void *init_class (xine_t *xine, void *visual_gen) {

  syncfb_class_t    *this;
  char*             device_name;
  int               fd;

  device_name = xine->config->register_filename(xine->config, "video.device.syncfb_device", "/dev/syncfb",
				  	XINE_CONFIG_STRING_IS_DEVICE_NAME,
					_("SyncFB device name"),
					_("Specifies the file name for the SyncFB (TeleTux) device "
					  "to be used.\nThis setting is security critical, "
					  "because when changed to a different file, xine "
					  "can be used to fill this file with arbitrary content. "
					  "So you should be careful that the value you enter "
					  "really is a proper framebuffer device."),
					XINE_CONFIG_SECURITY, NULL, NULL);
  
  /* check for syncfb device */
  if((fd = open(device_name, O_RDWR)) < 0) {
     xprintf(xine, XINE_VERBOSITY_DEBUG,
	     "video_out_syncfb: aborting. (unable to open syncfb device \"%s\")\n", device_name);
     return NULL;
  }
  close(fd);
    
  /*
   * from this point on, nothing should go wrong anymore
   */
  this = (syncfb_class_t *) xine_xmalloc (sizeof (syncfb_class_t));

  this->driver_class.open_plugin     = open_plugin;
  this->driver_class.identifier      = "SyncFB";
  this->driver_class.description     = N_("xine video output plugin using the SyncFB module for Matrox G200/G400 cards");
  this->driver_class.dispose         = dispose_class;

  this->config            = xine->config;
  this->xine              = xine;
  this->device_name       = device_name;
  
  return this;
}

static const vo_info_t vo_info_syncfb = {
  7,                    /* priority    */
  XINE_VISUAL_TYPE_X11  /* visual type */
};

/*
 * exported plugin catalog entry
 */

const plugin_info_t xine_plugin_info[] EXPORTED = {
  /* type, API, "name", version, special_info, init_function */
  { PLUGIN_VIDEO_OUT, 21, "SyncFB", XINE_VERSION_CODE, &vo_info_syncfb, init_class },
  { PLUGIN_NONE, 0, "", 0, NULL, NULL }
};

