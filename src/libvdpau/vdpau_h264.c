/*
 * Copyright (C) 2008 Julian Scheel
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
 * foovideo.c: This is a reference video decoder for the xine multimedia
 * player. It really works too! It will output frames of packed YUY2 data
 * where each byte in the map is the same value, which is 3 larger than the
 * value from the last frame. This creates a slowly rotating solid color
 * frame when the frames are played in succession.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>
#include <vdpau/vdpau.h>

#include "xine_internal.h"
#include "video_out.h"
#include "buffer.h"
#include "xineutils.h"
#include "bswap.h"
#include "accel_vdpau.h"

#define VIDEOBUFSIZE 128*1024

typedef struct {
  video_decoder_class_t   decoder_class;
} vdpau_h264_class_t;

typedef struct vdpau_h264_decoder_s {
  video_decoder_t   video_decoder;  /* parent video decoder structure */

  vdpau_h264_class_t *class;
  xine_stream_t    *stream;

  /* these are traditional variables in a video decoder object */
  uint64_t          video_step;  /* frame duration in pts units */
  int               decoder_ok;  /* current decoder status */
  int               decoder_initialized; /* vdpau init state */
  int               skipframes;

  unsigned char    *buf;         /* the accumulated buffer data */
  int               bufsize;     /* the maximum size of buf */
  int               size;        /* the current size of buf */

  int               width;       /* the width of a video frame */
  int               height;      /* the height of a video frame */
  double            ratio;       /* the width to height ratio */


  struct nal_parser *nal_parser;  /* h264 nal parser. extracts stream data for vdpau */

  VdpDecoderProfile profile;
  VdpPictureInfoH264 vdp_picture_info;
  vdpau_accel_t     *vdpau_accel;

  xine_t            *xine

} vdpau_h264_decoder_t;

/**************************************************************************
 * vdpau_h264 specific decode functions
 *************************************************************************/

/**************************************************************************
 * xine video plugin functions
 *************************************************************************/

/*
 * This function receives a buffer of data from the demuxer layer and
 * figures out how to handle it based on its header flags.
 */
static void vdpau_h264_decode_data (video_decoder_t *this_gen,
  buf_element_t *buf) {

  vdpau_h264_decoder_t *this = (vdpau_h264_decoder_t *) this_gen;

  VdpBitstreamBuffer vdp_buffer;
  vdp_buffer.struct_version = VDP_BITSTREAM_BUFFER_VERSION;

  vo_frame_t *img; /* video out frame */

  /* a video decoder does not care about this flag (?) */
  if (buf->decoder_flags & BUF_FLAG_PREVIEW)
    return;

  if (buf->decoder_flags & BUF_FLAG_FRAMERATE) {
    this->video_step = buf->decoder_info[0];
    _x_stream_info_set(this->stream, XINE_STREAM_INFO_FRAME_DURATION, this->video_step);
  }

  if (buf->decoder_flags & BUF_FLAG_STDHEADER) { /* need to initialize */
    return;
  }

  if (!this->decoder_ok) {
    /* parse the first nal packages to retrieve profile type */
    int len = 0;

    while(len < buf->size) {
      len += parse_frame(this->parser, buf->content + len, buf->size - len,
          &vdp_buffer.bitstream, &vdp_buffer.bitstream_bytes);

      if(this->nal_parser->current_nal->sps != NULL) {
        printf("SPS PARSED\n");

        this->width = this->nal_parser->current_nal->sps->pic_width;
        this->height = this->nal_parser->current_nal->sps->pic_height;

        /* FIXME: ratio should be calculated in some other way to
         * support anamorph codings...
         */
        this->ratio = (double)this->width / (double)this->height;

        switch(this->nal_parser->current_nal->sps->profile_idc) {
          case 100:
            this->profile = VDP_DECODER_PROFILE_H264_HIGH;
            break;
          case 77:
            this->profile = VDP_DECODER_PROFILE_H264_MAIN;
            break;
          case 66:
          default:
            this->profile = VDP_DECODER_PROFILE_H264_BASELINE;
            break;
        }

        /* get the vdpau context from vo */
        (this->stream->video_out->open) (this->stream->video_out, this->stream);
        img = this->stream->video_out->get_frame (this->stream->video_out,
                                          this->width, this->height,
                                          this->ratio,
                                          XINE_IMGFMT_VDPAU, VO_BOTH_FIELDS);

         this->vdpau_accel = (vdpau_accel_t*)img->accel_data;

         VdpBool is_supported;
         uint32_t max_level, max_references, max_width, max_height;

         VdpStatus status = this->vdpau_accel->vdp_decoder_create(this->vdpau_accel->vdp_device,
             this->profile, &is_supported, &max_level, &max_references,
             &max_width, &max_height);

         if(status != OK)
           xprintf(this->xine, XINE_VERBOSITY_LOG, "vdpau_h264: ERROR: VdpDecoderCreate returned status != OK (%d)\n", status);

         if(!is_supported)
           xprintf(this->xine, XINE_VERBOSITY_LOG, "vdpau_h264: ERROR: Profile not supported by VDPAU decoder.\n");

         if(max_width < this->width || max_height < this->height)
           xprintf(this->xine, XINE_VERBOSITY_LOG, "vdpau_h264: ERROR: Image size not supported by VDPAU decoder.\n");
      }

    }

  }

  if (this->decoder_ok) {

    if (this->size + buf->size > this->bufsize) {
      this->bufsize = this->size + 2 * buf->size;
      this->buf = realloc (this->buf, this->bufsize);
    }

    xine_fast_memcpy (&this->buf[this->size], buf->content, buf->size);

    this->size += buf->size;

    if (buf->decoder_flags & BUF_FLAG_FRAME_END) {

      img = this->stream->video_out->get_frame (this->stream->video_out,
                                        this->width, this->height,
                                        this->ratio,
                                        XINE_IMGFMT_VDPAU, VO_BOTH_FIELDS);

      img->duration  = this->video_step;
      img->pts       = buf->pts;
      img->bad_frame = 0;

      memset(img->base[0], this->current_yuv_byte,
        this->width * this->height * 2);
      this->current_yuv_byte += 3;

      img->draw(img, this->stream);
      img->free(img);

      this->size = 0;
    }
  }
}

/*
 * This function is called when xine needs to flush the system.
 */
static void vdpau_h264_flush (video_decoder_t *this_gen) {
}

/*
 * This function resets the video decoder.
 */
static void vdpau_h264_reset (video_decoder_t *this_gen) {
  vdpau_h264_decoder_t *this = (vdpau_h264_decoder_t *) this_gen;

  this->size = 0;
}

/*
 * The decoder should forget any stored pts values here.
 */
static void vdpau_h264_discontinuity (video_decoder_t *this_gen) {
  vdpau_h264_decoder_t *this = (vdpau_h264_decoder_t *) this_gen;

}

/*
 * This function frees the video decoder instance allocated to the decoder.
 */
static void vdpau_h264_dispose (video_decoder_t *this_gen) {

  vdpau_h264_decoder_t *this = (vdpau_h264_decoder_t *) this_gen;

  if (this->buf) {
    free (this->buf);
    this->buf = NULL;
  }

  if (this->decoder_ok) {
    this->decoder_ok = 0;
    this->stream->video_out->close(this->stream->video_out, this->stream);
  }

  free (this_gen);
}

/*
 * This function allocates, initializes, and returns a private video
 * decoder structure.
 */
static video_decoder_t *open_plugin (video_decoder_class_t *class_gen, xine_stream_t *stream) {

  vdpau_h264_decoder_t  *this ;

  this = (vdpau_h264_decoder_t *) calloc(1, sizeof(vdpau_h264_decoder_t));

  /* the videoout must be vdpau-capable to support this decoder */
  if(!stream->video_driver->get_capabilities() & VO_CAP_VDPAU_H264)
	  return NULL;

  this->video_decoder.decode_data         = vdpau_h264_decode_data;
  this->video_decoder.flush               = vdpau_h264_flush;
  this->video_decoder.reset               = vdpau_h264_reset;
  this->video_decoder.discontinuity       = vdpau_h264_discontinuity;
  this->video_decoder.dispose             = vdpau_h264_dispose;
  this->size                              = 0;

  this->stream                            = stream;
  this->xine                              = stream->xine;
  this->class                             = (vdpau_h264_class_t *) class_gen;

  this->decoder_ok    = 0;
  this->decoder_initialized = 0;
  this->nal_parser = init_parser();
  this->buf           = NULL;

  return &this->video_decoder;
}

/*
 * This function returns a brief string that describes (usually with the
 * decoder's most basic name) the video decoder plugin.
 */
static char *get_identifier (video_decoder_class_t *this) {
  return "vdpau_h264";
}

/*
 * This function returns a slightly longer string describing the video
 * decoder plugin.
 */
static char *get_description (video_decoder_class_t *this) {
  return "vdpau_h264: h264 decoder plugin using VDPAU hardware decoding.\n"
	  "Must be used along with video_out_vdpau.";
}

/*
 * This function frees the video decoder class and any other memory that was
 * allocated.
 */
static void dispose_class (video_decoder_class_t *this) {
  free (this);
}

/*
 * This function allocates a private video decoder class and initializes
 * the class's member functions.
 */
static void *init_plugin (xine_t *xine, void *data) {

  vdpau_h264_class_t *this;

  this = (vdpau_h264_class_t *) calloc(1, sizeof(vdpau_h264_class_t));

  this->decoder_class.open_plugin     = open_plugin;
  this->decoder_class.get_identifier  = get_identifier;
  this->decoder_class.get_description = get_description;
  this->decoder_class.dispose         = dispose_class;

  return this;
}

/*
 * This is a list of all of the internal xine video buffer types that
 * this decoder is able to handle. Check src/xine-engine/buffer.h for a
 * list of valid buffer types (and add a new one if the one you need does
 * not exist). Terminate the list with a 0.
 */
static const uint32_t video_types[] = {
  /* BUF_VIDEO_FOOVIDEO, */
  BUF_VIDEO_H264,
  0
};

/*
 * This data structure combines the list of supported xine buffer types and
 * the priority that the plugin should be given with respect to other
 * plugins that handle the same buffer type. A plugin with priority (n+1)
 * will be used instead of a plugin with priority (n).
 */
static const decoder_info_t dec_info_video = {
  video_types,         /* supported types */
  5                    /* priority        */
};

/*
 * The plugin catalog entry. This is the only information that this plugin
 * will export to the public.
 */
const plugin_info_t xine_plugin_info[] EXPORTED = {
  /* { type, API, "name", version, special_info, init_function } */
  { PLUGIN_VIDEO_DECODER, 18, "vdpau_h264", XINE_VERSION_CODE, &dec_info_video, init_plugin },
  { PLUGIN_NONE, 0, "", 0, NULL, NULL }
};
