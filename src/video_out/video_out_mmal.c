/*
 * Copyright (C) 2000-2014 the xine project
 * Copyright (C) 2014 Petri Hintukainen <phintuka@users.sourceforge.net>
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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>

#include <bcm_host.h>
#include <interface/mmal/mmal.h>
#include <interface/mmal/util/mmal_util.h>
#include <interface/mmal/util/mmal_default_components.h>
#include <interface/vmcs_host/vc_dispmanx.h>

#define LOG_MODULE "video_out_mmal"
#define LOG_VERBOSE
/*
#define LOG
*/
#define FRAME_ALLOC  /* allocate buffer based on frame size. if not defined, all buffers are suitable for 1920x1088 YUY2. */
#define HW_OVERLAY   /* draw overlay using HW. if undefined, draw in software. */

#include "xine.h"
#include <xine/xine_internal.h>
#include <xine/video_out.h>
#include <xine/xineutils.h>

#ifdef FRAME_ALLOC
#define MAX_VIDEO_WIDTH  (2*1920)
#define MAX_VIDEO_HEIGHT (2*1088)
#define MAX_VIDEO_FRAMES 20
#else
#define MAX_VIDEO_WIDTH  1920
#define MAX_VIDEO_HEIGHT 1088
#define MAX_VIDEO_FRAMES 20
#endif


typedef struct {
    vo_frame_t            vo_frame;

#ifdef FRAME_ALLOC
    MMAL_PORT_T          *input;
#endif
    MMAL_BUFFER_HEADER_T *buffer;
    int                   width, height, format;
    double                ratio;

    int                   displayed;
} mmal_frame_t;

typedef struct mmal_overlay_s mmal_overlay_t;
struct mmal_overlay_s {
  mmal_overlay_t *next;

  void     *mem; /* temp storage for rle -> argb */
  int       src_width, src_height, src_pitch;
  VC_RECT_T src_rect;
  VC_RECT_T dst_rect;

  DISPMANX_ELEMENT_HANDLE_T  element;
  DISPMANX_RESOURCE_HANDLE_T resource;
};

typedef struct {

  vo_driver_t        vo_driver;

  /* xine */
  xine_t            *xine;
#ifndef HW_OVERLAY
  alphablend_t       alphablend_extra_data;
#endif
  int                gui_width, gui_height;

  /* mmal */
  MMAL_COMPONENT_T  *renderer;
  MMAL_POOL_T       *pool;
  int                frames_in_renderer;
  double             renderer_ratio;

  /* dispmanx */
  DISPMANX_DISPLAY_HANDLE_T  dispmanx_handle;
  DISPMANX_UPDATE_HANDLE_T   overlay_update;

  /* overlays */
  mmal_overlay_t    *overlays;
  mmal_overlay_t    *old_overlays;

  pthread_mutex_t    mutex;
  pthread_cond_t     cond;
} mmal_driver_t;

typedef struct {
  video_driver_class_t driver_class;
  xine_t              *xine;
} mmal_class_t;

#define LOG_STATUS(msg) \
  xprintf(this->xine, XINE_VERBOSITY_LOG, LOG_MODULE": " msg ": %s (%d)\n", \
          mmal_status_to_string(status), status)

/*
 * display config
 */

static int update_tv_resolution(mmal_driver_t *this) {

  TV_DISPLAY_STATE_T display_state;

  if (vc_tv_get_display_state(&display_state) != 0) {
    xprintf(this->xine, XINE_VERBOSITY_LOG, LOG_MODULE": "
            "failed to query display resolution\n");
    return -1;
  }

  if (display_state.state & 0xFF) {
    this->gui_width  = display_state.display.hdmi.width;
    this->gui_height = display_state.display.hdmi.height;
  } else if (display_state.state & 0xFF00) {
    this->gui_width  = display_state.display.sdtv.width;
    this->gui_height = display_state.display.sdtv.height;
  } else {
    xprintf(this->xine, XINE_VERBOSITY_LOG, LOG_MODULE": "
            "invalid display state %x", (unsigned)display_state.state);
    return -1;
  }

  xprintf(this->xine, XINE_VERBOSITY_DEBUG, LOG_MODULE": "
          "display size %dx%d\n", this->gui_width, this->gui_height);
  return 0;
}

/*
 *
 */

static int config_display(mmal_driver_t *this,
                          int src_x, int src_y, int src_w, int src_h) {

  MMAL_DISPLAYREGION_T display_region;
  MMAL_STATUS_T status;

  display_region.hdr.id   = MMAL_PARAMETER_DISPLAYREGION;
  display_region.hdr.size = sizeof(MMAL_DISPLAYREGION_T);
  display_region.fullscreen       = MMAL_FALSE;
  display_region.src_rect.x       = src_x;
  display_region.src_rect.y       = src_y;
  display_region.src_rect.width   = src_w;
  display_region.src_rect.height  = src_h;
  display_region.dest_rect.x      = 0;
  display_region.dest_rect.y      = 0;
  display_region.dest_rect.width  = this->gui_width;
  display_region.dest_rect.height = this->gui_height;
  display_region.layer            = 1;
  display_region.set              = MMAL_DISPLAY_SET_FULLSCREEN |
                                    MMAL_DISPLAY_SET_SRC_RECT |
                                    MMAL_DISPLAY_SET_DEST_RECT |
                                    MMAL_DISPLAY_SET_LAYER;

  status = mmal_port_parameter_set(this->renderer->input[0], &display_region.hdr);
  if (status != MMAL_SUCCESS) {
    LOG_STATUS("failed to set display region");
    return -1;
  }
  return 0;
}

/*
 * MMAL callbacks
 */

static void control_port_cb(MMAL_PORT_T *port, MMAL_BUFFER_HEADER_T *buffer) {

  mmal_driver_t *this  = (mmal_driver_t *)port->userdata;
  MMAL_STATUS_T  status;

  if (buffer->cmd == MMAL_EVENT_ERROR) {
    status = *(uint32_t *)buffer->data;
    LOG_STATUS("MMAL error");
  }

  mmal_buffer_header_release(buffer);
}

static void input_port_cb(MMAL_PORT_T *port, MMAL_BUFFER_HEADER_T *buffer) {

  mmal_driver_t *this  = (mmal_driver_t *)port->userdata;
  vo_frame_t    *frame = (vo_frame_t *)buffer->user_data;

  pthread_mutex_lock(&this->mutex);
  --this->frames_in_renderer;
  pthread_cond_signal(&this->cond);
  pthread_mutex_unlock(&this->mutex);

  if (frame) {
    frame->free(frame);
  }
}

/*
 * renderer configuration
 */

static void disable_renderer(mmal_driver_t *this) {

  if (this->renderer) {

    if (this->renderer->control->is_enabled) {
      mmal_port_disable(this->renderer->control);
    }

    if (this->renderer->input[0]->is_enabled) {
      mmal_port_disable(this->renderer->input[0]);
    }

    if (this->renderer->is_enabled) {
      mmal_component_disable(this->renderer);
    }
  }
}

static int configure_renderer(mmal_driver_t *this, int format, int width, int height,
                              int crop_x, int crop_y, int crop_w, int crop_h, double ratio) {

  MMAL_PORT_T   *input = this->renderer->input[0];
  MMAL_STATUS_T  status;

  disable_renderer(this);

  this->renderer_ratio = ratio;

  input->userdata = (struct MMAL_PORT_USERDATA_T *)this;
  input->format->encoding = (format == XINE_IMGFMT_YV12 ? MMAL_ENCODING_I420 : MMAL_ENCODING_YUYV);
  input->format->es->video.width       = width;
  input->format->es->video.height      = height;
  input->format->es->video.crop.x      = crop_x;
  input->format->es->video.crop.y      = crop_y;
  input->format->es->video.crop.width  = crop_w;
  input->format->es->video.crop.height = crop_h;
  input->format->es->video.par.num     = height * ratio;
  input->format->es->video.par.den     = width;

  status = mmal_port_format_commit(input);
  if (status != MMAL_SUCCESS) {
    LOG_STATUS("failed to commit input format");
  }

  input->buffer_size = input->buffer_size_recommended;

  status = mmal_port_enable(this->renderer->control, control_port_cb);
  if (status != MMAL_SUCCESS) {
    LOG_STATUS("failed to enable control port");
    return -1;
  }

  status = mmal_port_enable(input, input_port_cb);
  if (status != MMAL_SUCCESS) {
    LOG_STATUS("failed to enable input port");
    return -1;
  }

  status = mmal_component_enable(this->renderer);
  if (status != MMAL_SUCCESS) {
    LOG_STATUS("failed to enable renderer component");
    return -1;
  }

  if (!this->pool) {
#ifdef FRAME_ALLOC
    this->pool = mmal_pool_create(MAX_VIDEO_FRAMES, 0);
    if (!this->pool) {
      xprintf(this->xine, XINE_VERBOSITY_LOG, LOG_MODULE": "
              "failed to create MMAL pool for %u buffers\n", MAX_VIDEO_FRAMES);
      return -1;
    }
#else
    int buffer_size = MAX_VIDEO_WIDTH * MAX_VIDEO_HEIGHT * 2;
    this->pool = mmal_pool_create_with_allocator(MAX_VIDEO_FRAMES, buffer_size,
                                  input,
                                  (mmal_pool_allocator_alloc_t)mmal_port_payload_alloc,
                                  (mmal_pool_allocator_free_t)mmal_port_payload_free);
    if (!this->pool) {
      xprintf(this->xine, XINE_VERBOSITY_LOG, LOG_MODULE": "
              "failed to create MMAL pool for %u buffers of size %d\n",
              MAX_VIDEO_FRAMES, buffer_size);
      return -1;
    }
#endif
  }

  return 0;
}

/*
 * xine interface
 */

static uint32_t mmal_get_capabilities (vo_driver_t *this_gen) {

  return
    VO_CAP_YUY2 | VO_CAP_YV12 |
    VO_CAP_CROP |
    VO_CAP_UNSCALED_OVERLAY |
    VO_CAP_CUSTOM_EXTENT_OVERLAY;
}

static void mmal_frame_field (vo_frame_t *vo_img, int which_field) {
}

static void mmal_frame_dispose (vo_frame_t *vo_img) {

  mmal_frame_t  *frame = (mmal_frame_t *) vo_img ;

  if (frame->buffer) {
#ifdef FRAME_ALLOC
    if (frame->buffer->data) {
      mmal_port_payload_free(frame->input, frame->buffer->data);
      frame->buffer->data = NULL;
      frame->buffer->alloc_size = 0;
    }
#endif
    frame->buffer->user_data = NULL;
    mmal_buffer_header_release(frame->buffer);
    frame->buffer = NULL;
  }

  free(frame);
}

static vo_frame_t *mmal_alloc_frame (vo_driver_t *this_gen) {

#ifdef FRAME_ALLOC
  mmal_driver_t    *this = (mmal_driver_t *) this_gen;
#endif
  mmal_frame_t     *frame;

  frame = (mmal_frame_t *) calloc(1, sizeof(mmal_frame_t));

  if (!frame)
    return NULL;

  pthread_mutex_init (&frame->vo_frame.mutex, NULL);

  frame->vo_frame.proc_slice = NULL;
  frame->vo_frame.proc_frame = NULL;
  frame->vo_frame.field      = mmal_frame_field;
  frame->vo_frame.dispose    = mmal_frame_dispose;

#ifdef FRAME_ALLOC
  frame->input = this->renderer->input[0];
#endif

  return (vo_frame_t *) frame;
}

static void mmal_update_frame_format (vo_driver_t *this_gen,
                                      vo_frame_t *frame_gen,
                                      uint32_t width, uint32_t height,
                                      double ratio, int format, int flags) {

  mmal_driver_t *this = (mmal_driver_t *)this_gen;
  mmal_frame_t  *frame = (mmal_frame_t *)frame_gen;

  /* limit frame size */
  if (width > MAX_VIDEO_WIDTH) {
    width = MAX_VIDEO_WIDTH;
    frame->vo_frame.width = width;
  }
  if (height > MAX_VIDEO_HEIGHT) {
    height = MAX_VIDEO_HEIGHT;
    frame->vo_frame.height = height;
  }

  /* alignment */
  width  = (width + 31) & ~31;
  height = (height + 15) & ~15;

#ifdef FRAME_ALLOC
  /* required storage */
  uint32_t size = width * height;
  if (format == XINE_IMGFMT_YV12) {
    size = size * 3 / 2;
  } else if (format == XINE_IMGFMT_YUY2) {
    size *= 2;
  }

  /* free buffer if it is too small */
  if (frame->buffer && frame->buffer->alloc_size < size) {
    mmal_port_payload_free(this->renderer->input[0], frame->buffer->data);
    frame->buffer->data = NULL;
    frame->buffer->user_data = NULL;
    mmal_buffer_header_release(frame->buffer);
    frame->buffer = NULL;
  }
#endif

  if (!frame->buffer) {
    frame->buffer = mmal_queue_wait(this->pool->queue);
    if (!frame->buffer) {
      xprintf(this->xine, XINE_VERBOSITY_LOG, LOG_MODULE": "
              "failed to get mmal buffer for frame\n");
      frame->vo_frame.width = frame->vo_frame.height = 0;
      return;
    }
#ifdef FRAME_ALLOC
    frame->buffer->data = mmal_port_payload_alloc(this->renderer->input[0], size);
    frame->buffer->alloc_size = size;
#endif
    frame->buffer->user_data = frame;
  }

  frame->width  = width;
  frame->height = height;
  frame->format = format;
  frame->ratio  = ratio;

  if (format == XINE_IMGFMT_YV12) {
    frame->vo_frame.pitches[0] = width;
    frame->vo_frame.pitches[1] = width/2;
    frame->vo_frame.pitches[2] = width/2;
    frame->vo_frame.base[0]    = frame->buffer->data;
    frame->vo_frame.base[1]    = frame->vo_frame.base[0] + width * height;
    frame->vo_frame.base[2]    = frame->vo_frame.base[1] + width/2 * height/2;
  } else if (format == XINE_IMGFMT_YUY2) {
    frame->vo_frame.pitches[0] = width * 2;
    frame->vo_frame.base[0]    = frame->buffer->data;
  } else {
    xprintf(this->xine, XINE_VERBOSITY_LOG, LOG_MODULE": "
            "unsupported frame format %x\n", format);
    frame->vo_frame.width = frame->vo_frame.height = 0;
  }

  frame->displayed = 0;
}


/*
 * overlay
 */

static void overlay_free(mmal_overlay_t *ovl, DISPMANX_UPDATE_HANDLE_T update) {

  if (ovl->resource != DISPMANX_NO_HANDLE) {
    vc_dispmanx_element_remove(update, ovl->element);
    vc_dispmanx_resource_delete(ovl->resource);
  }
  free(ovl->mem);
  free(ovl);
}

static void overlay_update(mmal_overlay_t *ovl, DISPMANX_UPDATE_HANDLE_T update, uint32_t *argb) {

  vc_dispmanx_resource_write_data(ovl->resource, VC_IMAGE_RGBA32,
                                  ovl->src_pitch, argb, &ovl->src_rect);
  vc_dispmanx_element_change_source(update, ovl->element, ovl->resource);
}

static mmal_overlay_t *overlay_new(mmal_driver_t *this,
                                   DISPMANX_UPDATE_HANDLE_T update,
                                   int src_width, int src_height, int src_pitch,
                                   int x, int y, int width, int height, int layer,
				   uint32_t *argb) {

  mmal_overlay_t      *ovl = calloc(1, sizeof(mmal_overlay_t));
  uint32_t             image_handle;
  static VC_DISPMANX_ALPHA_T  alpha;
  VC_RECT_T            src_rect;
  //src_width &= 31;
  //dst_width &= 15;
  ovl->src_pitch = src_pitch;
  ovl->src_width = src_width;
  ovl->src_height = src_height;

  vc_dispmanx_rect_set(&src_rect, 0, 0, src_width << 16, src_height << 16);
  vc_dispmanx_rect_set(&ovl->src_rect, 0, 0, src_width, src_height);
  vc_dispmanx_rect_set(&ovl->dst_rect, x, y, width, height);

  ovl->resource = vc_dispmanx_resource_create(VC_IMAGE_RGBA32,
					      src_pitch | (src_pitch<<16), src_height | (src_height<<16),
					      &image_handle);
  if (ovl->resource == DISPMANX_NO_HANDLE) {
    xprintf(this->xine, XINE_VERBOSITY_LOG, LOG_MODULE": "
	    "failed to create dispmanx resource for overlay\n");
    overlay_free(ovl, update);
    return NULL;
  }
  if (vc_dispmanx_resource_write_data(ovl->resource, VC_IMAGE_RGBA32,
				      src_pitch, argb, &ovl->src_rect)) {
    xprintf(this->xine, XINE_VERBOSITY_LOG, LOG_MODULE": "
	    "failed to write overlay data to dispmanx resource\n");
    overlay_free(ovl, update);
    return NULL;
  }

  alpha.flags = DISPMANX_FLAGS_ALPHA_FROM_SOURCE /*| DISPMANX_FLAGS_ALPHA_MIX*/;
  alpha.mask = DISPMANX_NO_HANDLE;
  ovl->element = vc_dispmanx_element_add(update, this->dispmanx_handle,
                                         layer, &ovl->dst_rect, ovl->resource,
                                         &src_rect, DISPMANX_PROTECTION_NONE,
                                         &alpha, NULL, VC_IMAGE_ROT0);

  if (ovl->element == DISPMANX_NO_HANDLE) {
    xprintf(this->xine, XINE_VERBOSITY_LOG, LOG_MODULE": "
	    "vc_dispmanx_element_add() failed\n");
    overlay_free(ovl, update);
    return NULL;
  }

  return ovl;
}

static void close_overlays(mmal_driver_t *this, mmal_overlay_t *ovls) {

  while (ovls) {
    mmal_overlay_t *tmp = ovls;
    ovls = ovls->next;
    overlay_free(tmp, this->overlay_update);
  }
}

/*
 *
 */

static void mmal_overlay_begin (vo_driver_t *this_gen,
                                vo_frame_t *frame_gen, int changed) {

#ifdef HW_OVERLAY
  mmal_driver_t *this = (mmal_driver_t *)this_gen;

  if (changed) {
    this->overlay_update = vc_dispmanx_update_start(10);
    /* re-create active overlay list to maintain blending order */
    this->old_overlays = this->overlays;
    this->overlays = NULL;
  }
#endif
}

static void mmal_overlay_blend (vo_driver_t *this_gen, vo_frame_t *frame, vo_overlay_t *overlay) {

  mmal_driver_t  *this = (mmal_driver_t *) this_gen;

  if (overlay->width <= 0 || overlay->height <= 0 || !overlay->rle)
    return;

#ifndef HW_OVERLAY
  this->alphablend_extra_data.offset_x = frame->overlay_offset_x;
  this->alphablend_extra_data.offset_y = frame->overlay_offset_y;

  if (overlay->rle) {
    if( frame->format == XINE_IMGFMT_YV12 )
      _x_blend_yuv( frame->base, overlay, frame->width, frame->height, frame->pitches, &this->alphablend_extra_data);
    else
      _x_blend_yuy2( frame->base[0], overlay, frame->width, frame->height, frame->pitches[0], &this->alphablend_extra_data);
  }
#else

  if (!this->overlay_update) {
    return;
  }

  int dst_x = overlay->x, dst_y = overlay->y, dst_w = overlay->width, dst_h = overlay->height;

  /* coordinate system */
  int extent_width  = overlay->extent_width;
  int extent_height = overlay->extent_height;
  if (extent_width < 1 || extent_height < 1) {
    if (overlay->unscaled) {
      extent_width = this->gui_width;
      extent_height = this->gui_height;
    } else {
      extent_width = frame->width;
      extent_height = frame->height;
    }
  }

  /* scale dst region if needed */
  if (extent_width != this->gui_width) {
    dst_x = dst_x * this->gui_width / extent_width;
    dst_w = dst_w * this->gui_width / extent_width;
  }
  if (extent_height != this->gui_height) {
    dst_y = dst_y * this->gui_height / extent_height;
    dst_h = dst_h * this->gui_height / extent_height;
  }

#ifdef LOG
  fprintf(stderr, "overlay: %d,%d %dx%d unscaled:%d extent: %dx%d argb: %d-> surface %dx%d\n",
          overlay->x, overlay->y, overlay->width, overlay->height,
          overlay->unscaled, overlay->extent_width, overlay->extent_height,
          (overlay->argb_layer && overlay->argb_layer->buffer),
          extent_width, extent_height);
#endif

  /* find suitable region, maintain overlay blending order */
  mmal_overlay_t *ovl = this->old_overlays, *prev = NULL;
  while (ovl) {
    /* source, dst coordinates must be same (= size + location + scaling) */
    if (ovl->src_width == overlay->width && ovl->src_height == overlay->height &&
        dst_x == ovl->dst_rect.x && dst_y == ovl->dst_rect.y && dst_w == ovl->dst_rect.width && dst_h == ovl->dst_rect.height) {
      if (prev) {
        prev->next = ovl->next;
      } else {
        this->old_overlays = ovl->next;
      }
      ovl->next = NULL;
      break;
    }
    prev = ovl;
    ovl = ovl->next;
  }

  /* new overlay */
  if (!ovl) {
    if (overlay->rle) {
      _x_overlay_clut_yuv2rgb(overlay, 0);

      void *mem = NULL;
      int src_pitch = (sizeof(uint32_t) * overlay->width + 31) & ~31;
      mem = malloc(src_pitch * overlay->height);
      _x_overlay_to_argb32(overlay, mem, src_pitch/4, "RGBA");
      ovl = overlay_new(this, this->overlay_update, overlay->width, overlay->height, src_pitch,
                        dst_x, dst_y, dst_w, dst_h, 2, mem);
      if (!ovl)
	return;
      ovl->mem = mem;
    }
  }

  /* update overlay */
  else if (overlay->rle) {
    _x_overlay_clut_yuv2rgb(overlay, 0);
 
    if (!ovl->mem) {
      int src_pitch = (sizeof(uint32_t) * overlay->width + 31) & ~31;
      ovl->mem = malloc(src_pitch * overlay->height);
      ovl->src_pitch = src_pitch;
    }
    _x_overlay_to_argb32(overlay, ovl->mem, ovl->src_pitch/4, "RGBA");
    overlay_update(ovl, this->overlay_update, ovl->mem);
  }

  /* add to list */
  mmal_overlay_t **tail = &this->overlays;
  while (*tail) {
    tail = &(*tail)->next;
  }
  *tail = ovl;
#endif /* HW_OVERLAY */
}

static void mmal_overlay_end (vo_driver_t *this_gen, vo_frame_t *frame_gen) {

#ifdef HW_OVERLAY
  mmal_driver_t *this  = (mmal_driver_t *)this_gen;

  if (!this->overlay_update) {
    return;
  }

  /* remove handles not in use */
  close_overlays(this, this->old_overlays);
  this->old_overlays = NULL;

  /* commit updates */
  vc_dispmanx_update_submit_sync(this->overlay_update);
  this->overlay_update = 0;
#endif
}

/*
 *
 */

static int mmal_redraw_needed (vo_driver_t *this_gen) {

  return 0;
}


static void mmal_display_frame (vo_driver_t *this_gen, vo_frame_t *frame_gen) {

  mmal_driver_t  *this = (mmal_driver_t *) this_gen;
  mmal_frame_t   *frame = (mmal_frame_t *) frame_gen;
  MMAL_PORT_T    *input = this->renderer->input[0];
  MMAL_STATUS_T   status;

  int visible_width  = frame_gen->width  - frame_gen->crop_left - frame_gen->crop_right;
  int visible_height = frame_gen->height - frame_gen->crop_top  - frame_gen->crop_bottom;

  if (input->format->es->video.width       != frame->width         ||
      input->format->es->video.height      != frame->height        ||
      this->renderer_ratio                 != frame_gen->ratio     ||
      input->format->es->video.crop.x      != frame_gen->crop_left ||
      input->format->es->video.crop.y      != frame_gen->crop_top  ||
      input->format->es->video.crop.width  != visible_width        ||
      input->format->es->video.crop.height != visible_height) {

    configure_renderer(this, frame->format, frame->width, frame->height,
                       frame_gen->crop_left, frame_gen->crop_top, visible_width, visible_height, frame_gen->ratio);

    config_display(this, 0, 0, frame_gen->width, frame_gen->height);
  }

  frame->buffer->cmd = 0;
  frame->buffer->length = this->renderer->input[0]->buffer_size;

  pthread_mutex_lock(&this->mutex);

  while (this->frames_in_renderer > 1) {
    pthread_cond_wait(&this->cond, &this->mutex);
  }

  status = mmal_port_send_buffer(this->renderer->input[0], frame->buffer);
  if (status == MMAL_SUCCESS) {
    this->frames_in_renderer++;
  }

  pthread_mutex_unlock(&this->mutex);

  if (status != MMAL_SUCCESS) {
    LOG_STATUS("failed to send frame to renderer input port");
    frame_gen->free(frame_gen);
    return;
  }

  if (frame->displayed) {
    frame_gen->free(frame_gen);
    return;
  }

  frame->displayed = 1;
}

static int mmal_get_property (vo_driver_t *this_gen, int property) {

  mmal_driver_t *this = (mmal_driver_t *) this_gen;

  switch (property) {
    case VO_PROP_WINDOW_WIDTH:
      return this->gui_width;
    case VO_PROP_WINDOW_HEIGHT:
      return this->gui_height;
    case VO_PROP_MAX_VIDEO_WIDTH:
      return MAX_VIDEO_WIDTH;
    case VO_PROP_MAX_VIDEO_HEIGHT:
      return MAX_VIDEO_HEIGHT;
    case VO_PROP_MAX_NUM_FRAMES:
      return MAX_VIDEO_FRAMES;
  }
  return 0;
}

static int mmal_set_property (vo_driver_t *this_gen, int property, int value) {

  return value;
}

static void mmal_get_property_min_max (vo_driver_t *this_gen, int property, int *min, int *max) {

  *min = *max = 0;
}

static int mmal_gui_data_exchange (vo_driver_t *this_gen, int data_type, void *data) {

  return -1;
}

static void mmal_dispose (vo_driver_t * this_gen) {

  mmal_driver_t      *this = (mmal_driver_t*) this_gen;

  if (this->dispmanx_handle) {

    if (this->overlays) {
      this->overlay_update = vc_dispmanx_update_start(10);
      close_overlays(this, this->overlays);
      this->overlays = NULL;
      vc_dispmanx_update_submit_sync(this->overlay_update);
      this->overlay_update = 0;
    }

    vc_dispmanx_display_close(this->dispmanx_handle);
    this->dispmanx_handle = DISPMANX_NO_HANDLE;
  }

  if (this->renderer) {
    disable_renderer(this);
    mmal_component_release(this->renderer);
  }

  if (this->pool) {
    mmal_pool_destroy(this->pool);
  }

#ifndef HW_OVERLAY
  _x_alphablend_free(&this->alphablend_extra_data);
#endif

  pthread_cond_destroy(&this->cond);
  pthread_mutex_destroy(&this->mutex);

  free(this);

  bcm_host_deinit();
}

static vo_driver_t *open_plugin (video_driver_class_t *class_gen, const void *visual_gen) {

  mmal_class_t  *class = (mmal_class_t*) class_gen;
  mmal_driver_t *this;
  MMAL_STATUS_T  status;

  this = (mmal_driver_t *) calloc(1, sizeof(mmal_driver_t));
  if (!this)
    return NULL;

  this->xine          = class->xine;

#ifndef HW_OVERLAY
  _x_alphablend_init(&this->alphablend_extra_data, class->xine);
#endif

  pthread_mutex_init (&this->mutex, NULL);
  pthread_cond_init (&this->cond, NULL);

  bcm_host_init();

  status = mmal_component_create(MMAL_COMPONENT_DEFAULT_VIDEO_RENDERER, &this->renderer);
  if (status != MMAL_SUCCESS) {
    LOG_STATUS("failed to create MMAL component " MMAL_COMPONENT_DEFAULT_VIDEO_RENDERER);
    mmal_dispose(&this->vo_driver);
    return NULL;
  }

  this->renderer->control->userdata  = (struct MMAL_PORT_USERDATA_T *)this;
  this->renderer->input[0]->userdata = (struct MMAL_PORT_USERDATA_T *)this;

  configure_renderer(this, XINE_IMGFMT_YV12, 720, 576, 0, 0, 720, 576, 4.0/3.0);
  update_tv_resolution(this);
  config_display(this, 0, 0, 720, 576);

  this->dispmanx_handle = vc_dispmanx_display_open(0);
  if (this->dispmanx_handle == DISPMANX_NO_HANDLE) {
    xprintf(this->xine, XINE_VERBOSITY_LOG, LOG_MODULE": "
	    "failed to open dispmanx display\n");
    mmal_dispose(&this->vo_driver);
    return NULL;
  }

  this->vo_driver.get_capabilities     = mmal_get_capabilities;
  this->vo_driver.alloc_frame          = mmal_alloc_frame;
  this->vo_driver.update_frame_format  = mmal_update_frame_format;
  this->vo_driver.overlay_begin        = mmal_overlay_begin;
  this->vo_driver.overlay_blend        = mmal_overlay_blend;
  this->vo_driver.overlay_end          = mmal_overlay_end;
  this->vo_driver.display_frame        = mmal_display_frame;
  this->vo_driver.get_property         = mmal_get_property;
  this->vo_driver.set_property         = mmal_set_property;
  this->vo_driver.get_property_min_max = mmal_get_property_min_max;
  this->vo_driver.gui_data_exchange    = mmal_gui_data_exchange;
  this->vo_driver.dispose              = mmal_dispose;
  this->vo_driver.redraw_needed        = mmal_redraw_needed;

  return &this->vo_driver;
}

/**
 * Class Functions
 */
static void *init_class (xine_t *xine, void *visual_gen) {
  mmal_class_t      *this;

  this = (mmal_class_t*) calloc(1, sizeof(mmal_class_t));

  this->driver_class.open_plugin      = open_plugin;
  this->driver_class.identifier       = "MMAL";
  this->driver_class.description      = N_("xine video output plugin using MMAL");
  this->driver_class.dispose          = default_video_driver_class_dispose;

  this->xine                          = xine;

  return this;
}

static const vo_info_t vo_info_mmal = {
  10,                  /* priority */
  XINE_VISUAL_TYPE_FB, /* visual type supported by this plugin */
};

const plugin_info_t xine_plugin_info[] EXPORTED = {
  /* type, API, "name", version, special_info, init_function */
  { PLUGIN_VIDEO_OUT, 22, "mmal", XINE_VERSION_CODE, &vo_info_mmal, init_class },
  { PLUGIN_NONE, 0, "" , 0 , NULL, NULL}
};
