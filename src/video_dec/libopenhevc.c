/*
 * Copyright (C) 2017 the xine project
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
 * libOpenHevc decoder wrapped by Petri Hintukainen <phintuka@users.sourceforge.net>
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#define LOG_MODULE "libopenhevc"

#include <stdlib.h>

#include <openHevcWrapper.h>

#include <xine/xine_internal.h>
#include <xine/video_out.h>
#include <xine/buffer.h>

typedef struct hevc_decoder_s {
  video_decoder_t   video_decoder;  /* parent video decoder structure */

  xine_stream_t    *stream;

  int64_t           pts;
  OpenHevc_Handle   handle;
  int               decoder_ok;  /* current decoder status */

  unsigned char    *buf;         /* the accumulated buffer data */
  int               bufsize;     /* the maximum size of buf */
  int               size;        /* the current size of buf */

  int               frame_flags; /* color matrix and fullrange */

} hevc_decoder_t;


static void hevc_decode_data (video_decoder_t *this_gen, buf_element_t *buf)
{
  hevc_decoder_t *this = (hevc_decoder_t *) this_gen;

  if (buf->decoder_flags & BUF_FLAG_COLOR_MATRIX) {
    VO_SET_FLAGS_CM (buf->decoder_info[4], this->frame_flags);
  }

  if (buf->decoder_flags & (BUF_FLAG_PREVIEW | BUF_FLAG_STDHEADER | BUF_FLAG_SPECIAL)) {
    return;
  }

  /* collect data */
  if (this->size + buf->size > this->bufsize) {
    this->bufsize = this->size + 2 * buf->size;
    this->buf = realloc (this->buf, this->bufsize);
  }
  xine_fast_memcpy (&this->buf[this->size], buf->content, buf->size);
  this->size += buf->size;

  /* save pts */
  if (buf->pts > 0) {
    this->pts = buf->pts;
  }

  if (!(buf->decoder_flags & BUF_FLAG_FRAME_END)) {
    return;
  }

  if (!this->decoder_ok) {
    (this->stream->video_out->open) (this->stream->video_out, this->stream);

    libOpenHevcStartDecoder(this->handle);
    this->decoder_ok = 1;

    _x_meta_info_set_utf8(this->stream, XINE_META_INFO_VIDEOCODEC, "HEVC");
  }

  /* decode */

  int got_pic = libOpenHevcDecode(this->handle, this->buf, this->size, this->pts);
  this->size = 0;

  if (got_pic > 0) {
    vo_frame_t         *img;
    OpenHevc_Frame_cpy  frame;
    OpenHevc_FrameInfo *info = &frame.frameInfo;
    float               ratio;

    memset(&frame, 0, sizeof(frame));
    libOpenHevcGetPictureInfo(this->handle, info);

    if (info->nBitDepth != 8) {
      xprintf(this->stream->xine, XINE_VERBOSITY_LOG,
              LOG_MODULE": Unsupported bit depth %d\n", info->nBitDepth);
      return;
    }
    if (info->chromat_format != YUV420) {
      xprintf(this->stream->xine, XINE_VERBOSITY_LOG,
              LOG_MODULE": Unsupported color space %d\n", info->chromat_format);
      return;
    }

    ratio = (float)info->sample_aspect_ratio.num / (float)info->sample_aspect_ratio.den;
    ratio = ratio * (float)info->nWidth / (float)info->nHeight,

    img = this->stream->video_out->get_frame (this->stream->video_out,
                                              info->nWidth, info->nHeight,
                                              ratio, XINE_IMGFMT_YV12,
                                              this->frame_flags | VO_BOTH_FIELDS);

    if (!img || img->width < info->nWidth || img->height < info->nHeight) {
      xprintf(this->stream->xine, XINE_VERBOSITY_LOG,
              LOG_MODULE": get_frame(%dx%d) failed\n", info->nWidth, info->nHeight);
      if (img) {
        img->free(img);
      }
      return;
    }

    img->pts = info->nTimeStamp;
    img->bad_frame = 0;
    img->progressive_frame = 1;
    img->width = info->nWidth;
    img->height = info->nHeight;

    frame.frameInfo.nYPitch = img->pitches[0];
    frame.frameInfo.nUPitch = img->pitches[1];
    frame.frameInfo.nVPitch = img->pitches[2];
    frame.pvY = (void*) img->base[0];
    frame.pvU = (void*) img->base[1];
    frame.pvV = (void*) img->base[2];

    if (!libOpenHevcGetOutputCpy(this->handle, 1, &frame)) {
      xprintf(this->stream->xine, XINE_VERBOSITY_LOG, LOG_MODULE": libOpenHevcGetOutputCpy failed\n");
      img->free(img);
      return;
    }

    img->draw(img, this->stream);
    img->free(img);
  }
}

static void hevc_flush (video_decoder_t *this_gen)
{
  hevc_decoder_t *this = (hevc_decoder_t *) this_gen;

  libOpenHevcFlush(this->handle);
}

static void hevc_reset (video_decoder_t *this_gen)
{
  hevc_decoder_t *this = (hevc_decoder_t *) this_gen;

  hevc_flush(this_gen);

  this->size = 0;
}

static void hevc_discontinuity (video_decoder_t *this_gen)
{
  //this->size = 0;
}

static void hevc_dispose (video_decoder_t *this_gen)
{
  hevc_decoder_t *this = (hevc_decoder_t *) this_gen;

  if (this->decoder_ok) {
    this->decoder_ok = 0;
    this->stream->video_out->close(this->stream->video_out, this->stream);
  }

  libOpenHevcClose(this->handle);

  free (this_gen);
}

static video_decoder_t *open_plugin (video_decoder_class_t *class_gen, xine_stream_t *stream)
{
  hevc_decoder_t  *this;

  this = (hevc_decoder_t *) calloc(1, sizeof(hevc_decoder_t));
  if (!this) {
    return NULL;
  }

  this->video_decoder.decode_data         = hevc_decode_data;
  this->video_decoder.flush               = hevc_flush;
  this->video_decoder.reset               = hevc_reset;
  this->video_decoder.discontinuity       = hevc_discontinuity;
  this->video_decoder.dispose             = hevc_dispose;

  this->stream       = stream;

  this->handle = libOpenHevcInit(0, 0); //int nb_pthreads, int thread_type);
  if (!this->handle) {
    xprintf(this->stream->xine, XINE_VERBOSITY_LOG, LOG_MODULE": libOpenHevcInit failed\n");
    free(this);
    return NULL;
  }

  xprintf(this->stream->xine, XINE_VERBOSITY_LOG,
          LOG_MODULE": Using libOpenHevc version %s\n",
          libOpenHevcVersion(this->handle));

  return &this->video_decoder;
}

static void *init_plugin (xine_t *xine, void *data)
{
  video_decoder_class_t *this;

  this = (video_decoder_class_t *) calloc(1, sizeof(video_decoder_class_t));

  this->open_plugin     = open_plugin;
  this->identifier      = "libopenhevc";
  this->description     = N_("HEVC video decoder plugin");
  this->dispose         = default_video_decoder_class_dispose;

  return this;
}

/*
 * exported plugin catalog entry
 */

static const uint32_t video_types[] = {
  BUF_VIDEO_HEVC,
  0
};

static const decoder_info_t dec_info_video = {
  video_types,         /* supported types */
  1                    /* priority        */
};

const plugin_info_t xine_plugin_info[] EXPORTED = {
  /* type, API, "name", version, special_info, init_function */
  { PLUGIN_VIDEO_DECODER, 19, "libopenhevc", XINE_VERSION_CODE, &dec_info_video, init_plugin },
  { PLUGIN_NONE, 0, "", 0, NULL, NULL }
};
