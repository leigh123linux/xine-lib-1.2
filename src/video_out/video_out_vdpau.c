/*
 * Copyright (C) 2008 the xine project
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
 *
 * video_out_vdpau.c, a video output plugin using VDPAU (Video Decode and Presentation Api for Unix)
 *
 * Christophe Thommeret <hftom@free.fr>
 *
 */

/* #define LOG */
#define LOG_MODULE "video_out_vdpau"

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <math.h>
#include <errno.h>
#include <ctype.h>
#include <pthread.h>

#include "xine.h"
#include "video_out.h"
#include "vo_scale.h"
#include "xine_internal.h"
#include "yuv2rgb.h"
#include "xineutils.h"

#include <vdpau/vdpau_x11.h>
#include "accel_vdpau.h"



VdpDevice vdp_device;
VdpPresentationQueue vdp_queue;
VdpPresentationQueueTarget vdp_queue_target;

VdpGetProcAddress *vdp_get_proc_address;

VdpGetApiVersion *vdp_get_api_version;
VdpGetInformationString *vdp_get_information_string;
VdpGetErrorString *vdp_get_error_string;

VdpVideoSurfaceQueryGetPutBitsYCbCrCapabilities *vdp_video_surface_query_get_put_bits_ycbcr_capabilities;
VdpVideoSurfaceCreate *vdp_video_surface_create;
VdpVideoSurfaceDestroy *vdp_video_surface_destroy;
VdpVideoSurfacePutBitsYCbCr *vdp_video_surface_putbits_ycbcr;

VdpOutputSurfaceCreate *vdp_output_surface_create;
VdpOutputSurfaceDestroy *vdp_output_surface_destroy;

VdpVideoMixerCreate *vdp_video_mixer_create;
VdpVideoMixerDestroy *vdp_video_mixer_destroy;
VdpVideoMixerRender *vdp_video_mixer_render;

VdpPresentationQueueTargetCreateX11 *vdp_queue_target_create_x11;
VdpPresentationQueueTargetDestroy *vdp_queue_target_destroy;
VdpPresentationQueueCreate *vdp_queue_create;
VdpPresentationQueueDestroy *vdp_queue_destroy;
VdpPresentationQueueDisplay *vdp_queue_display;
VdpPresentationQueueSetBackgroundColor *vdp_queue_set_backgroung_color;

VdpDecoderQueryCapabilities *vdp_decoder_query_capabilities;
VdpDecoderCreate *vdp_decoder_create;
VdpDecoderDestroy *vdp_decoder_destroy;
VdpDecoderRender *vdp_decoder_render;


typedef struct {
  vo_frame_t         vo_frame;

  int                width, height, format, flags;
  double             ratio;
  uint8_t           *chunk[3]; /* mem alloc by xmalloc_aligned           */

  vdpau_accel_t     vdpau_accel_data;
} vdpau_frame_t;


typedef struct {
  vo_driver_t        vo_driver;
  vo_scale_t         sc;

  Display           *display;
  int                screen;
  Drawable           drawable;

  config_values_t   *config;

  int ovl_changed;
  raw_overlay_t overlay;
  yuv2rgb_t *ovl_yuv2rgb;

  VdpVideoSurface soft_surface;
  uint32_t             soft_surface_width;
  uint32_t             soft_surface_height;
  int                  soft_surface_format;

  VdpOutputSurface output_surface;
  uint32_t             output_surface_width;
  uint32_t             output_surface_height;

  VdpVideoMixer        video_mixer;
  VdpChromaType        video_mixer_chroma;
  uint32_t             video_mixer_width;
  uint32_t             video_mixer_height;

  uint32_t          capabilities;
  xine_t            *xine;
} vdpau_driver_t;


typedef struct {
  video_driver_class_t driver_class;
  xine_t              *xine;
} vdpau_class_t;



static void vdpau_overlay_clut_yuv2rgb(vdpau_driver_t  *this, vo_overlay_t *overlay, vdpau_frame_t *frame)
{
  int i;
  clut_t* clut = (clut_t*) overlay->color;

  if (!overlay->rgb_clut) {
    for ( i=0; i<sizeof(overlay->color)/sizeof(overlay->color[0]); i++ ) {
      *((uint32_t *)&clut[i]) = this->ovl_yuv2rgb->yuv2rgb_single_pixel_fun(this->ovl_yuv2rgb, clut[i].y, clut[i].cb, clut[i].cr);
    }
    overlay->rgb_clut++;
  }
  if (!overlay->hili_rgb_clut) {
    clut = (clut_t*) overlay->hili_color;
    for ( i=0; i<sizeof(overlay->color)/sizeof(overlay->color[0]); i++) {
      *((uint32_t *)&clut[i]) = this->ovl_yuv2rgb->yuv2rgb_single_pixel_fun(this->ovl_yuv2rgb, clut[i].y, clut[i].cb, clut[i].cr);
    }
    overlay->hili_rgb_clut++;
  }
}


static int vdpau_process_ovl( vdpau_driver_t *this_gen, vo_overlay_t *overlay )
{
  raw_overlay_t *ovl = &this_gen->overlay;

  if ( overlay->width<=0 || overlay->height<=0 )
    return 0;

  if ( (overlay->width*overlay->height)!=(ovl->ovl_w*ovl->ovl_h) )
    ovl->ovl_rgba = (uint8_t*)realloc( ovl->ovl_rgba, overlay->width*overlay->height*4 );
  ovl->ovl_w = overlay->width;
  ovl->ovl_h = overlay->height;
  ovl->ovl_x = overlay->x;
  ovl->ovl_y = overlay->y;

  int num_rle = overlay->num_rle;
  rle_elem_t *rle = overlay->rle;
  uint8_t *rgba = ovl->ovl_rgba;
  clut_t *low_colors = (clut_t*)overlay->color;
  clut_t *hili_colors = (clut_t*)overlay->hili_color;
  uint8_t *low_trans = overlay->trans;
  uint8_t *hili_trans = overlay->hili_trans;
  clut_t *colors;
  uint8_t *trans;
  uint8_t alpha;
  int rlelen = 0;
  uint8_t clr = 0;
  int i, pos=0, x, y;

  while ( num_rle>0 ) {
    x = pos%ovl->ovl_w;
    y = pos/ovl->ovl_w;
    if ( (x>=overlay->hili_left && x<=overlay->hili_right) && (y>=overlay->hili_top && y<=overlay->hili_bottom) ) {
    	colors = hili_colors;
    	trans = hili_trans;
    }
    else {
    	colors = low_colors;
    	trans = low_trans;
    }
    rlelen = rle->len;
    clr = rle->color;
    alpha = trans[clr];
    for ( i=0; i<rlelen; ++i ) {
    	rgba[0] = colors[clr].y;
    	rgba[1] = colors[clr].cr;
    	rgba[2] = colors[clr].cb;
    	rgba[3] = alpha*255/15;
    	rgba+= 4;
    	++pos;
    }
    ++rle;
    --num_rle;
  }
  return 1;
}


static void vdpau_overlay_begin (vo_driver_t *this_gen, vo_frame_t *frame_gen, int changed)
{
  /*vdpau_driver_t  *this = (vdpau_driver_t *) this_gen;

  if ( !changed )
  	return;

  ++this->ovl_changed;*/
}


static void vdpau_overlay_blend (vo_driver_t *this_gen, vo_frame_t *frame_gen, vo_overlay_t *overlay)
{
  /*vdpau_driver_t  *this = (vdpau_driver_t *) this_gen;
  vdpau_frame_t *frame = (vdpau_frame_t *) frame_gen;

  if ( !this->ovl_changed )
    return;

  if (overlay->rle) {
    if (!overlay->rgb_clut || !overlay->hili_rgb_clut)
      vdpau_overlay_clut_yuv2rgb (this, overlay, frame);
    if ( vdpau_process_ovl( this, overlay ) )
      ++this->ovl_changed;
  }*/
}


static void vdpau_overlay_end (vo_driver_t *this_gen, vo_frame_t *vo_img)
{
  /*vdpau_driver_t  *this = (vdpau_driver_t *) this_gen;

  if ( !this->ovl_changed )
    return;

  this->raw_overlay_cb( this->user_data, this->ovl_changed-1, this->overlays );

  this->ovl_changed = 0;*/
}


static void vdpau_frame_proc_slice (vo_frame_t *vo_img, uint8_t **src)
{
  vdpau_frame_t  *frame = (vdpau_frame_t *) vo_img ;

  vo_img->proc_called = 1;

  if( frame->vo_frame.crop_left || frame->vo_frame.crop_top ||
      frame->vo_frame.crop_right || frame->vo_frame.crop_bottom )
  {
    /* TODO: ?!? */
    return;
  }
}



static void vdpau_frame_field (vo_frame_t *vo_img, int which_field)
{
}



static void vdpau_frame_dispose (vo_frame_t *vo_img)
{
  vdpau_frame_t  *frame = (vdpau_frame_t *) vo_img ;

  free (frame->chunk[0]);
  free (frame->chunk[1]);
  free (frame->chunk[2]);
  free (frame);
  if ( frame->vdpau_accel_data.surface != VDP_INVALID_HANDLE )
    vdp_video_surface_destroy( frame->vdpau_accel_data.surface );
}



static vo_frame_t *vdpau_alloc_frame (vo_driver_t *this_gen)
{
  vdpau_frame_t  *frame;
  vdpau_driver_t *this = (vdpau_driver_t *) this_gen;

  frame = (vdpau_frame_t *) calloc(1, sizeof(vdpau_frame_t));

  if (!frame)
    return NULL;

  frame->vo_frame.accel_data = &frame->vdpau_accel_data;

  pthread_mutex_init (&frame->vo_frame.mutex, NULL);

  /*
   * supply required functions/fields
   */
  frame->vo_frame.proc_slice = vdpau_frame_proc_slice;
  frame->vo_frame.proc_frame = NULL;
  frame->vo_frame.field      = vdpau_frame_field;
  frame->vo_frame.dispose    = vdpau_frame_dispose;
  frame->vo_frame.driver     = this_gen;

  frame->vdpau_accel_data.vdp_device = vdp_device;
  frame->vdpau_accel_data.surface = VDP_INVALID_HANDLE;
  frame->vdpau_accel_data.vdp_video_surface_create = vdp_video_surface_create;
  frame->vdpau_accel_data.vdp_video_surface_destroy = vdp_video_surface_destroy;
  frame->vdpau_accel_data.vdp_decoder_create = vdp_decoder_create;
  frame->vdpau_accel_data.vdp_decoder_destroy = vdp_decoder_destroy;
  frame->vdpau_accel_data.vdp_decoder_render = vdp_decoder_render;

  frame->width = frame->height = 0;

  return (vo_frame_t *) frame;
}



static void vdpau_update_frame_format (vo_driver_t *this_gen, vo_frame_t *frame_gen,
      uint32_t width, uint32_t height, double ratio, int format, int flags)
{
  vdpau_frame_t   *frame = (vdpau_frame_t *) frame_gen;

  /* Check frame size and format and reallocate if necessary */
  if ( (frame->width != width) || (frame->height != height) || (frame->format != format) || (frame->flags  != flags)) {
    /*lprintf ("updating frame to %d x %d (ratio=%g, format=%08x)\n", width, height, ratio, format); */

    /* (re-) allocate render space */
    free (frame->chunk[0]);
    free (frame->chunk[1]);
    free (frame->chunk[2]);

    if (format == XINE_IMGFMT_YV12) {
      frame->vo_frame.pitches[0] = 8*((width + 7) / 8);
      frame->vo_frame.pitches[1] = 8*((width + 15) / 16);
      frame->vo_frame.pitches[2] = 8*((width + 15) / 16);
      frame->vo_frame.base[0] = xine_xmalloc_aligned (16, frame->vo_frame.pitches[0] * height,  (void **) &frame->chunk[0]);
      frame->vo_frame.base[1] = xine_xmalloc_aligned (16, frame->vo_frame.pitches[1] * ((height+1)/2), (void **) &frame->chunk[1]);
      frame->vo_frame.base[2] = xine_xmalloc_aligned (16, frame->vo_frame.pitches[2] * ((height+1)/2), (void **) &frame->chunk[2]);
    } else if (format == XINE_IMGFMT_YUY2){
      frame->vo_frame.pitches[0] = 8*((width + 3) / 4);
      frame->vo_frame.base[0] = xine_xmalloc_aligned (16, frame->vo_frame.pitches[0] * height, (void **) &frame->chunk[0]);
      frame->chunk[1] = NULL;
      frame->chunk[2] = NULL;
    }

    if ( frame->vdpau_accel_data.surface != VDP_INVALID_HANDLE  ) {
      if ( (frame->width != width) || (frame->height != height) || (frame->format != XINE_IMGFMT_VDPAU) ) {
        vdp_video_surface_destroy( frame->vdpau_accel_data.surface );
        frame->vdpau_accel_data.surface = VDP_INVALID_HANDLE;
      }
    }

    frame->width = width;
    frame->height = height;
    frame->format = format;

    vdpau_frame_field ((vo_frame_t *)frame, flags);
  }

  frame->ratio = ratio;
}



static int vdpau_redraw_needed (vo_driver_t *this_gen)
{
  vdpau_driver_t  *this = (vdpau_driver_t *) this_gen;

  _x_vo_scale_compute_ideal_size( &this->sc );
  if ( _x_vo_scale_redraw_needed( &this->sc ) ) {
    _x_vo_scale_compute_output_size( &this->sc );
    return 1;
  }
  return 0;
}



static void vdpau_display_frame (vo_driver_t *this_gen, vo_frame_t *frame_gen)
{
  vdpau_driver_t  *this  = (vdpau_driver_t *) this_gen;
  vdpau_frame_t   *frame = (vdpau_frame_t *) frame_gen;
  VdpStatus st;
  VdpVideoSurface surface;
  VdpChromaType chroma = this->video_mixer_chroma;
  uint32_t mix_w = this->video_mixer_width;
  uint32_t mix_h = this->video_mixer_height;

  if ( (frame->width != this->sc.delivered_width) || (frame->height != this->sc.delivered_height) || (frame->ratio != this->sc.delivered_ratio) ) {
    this->sc.force_redraw = 1;    /* trigger re-calc of output size */
  }

  this->sc.delivered_height = frame->height;
  this->sc.delivered_width  = frame->width;
  this->sc.delivered_ratio  = frame->ratio;
  this->sc.crop_left        = frame->vo_frame.crop_left;
  this->sc.crop_right       = frame->vo_frame.crop_right;
  this->sc.crop_top         = frame->vo_frame.crop_top;
  this->sc.crop_bottom      = frame->vo_frame.crop_bottom;

  vdpau_redraw_needed( this_gen );

  if ( (frame->format == XINE_IMGFMT_YV12) || (frame->format == XINE_IMGFMT_YUY2) ) {
    surface = this->soft_surface;
    chroma = ( frame->format==XINE_IMGFMT_YV12 )? VDP_CHROMA_TYPE_420 : VDP_CHROMA_TYPE_422;
    if ( (frame->width > this->soft_surface_width) | (frame->height > this->soft_surface_height) || (frame->format != this->soft_surface_format) ) {
      printf( "vo_vdpau: soft_surface size update\n" );
      /* recreate surface and mixer to match frame changes */
      mix_w = this->soft_surface_width = frame->width;
      mix_h = this->soft_surface_height = frame->height;
      this->soft_surface_format = frame->format;
      vdp_video_surface_destroy( this->soft_surface );
      vdp_video_surface_create( vdp_device, chroma, this->soft_surface_width, this->soft_surface_height, &this->soft_surface );
    }
    /* FIXME: have to swap U and V planes to get correct colors !! nvidia ? */
    uint32_t pitches[] = { frame->vo_frame.pitches[0], frame->vo_frame.pitches[2], frame->vo_frame.pitches[1] };
    void* data[] = { frame->vo_frame.base[0], frame->vo_frame.base[2], frame->vo_frame.base[1] };
    if ( frame->format==XINE_IMGFMT_YV12 ) {
      st = vdp_video_surface_putbits_ycbcr( this->soft_surface, VDP_YCBCR_FORMAT_YV12, &data, pitches );
      if ( st != VDP_STATUS_OK )
        printf( "vo_vdpau: vdp_video_surface_putbits_ycbcr YV12 error : %s\n", vdp_get_error_string( st ) );
    }
    else {
      st = vdp_video_surface_putbits_ycbcr( this->soft_surface, VDP_YCBCR_FORMAT_YUYV, &data, pitches );
      if ( st != VDP_STATUS_OK )
        printf( "vo_vdpau: vdp_video_surface_putbits_ycbcr YUY2 error : %s\n", vdp_get_error_string( st ) );
    }
  }
  else if (frame->format == XINE_IMGFMT_VDPAU) {
    surface = frame->vdpau_accel_data.surface;
    mix_w = frame->width;
    mix_h = frame->height;
    chroma = VDP_CHROMA_TYPE_420;
  }
  else {
    /* unknown format */
    frame->vo_frame.free( &frame->vo_frame );
    return;
  }

  if ( (mix_w != this->video_mixer_width) || (mix_h != this->video_mixer_height) || (chroma != this->video_mixer_chroma) ) {
    vdp_video_mixer_destroy( this->video_mixer );
    VdpVideoMixerParameter params[] = { VDP_VIDEO_MIXER_PARAMETER_VIDEO_SURFACE_WIDTH, VDP_VIDEO_MIXER_PARAMETER_VIDEO_SURFACE_HEIGHT, VDP_VIDEO_MIXER_PARAMETER_CHROMA_TYPE };
    void const *param_values[] = { &mix_w, &mix_h, &chroma };
    vdp_video_mixer_create( vdp_device, 0, 0, 3, params, param_values, &this->video_mixer );
  }

  if ( (this->sc.gui_width > this->output_surface_width) || (this->sc.gui_height > this->output_surface_height) ) {
    /* recreate output surface to match window size */
    printf( "vo_vdpau: output_surface size update\n" );
    this->output_surface_width = this->sc.gui_width;
    this->output_surface_height = this->sc.gui_height;

    vdp_output_surface_destroy( this->output_surface );
    vdp_output_surface_create( vdp_device, VDP_RGBA_FORMAT_B8G8R8A8, this->output_surface_width, this->output_surface_height, &this->output_surface );
  }

  VdpRect vid_source = { this->sc.crop_left, this->sc.crop_top, this->sc.delivered_width-this->sc.crop_right, this->sc.delivered_height-this->sc.crop_bottom };
  VdpRect out_dest = { 0, 0, this->sc.gui_width, this->sc.gui_height };
  VdpRect vid_dest = { this->sc.output_xoffset, this->sc.output_yoffset, this->sc.output_xoffset+this->sc.output_width, this->sc.output_yoffset+this->sc.output_height };

  /*printf( "out_dest = %d %d %d %d - vid_dest = %d %d %d %d\n", out_dest.x0, out_dest.y0, out_dest.x1, out_dest.y1, vid_dest.x0, vid_dest.y0, vid_dest.x1, vid_dest.y1 );*/

  st = vdp_video_mixer_render( this->video_mixer, VDP_INVALID_HANDLE, 0, VDP_VIDEO_MIXER_PICTURE_STRUCTURE_FRAME,
                               0, 0, surface, 0, 0, &vid_source, this->output_surface, &out_dest, &vid_dest, 0, 0 );
  if ( st != VDP_STATUS_OK )
    printf( "vo_vdpau: vdp_video_mixer_render error : %s\n", vdp_get_error_string( st ) );

  XLockDisplay( this->display );
  vdp_queue_display( vdp_queue, this->output_surface, 0, 0, 0 ) ;
  XUnlockDisplay( this->display );

  frame->vo_frame.free( &frame->vo_frame );
}



static int vdpau_get_property (vo_driver_t *this_gen, int property)
{
  switch (property) {
  case VO_PROP_ASPECT_RATIO:
    return XINE_VO_ASPECT_AUTO;
  case VO_PROP_MAX_NUM_FRAMES:
    return 15;
  case VO_PROP_BRIGHTNESS:
    return 0;
  case VO_PROP_CONTRAST:
    return 128;
  case VO_PROP_SATURATION:
    return 128;
  case VO_PROP_WINDOW_WIDTH:
    return 0;
  case VO_PROP_WINDOW_HEIGHT:
    return 0;
  default:
    return 0;
  }
}



static int vdpau_set_property (vo_driver_t *this_gen, int property, int value)
{
  vdpau_driver_t *this = (vdpau_driver_t*)this_gen;

  /*switch (property) {
    case VO_PROP_ZOOM_X:
      if ((value >= XINE_VO_ZOOM_MIN) && (value <= XINE_VO_ZOOM_MAX)) {
        this->sc.zoom_factor_x = (double)value / (double)XINE_VO_ZOOM_STEP;
        _x_vo_scale_compute_ideal_size( &this->sc );
        this->sc.force_redraw = 1;    /* trigger re-calc of output size */
      /*}
      break;
    case VO_PROP_ZOOM_Y:
      if ((value >= XINE_VO_ZOOM_MIN) && (value <= XINE_VO_ZOOM_MAX)) {
        this->sc.zoom_factor_y = (double)value / (double)XINE_VO_ZOOM_STEP;
        _x_vo_scale_compute_ideal_size( &this->sc );
        this->sc.force_redraw = 1;    /* trigger re-calc of output size */
      /*}
      break;
  }*/

  return value;
}



static void vdpau_get_property_min_max (vo_driver_t *this_gen, int property, int *min, int *max)
{
  *min = 0;
  *max = 0;
}



static int vdpau_gui_data_exchange (vo_driver_t *this_gen, int data_type, void *data)
{
  vdpau_driver_t *this = (vdpau_driver_t*)this_gen;

  switch (data_type) {
#ifndef XINE_DISABLE_DEPRECATED_FEATURES
  case XINE_GUI_SEND_COMPLETION_EVENT:
    break;
#endif

  case XINE_GUI_SEND_EXPOSE_EVENT: {
    /* XExposeEvent * xev = (XExposeEvent *) data; */

    /*if (this->cur_frame) {
      int i;

      LOCK_DISPLAY(this);

      if (this->use_shm) {
  XvShmPutImage(this->display, this->xv_port,
          this->drawable, this->gc, this->cur_frame->image,
          this->sc.displayed_xoffset, this->sc.displayed_yoffset,
          this->sc.displayed_width, this->sc.displayed_height,
          this->sc.output_xoffset, this->sc.output_yoffset,
          this->sc.output_width, this->sc.output_height, True);
      } else {
  XvPutImage(this->display, this->xv_port,
       this->drawable, this->gc, this->cur_frame->image,
       this->sc.displayed_xoffset, this->sc.displayed_yoffset,
       this->sc.displayed_width, this->sc.displayed_height,
       this->sc.output_xoffset, this->sc.output_yoffset,
       this->sc.output_width, this->sc.output_height);
      }

      XSetForeground (this->display, this->gc, this->black.pixel);

      for( i = 0; i < 4; i++ ) {
  if( this->sc.border[i].w && this->sc.border[i].h ) {
    XFillRectangle(this->display, this->drawable, this->gc,
       this->sc.border[i].x, this->sc.border[i].y,
       this->sc.border[i].w, this->sc.border[i].h);
  }
      }

      if(this->xoverlay)
  x11osd_expose(this->xoverlay);

      XSync(this->display, False);
      UNLOCK_DISPLAY(this);
    }*/
  }
  break;

  case XINE_GUI_SEND_DRAWABLE_CHANGED: {
    VdpStatus st;
    XLockDisplay( this->display );
    this->drawable = (Drawable) data;
    vdp_queue_destroy( vdp_queue );
    vdp_queue_target_destroy( vdp_queue_target );
    st = vdp_queue_target_create_x11( vdp_device, this->drawable, &vdp_queue_target );
    if ( st != VDP_STATUS_OK ) {
      printf( "vo_vdpau: FATAL !! Can't recreate presentation queue target after drawable change !!\n" );
      break;
    }
    st = vdp_queue_create( vdp_device, vdp_queue_target, &vdp_queue );
    if ( st != VDP_STATUS_OK ) {
      printf( "vo_vdpau: FATAL !! Can't recreate presentation queue after drawable change !!\n" );
      break;
    }
    VdpColor backColor;
    backColor.red = backColor.green = backColor.blue = 0;
    backColor.alpha = 1;
    vdp_queue_set_backgroung_color( vdp_queue, &backColor );
    XUnlockDisplay( this->display );
    this->sc.force_redraw = 1;
    break;
  }

  case XINE_GUI_SEND_TRANSLATE_GUI_TO_VIDEO:
    /*{
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

      /* onefield_xv divide by 2 the number of lines */
      /*if (this->deinterlace_enabled
          && (this->deinterlace_method == DEINTERLACE_ONEFIELDXV)
          && (this->cur_frame->format == XINE_IMGFMT_YV12)) {
        rect->y = rect->y * 2;
        rect->h = rect->h * 2;
      }

    }*/
    break;
  }

  return 0;
}



static uint32_t vdpau_get_capabilities (vo_driver_t *this_gen)
{
  vdpau_driver_t *this = (vdpau_driver_t *) this_gen;

  return this->capabilities;
}



static void vdpau_dispose (vo_driver_t *this_gen)
{
  vdpau_driver_t *this = (vdpau_driver_t *) this_gen;
  int i;

  free( this->overlay.ovl_rgba );

  free (this);
}



static int vdpau_init_error( VdpStatus st, const char *msg, vo_driver_t *driver, int error_string )
{
  if ( st != VDP_STATUS_OK ) {
    if ( error_string )
      printf( "vo_vdpau: %s : %s\n", msg, vdp_get_error_string( st ) );
    else
      printf( "vo_vdpau: %s\n", msg );
    vdpau_dispose( driver );
    return 1;
  }
  return 0;
}



static vo_driver_t *vdpau_open_plugin (video_driver_class_t *class_gen, const void *visual_gen)
{
  vdpau_class_t       *class   = (vdpau_class_t *) class_gen;
  x11_visual_t         *visual  = (x11_visual_t *) visual_gen;
  vdpau_driver_t      *this;
  config_values_t      *config  = class->xine->config;
  int i;

  this = (vdpau_driver_t *) calloc(1, sizeof(vdpau_driver_t));

  if (!this)
    return NULL;

  this->display       = visual->display;
  this->screen        = visual->screen;
  this->drawable      = visual->d;

  _x_vo_scale_init(&this->sc, 1, 0, config);
  this->sc.frame_output_cb  = visual->frame_output_cb;
  this->sc.dest_size_cb     = visual->dest_size_cb;
  this->sc.user_data        = visual->user_data;
  this->sc.user_ratio       = XINE_VO_ASPECT_AUTO;

  this->ovl_changed             = 0;
  this->xine                    = class->xine;
  this->config                  = config;

  this->vo_driver.get_capabilities     = vdpau_get_capabilities;
  this->vo_driver.alloc_frame          = vdpau_alloc_frame;
  this->vo_driver.update_frame_format  = vdpau_update_frame_format;
  this->vo_driver.overlay_begin        = vdpau_overlay_begin;
  this->vo_driver.overlay_blend        = vdpau_overlay_blend;
  this->vo_driver.overlay_end          = vdpau_overlay_end;
  this->vo_driver.display_frame        = vdpau_display_frame;
  this->vo_driver.get_property         = vdpau_get_property;
  this->vo_driver.set_property         = vdpau_set_property;
  this->vo_driver.get_property_min_max = vdpau_get_property_min_max;
  this->vo_driver.gui_data_exchange    = vdpau_gui_data_exchange;
  this->vo_driver.dispose              = vdpau_dispose;
  this->vo_driver.redraw_needed        = vdpau_redraw_needed;

  this->overlay.ovl_w = this->overlay.ovl_h = 2;
  this->overlay.ovl_rgba = (uint8_t*)malloc(2*2*4);
  this->overlay.ovl_x = this->overlay.ovl_y = 0;

  /*  overlay converter */
  yuv2rgb_factory_t *factory = yuv2rgb_factory_init (MODE_24_BGR, 0, NULL);
  this->ovl_yuv2rgb = factory->create_converter( factory );
  factory->dispose( factory );

  VdpStatus st = vdp_device_create_x11( visual->display, visual->screen, &vdp_device, &vdp_get_proc_address );
  if ( st != VDP_STATUS_OK ) {
    printf( "vo_vdpau: Can't create vdp device : " );
    if ( st == VDP_STATUS_NO_IMPLEMENTATION )
      printf( "No vdpau implementation.\n" );
    else
      printf( "unsupported GPU?\n" );
    vdpau_dispose( &this->vo_driver );
    return NULL;
  }

  st = vdp_get_proc_address( vdp_device, VDP_FUNC_ID_GET_ERROR_STRING , (void*)&vdp_get_error_string );
  if ( vdpau_init_error( st, "Can't get GET_ERROR_STRING proc address !!", &this->vo_driver, 0 ) )
    return NULL;

  st = vdp_get_proc_address( vdp_device, VDP_FUNC_ID_GET_API_VERSION , (void*)&vdp_get_api_version );
  if ( vdpau_init_error( st, "Can't get GET_API_VERSION proc address !!", &this->vo_driver, 1 ) )
    return NULL;
  uint32_t tmp;
  vdp_get_api_version( &tmp );
  printf( "vo_vdpau: vdpau API version : %d\n", tmp );

  st = vdp_get_proc_address( vdp_device, VDP_FUNC_ID_GET_INFORMATION_STRING , (void*)&vdp_get_information_string );
  if ( vdpau_init_error( st, "Can't get GET_INFORMATION_STRING proc address !!", &this->vo_driver, 1 ) )
    return NULL;
  const char *s;
  st = vdp_get_information_string( &s );
  printf( "vo_vdpau: vdpau implementation description : %s\n", s );

  st = vdp_get_proc_address( vdp_device, VDP_FUNC_ID_VIDEO_SURFACE_QUERY_GET_PUT_BITS_Y_CB_CR_CAPABILITIES , (void*)&vdp_video_surface_query_get_put_bits_ycbcr_capabilities );
  if ( vdpau_init_error( st, "Can't get VIDEO_SURFACE_QUERY_GET_PUT_BITS_Y_CB_CR_CAPABILITIES proc address !!", &this->vo_driver, 1 ) )
    return NULL;

  VdpBool ok;
  st = vdp_video_surface_query_get_put_bits_ycbcr_capabilities( vdp_device, VDP_CHROMA_TYPE_422, VDP_YCBCR_FORMAT_YUYV, &ok );
  if ( vdpau_init_error( st, "Failed to check vdpau yuy2 capability", &this->vo_driver, 1 ) )
    return NULL;
  if ( !ok ) {
    printf( "vo_vdpau: VideoSurface doesn't support yuy2, sorry.\n");
    vdpau_dispose( &this->vo_driver );
    return NULL;
  }

  st = vdp_video_surface_query_get_put_bits_ycbcr_capabilities( vdp_device, VDP_CHROMA_TYPE_420, VDP_YCBCR_FORMAT_YV12, &ok );
  if ( vdpau_init_error( st, "Failed to check vdpau yv12 capability", &this->vo_driver, 1 ) )
    return NULL;
  if ( !ok ) {
    printf( "vo_vdpau: VideoSurface doesn't support yv12, sorry.\n");
    vdpau_dispose( &this->vo_driver );
    return NULL;
  }

  st = vdp_get_proc_address( vdp_device, VDP_FUNC_ID_VIDEO_SURFACE_CREATE , (void*)&vdp_video_surface_create );
  if ( vdpau_init_error( st, "Can't get VIDEO_SURFACE_CREATE proc address !!", &this->vo_driver, 1 ) )
    return NULL;

  st = vdp_get_proc_address( vdp_device, VDP_FUNC_ID_VIDEO_SURFACE_DESTROY , (void*)&vdp_video_surface_destroy );
  if ( vdpau_init_error( st, "Can't get VIDEO_SURFACE_DESTROY proc address !!", &this->vo_driver, 1 ) )
    return NULL;

  st = vdp_get_proc_address( vdp_device, VDP_FUNC_ID_VIDEO_SURFACE_PUT_BITS_Y_CB_CR , (void*)&vdp_video_surface_putbits_ycbcr );
  if ( vdpau_init_error( st, "Can't get VIDEO_SURFACE_PUT_BITS_Y_CB_CR proc address !!", &this->vo_driver, 1 ) )
    return NULL;

  st = vdp_get_proc_address( vdp_device, VDP_FUNC_ID_OUTPUT_SURFACE_CREATE , (void*)&vdp_output_surface_create );
  if ( vdpau_init_error( st, "Can't get OUTPUT_SURFACE_CREATE proc address !!", &this->vo_driver, 1 ) )
    return NULL;

  st = vdp_get_proc_address( vdp_device, VDP_FUNC_ID_OUTPUT_SURFACE_DESTROY , (void*)&vdp_output_surface_destroy );
  if ( vdpau_init_error( st, "Can't get OUTPUT_SURFACE_DESTROY proc address !!", &this->vo_driver, 1 ) )
    return NULL;

  st = vdp_get_proc_address( vdp_device, VDP_FUNC_ID_VIDEO_MIXER_CREATE , (void*)&vdp_video_mixer_create );
  if ( vdpau_init_error( st, "Can't get VIDEO_MIXER_CREATE proc address !!", &this->vo_driver, 1 ) )
    return NULL;

  st = vdp_get_proc_address( vdp_device, VDP_FUNC_ID_VIDEO_MIXER_DESTROY , (void*)&vdp_video_mixer_destroy );
  if ( vdpau_init_error( st, "Can't get VIDEO_MIXER_DESTROY proc address !!", &this->vo_driver, 1 ) )
    return NULL;

  st = vdp_get_proc_address( vdp_device, VDP_FUNC_ID_VIDEO_MIXER_RENDER , (void*)&vdp_video_mixer_render );
  if ( vdpau_init_error( st, "Can't get VIDEO_MIXER_RENDER proc address !!", &this->vo_driver, 1 ) )
    return NULL;

  st = vdp_get_proc_address( vdp_device, VDP_FUNC_ID_PRESENTATION_QUEUE_TARGET_CREATE_X11 , (void*)&vdp_queue_target_create_x11 );
  if ( vdpau_init_error( st, "Can't get PRESENTATION_QUEUE_TARGET_CREATE_X11 proc address !!", &this->vo_driver, 1 ) )
    return NULL;

  st = vdp_get_proc_address( vdp_device, VDP_FUNC_ID_PRESENTATION_QUEUE_TARGET_DESTROY , (void*)&vdp_queue_target_destroy );
  if ( vdpau_init_error( st, "Can't get PRESENTATION_QUEUE_TARGET_DESTROY proc address !!", &this->vo_driver, 1 ) )
    return NULL;

  st = vdp_get_proc_address( vdp_device, VDP_FUNC_ID_PRESENTATION_QUEUE_CREATE , (void*)&vdp_queue_create );
  if ( vdpau_init_error( st, "Can't get PRESENTATION_QUEUE_CREATE proc address !!", &this->vo_driver, 1 ) )
    return NULL;

  st = vdp_get_proc_address( vdp_device, VDP_FUNC_ID_PRESENTATION_QUEUE_DESTROY , (void*)&vdp_queue_destroy );
  if ( vdpau_init_error( st, "Can't get PRESENTATION_QUEUE_DESTROY proc address !!", &this->vo_driver, 1 ) )
    return NULL;

  st = vdp_get_proc_address( vdp_device, VDP_FUNC_ID_PRESENTATION_QUEUE_DISPLAY , (void*)&vdp_queue_display );
  if ( vdpau_init_error( st, "Can't get PRESENTATION_QUEUE_DISPLAY proc address !!", &this->vo_driver, 1 ) )
    return NULL;

  st = vdp_get_proc_address( vdp_device, VDP_FUNC_ID_PRESENTATION_QUEUE_SET_BACKGROUND_COLOR , (void*)&vdp_queue_set_backgroung_color );
  if ( vdpau_init_error( st, "Can't get PRESENTATION_QUEUE_SET_BACKGROUND_COLOR proc address !!", &this->vo_driver, 1 ) )
    return NULL;

  st = vdp_get_proc_address( vdp_device, VDP_FUNC_ID_DECODER_QUERY_CAPABILITIES , (void*)&vdp_decoder_query_capabilities );
  if ( vdpau_init_error( st, "Can't get DECODER_QUERY_CAPABILITIES proc address !!", &this->vo_driver, 1 ) )
    return NULL;

  st = vdp_get_proc_address( vdp_device, VDP_FUNC_ID_DECODER_CREATE , (void*)&vdp_decoder_create );
  if ( vdpau_init_error( st, "Can't get DECODER_CREATE proc address !!", &this->vo_driver, 1 ) )
    return NULL;

  st = vdp_get_proc_address( vdp_device, VDP_FUNC_ID_DECODER_DESTROY , (void*)&vdp_decoder_destroy );
  if ( vdpau_init_error( st, "Can't get DECODER_DESTROY proc address !!", &this->vo_driver, 1 ) )
    return NULL;

  st = vdp_get_proc_address( vdp_device, VDP_FUNC_ID_DECODER_RENDER , (void*)&vdp_decoder_render );
  if ( vdpau_init_error( st, "Can't get DECODER_RENDER proc address !!", &this->vo_driver, 1 ) )
    return NULL;

  st = vdp_queue_target_create_x11( vdp_device, this->drawable, &vdp_queue_target );
  if ( vdpau_init_error( st, "Can't create presentation queue target !!", &this->vo_driver, 1 ) )
    return NULL;

  st = vdp_queue_create( vdp_device, vdp_queue_target, &vdp_queue );
  if ( vdpau_init_error( st, "Can't create presentation queue !!", &this->vo_driver, 1 ) )
    return NULL;

  VdpColor backColor;
  backColor.red = backColor.green = backColor.blue = 0;
  backColor.alpha = 1;
  vdp_queue_set_backgroung_color( vdp_queue, &backColor );

  this->soft_surface_width = 720;
  this->soft_surface_height = 576;
  this->soft_surface_format = XINE_IMGFMT_YV12;
  VdpChromaType chroma = VDP_CHROMA_TYPE_420;
  st = vdp_video_surface_create( vdp_device, chroma, this->soft_surface_width, this->soft_surface_height, &this->soft_surface );
  if ( vdpau_init_error( st, "Can't create video surface !!", &this->vo_driver, 1 ) )
    return NULL;

  this->output_surface_width = 720;
  this->output_surface_height = 576;
  st = vdp_output_surface_create( vdp_device, VDP_RGBA_FORMAT_B8G8R8A8, this->output_surface_width, this->output_surface_height, &this->output_surface );
  if ( vdpau_init_error( st, "Can't create output surface !!", &this->vo_driver, 1 ) ) {
    vdp_video_surface_destroy( this->soft_surface );
    return NULL;
  }

  this->video_mixer_chroma = chroma;
  this->video_mixer_width = this->soft_surface_width;
  this->video_mixer_height = this->soft_surface_height;
  VdpVideoMixerParameter params[] = { VDP_VIDEO_MIXER_PARAMETER_VIDEO_SURFACE_WIDTH, VDP_VIDEO_MIXER_PARAMETER_VIDEO_SURFACE_HEIGHT, VDP_VIDEO_MIXER_PARAMETER_CHROMA_TYPE };
  void const *param_values[] = { &this->video_mixer_width, &this->video_mixer_height, &chroma };
  st = vdp_video_mixer_create( vdp_device, 0, 0, 3, params, param_values, &this->video_mixer );
  if ( vdpau_init_error( st, "Can't create video mixer !!", &this->vo_driver, 1 ) ) {
    vdp_video_surface_destroy( this->soft_surface );
    vdp_output_surface_destroy( this->output_surface );
    return NULL;
  }

  this->capabilities = VO_CAP_YV12 | VO_CAP_YUY2 | VO_CAP_CROP;
  ok = 0;
  uint32_t mw, mh, ml, mr;
  st = vdp_decoder_query_capabilities( vdp_device, VDP_DECODER_PROFILE_H264_MAIN, &ok, &ml, &mr, &mw, &mh );
  if ( st != VDP_STATUS_OK  )
    printf( "vo_vdpau: getting h264_supported failed! : %s\n", vdp_get_error_string( st ) );
  else if ( !ok )
    printf( "vo_vdpau: no support for h264 ! : no ok\n" );
  else
    this->capabilities |= VO_CAP_VDPAU_H264;


  return &this->vo_driver;
}

/*
 * class functions
 */

static char* vdpau_get_identifier (video_driver_class_t *this_gen)
{
  return "vdpau";
}



static char* vdpau_get_description (video_driver_class_t *this_gen)
{
  return _("xine video output plugin using VDPAU hardware acceleration");
}



static void vdpau_dispose_class (video_driver_class_t *this_gen)
{
  vdpau_class_t *this = (vdpau_class_t *) this_gen;
  free (this);
}



static void *vdpau_init_class (xine_t *xine, void *visual_gen)
{
  vdpau_class_t *this = (vdpau_class_t *) calloc(1, sizeof(vdpau_class_t));

  this->driver_class.open_plugin     = vdpau_open_plugin;
  this->driver_class.get_identifier  = vdpau_get_identifier;
  this->driver_class.get_description = vdpau_get_description;
  this->driver_class.dispose         = vdpau_dispose_class;
  this->xine                         = xine;

  return this;
}



static const vo_info_t vo_info_vdpau = {
  11,                    /* priority    */
  XINE_VISUAL_TYPE_X11  /* visual type */
};


/*
 * exported plugin catalog entry
 */

const plugin_info_t xine_plugin_info[] EXPORTED = {
  /* type, API, "name", version, special_info, init_function */
  { PLUGIN_VIDEO_OUT, 21, "vdpau", XINE_VERSION_CODE, &vo_info_vdpau, vdpau_init_class },
  { PLUGIN_NONE, 0, "", 0, NULL, NULL }
};