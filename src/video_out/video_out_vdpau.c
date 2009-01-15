/*
 * Copyright (C) 2008 Christophe Thommeret <hftom@free.fr>
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

#define NUM_FRAMES_BACK 1



const char *vdpau_deinterlace_methods[] = {
  "bob",
  "temporal",
  "temporal_spatial",
  NULL
};



VdpOutputSurfaceRenderBlendState blend = { VDP_OUTPUT_SURFACE_RENDER_BLEND_STATE_VERSION,
          VDP_OUTPUT_SURFACE_RENDER_BLEND_FACTOR_ONE ,
          VDP_OUTPUT_SURFACE_RENDER_BLEND_FACTOR_ONE_MINUS_SRC_COLOR,
          VDP_OUTPUT_SURFACE_RENDER_BLEND_FACTOR_ONE,
          VDP_OUTPUT_SURFACE_RENDER_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA,
          VDP_OUTPUT_SURFACE_RENDER_BLEND_EQUATION_ADD,
          VDP_OUTPUT_SURFACE_RENDER_BLEND_EQUATION_ADD };



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
VdpVideoSurfaceGetBitsYCbCr *vdp_video_surface_getbits_ycbcr;

VdpOutputSurfaceCreate *vdp_output_surface_create;
VdpOutputSurfaceDestroy *vdp_output_surface_destroy;
VdpOutputSurfaceRenderBitmapSurface *vdp_output_surface_render_bitmap_surface;
VdpOutputSurfacePutBitsNative *vdp_output_surface_put_bits;

VdpVideoMixerCreate *vdp_video_mixer_create;
VdpVideoMixerDestroy *vdp_video_mixer_destroy;
VdpVideoMixerRender *vdp_video_mixer_render;
VdpVideoMixerSetAttributeValues *vdp_video_mixer_set_attribute_values;
VdpVideoMixerSetFeatureEnables *vdp_video_mixer_set_feature_enables;
VdpVideoMixerGetFeatureEnables *vdp_video_mixer_get_feature_enables;

VdpGenerateCSCMatrix *vdp_generate_csc_matrix;

VdpPresentationQueueTargetCreateX11 *vdp_queue_target_create_x11;
VdpPresentationQueueTargetDestroy *vdp_queue_target_destroy;
VdpPresentationQueueCreate *vdp_queue_create;
VdpPresentationQueueDestroy *vdp_queue_destroy;
VdpPresentationQueueDisplay *vdp_queue_display;
VdpPresentationQueueBlockUntilSurfaceIdle *vdp_queue_block;
VdpPresentationQueueSetBackgroundColor *vdp_queue_set_background_color;
VdpPresentationQueueGetTime *vdp_queue_get_time;

VdpBitmapSurfacePutBitsNative *vdp_bitmap_put_bits;
VdpBitmapSurfaceCreate  *vdp_bitmap_create;
VdpBitmapSurfaceDestroy *vdp_bitmap_destroy;

VdpDecoderQueryCapabilities *vdp_decoder_query_capabilities;
VdpDecoderCreate *vdp_decoder_create;
VdpDecoderDestroy *vdp_decoder_destroy;
VdpDecoderRender *vdp_decoder_render;

VdpPreemptionCallbackRegister *vdp_preemption_callback_register;
static void vdp_preemption_callback( VdpDevice device, void *context );
static void vdpau_reinit( vo_driver_t *this_gen );

static VdpVideoSurfaceCreate *orig_vdp_video_surface_create;
static VdpVideoSurfaceDestroy *orig_vdp_video_surface_destroy;

static VdpDecoderCreate *orig_vdp_decoder_create;
static VdpDecoderDestroy *orig_vdp_decoder_destroy;
static VdpDecoderRender *orig_vdp_decoder_render;

static Display *guarded_display;

static VdpStatus guarded_vdp_video_surface_create(VdpDevice device, VdpChromaType chroma_type, uint32_t width, uint32_t height,VdpVideoSurface *surface)
{
  VdpStatus r;
  XLockDisplay(guarded_display);
  r = orig_vdp_video_surface_create(device, chroma_type, width, height, surface);
  XUnlockDisplay(guarded_display);
  return r;
}

static VdpStatus guarded_vdp_video_surface_destroy(VdpVideoSurface surface)
{
  VdpStatus r;
//  XLockDisplay(guarded_display);
  r = orig_vdp_video_surface_destroy(surface);
//  XUnlockDisplay(guarded_display);
  return r;
}

static VdpStatus guarded_vdp_decoder_create(VdpDevice device, VdpDecoderProfile profile, uint32_t width, uint32_t height, uint32_t max_references, VdpDecoder *decoder)
{
  VdpStatus r;
  XLockDisplay(guarded_display);
  r = orig_vdp_decoder_create(device, profile, width, height, max_references, decoder);
  XUnlockDisplay(guarded_display);
  return r;
}

static VdpStatus guarded_vdp_decoder_destroy(VdpDecoder decoder)
{
  VdpStatus r;
  XLockDisplay(guarded_display);
  r = orig_vdp_decoder_destroy(decoder);
  XUnlockDisplay(guarded_display);
  return r;
}

static VdpStatus guarded_vdp_decoder_render(VdpDecoder decoder, VdpVideoSurface target, VdpPictureInfo const *picture_info, uint32_t bitstream_buffer_count, VdpBitstreamBuffer const *bitstream_buffers)
{
  VdpStatus r;
  XLockDisplay(guarded_display);
  r = orig_vdp_decoder_render(decoder, target, picture_info, bitstream_buffer_count, bitstream_buffers);
  XUnlockDisplay(guarded_display);
  return r;
}



typedef struct {
  VdpBitmapSurface ovl_bitmap;
  uint32_t  bitmap_width, bitmap_height;
  int ovl_w, ovl_h; /* overlay's width and height */
  int ovl_x, ovl_y; /* overlay's top-left display position */
  int unscaled;
} vdpau_overlay_t;


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
  vdpau_overlay_t     overlays[XINE_VORAW_MAX_OVL];
  yuv2rgb_factory_t   *yuv2rgb_factory;
  yuv2rgb_t           *ovl_yuv2rgb;
  VdpOutputSurface    overlay_output;
  uint32_t            overlay_output_width;
  uint32_t            overlay_output_height;
  int                 has_overlay;

  VdpOutputSurface    overlay_unscaled;
  uint32_t            overlay_unscaled_width;
  uint32_t            overlay_unscaled_height;
  int                 has_unscaled;

  VdpOutputSurface    argb_overlay;
  uint32_t            argb_overlay_width;
  uint32_t            argb_overlay_height;
  int                 has_argb_overlay;
  int                 argb_osd_x;
  int                 argb_osd_y;
  int                 argb_osd_w;
  int                 argb_osd_h;

  VdpVideoSurface      soft_surface;
  uint32_t             soft_surface_width;
  uint32_t             soft_surface_height;
  int                  soft_surface_format;

  VdpOutputSurface     output_surface[2];
  uint8_t              current_output_surface;
  uint32_t             output_surface_width[2];
  uint32_t             output_surface_height[2];
  uint8_t              init_queue;

  VdpVideoMixer        video_mixer;
  VdpChromaType        video_mixer_chroma;
  uint32_t             video_mixer_width;
  uint32_t             video_mixer_height;

  VdpColor             back_color;

  vdpau_frame_t        *back_frame[ NUM_FRAMES_BACK ];

  uint32_t          capabilities;
  xine_t            *xine;

  int               hue;
  int               saturation;
  int               brightness;
  int               contrast;
  int               sharpness;
  int               noise;
  int               deinterlace;
  int               deinterlace_method;
  int               enable_inverse_telecine;
  int               honor_progressive;

  int               vdp_runtime_nr;
  int               reinit_needed;

  int               allocated_surfaces;

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



static int vdpau_process_argb_ovl( vdpau_driver_t *this_gen, vo_frame_t *frame_gen, vo_overlay_t *overlay )
{
  vdpau_driver_t  *this = (vdpau_driver_t *) this_gen;

  if(overlay->argb_layer == NULL)
    return 0;

  pthread_mutex_lock(&overlay->argb_layer->mutex);

  if (overlay->argb_layer->buffer != NULL) {
    int extent_width = overlay->extent_width;
    int extent_height = overlay->extent_height;
    if (extent_width <= 0 || extent_height <= 0) {
      extent_width  = frame_gen->width;
      extent_height = frame_gen->height;
    }

    if (extent_width > 0 && extent_height > 0) {
      if ( (this->argb_overlay_width != extent_width ) || (this->argb_overlay_height != extent_height) || (this->argb_overlay == VDP_INVALID_HANDLE) ) {
        if (this->argb_overlay != VDP_INVALID_HANDLE) {
          vdp_output_surface_destroy( this->argb_overlay );
        }
        VdpStatus st = vdp_output_surface_create( vdp_device, VDP_RGBA_FORMAT_B8G8R8A8, extent_width, extent_height, &this->argb_overlay );
        if ( st != VDP_STATUS_OK ) {
          printf( "vdpau_process_argb_ovl: vdp_output_surface_create failed : %s\n", vdp_get_error_string(st) );
        }
        this->argb_overlay_width  = extent_width;
        this->argb_overlay_height = extent_height;

        /* set stored osd location to extent as any smaller osd requires to clear the surface first */
        this->argb_osd_x = 0;
        this->argb_osd_y = 0;
        this->argb_osd_w = extent_width;
        this->argb_osd_h = extent_height;
      }

      /* wipe surface if osd layout changed */
      if (overlay->x != this->argb_osd_x || overlay->y != this->argb_osd_y || overlay->width != this->argb_osd_w || overlay->height != this->argb_osd_h) {
        this->argb_osd_x = overlay->x;
        this->argb_osd_y = overlay->y;
        this->argb_osd_w = overlay->width;
        this->argb_osd_h = overlay->height;

        uint32_t *zeros = calloc(4 * extent_width, extent_height);
        if (zeros) {
          uint32_t pitch = extent_width * 4;
          VdpRect dest = { 0, 0, extent_width, extent_height };
          VdpStatus st = vdp_output_surface_put_bits( this->argb_overlay, (void*)&(zeros), &pitch, &dest );
          if ( st != VDP_STATUS_OK )
            printf( "vdpau_process_argb_ovl: vdp_output_surface_put_bits_native failed : %s\n", vdp_get_error_string(st) );
          free(zeros);
        }
      }

      /* set destination area according to dirty area of argb layer and reset dirty area */
      uint32_t pitch = overlay->width * 4;
      uint32_t *buffer_start = overlay->argb_layer->buffer + overlay->argb_layer->y1 * overlay->width + overlay->argb_layer->x1;
      VdpRect dest = { overlay->x + overlay->argb_layer->x1, overlay->y + overlay->argb_layer->y1, overlay->x + overlay->argb_layer->x2, overlay->y + overlay->argb_layer->y2 };
      overlay->argb_layer->x1 = overlay->width;
      overlay->argb_layer->y1 = overlay->height;
      overlay->argb_layer->x2 = 0;
      overlay->argb_layer->y2 = 0;

      VdpStatus st = vdp_output_surface_put_bits( this->argb_overlay, (void*)&(buffer_start), &pitch, &dest );
      if ( st != VDP_STATUS_OK ) {
        printf( "vdpau_process_argb_ovl: vdp_output_surface_put_bits_native failed : %s\n", vdp_get_error_string(st) );
      } else
        this->has_argb_overlay = 1;
    }
  }

  pthread_mutex_unlock(&overlay->argb_layer->mutex);

  return 1;
}



static int vdpau_process_ovl( vdpau_driver_t *this_gen, vo_overlay_t *overlay )
{
  vdpau_overlay_t *ovl = &this_gen->overlays[this_gen->ovl_changed-1];

  if ( overlay->width<=0 || overlay->height<=0 )
    return 0;

  if ( (ovl->bitmap_width < overlay->width ) || (ovl->bitmap_height < overlay->height) || (ovl->ovl_bitmap == VDP_INVALID_HANDLE) ) {
    if (ovl->ovl_bitmap != VDP_INVALID_HANDLE) {
      vdp_bitmap_destroy( ovl->ovl_bitmap );
    }
    VdpStatus st = vdp_bitmap_create( vdp_device, VDP_RGBA_FORMAT_B8G8R8A8, overlay->width, overlay->height, 0, &ovl->ovl_bitmap );
    if ( st != VDP_STATUS_OK ) {
      printf( "vdpau_process_ovl: vdp_bitmap_create failed : %s\n", vdp_get_error_string(st) );
    }
    ovl->bitmap_width = overlay->width;
    ovl->bitmap_height = overlay->height;
  }
  ovl->ovl_w = overlay->width;
  ovl->ovl_h = overlay->height;
  ovl->ovl_x = overlay->x;
  ovl->ovl_y = overlay->y;
  ovl->unscaled = overlay->unscaled;
  uint32_t *buf = (uint32_t*)malloc(ovl->ovl_w*ovl->ovl_h*4);
  if ( !buf )
    return 0;

  int num_rle = overlay->num_rle;
  rle_elem_t *rle = overlay->rle;
  uint32_t *rgba = buf;
  uint32_t red, green, blue, alpha;
  clut_t *low_colors = (clut_t*)overlay->color;
  clut_t *hili_colors = (clut_t*)overlay->hili_color;
  uint8_t *low_trans = overlay->trans;
  uint8_t *hili_trans = overlay->hili_trans;
  clut_t *colors;
  uint8_t *trans;
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
    for ( i=0; i<rlelen; ++i ) {
      red = colors[clr].y; /* red */
      green = colors[clr].cr; /* green */
      blue = colors[clr].cb; /* blue */
      alpha = trans[clr]*255/15;
      *rgba = (alpha<<24) | (red<<16) | (green<<8) | blue;
      rgba++;
      ++pos;
    }
    ++rle;
    --num_rle;
  }
  uint32_t pitch = ovl->ovl_w*4;
  VdpRect dest = { 0, 0, ovl->ovl_w, ovl->ovl_h };
  VdpStatus st = vdp_bitmap_put_bits( ovl->ovl_bitmap, &buf, &pitch, &dest);
  if ( st != VDP_STATUS_OK ) {
    printf( "vdpau_process_ovl: vdp_bitmap_put_bits failed : %s\n", vdp_get_error_string(st) );
  }
  free(buf);
  return 1;
}



static void vdpau_overlay_begin (vo_driver_t *this_gen, vo_frame_t *frame_gen, int changed)
{
  vdpau_driver_t  *this = (vdpau_driver_t *) this_gen;

  if ( !changed )
    return;

  this->has_overlay = this->has_unscaled = 0;
  this->has_argb_overlay = 0;
  ++this->ovl_changed;
}



static void vdpau_overlay_blend (vo_driver_t *this_gen, vo_frame_t *frame_gen, vo_overlay_t *overlay)
{
  vdpau_driver_t  *this = (vdpau_driver_t *) this_gen;
  vdpau_frame_t *frame = (vdpau_frame_t *) frame_gen;

  if ( !this->ovl_changed || this->ovl_changed>XINE_VORAW_MAX_OVL )
    return;

  if (overlay->rle) {
    if (!overlay->rgb_clut || !overlay->hili_rgb_clut)
      vdpau_overlay_clut_yuv2rgb (this, overlay, frame);
    if ( vdpau_process_ovl( this, overlay ) )
      ++this->ovl_changed;
  }

  if(overlay->argb_layer)
    vdpau_process_argb_ovl( this, frame_gen, overlay );
}



static void vdpau_overlay_end (vo_driver_t *this_gen, vo_frame_t *frame)
{
  vdpau_driver_t  *this = (vdpau_driver_t *) this_gen;
  int i;
  VdpStatus st;

  if ( !this->ovl_changed )
    return;

  if ( !(this->ovl_changed-1) ) {
    this->ovl_changed = 0;
    this->has_overlay = 0;
    this->has_unscaled = 0;
    return;
  }

  int w=0, h=0;
  for ( i=0; i<this->ovl_changed-1; ++i ) {
    if ( this->overlays[i].unscaled )
      continue;
    if ( w < (this->overlays[i].ovl_x+this->overlays[i].ovl_w) )
      w = this->overlays[i].ovl_x+this->overlays[i].ovl_w;
    if ( h < (this->overlays[i].ovl_y+this->overlays[i].ovl_h) )
      h = this->overlays[i].ovl_y+this->overlays[i].ovl_h;
  }

  int out_w = (w>frame->width) ? w : frame->width;
  int out_h = (h>frame->height) ? h : frame->height;

  if ( (this->overlay_output_width!=out_w || this->overlay_output_height!=out_h) && this->overlay_output != VDP_INVALID_HANDLE ) {
    st = vdp_output_surface_destroy( this->overlay_output );
    if ( st != VDP_STATUS_OK ) {
      printf( "vdpau_overlay_end: vdp_output_surface_destroy failed : %s\n", vdp_get_error_string(st) );
    }
    this->overlay_output = VDP_INVALID_HANDLE;
  }

  this->overlay_output_width = out_w;
  this->overlay_output_height = out_h;

  w = 64; h = 64;
  for ( i=0; i<this->ovl_changed-1; ++i ) {
    if ( !this->overlays[i].unscaled )
      continue;
    if ( w < (this->overlays[i].ovl_x+this->overlays[i].ovl_w) )
      w = this->overlays[i].ovl_x+this->overlays[i].ovl_w;
    if ( h < (this->overlays[i].ovl_y+this->overlays[i].ovl_h) )
      h = this->overlays[i].ovl_y+this->overlays[i].ovl_h;
  }

  if ( (this->overlay_unscaled_width!=w || this->overlay_unscaled_height!=h) && this->overlay_unscaled != VDP_INVALID_HANDLE ) {
    st = vdp_output_surface_destroy( this->overlay_unscaled );
    if ( st != VDP_STATUS_OK ) {
      printf( "vdpau_overlay_end: vdp_output_surface_destroy failed : %s\n", vdp_get_error_string(st) );
    }
    this->overlay_unscaled = VDP_INVALID_HANDLE;
  }

  this->overlay_unscaled_width = w;
  this->overlay_unscaled_height = h;

  if ( this->overlay_unscaled == VDP_INVALID_HANDLE ) {
    st = vdp_output_surface_create( vdp_device, VDP_RGBA_FORMAT_B8G8R8A8, this->overlay_unscaled_width, this->overlay_unscaled_height, &this->overlay_unscaled );
    if ( st != VDP_STATUS_OK )
      printf( "vdpau_overlay_end: vdp_output_surface_create failed : %s\n", vdp_get_error_string(st) );
  }

  if ( this->overlay_output == VDP_INVALID_HANDLE ) {
    st = vdp_output_surface_create( vdp_device, VDP_RGBA_FORMAT_B8G8R8A8, this->overlay_output_width, this->overlay_output_height, &this->overlay_output );
    if ( st != VDP_STATUS_OK )
      printf( "vdpau_overlay_end: vdp_output_surface_create failed : %s\n", vdp_get_error_string(st) );
  }

  w = (this->overlay_unscaled_width>this->overlay_output_width) ? this->overlay_unscaled_width : this->overlay_output_width;
  h = (this->overlay_unscaled_height>this->overlay_output_height) ? this->overlay_unscaled_height : this->overlay_output_height;

  uint32_t *buf = (uint32_t*)malloc(w*h*4);
  uint32_t pitch = w*4;
  memset( buf, 0, w*h*4 );
  VdpRect clear = { 0, 0, this->overlay_output_width, this->overlay_output_height };
  st = vdp_output_surface_put_bits( this->overlay_output, &buf, &pitch, &clear );
  if ( st != VDP_STATUS_OK ) {
    printf( "vdpau_overlay_end: vdp_output_surface_put_bits (clear) failed : %s\n", vdp_get_error_string(st) );
  }
  clear.x1 = this->overlay_unscaled_width; clear.y1 = this->overlay_unscaled_height;
  st = vdp_output_surface_put_bits( this->overlay_unscaled, &buf, &pitch, &clear );
  if ( st != VDP_STATUS_OK ) {
    printf( "vdpau_overlay_end: vdp_output_surface_put_bits (clear) failed : %s\n", vdp_get_error_string(st) );
  }
  free(buf);

  VdpOutputSurface *surface;
  for ( i=0; i<this->ovl_changed-1; ++i ) {
    VdpRect dest = { this->overlays[i].ovl_x, this->overlays[i].ovl_y, this->overlays[i].ovl_x+this->overlays[i].ovl_w, this->overlays[i].ovl_y+this->overlays[i].ovl_h };
    VdpRect src = { 0, 0, this->overlays[i].ovl_w, this->overlays[i].ovl_h };
    surface = (this->overlays[i].unscaled) ? &this->overlay_unscaled : &this->overlay_output;
    st = vdp_output_surface_render_bitmap_surface( *surface, &dest, this->overlays[i].ovl_bitmap, &src, 0, &blend, 0 );
    if ( st != VDP_STATUS_OK ) {
      printf( "vdpau_overlay_end: vdp_output_surface_render_bitmap_surface failed : %s\n", vdp_get_error_string(st) );
    }
  }
  this->has_overlay = 1;
  this->ovl_changed = 0;
}



static void vdpau_frame_proc_slice (vo_frame_t *vo_img, uint8_t **src)
{
  vdpau_frame_t  *frame = (vdpau_frame_t *) vo_img ;

  vo_img->proc_called = 1;
}



static void vdpau_frame_field (vo_frame_t *vo_img, int which_field)
{
}



static void vdpau_frame_dispose (vo_frame_t *vo_img)
{
  vdpau_frame_t  *frame = (vdpau_frame_t *) vo_img ;

  if ( frame->chunk[0] )
    free (frame->chunk[0]);
  if ( frame->chunk[1] )
    free (frame->chunk[1]);
  if ( frame->chunk[2] )
    free (frame->chunk[2]);
  if ( frame->vdpau_accel_data.surface != VDP_INVALID_HANDLE )
    vdp_video_surface_destroy( frame->vdpau_accel_data.surface );
  free (frame);
}



static vo_frame_t *vdpau_alloc_frame (vo_driver_t *this_gen)
{
  vdpau_frame_t  *frame;
  vdpau_driver_t *this = (vdpau_driver_t *) this_gen;

  printf( "vo_vdpau: vdpau_alloc_frame\n" );

  frame = (vdpau_frame_t *) calloc(1, sizeof(vdpau_frame_t));

  if (!frame)
    return NULL;

  frame->chunk[0] = frame->chunk[1] = frame->chunk[2] = NULL;
  frame->width = frame->height = frame->format = frame->flags = 0;

  frame->vo_frame.accel_data = &frame->vdpau_accel_data;

  pthread_mutex_init (&frame->vo_frame.mutex, NULL);

  /*
   * supply required functions/fields
   */
  frame->vo_frame.proc_duplicate_frame_data = NULL;
  frame->vo_frame.proc_slice = vdpau_frame_proc_slice;
  frame->vo_frame.proc_frame = NULL;
  frame->vo_frame.field      = vdpau_frame_field;
  frame->vo_frame.dispose    = vdpau_frame_dispose;
  frame->vo_frame.driver     = this_gen;

  frame->vdpau_accel_data.vdp_device = vdp_device;
  frame->vdpau_accel_data.surface = VDP_INVALID_HANDLE;
  frame->vdpau_accel_data.chroma = VDP_CHROMA_TYPE_420;
  frame->vdpau_accel_data.vdp_decoder_create = vdp_decoder_create;
  frame->vdpau_accel_data.vdp_decoder_destroy = vdp_decoder_destroy;
  frame->vdpau_accel_data.vdp_decoder_render = vdp_decoder_render;
  frame->vdpau_accel_data.vdp_get_error_string = vdp_get_error_string;
  frame->vdpau_accel_data.vdp_runtime_nr = this->vdp_runtime_nr;
  frame->vdpau_accel_data.current_vdp_runtime_nr = &this->vdp_runtime_nr;

  return (vo_frame_t *) frame;
}



static void vdpau_provide_standard_frame_data (vo_frame_t *this_gen, xine_current_frame_data_t *data)
{
  vdpau_frame_t *this = (vdpau_frame_t *)this_gen;
  VdpStatus st;
  VdpYCbCrFormat format;

  if (this->vo_frame.format != XINE_IMGFMT_VDPAU) {
    fprintf(stderr, "vdpau_provide_standard_frame_data: unexpected frame format 0x%08x!\n", this->vo_frame.format);
    return;
  }

  if (!(this->flags & VO_CHROMA_422)) {
    data->format = XINE_IMGFMT_YV12;
    data->img_size = this->vo_frame.width * this->vo_frame.height
                   + ((this->vo_frame.width + 1) / 2) * ((this->vo_frame.height + 1) / 2)
                   + ((this->vo_frame.width + 1) / 2) * ((this->vo_frame.height + 1) / 2);
    if (data->img) {
      this->vo_frame.pitches[0] = 8*((this->vo_frame.width + 7) / 8);
      this->vo_frame.pitches[1] = 8*((this->vo_frame.width + 15) / 16);
      this->vo_frame.pitches[2] = 8*((this->vo_frame.width + 15) / 16);
      this->vo_frame.base[0] = xine_xmalloc_aligned(16, this->vo_frame.pitches[0] * this->vo_frame.height, (void **)&this->chunk[0]);
      this->vo_frame.base[1] = xine_xmalloc_aligned(16, this->vo_frame.pitches[1] * ((this->vo_frame.height+1)/2), (void **)&this->chunk[1]);
      this->vo_frame.base[2] = xine_xmalloc_aligned(16, this->vo_frame.pitches[2] * ((this->vo_frame.height+1)/2), (void **)&this->chunk[2]);
      format = VDP_YCBCR_FORMAT_YV12;
    }
  } else {
    data->format = XINE_IMGFMT_YUY2;
    data->img_size = this->vo_frame.width * this->vo_frame.height
                   + ((this->vo_frame.width + 1) / 2) * this->vo_frame.height
                   + ((this->vo_frame.width + 1) / 2) * this->vo_frame.height;
    if (data->img) {
      this->vo_frame.pitches[0] = 8*((this->vo_frame.width + 3) / 4);
      this->vo_frame.base[0] = xine_xmalloc_aligned(16, this->vo_frame.pitches[0] * this->vo_frame.height, (void **)&this->chunk[0]);
      format = VDP_YCBCR_FORMAT_YUYV;
    }
  }

  if (data->img) {
    st = vdp_video_surface_getbits_ycbcr(this->vdpau_accel_data.surface, format, this->vo_frame.base, this->vo_frame.pitches);
    if (st != VDP_STATUS_OK)
      printf("vo_vdpau: failed to get surface bits !! %s\n", vdp_get_error_string(st));

    if (format == VDP_YCBCR_FORMAT_YV12) {
      yv12_to_yv12(
       /* Y */
        this->vo_frame.base[0], this->vo_frame.pitches[0],
        data->img, this->vo_frame.width,
       /* U */
        this->vo_frame.base[2], this->vo_frame.pitches[2],
        data->img+this->vo_frame.width*this->vo_frame.height, this->vo_frame.width/2,
       /* V */
        this->vo_frame.base[1], this->vo_frame.pitches[1],
        data->img+this->vo_frame.width*this->vo_frame.height+this->vo_frame.width*this->vo_frame.height/4, this->vo_frame.width/2,
       /* width x height */
        this->vo_frame.width, this->vo_frame.height);
    } else {
      yuy2_to_yuy2(
       /* src */
        this->vo_frame.base[0], this->vo_frame.pitches[0],
       /* dst */
        data->img, this->vo_frame.width*2,
       /* width x height */
        this->vo_frame.width, this->vo_frame.height);
    }

    if (this->chunk[0])
      free(this->chunk[0]);
    if (this->chunk[1])
      free(this->chunk[1]);
    if (this->chunk[2])
      free(this->chunk[2]);
    this->chunk[0] = this->chunk[1] = this->chunk[2] = NULL;
  }
}

static void vdpau_duplicate_frame_data (vo_frame_t *this_gen, vo_frame_t *original)
{
  vdpau_frame_t *this = (vdpau_frame_t *)this_gen;
  vdpau_frame_t *orig = (vdpau_frame_t *)original;
  VdpStatus st;
  VdpYCbCrFormat format;

  if (orig->vo_frame.format != XINE_IMGFMT_VDPAU) {
    fprintf(stderr, "vdpau_duplicate_frame_data: unexpected frame format 0x%08x!\n", orig->vo_frame.format);
    return;
  }

  if (!(orig->flags & VO_CHROMA_422)) {
    this->vo_frame.pitches[0] = 8*((orig->vo_frame.width + 7) / 8);
    this->vo_frame.pitches[1] = 8*((orig->vo_frame.width + 15) / 16);
    this->vo_frame.pitches[2] = 8*((orig->vo_frame.width + 15) / 16);
    this->vo_frame.base[0] = xine_xmalloc_aligned(16, this->vo_frame.pitches[0] * orig->vo_frame.height, (void **)&this->chunk[0]);
    this->vo_frame.base[1] = xine_xmalloc_aligned(16, this->vo_frame.pitches[1] * ((orig->vo_frame.height+1)/2), (void **)&this->chunk[1]);
    this->vo_frame.base[2] = xine_xmalloc_aligned(16, this->vo_frame.pitches[2] * ((orig->vo_frame.height+1)/2), (void **)&this->chunk[2]);
    format = VDP_YCBCR_FORMAT_YV12;
  } else {
    this->vo_frame.pitches[0] = 8*((orig->vo_frame.width + 3) / 4);
    this->vo_frame.base[0] = xine_xmalloc_aligned(16, this->vo_frame.pitches[0] * orig->vo_frame.height, (void **)&this->chunk[0]);
    format = VDP_YCBCR_FORMAT_YUYV;
  }

  st = vdp_video_surface_getbits_ycbcr(orig->vdpau_accel_data.surface, format, this->vo_frame.base, this->vo_frame.pitches);
  if (st != VDP_STATUS_OK)
    printf("vo_vdpau: failed to get surface bits !! %s\n", vdp_get_error_string(st));

  st = vdp_video_surface_putbits_ycbcr(this->vdpau_accel_data.surface, format, this->vo_frame.base, this->vo_frame.pitches);
  if (st != VDP_STATUS_OK)
    printf("vo_vdpau: failed to put surface bits !! %s\n", vdp_get_error_string(st));

  if (this->chunk[0])
    free(this->chunk[0]);
  if (this->chunk[1])
    free(this->chunk[1]);
  if (this->chunk[2])
    free(this->chunk[2]);
  this->chunk[0] = this->chunk[1] = this->chunk[2] = NULL;
}



static void vdpau_update_frame_format (vo_driver_t *this_gen, vo_frame_t *frame_gen,
      uint32_t width, uint32_t height, double ratio, int format, int flags)
{
  vdpau_driver_t *this = (vdpau_driver_t *) this_gen;
  vdpau_frame_t   *frame = (vdpau_frame_t *) frame_gen;

  VdpChromaType chroma = (flags & VO_CHROMA_422) ? VDP_CHROMA_TYPE_422 : VDP_CHROMA_TYPE_420;

  /* Check frame size and format and reallocate if necessary */
  if ( (frame->width != width) || (frame->height != height) || (frame->format != format) || (frame->format==XINE_IMGFMT_VDPAU && frame->vdpau_accel_data.chroma!=chroma) ||
        (frame->vdpau_accel_data.vdp_runtime_nr != this->vdp_runtime_nr)) {
    //printf("vo_vdpau: updating frame to %d x %d (ratio=%g, format=%08X)\n", width, height, ratio, format);

    /* (re-) allocate render space */
    if ( frame->chunk[0] )
      free (frame->chunk[0]);
    if ( frame->chunk[1] )
      free (frame->chunk[1]);
    if ( frame->chunk[2] )
      free (frame->chunk[2]);
    frame->chunk[0] = frame->chunk[1] = frame->chunk[2] = NULL;

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

    if ( frame->vdpau_accel_data.vdp_runtime_nr != this->vdp_runtime_nr ) {
      frame->vdpau_accel_data.surface = VDP_INVALID_HANDLE;
      frame->vdpau_accel_data.vdp_runtime_nr = this->vdp_runtime_nr;
      frame->vdpau_accel_data.vdp_device = vdp_device;
      frame->vo_frame.proc_duplicate_frame_data = NULL;
      frame->vo_frame.proc_provide_standard_frame_data = NULL;
    }

    if ( frame->vdpau_accel_data.surface != VDP_INVALID_HANDLE  ) {
      if ( (frame->width != width) || (frame->height != height) || (format != XINE_IMGFMT_VDPAU) || frame->vdpau_accel_data.chroma != chroma ) {
        printf("vo_vdpau: update_frame - destroy surface\n");
        vdp_video_surface_destroy( frame->vdpau_accel_data.surface );
        frame->vdpau_accel_data.surface = VDP_INVALID_HANDLE;
        --this->allocated_surfaces;
        frame->vo_frame.proc_duplicate_frame_data = NULL;
        frame->vo_frame.proc_provide_standard_frame_data = NULL;
      }
    }

    if ( (format == XINE_IMGFMT_VDPAU) && (frame->vdpau_accel_data.surface == VDP_INVALID_HANDLE) ) {
      VdpStatus st = vdp_video_surface_create( vdp_device, chroma, width, height, &frame->vdpau_accel_data.surface );
      if ( st!=VDP_STATUS_OK )
        printf( "vo_vdpau: failed to create surface !! %s\n", vdp_get_error_string( st ) );
      else {
        frame->vdpau_accel_data.chroma = chroma;
        ++this->allocated_surfaces;
        frame->vo_frame.proc_duplicate_frame_data = vdpau_duplicate_frame_data;
        frame->vo_frame.proc_provide_standard_frame_data = vdpau_provide_standard_frame_data;
      }
    }

    frame->width = width;
    frame->height = height;
    frame->format = format;
    frame->flags = flags;

    vdpau_frame_field ((vo_frame_t *)frame, flags);
  }

  //printf("vo_vdpau: allocated_surfaces=%d\n", this->allocated_surfaces );

  frame->ratio = ratio;
  frame->vo_frame.future_frame = NULL;
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



static void vdpau_release_back_frames( vo_driver_t *this_gen )
{
  vdpau_driver_t  *this  = (vdpau_driver_t *) this_gen;
  int i;

  for ( i=0; i<NUM_FRAMES_BACK; ++i ) {
    if ( this->back_frame[ i ])
      this->back_frame[ i ]->vo_frame.free( &this->back_frame[ i ]->vo_frame );
    this->back_frame[ i ] = NULL;
  }
}



static void vdpau_backup_frame( vo_driver_t *this_gen, vo_frame_t *frame_gen )
{
  vdpau_driver_t  *this  = (vdpau_driver_t *) this_gen;
  vdpau_frame_t   *frame = (vdpau_frame_t *) frame_gen;

  int i;
  if ( this->back_frame[NUM_FRAMES_BACK-1]) {
    this->back_frame[NUM_FRAMES_BACK-1]->vo_frame.free (&this->back_frame[NUM_FRAMES_BACK-1]->vo_frame);
  }
  for ( i=NUM_FRAMES_BACK-1; i>0; i-- )
    this->back_frame[i] = this->back_frame[i-1];
  this->back_frame[0] = frame;
}



static void vdpau_set_deinterlace( vo_driver_t *this_gen )
{
  vdpau_driver_t  *this  = (vdpau_driver_t *) this_gen;

  VdpVideoMixerFeature features[] = { VDP_VIDEO_MIXER_FEATURE_DEINTERLACE_TEMPORAL, VDP_VIDEO_MIXER_FEATURE_DEINTERLACE_TEMPORAL_SPATIAL };
  VdpBool feature_enables[2];
  if ( this->deinterlace ) {
    if ( this->video_mixer_width<800 )
      feature_enables[0] = feature_enables[1] = 1;
    else {
      switch ( this->deinterlace_method ) {
        case 0: feature_enables[0] = feature_enables[1] = 0; break; /* bob */
        case 1: feature_enables[0] = 1; feature_enables[1] = 0; break; /* temporal */
        case 2: feature_enables[0] = feature_enables[1] = 1; break; /* temporal_spatial */
      }
    }
  }
  else
    feature_enables[0] = feature_enables[1] = 0;

  vdp_video_mixer_set_feature_enables( this->video_mixer, 2, features, feature_enables );
  vdp_video_mixer_get_feature_enables( this->video_mixer, 2, features, feature_enables );
  printf("vo_vdpau: enabled features: temporal=%d, temporal_spatial=%d\n", feature_enables[0], feature_enables[1] );
}



static void vdpau_set_inverse_telecine( vo_driver_t *this_gen )
{
  vdpau_driver_t  *this  = (vdpau_driver_t *) this_gen;

  VdpVideoMixerFeature features[] = { VDP_VIDEO_MIXER_FEATURE_INVERSE_TELECINE };
  VdpBool feature_enables[1];
  if ( this->deinterlace && this->enable_inverse_telecine )
    feature_enables[0] = 1;
  else
    feature_enables[0] = 0;

  vdp_video_mixer_set_feature_enables( this->video_mixer, 1, features, feature_enables );
  vdp_video_mixer_get_feature_enables( this->video_mixer, 1, features, feature_enables );
  printf("vo_vdpau: enabled features: inverse_telecine=%d\n", feature_enables[0] );
}



static void vdpau_update_deinterlace_method( void *this_gen, xine_cfg_entry_t *entry )
{
  vdpau_driver_t  *this  = (vdpau_driver_t *) this_gen;

  this->deinterlace_method = entry->num_value;
  printf( "vo_vdpau: deinterlace_method=%d\n", this->deinterlace_method );
  vdpau_set_deinterlace( (vo_driver_t*)this_gen );
}



static void vdpau_update_enable_inverse_telecine( void *this_gen, xine_cfg_entry_t *entry )
{
  vdpau_driver_t  *this  = (vdpau_driver_t *) this_gen;

  this->enable_inverse_telecine = entry->num_value;
  printf( "vo_vdpau: enable inverse_telecine=%d\n", this->enable_inverse_telecine );
  vdpau_set_inverse_telecine( (vo_driver_t*)this_gen );
}



static void vdpau_honor_progressive_flag( void *this_gen, xine_cfg_entry_t *entry )
{
  vdpau_driver_t  *this  = (vdpau_driver_t *) this_gen;

  this->honor_progressive = entry->num_value;
}



static void vdpau_update_noise( vdpau_driver_t *this_gen )
{
  float value = this_gen->noise/100.0;
  if ( value==0 ) {
    VdpVideoMixerFeature features[] = { VDP_VIDEO_MIXER_FEATURE_NOISE_REDUCTION };
    VdpBool feature_enables[] = { 0 };
    vdp_video_mixer_set_feature_enables( this_gen->video_mixer, 1, features, feature_enables );
    printf( "vo_vdpau: disable noise reduction.\n" );
    return;
  }
  else {
    VdpVideoMixerFeature features[] = { VDP_VIDEO_MIXER_FEATURE_NOISE_REDUCTION };
    VdpBool feature_enables[] = { 1 };
    vdp_video_mixer_set_feature_enables( this_gen->video_mixer, 1, features, feature_enables );
    printf( "vo_vdpau: enable noise reduction.\n" );
  }

  VdpVideoMixerAttribute attributes [] = { VDP_VIDEO_MIXER_ATTRIBUTE_NOISE_REDUCTION_LEVEL };
  void* attribute_values[] = { &value };
  VdpStatus st = vdp_video_mixer_set_attribute_values( this_gen->video_mixer, 1, attributes, attribute_values );
  if ( st != VDP_STATUS_OK )
    printf( "vo_vdpau: error, can't set noise reduction level !!\n" );
}



static void vdpau_update_sharpness( vdpau_driver_t *this_gen )
{
  float value = this_gen->sharpness/100.0;
  if ( value==0 ) {
    VdpVideoMixerFeature features[] = { VDP_VIDEO_MIXER_FEATURE_SHARPNESS  };
    VdpBool feature_enables[] = { 0 };
    vdp_video_mixer_set_feature_enables( this_gen->video_mixer, 1, features, feature_enables );
    printf( "vo_vdpau: disable sharpness.\n" );
    return;
  }
  else {
    VdpVideoMixerFeature features[] = { VDP_VIDEO_MIXER_FEATURE_SHARPNESS  };
    VdpBool feature_enables[] = { 1 };
    vdp_video_mixer_set_feature_enables( this_gen->video_mixer, 1, features, feature_enables );
    printf( "vo_vdpau: enable sharpness.\n" );
  }

  VdpVideoMixerAttribute attributes [] = { VDP_VIDEO_MIXER_ATTRIBUTE_SHARPNESS_LEVEL };
  void* attribute_values[] = { &value };
  VdpStatus st = vdp_video_mixer_set_attribute_values( this_gen->video_mixer, 1, attributes, attribute_values );
  if ( st != VDP_STATUS_OK )
    printf( "vo_vdpau: error, can't set sharpness level !!\n" );
}



static void vdpau_update_csc( vdpau_driver_t *this_gen )
{
  float hue = this_gen->hue/100.0;
  float saturation = this_gen->saturation/100.0;
  float contrast = this_gen->contrast/100.0;
  float brightness = this_gen->brightness/100.0;

  printf( "vo_vdpau: vdpau_update_csc: hue=%f, saturation=%f, contrast=%f, brightness=%f\n", hue, saturation, contrast, brightness );

  VdpCSCMatrix matrix;
  VdpProcamp procamp = { VDP_PROCAMP_VERSION, brightness, contrast, saturation, hue };

  VdpStatus st = vdp_generate_csc_matrix( &procamp, VDP_COLOR_STANDARD_ITUR_BT_601, &matrix );
  if ( st != VDP_STATUS_OK ) {
    printf( "vo_vdpau: error, can't generate csc matrix !!\n" );
    return;
  }
  VdpVideoMixerAttribute attributes [] = { VDP_VIDEO_MIXER_ATTRIBUTE_CSC_MATRIX };
  void* attribute_values[] = { &matrix };
  st = vdp_video_mixer_set_attribute_values( this_gen->video_mixer, 1, attributes, attribute_values );
  if ( st != VDP_STATUS_OK )
    printf( "vo_vdpau: error, can't set csc matrix !!\n" );
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
  VdpTime stream_speed;


  if(this->reinit_needed)
    vdpau_reinit(this_gen);

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
    //printf( "vo_vdpau: got a yuv image -------------\n" );
    chroma = ( frame->format==XINE_IMGFMT_YV12 )? VDP_CHROMA_TYPE_420 : VDP_CHROMA_TYPE_422;
    if ( (frame->width > this->soft_surface_width) || (frame->height > this->soft_surface_height) || (frame->format != this->soft_surface_format) ) {
      printf( "vo_vdpau: soft_surface size update\n" );
      /* recreate surface to match frame changes */
      this->soft_surface_width = frame->width;
      this->soft_surface_height = frame->height;
      this->soft_surface_format = frame->format;
      vdp_video_surface_destroy( this->soft_surface );
      vdp_video_surface_create( vdp_device, chroma, this->soft_surface_width, this->soft_surface_height, &this->soft_surface );
    }
    /* FIXME: have to swap U and V planes to get correct colors !! */
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
    surface = this->soft_surface;
    mix_w = this->soft_surface_width;
    mix_h = this->soft_surface_height;
  }
  else if (frame->format == XINE_IMGFMT_VDPAU) {
    //printf( "vo_vdpau: got a vdpau image -------------\n" );
    surface = frame->vdpau_accel_data.surface;
    mix_w = frame->width;
    mix_h = frame->height;
    chroma = (frame->vo_frame.flags & VO_CHROMA_422) ? VDP_CHROMA_TYPE_422 : VDP_CHROMA_TYPE_420;
  }
  else {
    /* unknown format */
    printf( "vo_vdpau: got an unknown image -------------\n" );
    frame->vo_frame.free( &frame->vo_frame );
    return;
  }

  if ( (mix_w != this->video_mixer_width) || (mix_h != this->video_mixer_height) || (chroma != this->video_mixer_chroma) ) {
    vdpau_release_back_frames( this_gen ); /* empty past frames array */
    printf("vo_vdpau: recreate mixer to match frames: width=%d, height=%d, chroma=%d\n", mix_w, mix_h, chroma);
    vdp_video_mixer_destroy( this->video_mixer );
    VdpVideoMixerFeature features[] = { VDP_VIDEO_MIXER_FEATURE_NOISE_REDUCTION, VDP_VIDEO_MIXER_FEATURE_SHARPNESS,
          VDP_VIDEO_MIXER_FEATURE_DEINTERLACE_TEMPORAL, VDP_VIDEO_MIXER_FEATURE_DEINTERLACE_TEMPORAL_SPATIAL };
    VdpVideoMixerParameter params[] = { VDP_VIDEO_MIXER_PARAMETER_VIDEO_SURFACE_WIDTH, VDP_VIDEO_MIXER_PARAMETER_VIDEO_SURFACE_HEIGHT,
          VDP_VIDEO_MIXER_PARAMETER_CHROMA_TYPE, VDP_VIDEO_MIXER_PARAMETER_LAYERS };
    int num_layers = 3;
    void const *param_values[] = { &mix_w, &mix_h, &chroma, &num_layers };
    vdp_video_mixer_create( vdp_device, 4, features, 4, params, param_values, &this->video_mixer );
    this->video_mixer_chroma = chroma;
    this->video_mixer_width = mix_w;
    this->video_mixer_height = mix_h;
    vdpau_set_deinterlace( this_gen );
    vdpau_set_inverse_telecine( this_gen );
    vdpau_update_noise( this );
    vdpau_update_sharpness( this );
    vdpau_update_csc( this );
  }

  if ( (this->sc.gui_width > this->output_surface_width[this->current_output_surface]) || (this->sc.gui_height > this->output_surface_height[this->current_output_surface]) ) {
    /* recreate output surface to match window size */
    printf( "vo_vdpau: output_surface size update\n" );
    this->output_surface_width[this->current_output_surface] = this->sc.gui_width;
    this->output_surface_height[this->current_output_surface] = this->sc.gui_height;

    vdp_output_surface_destroy( this->output_surface[this->current_output_surface] );
    vdp_output_surface_create( vdp_device, VDP_RGBA_FORMAT_B8G8R8A8, this->output_surface_width[this->current_output_surface], this->output_surface_height[this->current_output_surface], &this->output_surface[this->current_output_surface] );
  }

  VdpRect vid_source = { this->sc.displayed_xoffset, this->sc.displayed_yoffset, this->sc.displayed_width+this->sc.displayed_xoffset, this->sc.displayed_height+this->sc.displayed_yoffset };
  VdpRect out_dest = { 0, 0, this->sc.gui_width, this->sc.gui_height };
  VdpRect vid_dest = { this->sc.output_xoffset, this->sc.output_yoffset, this->sc.output_xoffset+this->sc.output_width, this->sc.output_yoffset+this->sc.output_height };

  //printf( "vid_src = %d %d %d %d - out_dest = %d %d %d %d - vid_dest = %d %d %d %d\n",
          //vid_source.x0, vid_source.y0, vid_source.x1, vid_source.y1, out_dest.x0, out_dest.y0, out_dest.x1, out_dest.y1, vid_dest.x0, vid_dest.y0, vid_dest.x1, vid_dest.y1 );

  /* prepare field delay calculation to not run into a deadlock while display locked */
  stream_speed = frame->vo_frame.stream ? xine_get_param(frame->vo_frame.stream, XINE_PARAM_FINE_SPEED) : 0;
  if (stream_speed != 0) {
    int vo_bufs_in_fifo = 0;
    _x_query_buffer_usage(frame->vo_frame.stream, NULL, NULL, &vo_bufs_in_fifo, NULL);
    //fprintf(stderr, "vo_bufs: %d\n", vo_bufs_in_fifo);
    if (vo_bufs_in_fifo <= 0)
      stream_speed = 0; /* still image -> no delay */
  }

  VdpTime last_time;

  if ( this->init_queue>1 )
    vdp_queue_block( vdp_queue, this->output_surface[this->current_output_surface], &last_time );

  XLockDisplay( this->display );

  uint32_t layer_count;
  VdpLayer layer[3];
  VdpRect layersrc, unscaledsrc;
  if ( this->has_overlay ) {
    //printf("vdpau_display_frame: overlay should be visible !\n");
    layer_count = 2;
    layersrc.x0 = 0; layersrc.y0 = 0; layersrc.x1 = this->overlay_output_width; layersrc.y1 = this->overlay_output_height;
    layer[0].struct_version = VDP_LAYER_VERSION; layer[0].source_surface = this->overlay_output; layer[0].source_rect = &layersrc; layer[0].destination_rect = &vid_dest;
    unscaledsrc.x0 = 0; unscaledsrc.y0 = 0; unscaledsrc.x1 = this->overlay_unscaled_width; unscaledsrc.y1 = this->overlay_unscaled_height;
    layer[1].struct_version = VDP_LAYER_VERSION; layer[1].source_surface = this->overlay_unscaled; layer[1].source_rect = &unscaledsrc; layer[1].destination_rect = &unscaledsrc;
    //printf( "layersrc = %d %d %d %d \n", layersrc.x0, layersrc.y0, layersrc.x1, layersrc.y1 );
  }
  else {
    layer_count = 0;
  }

  VdpRect argb_rect = {0, 0, this->argb_overlay_width, this->argb_overlay_height };
  if( this->has_argb_overlay ) {
    layer_count++;
    layer[layer_count-1].destination_rect = &vid_dest;
    layer[layer_count-1].source_rect = &argb_rect;
    layer[layer_count-1].source_surface = this->argb_overlay;
    layer[layer_count-1].struct_version = VDP_LAYER_VERSION;
  }

  int non_progressive = (this->honor_progressive && !frame->vo_frame.progressive_frame) || !this->honor_progressive;
  if ( frame->vo_frame.duration>2500 && this->deinterlace && non_progressive && frame->format==XINE_IMGFMT_VDPAU ) {
    VdpTime current_time = 0;
    VdpVideoSurface past[2];
    VdpVideoSurface future[1];
    VdpVideoMixerPictureStructure picture_structure;

    past[1] = past[0] = (this->back_frame[0] && (this->back_frame[0]->format==XINE_IMGFMT_VDPAU)) ? this->back_frame[0]->vdpau_accel_data.surface : VDP_INVALID_HANDLE;
    future[0] = surface;
    picture_structure = ( frame->vo_frame.top_field_first ) ? VDP_VIDEO_MIXER_PICTURE_STRUCTURE_TOP_FIELD : VDP_VIDEO_MIXER_PICTURE_STRUCTURE_BOTTOM_FIELD;

    st = vdp_video_mixer_render( this->video_mixer, VDP_INVALID_HANDLE, 0, picture_structure,
                               2, past, surface, 1, future, &vid_source, this->output_surface[this->current_output_surface], &out_dest, &vid_dest, layer_count, layer_count?layer:NULL );
    if ( st != VDP_STATUS_OK )
      printf( "vo_vdpau: vdp_video_mixer_render error : %s\n", vdp_get_error_string( st ) );
    //else
      //printf( "vo_vdpau: vdp_video_mixer_render: top_field, past1=%d, past0=%d, current=%d, future=%d\n", past[1], past[0], surface, future[0] );

    vdp_queue_get_time( vdp_queue, &current_time );
    vdp_queue_display( vdp_queue, this->output_surface[this->current_output_surface], 0, 0, current_time );
    if ( this->init_queue<2 ) ++this->init_queue;
    this->current_output_surface ^= 1;
    if ( this->init_queue>1 ) {
      XUnlockDisplay(this->display);
      vdp_queue_block( vdp_queue, this->output_surface[this->current_output_surface], &last_time );
      XLockDisplay(this->display);
    }

    if ( (this->sc.gui_width > this->output_surface_width[this->current_output_surface]) || (this->sc.gui_height > this->output_surface_height[this->current_output_surface]) ) {
      /* recreate output surface to match window size */
      printf( "vo_vdpau: output_surface size update\n" );
      this->output_surface_width[this->current_output_surface] = this->sc.gui_width;
      this->output_surface_height[this->current_output_surface] = this->sc.gui_height;

      vdp_output_surface_destroy( this->output_surface[this->current_output_surface] );
      vdp_output_surface_create( vdp_device, VDP_RGBA_FORMAT_B8G8R8A8, this->output_surface_width[this->current_output_surface], this->output_surface_height[this->current_output_surface], &this->output_surface[this->current_output_surface] );
    }

    past[0] = surface;
    if ( frame->vo_frame.future_frame!=NULL )
      future[0] = ((vdpau_frame_t*)(frame->vo_frame.future_frame))->vdpau_accel_data.surface;
    else
      future[0] = VDP_INVALID_HANDLE;
    picture_structure = ( frame->vo_frame.top_field_first ) ? VDP_VIDEO_MIXER_PICTURE_STRUCTURE_BOTTOM_FIELD : VDP_VIDEO_MIXER_PICTURE_STRUCTURE_TOP_FIELD;

    st = vdp_video_mixer_render( this->video_mixer, VDP_INVALID_HANDLE, 0, picture_structure,
                               2, past, surface, 1, future, &vid_source, this->output_surface[this->current_output_surface], &out_dest, &vid_dest, layer_count, layer_count?layer:NULL );
    if ( st != VDP_STATUS_OK )
      printf( "vo_vdpau: vdp_video_mixer_render error : %s\n", vdp_get_error_string( st ) );
    //else
      //printf( "vo_vdpau: vdp_video_mixer_render: bottom_field, past1=%d, past0=%d, current=%d, future=%d\n", past[1], past[0], surface, future[0] );

    /* calculate delay for second field: there should be no delay for still images otherwise, take replay speed into account */
    if (stream_speed > 0)
      current_time += frame->vo_frame.duration * 100000ull * XINE_FINE_SPEED_NORMAL / (18 * stream_speed);
    else
      current_time = 0; /* immediately i. e. no delay */

    vdp_queue_display( vdp_queue, this->output_surface[this->current_output_surface], 0, 0, current_time );
    if ( this->init_queue<2 ) ++this->init_queue;
    this->current_output_surface ^= 1;
  }
  else {
    st = vdp_video_mixer_render( this->video_mixer, VDP_INVALID_HANDLE, 0, VDP_VIDEO_MIXER_PICTURE_STRUCTURE_FRAME,
                               0, 0, surface, 0, 0, &vid_source, this->output_surface[this->current_output_surface], &out_dest, &vid_dest, layer_count, layer_count?layer:NULL );
    if ( st != VDP_STATUS_OK )
      printf( "vo_vdpau: vdp_video_mixer_render error : %s\n", vdp_get_error_string( st ) );

    vdp_queue_display( vdp_queue, this->output_surface[this->current_output_surface], 0, 0, 0 );
    if ( this->init_queue<2 ) ++this->init_queue;
    this->current_output_surface ^= 1;
  }

  XUnlockDisplay( this->display );

  vdpau_backup_frame( this_gen, frame_gen );
}



static int vdpau_get_property (vo_driver_t *this_gen, int property)
{
  vdpau_driver_t *this = (vdpau_driver_t*)this_gen;

  switch (property) {
    case VO_PROP_MAX_NUM_FRAMES:
      return 22;
    case VO_PROP_WINDOW_WIDTH:
      return this->sc.gui_width;
    case VO_PROP_WINDOW_HEIGHT:
      return this->sc.gui_height;
    case VO_PROP_OUTPUT_WIDTH:
      return this->sc.output_width;
    case VO_PROP_OUTPUT_HEIGHT:
      return this->sc.output_height;
    case VO_PROP_OUTPUT_XOFFSET:
      return this->sc.output_xoffset;
    case VO_PROP_OUTPUT_YOFFSET:
      return this->sc.output_yoffset;
    case VO_PROP_HUE:
      return this->hue;
    case VO_PROP_SATURATION:
      return this->saturation;
    case VO_PROP_CONTRAST:
      return this->contrast;
    case VO_PROP_BRIGHTNESS:
      return this->brightness;
    case VO_PROP_SHARPNESS:
      return this->sharpness;
    case VO_PROP_NOISE_REDUCTION:
      return this->noise;
  }

  return -1;
}



static int vdpau_set_property (vo_driver_t *this_gen, int property, int value)
{
  vdpau_driver_t *this = (vdpau_driver_t*)this_gen;

  printf("vdpau_set_property: property=%d, value=%d\n", property, value );

  switch (property) {
    case VO_PROP_INTERLACED:
      this->deinterlace = value;
      vdpau_set_deinterlace( this_gen );
      break;
    case VO_PROP_ZOOM_X:
      if ((value >= XINE_VO_ZOOM_MIN) && (value <= XINE_VO_ZOOM_MAX)) {
        this->sc.zoom_factor_x = (double)value / (double)XINE_VO_ZOOM_STEP;
        _x_vo_scale_compute_ideal_size( &this->sc );
        this->sc.force_redraw = 1;    /* trigger re-calc of output size */
      }
      break;
    case VO_PROP_ZOOM_Y:
      if ((value >= XINE_VO_ZOOM_MIN) && (value <= XINE_VO_ZOOM_MAX)) {
        this->sc.zoom_factor_y = (double)value / (double)XINE_VO_ZOOM_STEP;
        _x_vo_scale_compute_ideal_size( &this->sc );
        this->sc.force_redraw = 1;    /* trigger re-calc of output size */
      }
      break;
    case VO_PROP_ASPECT_RATIO:
      if ( value>=XINE_VO_ASPECT_NUM_RATIOS )
        value = XINE_VO_ASPECT_AUTO;
      this->sc.user_ratio = value;
      this->sc.force_redraw = 1;    /* trigger re-calc of output size */
      break;
    case VO_PROP_HUE: this->hue = value; vdpau_update_csc( this ); break;
    case VO_PROP_SATURATION: this->saturation = value; vdpau_update_csc( this ); break;
    case VO_PROP_CONTRAST: this->contrast = value; vdpau_update_csc( this ); break;
    case VO_PROP_BRIGHTNESS: this->brightness = value; vdpau_update_csc( this ); break;
    case VO_PROP_SHARPNESS: this->sharpness = value; vdpau_update_sharpness( this ); break;
    case VO_PROP_NOISE_REDUCTION: this->noise = value; vdpau_update_noise( this ); break;
  }

  return value;
}



static void vdpau_get_property_min_max (vo_driver_t *this_gen, int property, int *min, int *max)
{
  switch ( property ) {
    case VO_PROP_HUE:
      *max = 314; *min = -314; break;
    case VO_PROP_SATURATION:
      *max = 1000; *min = 0; break;
    case VO_PROP_CONTRAST:
      *max = 1000; *min = 0; break;
    case VO_PROP_BRIGHTNESS:
      *max = 100; *min = -100; break;
    case VO_PROP_SHARPNESS:
      *max = 100; *min = -100; break;
    case VO_PROP_NOISE_REDUCTION:
      *max = 100; *min = 0; break;
    default:
      *max = 0; *min = 0;
  }
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
      if ( this->init_queue ) {
        XLockDisplay( this->display );
        int previous = this->current_output_surface ^ 1;
        vdp_queue_display( vdp_queue, this->output_surface[previous], 0, 0, 0 );
        XUnlockDisplay( this->display );
      }
      break;
    }

    case XINE_GUI_SEND_DRAWABLE_CHANGED: {
      VdpStatus st;
      XLockDisplay( this->display );
      this->drawable = (Drawable) data;
      vdp_queue_destroy( vdp_queue );
      vdp_queue_target_destroy( vdp_queue_target );
      st = vdp_queue_target_create_x11( vdp_device, this->drawable, &vdp_queue_target );
      if ( st != VDP_STATUS_OK ) {
        printf( "vo_vdpau: FATAL !! Can't recreate presentation queue target after drawable change !!\n" );
        XUnlockDisplay( this->display );
        break;
      }
      st = vdp_queue_create( vdp_device, vdp_queue_target, &vdp_queue );
      if ( st != VDP_STATUS_OK ) {
        printf( "vo_vdpau: FATAL !! Can't recreate presentation queue after drawable change !!\n" );
        XUnlockDisplay( this->display );
        break;
      }
      vdp_queue_set_background_color( vdp_queue, &this->back_color );
      XUnlockDisplay( this->display );
      this->sc.force_redraw = 1;
      break;
    }

    case XINE_GUI_SEND_TRANSLATE_GUI_TO_VIDEO: {
      int x1, y1, x2, y2;
      x11_rectangle_t *rect = data;

      _x_vo_scale_translate_gui2video(&this->sc, rect->x, rect->y, &x1, &y1);
      _x_vo_scale_translate_gui2video(&this->sc, rect->x + rect->w, rect->y + rect->h, &x2, &y2);
      rect->x = x1;
      rect->y = y1;
      rect->w = x2-x1;
      rect->h = y2-y1;
      break;
    }

    default:
      return -1;
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

  this->ovl_yuv2rgb->dispose(this->ovl_yuv2rgb);
  this->yuv2rgb_factory->dispose (this->yuv2rgb_factory);

  for ( i=0; i<XINE_VORAW_MAX_OVL; ++i ) {
    if ( this->overlays[i].ovl_bitmap != VDP_INVALID_HANDLE )
      vdp_bitmap_destroy( this->overlays[i].ovl_bitmap );
  }

  if ( this->video_mixer!=VDP_INVALID_HANDLE )
    vdp_video_mixer_destroy( this->video_mixer );
  if ( this->overlay_unscaled!=VDP_INVALID_HANDLE )
    vdp_output_surface_destroy( this->overlay_unscaled );
  if ( this->overlay_output!=VDP_INVALID_HANDLE )
    vdp_output_surface_destroy( this->overlay_output );
  if ( this->output_surface[0]!=VDP_INVALID_HANDLE )
    vdp_output_surface_destroy( this->output_surface[0] );
  if ( this->output_surface[1]!=VDP_INVALID_HANDLE )
    vdp_output_surface_destroy( this->output_surface[1] );
  if ( this->soft_surface != VDP_INVALID_HANDLE )
    vdp_video_surface_destroy( this->soft_surface );
  vdp_queue_destroy( vdp_queue );
  vdp_queue_target_destroy( vdp_queue_target );

  for ( i=0; i<NUM_FRAMES_BACK; i++ )
    if ( this->back_frame[i] )
      this->back_frame[i]->vo_frame.dispose( &this->back_frame[i]->vo_frame );

  free (this);
}



static int vdpau_reinit_error( VdpStatus st, const char *msg )
{
  if ( st != VDP_STATUS_OK ) {
    printf( "vo_vdpau: %s : %s\n", msg, vdp_get_error_string( st ) );
    return 1;
  }
  return 0;
}



static void vdpau_reinit( vo_driver_t *this_gen )
{
  printf("vo_vdpau: VDPAU was pre-empted. Reinit.\n");
  vdpau_driver_t *this = (vdpau_driver_t *)this_gen;

  XLockDisplay(guarded_display);
  vdpau_release_back_frames(this_gen);

  VdpStatus st = vdp_device_create_x11( this->display, this->screen, &vdp_device, &vdp_get_proc_address );

  if ( st != VDP_STATUS_OK ) {
    printf( "vo_vdpau: Can't create vdp device : " );
    if ( st == VDP_STATUS_NO_IMPLEMENTATION )
      printf( "No vdpau implementation.\n" );
    else
      printf( "unsupported GPU?\n" );
    return;
  }

  st = vdp_queue_target_create_x11( vdp_device, this->drawable, &vdp_queue_target );
  if ( vdpau_reinit_error( st, "Can't create presentation queue target !!" ) )
    return;
  st = vdp_queue_create( vdp_device, vdp_queue_target, &vdp_queue );
  if ( vdpau_reinit_error( st, "Can't create presentation queue !!" ) )
    return;


  VdpChromaType chroma = VDP_CHROMA_TYPE_420;
  st = orig_vdp_video_surface_create( vdp_device, chroma, this->soft_surface_width, this->soft_surface_height, &this->soft_surface );
  if ( vdpau_reinit_error( st, "Can't create video surface !!" ) )
    return;

  this->current_output_surface = 0;
  this->init_queue = 0;
  st = vdp_output_surface_create( vdp_device, VDP_RGBA_FORMAT_B8G8R8A8, this->output_surface_width[0], this->output_surface_height[0], &this->output_surface[0] );
  if ( vdpau_reinit_error( st, "Can't create first output surface !!" ) ) {
    orig_vdp_video_surface_destroy( this->soft_surface );
    return;
  }
  st = vdp_output_surface_create( vdp_device, VDP_RGBA_FORMAT_B8G8R8A8, this->output_surface_width[0], this->output_surface_height[0], &this->output_surface[1] );
  if ( vdpau_reinit_error( st, "Can't create second output surface !!" ) ) {
    orig_vdp_video_surface_destroy( this->soft_surface );
    vdp_output_surface_destroy( this->output_surface[0] );
    return;
  }

  this->video_mixer_chroma = chroma;
  VdpVideoMixerFeature features[] = { VDP_VIDEO_MIXER_FEATURE_NOISE_REDUCTION, VDP_VIDEO_MIXER_FEATURE_SHARPNESS,
        VDP_VIDEO_MIXER_FEATURE_DEINTERLACE_TEMPORAL, VDP_VIDEO_MIXER_FEATURE_DEINTERLACE_TEMPORAL_SPATIAL };
  VdpVideoMixerParameter params[] = { VDP_VIDEO_MIXER_PARAMETER_VIDEO_SURFACE_WIDTH, VDP_VIDEO_MIXER_PARAMETER_VIDEO_SURFACE_HEIGHT, VDP_VIDEO_MIXER_PARAMETER_CHROMA_TYPE, VDP_VIDEO_MIXER_PARAMETER_LAYERS };
  int num_layers = 3;
  void const *param_values[] = { &this->video_mixer_width, &this->video_mixer_height, &chroma, &num_layers };
  st = vdp_video_mixer_create( vdp_device, 4, features, 4, params, param_values, &this->video_mixer );
  if ( vdpau_reinit_error( st, "Can't create video mixer !!" ) ) {
    orig_vdp_video_surface_destroy( this->soft_surface );
    vdp_output_surface_destroy( this->output_surface[0] );
    vdp_output_surface_destroy( this->output_surface[1] );
    return;
  }

  vdp_preemption_callback_register(vdp_device, &vdp_preemption_callback, (void*)this);

  XUnlockDisplay(guarded_display);
  printf("vo_vdpau: Reinit done.\n");
  this->vdp_runtime_nr++;
  this->reinit_needed = 0;
}



static void vdp_preemption_callback(VdpDevice device, void *context)
{
  printf("vo_vdpau: VDPAU preemption callback\n");
  vdpau_driver_t *this = (vdpau_driver_t *)context;
  this->reinit_needed = 1;
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

  guarded_display     = visual->display;
  this->display       = visual->display;
  this->screen        = visual->screen;
  this->drawable      = visual->d;

  _x_vo_scale_init(&this->sc, 1, 0, config);
  this->sc.frame_output_cb  = visual->frame_output_cb;
  this->sc.dest_size_cb     = visual->dest_size_cb;
  this->sc.user_data        = visual->user_data;
  this->sc.user_ratio       = XINE_VO_ASPECT_AUTO;

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

  for ( i=0; i<XINE_VORAW_MAX_OVL; ++i ) {
    this->overlays[i].ovl_w = this->overlays[i].ovl_h = 0;
    this->overlays[i].bitmap_width = this->overlays[i].bitmap_height = 0;
    this->overlays[i].ovl_bitmap = VDP_INVALID_HANDLE;
    this->overlays[i].ovl_x = this->overlays[i].ovl_y = 0;
  }
  this->overlay_output = VDP_INVALID_HANDLE;
  this->overlay_output_width = this->overlay_output_height = 0;
  this->overlay_unscaled = VDP_INVALID_HANDLE;
  this->overlay_unscaled_width = this->overlay_unscaled_height = 0;
  this->ovl_changed = 0;
  this->has_overlay = 0;
  this->has_unscaled = 0;

  this->argb_overlay = VDP_INVALID_HANDLE;
  this->argb_overlay_width = this->argb_overlay_height = 0;
  this->has_argb_overlay = 0;

  /*  overlay converter */
  this->yuv2rgb_factory = yuv2rgb_factory_init (MODE_24_BGR, 0, NULL);
  this->ovl_yuv2rgb = this->yuv2rgb_factory->create_converter( this->yuv2rgb_factory );

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
  st = vdp_get_proc_address( vdp_device, VDP_FUNC_ID_VIDEO_SURFACE_CREATE , (void*)&orig_vdp_video_surface_create ); vdp_video_surface_create = guarded_vdp_video_surface_create;
  if ( vdpau_init_error( st, "Can't get VIDEO_SURFACE_CREATE proc address !!", &this->vo_driver, 1 ) )
    return NULL;
  st = vdp_get_proc_address( vdp_device, VDP_FUNC_ID_VIDEO_SURFACE_DESTROY , (void*)&orig_vdp_video_surface_destroy ); vdp_video_surface_destroy = guarded_vdp_video_surface_destroy;
  if ( vdpau_init_error( st, "Can't get VIDEO_SURFACE_DESTROY proc address !!", &this->vo_driver, 1 ) )
    return NULL;
  st = vdp_get_proc_address( vdp_device, VDP_FUNC_ID_VIDEO_SURFACE_PUT_BITS_Y_CB_CR , (void*)&vdp_video_surface_putbits_ycbcr );
  if ( vdpau_init_error( st, "Can't get VIDEO_SURFACE_PUT_BITS_Y_CB_CR proc address !!", &this->vo_driver, 1 ) )
    return NULL;
  st = vdp_get_proc_address( vdp_device, VDP_FUNC_ID_VIDEO_SURFACE_GET_BITS_Y_CB_CR , (void*)&vdp_video_surface_getbits_ycbcr );
  if ( vdpau_init_error( st, "Can't get VIDEO_SURFACE_GET_BITS_Y_CB_CR proc address !!", &this->vo_driver, 1 ) )
    return NULL;
  st = vdp_get_proc_address( vdp_device, VDP_FUNC_ID_OUTPUT_SURFACE_CREATE , (void*)&vdp_output_surface_create );
  if ( vdpau_init_error( st, "Can't get OUTPUT_SURFACE_CREATE proc address !!", &this->vo_driver, 1 ) )
    return NULL;
  st = vdp_get_proc_address( vdp_device, VDP_FUNC_ID_OUTPUT_SURFACE_DESTROY , (void*)&vdp_output_surface_destroy );
  if ( vdpau_init_error( st, "Can't get OUTPUT_SURFACE_DESTROY proc address !!", &this->vo_driver, 1 ) )
    return NULL;
  st = vdp_get_proc_address( vdp_device, VDP_FUNC_ID_OUTPUT_SURFACE_RENDER_BITMAP_SURFACE , (void*)&vdp_output_surface_render_bitmap_surface );
  if ( vdpau_init_error( st, "Can't get OUTPUT_SURFACE_RENDER_BITMAP_SURFACE proc address !!", &this->vo_driver, 1 ) )
    return NULL;
  st = vdp_get_proc_address( vdp_device, VDP_FUNC_ID_OUTPUT_SURFACE_PUT_BITS_NATIVE , (void*)&vdp_output_surface_put_bits );
  if ( vdpau_init_error( st, "Can't get VDP_FUNC_ID_OUTPUT_SURFACE_PUT_BITS_NATIVE proc address !!", &this->vo_driver, 1 ) )
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
  st = vdp_get_proc_address( vdp_device, VDP_FUNC_ID_VIDEO_MIXER_SET_ATTRIBUTE_VALUES , (void*)&vdp_video_mixer_set_attribute_values );
  if ( vdpau_init_error( st, "Can't get VIDEO_MIXER_SET_ATTRIBUTE_VALUES proc address !!", &this->vo_driver, 1 ) )
    return NULL;
  st = vdp_get_proc_address( vdp_device, VDP_FUNC_ID_VIDEO_MIXER_SET_FEATURE_ENABLES , (void*)&vdp_video_mixer_set_feature_enables );
  if ( vdpau_init_error( st, "Can't get VIDEO_MIXER_SET_FEATURE_ENABLES proc address !!", &this->vo_driver, 1 ) )
    return NULL;
  st = vdp_get_proc_address( vdp_device, VDP_FUNC_ID_VIDEO_MIXER_GET_FEATURE_ENABLES , (void*)&vdp_video_mixer_get_feature_enables );
  if ( vdpau_init_error( st, "Can't get VIDEO_MIXER_GET_FEATURE_ENABLES proc address !!", &this->vo_driver, 1 ) )
    return NULL;
  st = vdp_get_proc_address( vdp_device, VDP_FUNC_ID_GENERATE_CSC_MATRIX , (void*)&vdp_generate_csc_matrix );
  if ( vdpau_init_error( st, "Can't get GENERATE_CSC_MATRIX proc address !!", &this->vo_driver, 1 ) )
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
  st = vdp_get_proc_address( vdp_device, VDP_FUNC_ID_PRESENTATION_QUEUE_BLOCK_UNTIL_SURFACE_IDLE , (void*)&vdp_queue_block );
  if ( vdpau_init_error( st, "Can't get PRESENTATION_QUEUE_BLOCK_UNTIL_SURFACE_IDLE proc address !!", &this->vo_driver, 1 ) )
    return NULL;
  st = vdp_get_proc_address( vdp_device, VDP_FUNC_ID_PRESENTATION_QUEUE_SET_BACKGROUND_COLOR , (void*)&vdp_queue_set_background_color );
  if ( vdpau_init_error( st, "Can't get PRESENTATION_QUEUE_SET_BACKGROUND_COLOR proc address !!", &this->vo_driver, 1 ) )
    return NULL;
  st = vdp_get_proc_address( vdp_device, VDP_FUNC_ID_PRESENTATION_QUEUE_GET_TIME , (void*)&vdp_queue_get_time );
  if ( vdpau_init_error( st, "Can't get PRESENTATION_QUEUE_GET_TIME proc address !!", &this->vo_driver, 1 ) )
    return NULL;
  st = vdp_get_proc_address( vdp_device, VDP_FUNC_ID_DECODER_QUERY_CAPABILITIES , (void*)&vdp_decoder_query_capabilities );
  if ( vdpau_init_error( st, "Can't get DECODER_QUERY_CAPABILITIES proc address !!", &this->vo_driver, 1 ) )
    return NULL;
  st = vdp_get_proc_address( vdp_device, VDP_FUNC_ID_DECODER_CREATE , (void*)&orig_vdp_decoder_create ); vdp_decoder_create = guarded_vdp_decoder_create;
  if ( vdpau_init_error( st, "Can't get DECODER_CREATE proc address !!", &this->vo_driver, 1 ) )
    return NULL;
  st = vdp_get_proc_address( vdp_device, VDP_FUNC_ID_DECODER_DESTROY , (void*)&orig_vdp_decoder_destroy ); vdp_decoder_destroy = guarded_vdp_decoder_destroy;
  if ( vdpau_init_error( st, "Can't get DECODER_DESTROY proc address !!", &this->vo_driver, 1 ) )
    return NULL;
  st = vdp_get_proc_address( vdp_device, VDP_FUNC_ID_DECODER_RENDER , (void*)&orig_vdp_decoder_render ); vdp_decoder_render = guarded_vdp_decoder_render;
  if ( vdpau_init_error( st, "Can't get DECODER_RENDER proc address !!", &this->vo_driver, 1 ) )
    return NULL;
  st = vdp_get_proc_address( vdp_device, VDP_FUNC_ID_BITMAP_SURFACE_CREATE , (void*)&vdp_bitmap_create );
  if ( vdpau_init_error( st, "Can't get BITMAP_SURFACE_CREATE proc address !!", &this->vo_driver, 1 ) )
    return NULL;
  st = vdp_get_proc_address( vdp_device, VDP_FUNC_ID_BITMAP_SURFACE_DESTROY , (void*)&vdp_bitmap_destroy );
  if ( vdpau_init_error( st, "Can't get BITMAP_SURFACE_DESTROY proc address !!", &this->vo_driver, 1 ) )
    return NULL;
  st = vdp_get_proc_address( vdp_device, VDP_FUNC_ID_BITMAP_SURFACE_PUT_BITS_NATIVE , (void*)&vdp_bitmap_put_bits );
  if ( vdpau_init_error( st, "Can't get BITMAP_SURFACE_PUT_BITS_NATIVE proc address !!", &this->vo_driver, 1 ) )
    return NULL;
  st = vdp_get_proc_address( vdp_device, VDP_FUNC_ID_PREEMPTION_CALLBACK_REGISTER, (void*)&vdp_preemption_callback_register );
  if ( vdpau_init_error( st, "Can't get PREEMPTION_CALLBACK_REGISTER proc address !!", &this->vo_driver, 1 ) )
    return NULL;

  st = vdp_preemption_callback_register(vdp_device, &vdp_preemption_callback, (void*)this);
  if ( vdpau_init_error( st, "Can't register preemption callback !!", &this->vo_driver, 1 ) )
    return NULL;

  st = vdp_queue_target_create_x11( vdp_device, this->drawable, &vdp_queue_target );
  if ( vdpau_init_error( st, "Can't create presentation queue target !!", &this->vo_driver, 1 ) )
    return NULL;
  st = vdp_queue_create( vdp_device, vdp_queue_target, &vdp_queue );
  if ( vdpau_init_error( st, "Can't create presentation queue !!", &this->vo_driver, 1 ) )
    return NULL;

  /* choose almost black as backcolor for color keying */
  this->back_color.red = 0.02;
  this->back_color.green = 0.01;
  this->back_color.blue = 0.03;
  this->back_color.alpha = 1;
  vdp_queue_set_background_color( vdp_queue, &this->back_color );

  this->soft_surface_width = 320;
  this->soft_surface_height = 240;
  this->soft_surface_format = XINE_IMGFMT_YV12;
  VdpChromaType chroma = VDP_CHROMA_TYPE_420;
  st = vdp_video_surface_create( vdp_device, chroma, this->soft_surface_width, this->soft_surface_height, &this->soft_surface );
  if ( vdpau_init_error( st, "Can't create video surface !!", &this->vo_driver, 1 ) )
    return NULL;

  this->output_surface_width[0] = this->output_surface_width[1] = 320;
  this->output_surface_height[0] = this->output_surface_height[1] = 240;
  this->current_output_surface = 0;
  this->init_queue = 0;
  st = vdp_output_surface_create( vdp_device, VDP_RGBA_FORMAT_B8G8R8A8, this->output_surface_width[0], this->output_surface_height[0], &this->output_surface[0] );
  if ( vdpau_init_error( st, "Can't create first output surface !!", &this->vo_driver, 1 ) ) {
    vdp_video_surface_destroy( this->soft_surface );
    return NULL;
  }
  st = vdp_output_surface_create( vdp_device, VDP_RGBA_FORMAT_B8G8R8A8, this->output_surface_width[0], this->output_surface_height[0], &this->output_surface[1] );
  if ( vdpau_init_error( st, "Can't create second output surface !!", &this->vo_driver, 1 ) ) {
    vdp_video_surface_destroy( this->soft_surface );
    vdp_output_surface_destroy( this->output_surface[0] );
    return NULL;
  }

  this->video_mixer_chroma = chroma;
  this->video_mixer_width = this->soft_surface_width;
  this->video_mixer_height = this->soft_surface_height;
  VdpVideoMixerFeature features[] = { VDP_VIDEO_MIXER_FEATURE_NOISE_REDUCTION, VDP_VIDEO_MIXER_FEATURE_SHARPNESS,
        VDP_VIDEO_MIXER_FEATURE_DEINTERLACE_TEMPORAL, VDP_VIDEO_MIXER_FEATURE_DEINTERLACE_TEMPORAL_SPATIAL };
  VdpVideoMixerParameter params[] = { VDP_VIDEO_MIXER_PARAMETER_VIDEO_SURFACE_WIDTH, VDP_VIDEO_MIXER_PARAMETER_VIDEO_SURFACE_HEIGHT, VDP_VIDEO_MIXER_PARAMETER_CHROMA_TYPE, VDP_VIDEO_MIXER_PARAMETER_LAYERS };
  int num_layers = 3;
  void const *param_values[] = { &this->video_mixer_width, &this->video_mixer_height, &chroma, &num_layers };
  st = vdp_video_mixer_create( vdp_device, 4, features, 4, params, param_values, &this->video_mixer );
  if ( vdpau_init_error( st, "Can't create video mixer !!", &this->vo_driver, 1 ) ) {
    vdp_video_surface_destroy( this->soft_surface );
    vdp_output_surface_destroy( this->output_surface[0] );
    vdp_output_surface_destroy( this->output_surface[1] );
    return NULL;
  }

  this->deinterlace_method = config->register_enum( config, "video.output.vdpau_deinterlace_method", 1,
         vdpau_deinterlace_methods, _("vdpau: HD deinterlace method"),
         _("bob\n"
           "Basic deinterlacing, doing 50i->50p.\n\n"
           "temporal\n"
           "Very good, 50i->50p\n\n"
           "temporal_spatial\n"
           "The best, but very GPU intensive.\n\n"),
         10, vdpau_update_deinterlace_method, this );

  this->enable_inverse_telecine = config->register_bool( config, "video.output.vdpau_enable_inverse_telecine", 1,
      _("vdpau: Try to recreate progressive frames from pulldown material"),
      _("Enable this to detect bad-flagged progressive content to which\n"
        "a 2:2 or 3:2 pulldown was applied.\n\n"),
        10, vdpau_update_enable_inverse_telecine, this );

  this->honor_progressive = config->register_bool( config, "video.output.vdpau_honor_progressive", 0,
        _("vdpau: disable deinterlacing when progressive_frame flag is set"),
        _("Set to true if you want to trust the progressive_frame stream's flag.\n"
          "This flag is not always reliable.\n\n"),
        10, vdpau_honor_progressive_flag, this );

  this->capabilities = VO_CAP_YV12 | VO_CAP_YUY2 | VO_CAP_CROP | VO_CAP_UNSCALED_OVERLAY | VO_CAP_CUSTOM_EXTENT_OVERLAY | VO_CAP_ARGB_LAYER_OVERLAY;
  ok = 0;
  uint32_t mw, mh, ml, mr;
  st = vdp_decoder_query_capabilities( vdp_device, VDP_DECODER_PROFILE_H264_MAIN, &ok, &ml, &mr, &mw, &mh );
  if ( st != VDP_STATUS_OK  )
    printf( "vo_vdpau: getting h264_supported failed! : %s\n", vdp_get_error_string( st ) );
  else if ( !ok )
    printf( "vo_vdpau: no support for h264 ! : no ok\n" );
  else
    this->capabilities |= VO_CAP_VDPAU_H264;

  st = vdp_decoder_query_capabilities( vdp_device, VDP_DECODER_PROFILE_MPEG2_MAIN, &ok, &ml, &mr, &mw, &mh );
  if ( st != VDP_STATUS_OK  )
    printf( "vo_vdpau: getting mpeg12_supported failed! : %s\n", vdp_get_error_string( st ) );
  else if ( !ok )
    printf( "vo_vdpau: no support for mpeg1/2 ! : no ok\n" );
  else
    this->capabilities |= VO_CAP_VDPAU_MPEG12;

  for ( i=0; i<NUM_FRAMES_BACK; i++)
    this->back_frame[i] = NULL;

  this->hue = 0;
  this->saturation = 100;
  this->contrast = 100;
  this->brightness = 0;
  this->sharpness = 0;
  this->noise = 0;
  this->deinterlace = 0;

  this->allocated_surfaces = 0;

  this->vdp_runtime_nr = 1;

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