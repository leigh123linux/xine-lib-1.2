/*
 * Copyright (C) 2003-2018 the xine project
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
 * a image video decoder
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif


#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>
#include <stdio.h>

#define LOG_MODULE "image_video_decoder"
#define LOG_VERBOSE
/*
#define LOG
*/

#ifdef HAVE_MAGICKWAND_MAGICKWAND_H
#include <MagickWand/MagickWand.h>
#else
#include <wand/magick_wand.h>
#endif
#ifdef PACKAGE_NAME
#undef PACKAGE_BUGREPORT
#undef PACKAGE_NAME
#undef PACKAGE_STRING
#undef PACKAGE_TARNAME
#undef PACKAGE_VERSION
#endif

#include <xine/xine_internal.h>
#include <xine/video_out.h>
#include <xine/buffer.h>
#include <xine/xineutils.h>
#include <xine/xine_buffer.h>
#include "bswap.h"

#ifdef HAVE_GRAPHICSMAGICK
# define MAGICK_VERSION 0x660
#else
# if !defined(MagickLibVersion) || MagickLibVersion < 0x661
#  define MAGICK_VERSION 0x660
# else
#  define MAGICK_VERSION MagickLibVersion
# endif
#endif


typedef struct image_decoder_s {
  video_decoder_t   video_decoder;

  xine_stream_t    *stream;
  int               video_open;

  unsigned char    *image;
  int               index;

} image_decoder_t;


static void image_decode_data (video_decoder_t *this_gen, buf_element_t *buf) {
  image_decoder_t *this = (image_decoder_t *) this_gen;

  if (!this->video_open) {
    lprintf("opening video\n");
    (this->stream->video_out->open) (this->stream->video_out, this->stream);
    this->video_open = 1;
  }

  xine_buffer_copyin(this->image, this->index, buf->mem, buf->size);
  this->index += buf->size;

  if (buf->decoder_flags & BUF_FLAG_FRAME_END) {
    int                width, height, img_stride;
    int                status;
    MagickWand        *wand;
    uint8_t           *img_buf;
    vo_frame_t        *img;

    void              *rgb2yuy2;
    int                frame_flags, cm, format;

    /*
     * this->image -> rgb data
     */
#if MAGICK_VERSION < 0x661
    InitializeMagick(NULL);
#else
    MagickWandGenesis();
#endif
    wand = NewMagickWand();
    status = MagickReadImageBlob(wand, this->image, this->index);

    this->index = 0;

    if (!status) {
      DestroyMagickWand(wand);
#if MAGICK_VERSION < 0x661
      DestroyMagick();
#else
      MagickWandTerminus();
#endif
      lprintf("error loading image\n");
      return;
    }

    width = MagickGetImageWidth(wand);
    height = MagickGetImageHeight(wand);
    img_stride = 3 * width;
    img_buf = malloc (img_stride * height);
#if MAGICK_VERSION < 0x661
    MagickGetImagePixels(wand, 0, 0, width, height, "RGB", CharPixel, img_buf);
    DestroyMagickWand(wand);
    DestroyMagick();
#else
    MagickExportImagePixels(wand, 0, 0, width, height, "RGB", CharPixel, img_buf);
    DestroyMagickWand(wand);
    MagickWandTerminus();
#endif

    _x_stream_info_set(this->stream, XINE_STREAM_INFO_VIDEO_WIDTH, width);
    _x_stream_info_set(this->stream, XINE_STREAM_INFO_VIDEO_HEIGHT, height);

    lprintf("image loaded successfully\n");

    frame_flags = VO_BOTH_FIELDS;
    cm = 10; /* mpeg range ITU-R 601 */
    if (this->stream->video_out->get_capabilities (this->stream->video_out) & VO_CAP_FULLRANGE)
      cm = 11; /* full range */
    VO_SET_FLAGS_CM (cm, frame_flags);

    /*
     * alloc video frame and set cropping
     */
    format = (this->stream->video_out->get_capabilities (this->stream->video_out) & VO_CAP_YUY2) ?
             XINE_IMGFMT_YUY2 : XINE_IMGFMT_YV12;
    img = this->stream->video_out->get_frame (this->stream->video_out, width, height,
					      (double)width / (double)height,
                                              format,
                                              frame_flags | VO_GET_FRAME_MAY_FAIL);
    if (!img) {
      xprintf(this->stream->xine, XINE_VERBOSITY_LOG,
              LOG_MODULE ": get_frame(%dx%d) failed\n", width, height);
      free (img_buf);
      return;
    }

    if (width > img->width)
      width = img->width;
    if (height > img->height)
      height = img->height;
    img->ratio = (double)width / (double)height;

    /*
     * rgb data -> yuv_planes
     */
    rgb2yuy2 = rgb2yuy2_alloc (cm, "rgb");
    if (img->format == XINE_IMGFMT_YV12) {
      rgb2yv12_slice (rgb2yuy2, img_buf, img_stride,
                      img->base[0], img->pitches[0],
                      img->base[1], img->pitches[1],
                      img->base[2], img->pitches[2],
                      width, height);
    } else {
      rgb2yuy2_slice (rgb2yuy2, img_buf, img_stride, img->base[0], img->pitches[0], width, height);
    }
    rgb2yuy2_free (rgb2yuy2);
    free (img_buf);

    /*
     * draw video frame
     */
    img->pts = buf->pts;
    img->duration = 3600;
    img->bad_frame = 0;

    _x_stream_info_set(this->stream, XINE_STREAM_INFO_FRAME_DURATION, img->duration);

    img->draw(img, this->stream);
    img->free(img);
  }
}


static void image_flush (video_decoder_t *this_gen) {
  /* image_decoder_t *this = (image_decoder_t *) this_gen; */

  /*
   * flush out any frames that are still stored in the decoder
   */
}


static void image_reset (video_decoder_t *this_gen) {
  image_decoder_t *this = (image_decoder_t *) this_gen;

  /*
   * reset decoder after engine flush (prepare for new
   * video data not related to recently decoded data)
   */

  this->index = 0;
}


static void image_discontinuity (video_decoder_t *this_gen) {
  /* image_decoder_t *this = (image_decoder_t *) this_gen; */

  /*
   * a time reference discontinuity has happened.
   * that is, it must forget any currently held pts value
   */
}

static void image_dispose (video_decoder_t *this_gen) {
  image_decoder_t *this = (image_decoder_t *) this_gen;

  if (this->video_open) {
    lprintf("closing video\n");

    this->stream->video_out->close(this->stream->video_out, this->stream);
    this->video_open = 0;
  }

  xine_buffer_free(this->image);

  lprintf("closed\n");
  free (this);
}


static video_decoder_t *open_plugin (video_decoder_class_t *class_gen,
				     xine_stream_t *stream) {

  image_decoder_t *this;

  lprintf("opened\n");

  this = (image_decoder_t *) calloc(1, sizeof(image_decoder_t));

  this->video_decoder.decode_data         = image_decode_data;
  this->video_decoder.flush               = image_flush;
  this->video_decoder.reset               = image_reset;
  this->video_decoder.discontinuity       = image_discontinuity;
  this->video_decoder.dispose             = image_dispose;
  this->stream                            = stream;

  /*
   * initialisation of privates
   */

  this->image = xine_buffer_init(10240);

  return &this->video_decoder;
}

/*
 * image plugin class
 */
static void *init_class (xine_t *xine, const void *data) {

  static const video_decoder_class_t decode_video_image_class = {
    .open_plugin     = open_plugin,
    .identifier      = "imagevdec",
    .description     = N_("image video decoder plugin"),
    .dispose         = NULL,
  };

  return (void*)&decode_video_image_class;
}

/*
 * exported plugin catalog entry
 */

static const uint32_t supported_types[] = { BUF_VIDEO_IMAGE, BUF_VIDEO_JPEG, 0 };

static const decoder_info_t dec_info_image = {
  supported_types,     /* supported types */
  7                    /* priority        */
};

const plugin_info_t xine_plugin_info[] EXPORTED = {
  /* type, API, "name", version, special_info, init_function */
  { PLUGIN_VIDEO_DECODER, 19, "image", XINE_VERSION_CODE, &dec_info_image, init_class },
  { PLUGIN_NONE, 0, "", 0, NULL, NULL }
};
