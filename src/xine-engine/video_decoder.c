/* 
 * Copyright (C) 2000-2001 the xine project
 * 
 * This file is part of xine, a unix video player.
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
 * $Id: video_decoder.c,v 1.38 2001/08/15 11:35:47 f1rmb Exp $
 *
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "xine_internal.h"
#include "monitor.h"
#include <sched.h>

static spu_decoder_t* update_spu_decoder(xine_t *this, int type) {
  int streamtype = (type>>16) & 0xFF;
  spu_decoder_t *spu_decoder = this->spu_decoder_plugins [streamtype];

  if (spu_decoder && this->cur_spu_decoder_plugin != spu_decoder) {

    if (this->cur_spu_decoder_plugin)
      this->cur_spu_decoder_plugin->close (this->cur_spu_decoder_plugin);

    this->cur_spu_decoder_plugin = spu_decoder;

    this->cur_spu_decoder_plugin->init (this->cur_spu_decoder_plugin,
                                        this->video_out);
  }
  return spu_decoder;
}

void *video_decoder_loop (void *this_gen) {

  buf_element_t   *buf;
  xine_t          *this = (xine_t *) this_gen;
  int              running = 1;
  int              i;
  int              streamtype;
  video_decoder_t *decoder;
  spu_decoder_t   *spu_decoder;

  while (running) {

    /* printf ("video_decoder: getting buffer...\n");  */

    buf = this->video_fifo->get (this->video_fifo);
    if (buf->input_pos)
      this->cur_input_pos = buf->input_pos;

    /* printf ("video_decoder: got buffer %d\n", buf->type);    */

    switch (buf->type & 0xffff0000) {
    case BUF_CONTROL_START:

      if (this->cur_video_decoder_plugin) {
	this->cur_video_decoder_plugin->close (this->cur_video_decoder_plugin);
	this->cur_video_decoder_plugin = NULL;
      }

      if (this->cur_spu_decoder_plugin) {
        this->cur_spu_decoder_plugin->close (this->cur_spu_decoder_plugin);
        this->cur_spu_decoder_plugin = NULL;
      }

      pthread_mutex_lock (&this->xine_lock);
      this->video_finished = 0;
      this->spu_finished = 0;
/* FIXME: I don't think we need spu_track_map. */
      for (i=0 ; i<50; i++)
        this->spu_track_map[0] = 0;

      this->spu_track_map_entries = 0;

      pthread_mutex_unlock (&this->xine_lock);

      this->metronom->video_stream_start (this->metronom);

      break;

    case BUF_SPU_CLUT:
      spu_decoder = update_spu_decoder(this, buf->type);

      if (spu_decoder)
        spu_decoder->decode_data (spu_decoder, buf);
      break;

    case BUF_SPU_PACKAGE:
      /* now, decode this buffer if it's the right track */
      if ( (buf->type  & 0xFFFF)== this->spu_channel) {
        spu_decoder = update_spu_decoder (this, buf->type);

        if (spu_decoder)
          spu_decoder->decode_data (spu_decoder, buf);
      }
      break;

    case BUF_VIDEO_MPEG:
    case BUF_VIDEO_AVI:

      /*
      printf ("video_decoder: got package %d, decoder_info[0]:%d\n", 
	      buf, buf->decoder_info[0]);
      */      

      streamtype = (buf->type>>16) & 0xFF;

      decoder = this->video_decoder_plugins [streamtype];

      if (decoder) {

	if (this->cur_video_decoder_plugin != decoder) {

	  if (this->cur_video_decoder_plugin) 
	    this->cur_video_decoder_plugin->close (this->cur_video_decoder_plugin);

	  this->cur_video_decoder_plugin = decoder;
	  this->cur_video_decoder_plugin->init (this->cur_video_decoder_plugin, this->video_out);

	  printf ("video_decoder: using decoder >%s< \n",
		  decoder->get_identifier());

	}
	
	profiler_start_count (0);
	decoder->decode_data (this->cur_video_decoder_plugin, buf);  
	profiler_stop_count (0);
      }
      break;

    case BUF_CONTROL_END:

      this->metronom->video_stream_end (this->metronom);

      if (this->cur_video_decoder_plugin) {
	this->cur_video_decoder_plugin->close (this->cur_video_decoder_plugin);
	this->cur_video_decoder_plugin = NULL;
      }
      if (this->cur_spu_decoder_plugin) {
        this->cur_spu_decoder_plugin->close (this->cur_spu_decoder_plugin);
        this->cur_spu_decoder_plugin = NULL;
      }
      this->spu_finished = 1;
      pthread_mutex_lock (&this->xine_lock);
      
      if (!this->video_finished && (buf->decoder_info[0]==0)) {

	this->video_finished = 1;
	
	if (this->audio_finished) {
	  pthread_mutex_unlock (&this->xine_lock);
	  xine_notify_stream_finished (this);
	} else
	  pthread_mutex_unlock (&this->xine_lock);
      } else
	pthread_mutex_unlock (&this->xine_lock);

      break;

    case BUF_CONTROL_QUIT:
      if (this->cur_video_decoder_plugin) {
	this->cur_video_decoder_plugin->close (this->cur_video_decoder_plugin);
	this->cur_video_decoder_plugin = NULL;
      }
      if (this->cur_spu_decoder_plugin) {
        this->cur_spu_decoder_plugin->close (this->cur_spu_decoder_plugin);
        this->cur_spu_decoder_plugin = NULL;
      }
      
      running = 0;
      break;

    }

    buf->free_buffer (buf);
  }

  pthread_exit(NULL);
}

void video_decoder_init (xine_t *this) {
  
  pthread_attr_t       pth_attrs;
  struct sched_param   pth_params;

  this->video_fifo = fifo_buffer_new (500, 65536);

  pthread_attr_init(&pth_attrs);
  pthread_attr_getschedparam(&pth_attrs, &pth_params);
  pth_params.sched_priority = sched_get_priority_min(SCHED_OTHER);
  pthread_attr_setschedparam(&pth_attrs, &pth_params);
  
  pthread_create (&this->video_thread, &pth_attrs, video_decoder_loop, this) ;
}

void video_decoder_shutdown (xine_t *this) {

  buf_element_t *buf;
  void          *p;

  /* this->video_fifo->clear(this->video_fifo); */

  buf = this->video_fifo->buffer_pool_alloc (this->video_fifo);
  buf->type = BUF_CONTROL_QUIT;
  this->video_fifo->put (this->video_fifo, buf);

  pthread_join (this->video_thread, &p);

  this->video_out->exit (this->video_out);
}

