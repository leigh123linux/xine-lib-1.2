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
 * $Id: xine_decoder.c,v 1.39 2002/09/05 20:44:40 mroi Exp $
 *
 * stuff needed to turn libmpeg2 into a xine decoder plugin
 */


#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

#include "xine_internal.h"
#include "video_out.h"
#include "mpeg2.h"
#include "mpeg2_internal.h"
#include "buffer.h"

/*
#define LOG
*/

typedef struct mpeg2dec_decoder_s {
  video_decoder_t  video_decoder;
  mpeg2dec_t       mpeg2;
  vo_instance_t   *video_out;
  pthread_mutex_t  lock; /* mutex for async flush */
} mpeg2dec_decoder_t;

static void mpeg2dec_init (video_decoder_t *this_gen, vo_instance_t *video_out) {

  mpeg2dec_decoder_t *this = (mpeg2dec_decoder_t *) this_gen;

#ifdef LOG
  printf ("libmpeg2: init... \n");
#endif

  pthread_mutex_lock (&this->lock);

  mpeg2_init (&this->mpeg2, video_out);
  video_out->open(video_out);
  this->video_out = video_out;
  this->mpeg2.force_aspect = 0;

  pthread_mutex_unlock (&this->lock);

#ifdef LOG
  printf ("libmpeg2: init...done\n");
#endif
}

static void mpeg2dec_decode_data (video_decoder_t *this_gen, buf_element_t *buf) {
  mpeg2dec_decoder_t *this = (mpeg2dec_decoder_t *) this_gen;

#ifdef LOG
  printf ("libmpeg2: decode_data...\n");
#endif

  pthread_mutex_lock (&this->lock);
  
  /* handle aspect hints from xine-dvdnav */
  if (buf->decoder_flags & BUF_FLAG_SPECIAL) {
    if (buf->decoder_info[1] == BUF_SPECIAL_ASPECT) {
      this->mpeg2.force_aspect = buf->decoder_info[2];
      if (buf->decoder_info[3] == 0x1 && buf->decoder_info[2] == XINE_VO_ASPECT_ANAMORPHIC)
        /* letterboxing is denied, we have to do pan&scan */
        this->mpeg2.force_aspect = XINE_VO_ASPECT_PAN_SCAN;
    }
    pthread_mutex_unlock (&this->lock);
    return;
  }
  
  if (buf->decoder_flags & BUF_FLAG_PREVIEW) {
    mpeg2_find_sequence_header (&this->mpeg2, buf->content, buf->content + buf->size);
  } else {
    
    mpeg2_decode_data (&this->mpeg2, buf->content, buf->content + buf->size,
		       buf->pts);
  }

  pthread_mutex_unlock (&this->lock);

#ifdef LOG
  printf ("libmpeg2: decode_data...done\n");
#endif
}

static void mpeg2dec_flush (video_decoder_t *this_gen) {
  mpeg2dec_decoder_t *this = (mpeg2dec_decoder_t *) this_gen;

  pthread_mutex_lock (&this->lock);
#ifdef LOG
  printf ("libmpeg2: flush\n");
#endif

  mpeg2_flush (&this->mpeg2);

  pthread_mutex_unlock (&this->lock);
}

static void mpeg2dec_reset (video_decoder_t *this_gen) {
  mpeg2dec_decoder_t *this = (mpeg2dec_decoder_t *) this_gen;

  pthread_mutex_lock (&this->lock);

  mpeg2_reset (&this->mpeg2);

  pthread_mutex_unlock (&this->lock);
}


static void mpeg2dec_close (video_decoder_t *this_gen) {

  mpeg2dec_decoder_t *this = (mpeg2dec_decoder_t *) this_gen;

#ifdef LOG
  printf ("libmpeg2: close\n");
#endif

  pthread_mutex_lock (&this->lock);

  mpeg2_close (&this->mpeg2);

  this->video_out->close(this->video_out);

  pthread_mutex_unlock (&this->lock);
}

static char *mpeg2dec_get_id(void) {
  return "mpeg2dec";
}

static void mpeg2dec_dispose (video_decoder_t *this_gen) {
  mpeg2dec_decoder_t *this = (mpeg2dec_decoder_t *) this_gen;

  pthread_mutex_destroy (&this->lock);
  free (this);
}

static void *init_video_decoder_plugin (xine_t *xine, void *data) {

  mpeg2dec_decoder_t *this ;

  this = (mpeg2dec_decoder_t *) malloc (sizeof (mpeg2dec_decoder_t));
  memset(this, 0, sizeof (mpeg2dec_decoder_t));

  this->video_decoder.init                = mpeg2dec_init;
  this->video_decoder.decode_data         = mpeg2dec_decode_data;
  this->video_decoder.flush               = mpeg2dec_flush;
  this->video_decoder.reset               = mpeg2dec_reset;
  this->video_decoder.close               = mpeg2dec_close;
  this->video_decoder.get_identifier      = mpeg2dec_get_id;
  this->video_decoder.dispose             = mpeg2dec_dispose;
  this->video_decoder.priority            = 6; /* higher than ffmpeg */

  this->mpeg2.xine = xine;
  pthread_mutex_init (&this->lock, NULL);

  return this;
}

/*
 * exported plugin catalog entry
 */

static uint32_t supported_types[] = { BUF_VIDEO_MPEG, 0 };

static decoder_info_t dec_info_mpeg2 = {
  supported_types,     /* supported types */
  5                    /* priority        */
};

plugin_info_t xine_plugin_info[] = {
  /* type, API, "name", version, special_info, init_function */  
  { PLUGIN_VIDEO_DECODER, 10, "mpeg2", XINE_VERSION_CODE, &dec_info_mpeg2, init_video_decoder_plugin },
  { PLUGIN_NONE, 0, "", 0, NULL, NULL }
};
