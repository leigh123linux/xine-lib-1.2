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
 * $Id: video_decoder.h,v 1.3 2002/10/14 15:47:40 guenter Exp $
 *
 * xine video decoder plugin interface
 *
 */

#ifndef HAVE_VIDEO_DECODER_H
#define HAVE_VIDEO_DECODER_H

#include <inttypes.h>
#include "buffer.h"

#define VIDEO_DECODER_IFACE_VERSION 11

/*
 * generic xine video decoder plugin interface
 */

typedef struct video_decoder_class_s video_decoder_class_t;

struct video_decoder_class_s {

  /*
   * return short, human readable identifier for this plugin class
   */
  char* (*get_identifier) (video_decoder_class_t *this);

  /*
   * return human readable (verbose = 1 line) description for 
   * this plugin class
   */
  char* (*get_description) (video_decoder_class_t *this);
};


typedef struct video_decoder_s video_decoder_t;

struct video_decoder_s {

  /*
   * decode data from buf and feed decoded frames to 
   * video output 
   */
  void (*decode_data) (video_decoder_t *this, buf_element_t *buf);

  /*
   * reset decoder after engine flush (prepare for new
   * video data not related to recently decoded data)
   */
  void (*reset) (video_decoder_t *this);
  
  /*
   * flush out any frames that are still stored in the decoder
   */
  void (*flush) (video_decoder_t *this);

  /*
   * close down, free all resources
   */
  void (*dispose) (video_decoder_t *this);


  void *node; /*used by plugin loader */

};

#endif
