/*
 * Copyright (C) 2000-2004 the xine project
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
 * $Id: video_out.c,v 1.197 2004/06/02 19:46:11 tmattern Exp $
 *
 * frame allocation / queuing / scheduling / output functions
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <signal.h>
#include <sys/time.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <zlib.h>
#include <pthread.h>
#include <inttypes.h>
#include <assert.h>

#define XINE_ENABLE_EXPERIMENTAL_FEATURES
#define XINE_ENGINE_INTERNAL

#define LOG_MODULE "video_out"
#define LOG_VERBOSE
/*
#define LOG
*/

#include "xine_internal.h"
#include "video_out.h"
#include "metronom.h"
#include "xineutils.h"

#define NUM_FRAME_BUFFERS          15
#define MAX_USEC_TO_SLEEP       20000
#define DEFAULT_FRAME_DURATION   3000    /* 30 frames per second */

/* wait this delay if the first frame is still referenced */
#define FIRST_FRAME_POLL_DELAY   3000
#define FIRST_FRAME_MAX_POLL       10    /* poll n times at most */

typedef struct {
  vo_frame_t        *first;
  vo_frame_t        *last;
  int                num_buffers;

  int                locked_for_read;
  pthread_mutex_t    mutex;
  pthread_cond_t     not_empty;
} img_buf_fifo_t;

typedef struct {
  
  xine_video_port_t         vo; /* public part */

  vo_driver_t              *driver;
  pthread_mutex_t           driver_lock;
  xine_t                   *xine;
  metronom_clock_t         *clock;
  xine_list_t              *streams;
  pthread_mutex_t           streams_lock;
  
  img_buf_fifo_t           *free_img_buf_queue;
  img_buf_fifo_t           *display_img_buf_queue;

  vo_frame_t               *last_frame;
  vo_frame_t               *img_backup;
  int                       redraw_needed;
  int                       discard_frames;
  
  int                       video_loop_running;
  int                       video_opened;
  pthread_t                 video_thread;

  int                       num_frames_delivered;
  int                       num_frames_skipped;
  int                       num_frames_discarded;

  /* threshold for sending XINE_EVENT_DROPPED_FRAMES */
  int                       warn_skipped_threshold;
  int                       warn_discarded_threshold;
  int                       warn_threshold_exceeded;
  int                       warn_threshold_event_sent;

  /* pts value when decoder delivered last video frame */
  int64_t                   last_delivery_pts; 


  video_overlay_manager_t  *overlay_source;
  int                       overlay_enabled;

  /* do we true real-time output or is this a grab only instance ? */
  int                       grab_only;

  extra_info_t             *extra_info_base; /* used to free mem chunk */

  int                       current_width, current_height;
  int64_t                   current_duration;
  int                       frame_drop_limit;
  int                       frame_drop_cpt;
} vos_t;


/*
 * frame queue (fifo) util functions
 */

static img_buf_fifo_t *vo_new_img_buf_queue () {

  img_buf_fifo_t *queue;

  queue = (img_buf_fifo_t *) xine_xmalloc (sizeof (img_buf_fifo_t));
  if( queue ) {
    queue->first       = NULL;
    queue->last        = NULL;
    queue->num_buffers = 0;
    queue->locked_for_read = 0;
    pthread_mutex_init (&queue->mutex, NULL);
    pthread_cond_init  (&queue->not_empty, NULL);
  }
  return queue;
}

static void vo_append_to_img_buf_queue_int (img_buf_fifo_t *queue,
					vo_frame_t *img) {

  /* img already enqueue? (serious leak) */
  assert (img->next==NULL);

  img->next = NULL;

  if (!queue->first) {
    queue->first = img;
    queue->last  = img;
    queue->num_buffers = 0;
  }
  else if (queue->last) {
    queue->last->next = img;
    queue->last  = img;
  }

  queue->num_buffers++;

  pthread_cond_signal (&queue->not_empty);
}

static void vo_append_to_img_buf_queue (img_buf_fifo_t *queue,
					vo_frame_t *img) {
  pthread_mutex_lock (&queue->mutex);
  vo_append_to_img_buf_queue_int (queue, img);
  pthread_mutex_unlock (&queue->mutex);
}

static vo_frame_t *vo_remove_from_img_buf_queue_int (img_buf_fifo_t *queue, int blocking) {
  vo_frame_t *img;

  while (!queue->first || queue->locked_for_read) {
    if (blocking)
      pthread_cond_wait (&queue->not_empty, &queue->mutex);
    else {
      struct timeval tv;
      struct timespec ts;
      gettimeofday(&tv, NULL);
      ts.tv_sec  = tv.tv_sec + 1;
      ts.tv_nsec = tv.tv_usec * 1000;
      if (pthread_cond_timedwait (&queue->not_empty, &queue->mutex, &ts) != 0)
        return NULL;
    }
  }

  img = queue->first;

  if (img) {
    queue->first = img->next;
    img->next = NULL;
    if (!queue->first) {
      queue->last = NULL;
      queue->num_buffers = 0;
    }
    else {
      queue->num_buffers--;
    }
  }
    
  return img;
}

static vo_frame_t *vo_remove_from_img_buf_queue (img_buf_fifo_t *queue) {
  vo_frame_t *img;

  pthread_mutex_lock (&queue->mutex);
  img = vo_remove_from_img_buf_queue_int(queue, 1);
  pthread_mutex_unlock (&queue->mutex);

  return img;
}

static vo_frame_t *vo_remove_from_img_buf_queue_nonblock (img_buf_fifo_t *queue) {
  vo_frame_t *img;

  pthread_mutex_lock (&queue->mutex);
  img = vo_remove_from_img_buf_queue_int(queue, 0);
  pthread_mutex_unlock (&queue->mutex);

  return img;
}

/*
 * functions to maintain lock_counter
 */
static void vo_frame_inc_lock (vo_frame_t *img) {
  
  pthread_mutex_lock (&img->mutex);

  img->lock_counter++;

  pthread_mutex_unlock (&img->mutex);
}

static void vo_frame_dec_lock (vo_frame_t *img) {
  
  pthread_mutex_lock (&img->mutex);

  img->lock_counter--;
  if (!img->lock_counter) {    
    vos_t *this = (vos_t *) img->port;
    vo_append_to_img_buf_queue (this->free_img_buf_queue, img);
  }

  pthread_mutex_unlock (&img->mutex);
}

/* call vo_driver->proc methods for the entire frame */
static void vo_frame_driver_proc(vo_frame_t *img)
{
  if (img->proc_frame) {
    img->proc_frame(img);
  }
  if (img->proc_called) return;
  
  if (img->proc_slice) {
    if (img->format == XINE_IMGFMT_YV12) {
      int height = img->height;
      uint8_t* src[3];
  
      src[0] = img->base[0];
      src[1] = img->base[1];
      src[2] = img->base[2];
      while ((height -= 16) > -16) {
        img->proc_slice(img, src);
        src[0] += 16 * img->pitches[0];
        src[1] +=  8 * img->pitches[1];
        src[2] +=  8 * img->pitches[2];
      }
    } else {
      int height = img->height;
      uint8_t* src[3];
      
      src[0] = img->base[0];
      
      while ((height -= 16) > -16) {
        img->proc_slice(img, src);
        src[0] += 16 * img->pitches[0];
      }
    }
  }
}

/*
 * 
 * functions called by video decoder:
 *
 * get_frame => alloc frame for rendering
 *
 * frame_draw=> queue finished frame for display
 *
 * frame_free=> frame no longer used as reference frame by decoder
 *
 */

static vo_frame_t *vo_get_frame (xine_video_port_t *this_gen,
				 uint32_t width, uint32_t height,
				 double ratio, int format,
				 int flags) {

  vo_frame_t *img;
  vos_t      *this = (vos_t *) this_gen;

  lprintf ("get_frame (%d x %d)\n", width, height);

  while (!(img = vo_remove_from_img_buf_queue_nonblock (this->free_img_buf_queue)))
    if (this->xine->port_ticket->ticket_revoked)
      this->xine->port_ticket->renew(this->xine->port_ticket, 1);

  lprintf ("got a frame -> pthread_mutex_lock (&img->mutex)\n");

  /* some decoders report strange ratios */
  if (ratio <= 0.0)
    ratio = (double)width / (double)height;
  
  pthread_mutex_lock (&img->mutex);
  img->lock_counter   = 1;
  img->width          = width;
  img->height         = height;
  img->ratio          = ratio;
  img->format         = format;
  img->flags          = flags;
  img->proc_called    = 0;
  img->bad_frame      = 0;
  img->progressive_frame  = 0;
  img->repeat_first_field = 0;
  img->top_field_first    = 1;
  img->macroblocks        = NULL;
  _x_extra_info_reset ( img->extra_info );

  /* let driver ensure this image has the right format */

  this->driver->update_frame_format (this->driver, img, width, height, 
				     ratio, format, flags);

  pthread_mutex_unlock (&img->mutex);
  
  lprintf ("get_frame (%d x %d) done\n", width, height);

  return img;
}

static int vo_frame_draw (vo_frame_t *img, xine_stream_t *stream) {

  vos_t         *this = (vos_t *) img->port;
  int64_t        diff;
  int64_t        cur_vpts;
  int64_t        pic_vpts ;
  int            frames_to_skip;
  int            duration;

  /* handle anonymous streams like NULL for easy checking */
  if (stream == XINE_ANON_STREAM) stream = NULL;
  
  img->stream = stream;
  this->current_width = img->width;
  this->current_height = img->height;
  
  if (stream) {
    _x_extra_info_merge( img->extra_info, stream->video_decoder_extra_info );
    stream->metronom->got_video_frame (stream->metronom, img);
  }
  this->current_duration = img->duration;

  if (!this->grab_only) {

    pic_vpts = img->vpts;
    img->extra_info->vpts = img->vpts;

    cur_vpts = this->clock->get_current_time(this->clock);
    this->last_delivery_pts = cur_vpts;

    lprintf ("got image oat master vpts %" PRId64 ". vpts for picture is %" PRId64 " (pts was %" PRId64 ")\n",
	     cur_vpts, pic_vpts, img->pts);

    this->num_frames_delivered++;

    diff = pic_vpts - cur_vpts;
    
    /* avoid division by zero */
    if( img->duration <= 0 )
      duration = DEFAULT_FRAME_DURATION;
    else
      duration = img->duration;
    
    /* Frame dropping slow start:
     *   The engine starts to drop frames if there is less than frame_drop_limit
     *   frames in advance. There might be a problem just after a seek because
     *   there is no frame in advance yet.
     *   The following code increases progressively the frame_drop_limit (-2 -> 3)
     *   after a seek to give a chance to the engine to display the first frames
     *   smootly before starting to drop frames if the decoder is really too
     *   slow.
     */
    if (stream && stream->first_frame_flag == 2)
      this->frame_drop_cpt = 10;

    if (this->frame_drop_cpt) {
      this->frame_drop_limit = 3 - (this->frame_drop_cpt / 2);
      this->frame_drop_cpt--;
    }
    frames_to_skip = ((-1 * diff) / duration + this->frame_drop_limit) * 2;

    if (frames_to_skip < 0)
      frames_to_skip = 0;

    lprintf ("delivery diff : %" PRId64 ", current vpts is %" PRId64 ", %d frames to skip\n",
	     diff, cur_vpts, frames_to_skip);
    
  } else {
    frames_to_skip = 0;

    if (this->discard_frames) {
      lprintf ("i'm in flush mode, not appending this frame to queue\n");

      return 0;
    }
  }


  if (!img->bad_frame) {

    /* do not call proc_*() for frames that will be dropped */
    if( !frames_to_skip && !img->proc_called )
      vo_frame_driver_proc(img);
    
    /*
     * put frame into FIFO-Buffer
     */

    lprintf ("frame is ok => appending to display buffer\n");

    /*
     * check for first frame after seek and mark it
     */
    img->is_first = 0;
    pthread_mutex_lock(&this->streams_lock);
    for (stream = xine_list_first_content(this->streams); stream;
         stream = xine_list_next_content(this->streams)) {
      if (stream == XINE_ANON_STREAM) continue;
      pthread_mutex_lock (&stream->first_frame_lock);
      if (stream->first_frame_flag == 2) {
        stream->first_frame_flag = (this->grab_only) ? 0 : 1;
        img->is_first = FIRST_FRAME_MAX_POLL;

        lprintf ("get_next_video_frame first_frame_reached\n");
      }
      pthread_mutex_unlock (&stream->first_frame_lock);
    }
    pthread_mutex_unlock(&this->streams_lock);

    vo_frame_inc_lock( img );
    vo_append_to_img_buf_queue (this->display_img_buf_queue, img);

  } else {
    lprintf ("bad_frame\n");

    if (stream) {
      pthread_mutex_lock( &stream->current_extra_info_lock );
      _x_extra_info_merge( stream->current_extra_info, img->extra_info );
      pthread_mutex_unlock( &stream->current_extra_info_lock );
    }

    this->num_frames_skipped++;
  }

  /*
   * performance measurement
   */

  if ((this->num_frames_delivered % 200) == 0 && this->num_frames_delivered) {
    int send_event;

    if( (100 * this->num_frames_skipped / this->num_frames_delivered) >
         this->warn_skipped_threshold ||
        (100 * this->num_frames_discarded / this->num_frames_delivered) >
         this->warn_discarded_threshold )
      this->warn_threshold_exceeded++;
    else
      this->warn_threshold_exceeded = 0;

    /* make sure threshold has being consistently exceeded - 5 times in a row
     * (that is, this is not just a small burst of dropped frames).
     */
    send_event = (this->warn_threshold_exceeded == 5 && 
                  !this->warn_threshold_event_sent);
    this->warn_threshold_event_sent += send_event;

    pthread_mutex_lock(&this->streams_lock);
    for (stream = xine_list_first_content(this->streams); stream;
         stream = xine_list_next_content(this->streams)) {
      if (stream == XINE_ANON_STREAM) continue;
      _x_stream_info_set(stream, XINE_STREAM_INFO_SKIPPED_FRAMES, 
			 1000 * this->num_frames_skipped / this->num_frames_delivered);
      _x_stream_info_set(stream, XINE_STREAM_INFO_DISCARDED_FRAMES,
			 1000 * this->num_frames_discarded / this->num_frames_delivered);

      /* we send XINE_EVENT_DROPPED_FRAMES to frontend to warn that
       * number of skipped or discarded frames is too high.
       */
      if( send_event ) {
         xine_event_t          event;
         xine_dropped_frames_t data;

         event.type        = XINE_EVENT_DROPPED_FRAMES;
         event.stream      = stream;
         event.data        = &data;
         event.data_length = sizeof(data);
         data.skipped_frames = _x_stream_info_get(stream, XINE_STREAM_INFO_SKIPPED_FRAMES);
         data.skipped_threshold = this->warn_skipped_threshold * 10;
         data.discarded_frames = _x_stream_info_get(stream, XINE_STREAM_INFO_DISCARDED_FRAMES);
         data.discarded_threshold = this->warn_discarded_threshold * 10;
         xine_event_send(stream, &event);
      }
    }
    pthread_mutex_unlock(&this->streams_lock);


    if( this->num_frames_skipped || this->num_frames_discarded ) {
      xine_log(this->xine, XINE_LOG_MSG,
	       _("%d frames delivered, %d frames skipped, %d frames discarded\n"), 
	       this->num_frames_delivered, 
	       this->num_frames_skipped, this->num_frames_discarded);
    }

    this->num_frames_delivered = 0;
    this->num_frames_discarded = 0;
    this->num_frames_skipped   = 0;
  }
  
  return frames_to_skip;
}

/*
 *
 * video out loop related functions
 *
 */

/* duplicate_frame(): this function is used to keep playing frames 
 * while video is still or player paused. 
 * 
 * frame allocation inside vo loop is dangerous:
 * we must never wait for a free frame -> deadlock condition.
 * to avoid deadlocks we don't use vo_remove_from_img_buf_queue()
 * and reimplement a slightly modified version here.
 * free_img_buf_queue->mutex must be grabbed prior entering it.
 * (must assure that free frames won't be exhausted by decoder thread).
 */
static vo_frame_t * duplicate_frame( vos_t *this, vo_frame_t *img ) {

  vo_frame_t *dupl;
  int         image_size;

  if( !this->free_img_buf_queue->first)
    return NULL;

  dupl = this->free_img_buf_queue->first;
  this->free_img_buf_queue->first = dupl->next;
  dupl->next = NULL;
  if (!this->free_img_buf_queue->first) {
    this->free_img_buf_queue->last = NULL;
    this->free_img_buf_queue->num_buffers = 0;
  }
  else {
    this->free_img_buf_queue->num_buffers--;
  }
      
  pthread_mutex_lock (&dupl->mutex);
  dupl->lock_counter   = 1;
  dupl->width          = img->width;
  dupl->height         = img->height;
  dupl->ratio          = img->ratio;
  dupl->format         = img->format;
  dupl->flags          = img->flags | VO_BOTH_FIELDS;
  
  this->driver->update_frame_format (this->driver, dupl, dupl->width, dupl->height, 
				     dupl->ratio, dupl->format, dupl->flags);

  pthread_mutex_unlock (&dupl->mutex);
  
  image_size = img->pitches[0] * img->height;

  switch (img->format) {
  case XINE_IMGFMT_YV12:
    yv12_to_yv12(
     /* Y */
      img->base[0], img->pitches[0],
      dupl->base[0], dupl->pitches[0],
     /* U */
      img->base[1], img->pitches[1],
      dupl->base[1], dupl->pitches[1],
     /* V */
      img->base[2], img->pitches[2],
      dupl->base[2], dupl->pitches[2],
     /* width x height */
      img->width, img->height);
    break;
  case XINE_IMGFMT_YUY2:
    yuy2_to_yuy2(
     /* src */
      img->base[0], img->pitches[0],
     /* dst */
      dupl->base[0], dupl->pitches[0],
     /* width x height */
      img->width, img->height);
    break;
  }
  
  dupl->bad_frame   = 0;
  dupl->pts         = 0;
  dupl->vpts        = 0;
  dupl->proc_called = 0;

  dupl->duration  = img->duration;
  dupl->is_first  = img->is_first;

  dupl->stream    = img->stream;
  memcpy( dupl->extra_info, img->extra_info, sizeof(extra_info_t) );
  
  /* delay frame processing for now, we might not even need it (eg. frame will be discarded) */
  /* vo_frame_driver_proc(dupl); */
  
  return dupl;
}


static void expire_frames (vos_t *this, int64_t cur_vpts) {

  int64_t       pts;
  int64_t       diff;
  vo_frame_t   *img;
  int           duration;

  pthread_mutex_lock(&this->display_img_buf_queue->mutex);
  
  img = this->display_img_buf_queue->first;

  /*
   * throw away expired frames
   */

  diff = 1000000; /* always enter the while-loop */
  duration = 0;

  while (img) {

    if (img->is_first) {
      lprintf("expire_frames: first_frame !\n");

      /*
       * before displaying the first frame without
       * "metronom prebuffering" we should make sure it's 
       * not used as a decoder reference anymore.
       */
      if( img->lock_counter == 1 ) {
        /* display it immediately */
        img->vpts = cur_vpts;
      } else {
        /* poll */
        lprintf("frame still referenced %d times, is_first=%d\n", img->lock_counter, img->is_first);
        img->vpts = cur_vpts + FIRST_FRAME_POLL_DELAY;
      }
      img->is_first--;
      break;
    }

    if( !img->duration ) {
      if( img->next )
        duration = img->next->vpts - img->vpts;
      else
        duration = DEFAULT_FRAME_DURATION;
    } else
      duration = img->duration;
    
    pts = img->vpts;
    diff = cur_vpts - pts;
      
    if (diff > duration || this->discard_frames) {
     
      if( !this->discard_frames ) {
        xine_log(this->xine, XINE_LOG_MSG,
	         _("video_out: throwing away image with pts %" PRId64 " because it's too old (diff : %" PRId64 ").\n"), pts, diff);

        this->num_frames_discarded++;
      }
      
      img = vo_remove_from_img_buf_queue_int (this->display_img_buf_queue, 1);
  
      if (img->stream) {
	pthread_mutex_lock( &img->stream->current_extra_info_lock );
	_x_extra_info_merge( img->stream->current_extra_info, img->extra_info );
	pthread_mutex_unlock( &img->stream->current_extra_info_lock );
      }

      /* when flushing frames, keep the first one as backup */
      if( this->discard_frames ) {
        
        if (!this->img_backup) {
	  this->img_backup = img;
        } else {
	  vo_frame_dec_lock( img );
        }     
           
      } else {
        /*
         * last frame? back it up for 
         * still frame creation
         */
        
        if (!this->display_img_buf_queue->first) {
	  
	  if (this->img_backup) {
	    lprintf("overwriting frame backup\n");

	    vo_frame_dec_lock( this->img_backup );
	  }

	  lprintf("possible still frame (old)\n");

	  this->img_backup = img;
	
	  /* wait 4 frames before drawing this one. 
	     this allow slower systems to recover. */
	  this->redraw_needed = 4; 
        } else {
	  vo_frame_dec_lock( img );
        }
      }
      img = this->display_img_buf_queue->first;
      
    } else
      break;
  }
  
  pthread_mutex_unlock(&this->display_img_buf_queue->mutex);
}

/* If it's not the time to display the next frame,
 * the vpts of the next frame (if any) is returned, 0 otherwise.
 */
static vo_frame_t *get_next_frame (vos_t *this, int64_t cur_vpts,
                                   int64_t *next_frame_vpts) {
  
  vo_frame_t   *img;

  pthread_mutex_lock(&this->display_img_buf_queue->mutex);
  
  img = this->display_img_buf_queue->first;

  *next_frame_vpts = 0;

  /* 
   * still frame detection:
   */

  /* no frame? => still frame detection */

  if (!img) {

    pthread_mutex_unlock(&this->display_img_buf_queue->mutex);

    lprintf ("no frame\n");

    if (this->img_backup && (this->redraw_needed==1)) {

      lprintf("generating still frame (cur_vpts = %" PRId64 ") \n", cur_vpts);

      /* keep playing still frames */
      pthread_mutex_lock( &this->free_img_buf_queue->mutex );
      img = duplicate_frame (this, this->img_backup );
      pthread_mutex_unlock( &this->free_img_buf_queue->mutex );
      if( img ) {
        img->vpts = cur_vpts;
        /* extra info of the backup is thrown away, because it is not up to date */
        _x_extra_info_reset(img->extra_info);
      }
        
      return img;

    } else {
    
      if( this->redraw_needed )
        this->redraw_needed--;

      lprintf ("no frame, but no backup frame\n");

      return NULL;
    }
  } else {

    int64_t diff;

    diff = cur_vpts - img->vpts;

    /*
     * time to display frame "img" ?
     */

    lprintf ("diff %" PRId64 "\n", diff);

    if (diff < 0) {
      *next_frame_vpts = img->vpts;
      pthread_mutex_unlock(&this->display_img_buf_queue->mutex);
      return NULL;
    }

    if (this->img_backup) {
      lprintf("freeing frame backup\n");

      vo_frame_dec_lock( this->img_backup );
      this->img_backup = NULL;
    }
      
    /* 
     * last frame? make backup for possible still image 
     */
    pthread_mutex_lock( &this->free_img_buf_queue->mutex );
    if (img && !img->next) {
      
      if (!img->stream ||
          _x_stream_info_get(img->stream, XINE_STREAM_INFO_VIDEO_HAS_STILL) ||
          img->stream->video_fifo->size(img->stream->video_fifo) < 10) {

        lprintf ("possible still frame\n");

        this->img_backup = duplicate_frame (this, img);
      }
    }
    pthread_mutex_unlock( &this->free_img_buf_queue->mutex );

    /*
     * remove frame from display queue and show it
     */
    
    img = vo_remove_from_img_buf_queue_int (this->display_img_buf_queue, 1);
    pthread_mutex_unlock(&this->display_img_buf_queue->mutex);

    return img;
  }
}

static void overlay_and_display_frame (vos_t *this, 
				       vo_frame_t *img, int64_t vpts) {
  xine_stream_t *stream;

  lprintf ("displaying image with vpts = %" PRId64 "\n", img->vpts);

  /* no, this is not were proc_*() is usually called.
   * it's just to catch special cases like late or duplicated frames.
   */
  if(!img->proc_called )
    vo_frame_driver_proc(img);
  
  if (img->stream) {
    int64_t diff;
    pthread_mutex_lock( &img->stream->current_extra_info_lock );
    diff = img->extra_info->vpts - img->stream->current_extra_info->vpts;
    if ((diff > 3000) || (diff<-300000)) 
      _x_extra_info_merge( img->stream->current_extra_info, img->extra_info );
    pthread_mutex_unlock( &img->stream->current_extra_info_lock );
  }

  if (this->overlay_source) {
    this->overlay_source->multiple_overlay_blend (this->overlay_source, 
						  vpts, 
						  this->driver, img,
						  this->video_loop_running && this->overlay_enabled);
  }

  /* hold current frame for snapshot feature */
  if( this->last_frame ) {
    vo_frame_dec_lock( this->last_frame );
  }
  vo_frame_inc_lock( img );
  this->last_frame = img;

  this->driver->display_frame (this->driver, img);
  
  /*
   * Wake up xine_play if it's waiting for a frame
   */
  if( this->last_frame->is_first ) {
    pthread_mutex_lock(&this->streams_lock);
    for (stream = xine_list_first_content(this->streams); stream;
         stream = xine_list_next_content(this->streams)) {
      if (stream == XINE_ANON_STREAM) continue;
      pthread_mutex_lock (&stream->first_frame_lock);
      if (stream->first_frame_flag) {
        stream->first_frame_flag = 0;
        pthread_cond_broadcast(&stream->first_frame_reached);
      }
      pthread_mutex_unlock (&stream->first_frame_lock);
    }
    pthread_mutex_unlock(&this->streams_lock);
  }

  this->redraw_needed = 0; 
}

static void check_redraw_needed (vos_t *this, int64_t vpts) {

  if (this->overlay_source) {
    if( this->overlay_source->redraw_needed (this->overlay_source, vpts) )
      this->redraw_needed = 1; 
  }
  
  if( this->driver->redraw_needed (this->driver) )
    this->redraw_needed = 1;
}

/* special loop for paused mode
 * needed to update screen due overlay changes, resize, window
 * movement, brightness adjusting etc.
 */                   
static void paused_loop( vos_t *this, int64_t vpts )
{
  vo_frame_t   *img;
  
  pthread_mutex_lock( &this->free_img_buf_queue->mutex );
  /* prevent decoder thread from allocating new frames */
  this->free_img_buf_queue->locked_for_read = 1;
  
  while (this->clock->speed == XINE_SPEED_PAUSE) {
  
    /* we need at least one free frame to keep going */
    if( this->display_img_buf_queue->first &&
       !this->free_img_buf_queue->first ) {
    
      img = vo_remove_from_img_buf_queue (this->display_img_buf_queue);
      img->next = NULL;
      this->free_img_buf_queue->first = img;
      this->free_img_buf_queue->last  = img;
      this->free_img_buf_queue->num_buffers = 1;
    }
    
    /* set img_backup to play the same frame several times */
    if( this->display_img_buf_queue->first && !this->img_backup ) {
      this->img_backup = vo_remove_from_img_buf_queue (this->display_img_buf_queue);
      this->redraw_needed = 1;
    }
    
    check_redraw_needed( this, vpts );
    
    if( this->redraw_needed && this->img_backup ) {
      img = duplicate_frame (this, this->img_backup );
      if( img ) {
        /* extra info of the backup is thrown away, because it is not up to date */
        _x_extra_info_reset(img->extra_info);
        pthread_mutex_unlock( &this->free_img_buf_queue->mutex );
        overlay_and_display_frame (this, img, vpts);
        pthread_mutex_lock( &this->free_img_buf_queue->mutex );
      }  
    }
    
    pthread_mutex_unlock( &this->free_img_buf_queue->mutex );
    xine_usec_sleep (20000);
    pthread_mutex_lock( &this->free_img_buf_queue->mutex );
  } 
  
  this->free_img_buf_queue->locked_for_read = 0;
   
  if( this->free_img_buf_queue->first )
    pthread_cond_signal (&this->free_img_buf_queue->not_empty);
  pthread_mutex_unlock( &this->free_img_buf_queue->mutex );
}

static void *video_out_loop (void *this_gen) {

  int64_t            vpts, diff;
  vo_frame_t        *img;
  vos_t             *this = (vos_t *) this_gen;
  int64_t            next_frame_vpts = 0;
  int64_t            usec_to_sleep;
 
#ifndef WIN32
  /* nice(-value) will fail silently for normal users.
   * however when running as root this may provide smoother
   * playback. follow the link for more information:
   * http://cambuca.ldhs.cetuc.puc-rio.br/~miguel/multimedia_sim/
   */
  nice(-2);
#endif /* WIN32 */

  /*
   * here it is - the heart of xine (or rather: one of the hearts
   * of xine) : the video output loop
   */
   
  lprintf ("loop starting...\n");

  while ( this->video_loop_running ) {

    /*
     * get current time and find frame to display
     */

    vpts = this->clock->get_current_time (this->clock);

    lprintf ("loop iteration at %" PRId64 "\n", vpts);

    expire_frames (this, vpts);

    img = get_next_frame (this, vpts, &next_frame_vpts);

    /*
     * if we have found a frame, display it
     */

    if (img) {
      lprintf ("displaying frame (id=%d)\n", img->id);

      overlay_and_display_frame (this, img, vpts);

    } else {

      check_redraw_needed( this, vpts );
    }

    /*
     * if we haven't heared from the decoder for some time
     * flush it
     * test display fifo empty to protect from deadlocks
     */

    diff = vpts - this->last_delivery_pts;
    if (diff > 30000 && !this->display_img_buf_queue->first) {
      xine_stream_t *stream;
      
      pthread_mutex_lock(&this->streams_lock);
      for (stream = xine_list_first_content(this->streams); stream;
           stream = xine_list_next_content(this->streams)) {
	if (stream == XINE_ANON_STREAM) continue;
        if (stream->video_decoder_plugin && stream->video_fifo) {
          buf_element_t *buf;
          
	  lprintf ("flushing current video decoder plugin\n");
          
          buf = stream->video_fifo->buffer_pool_try_alloc (stream->video_fifo);
          if( buf ) {
            buf->type = BUF_CONTROL_FLUSH_DECODER;
            stream->video_fifo->insert(stream->video_fifo, buf);
          }
        }
      }
      pthread_mutex_unlock(&this->streams_lock);

      this->last_delivery_pts = vpts;
    }

    /*
     * wait until it's time to display next frame
     */
    if (img) {
      next_frame_vpts = img->vpts + img->duration;
    }
    /* else next_frame_vpts is returned by get_next_frame */
    
    lprintf ("next_frame_vpts is %" PRId64 "\n", next_frame_vpts);
 
    do {
      vpts = this->clock->get_current_time (this->clock);
  
      if (this->clock->speed == XINE_SPEED_PAUSE)
        paused_loop (this, vpts);

      if (next_frame_vpts) {
        usec_to_sleep = (next_frame_vpts - vpts) * 100 / 9;
      } else {
        /* we don't know when the next frame is due, only wait a little */
        usec_to_sleep = 1000;
        next_frame_vpts = vpts; /* wait only once */
      }

      /* limit usec_to_sleep to maintain responsiveness */
      if (usec_to_sleep > MAX_USEC_TO_SLEEP)
        usec_to_sleep = MAX_USEC_TO_SLEEP;

      lprintf ("%" PRId64 " usec to sleep at master vpts %" PRId64 "\n", usec_to_sleep, vpts);
      
      if ( (next_frame_vpts - vpts) > 2*90000 )
        xprintf(this->xine, XINE_VERBOSITY_DEBUG,
		"video_out: vpts/clock error, next_vpts=%" PRId64 " cur_vpts=%" PRId64 "\n", next_frame_vpts,vpts);
               
      if (usec_to_sleep > 0)
        xine_usec_sleep (usec_to_sleep);

      if (this->discard_frames)
        break;

    } while ( (usec_to_sleep > 0) && this->video_loop_running);
  }

  /*
   * throw away undisplayed frames
   */
  
  pthread_mutex_lock(&this->display_img_buf_queue->mutex);
  img = this->display_img_buf_queue->first;
  while (img) {

    img = vo_remove_from_img_buf_queue_int (this->display_img_buf_queue, 1);
    vo_frame_dec_lock( img );

    img = this->display_img_buf_queue->first;
  }
  pthread_mutex_unlock(&this->display_img_buf_queue->mutex);

  if (this->img_backup) {
    vo_frame_dec_lock( this->img_backup );
    this->img_backup = NULL;
  }
  if (this->last_frame) {
    vo_frame_dec_lock( this->last_frame );
    this->last_frame = NULL;
  }

  return NULL;
}

/*
 * public function for video processing frontends to manually
 * consume video frames
 */

int xine_get_next_video_frame (xine_video_port_t *this_gen,
			       xine_video_frame_t *frame) {

  vos_t         *this   = (vos_t *) this_gen;
  vo_frame_t    *img    = NULL;
  xine_stream_t *stream = NULL;

  while (!img || !stream) {
    stream = xine_list_first_content(this->streams);
    if (!stream) {
      xine_usec_sleep (5000);
      continue;
    }

    
    /* FIXME: ugly, use conditions and locks instead? */
    
    pthread_mutex_lock(&this->display_img_buf_queue->mutex);
    img = this->display_img_buf_queue->first;
    if (!img) {
      pthread_mutex_unlock(&this->display_img_buf_queue->mutex);
      if (stream != XINE_ANON_STREAM && stream->video_fifo->fifo_size == 0 &&
          stream->demux_plugin->get_status(stream->demux_plugin) != DEMUX_OK)
        /* no further data can be expected here */
        return 0;
      xine_usec_sleep (5000);
      continue;
    }
  }

  /*
   * remove frame from display queue and show it
   */
    
  img = vo_remove_from_img_buf_queue_int (this->display_img_buf_queue, 1);
  pthread_mutex_unlock(&this->display_img_buf_queue->mutex);

  frame->vpts         = img->vpts;
  frame->duration     = img->duration;
  frame->width        = img->width;
  frame->height       = img->height;
  frame->pos_stream   = img->extra_info->input_pos;
  frame->pos_time     = img->extra_info->input_time;
  frame->aspect_ratio = img->ratio;
  frame->colorspace   = img->format;
  frame->data         = img->base[0];
  frame->xine_frame   = img;

  return 1;
}

void xine_free_video_frame (xine_video_port_t *port, 
			    xine_video_frame_t *frame) {

  vo_frame_t *img = (vo_frame_t *) frame->xine_frame;

  vo_frame_dec_lock (img);
}


static uint32_t vo_get_capabilities (xine_video_port_t *this_gen) {
  vos_t      *this = (vos_t *) this_gen;
  return this->driver->get_capabilities (this->driver);
}

static void vo_open (xine_video_port_t *this_gen, xine_stream_t *stream) {

  vos_t      *this = (vos_t *) this_gen;

  lprintf("vo_open\n");

  this->video_opened = 1;
  this->discard_frames = 0;
  this->last_delivery_pts = 0;
  this->warn_threshold_event_sent = this->warn_threshold_exceeded = 0;
  if (!this->overlay_enabled && (stream == NULL || stream->spu_channel_user > -2))
    /* enable overlays if our new stream might want to show some */
    this->overlay_enabled = 1;
  pthread_mutex_lock(&this->streams_lock);
  xine_list_append_content(this->streams, stream);
  pthread_mutex_unlock(&this->streams_lock);
}

static void vo_close (xine_video_port_t *this_gen, xine_stream_t *stream) {

  vos_t      *this = (vos_t *) this_gen;
  xine_stream_t *cur;

  /* this will make sure all hide events were processed */
  if (this->overlay_source)
    this->overlay_source->flush_events (this->overlay_source);

  this->video_opened = 0;
  
  /* unregister stream */
  pthread_mutex_lock(&this->streams_lock);
  for (cur = xine_list_first_content(this->streams); cur;
       cur = xine_list_next_content(this->streams))
    if (cur == stream) {
      xine_list_delete_current(this->streams);
      break;
    }
  pthread_mutex_unlock(&this->streams_lock);
}


static int vo_get_property (xine_video_port_t *this_gen, int property) {
  vos_t *this = (vos_t *) this_gen;
  int ret;

  switch (property) {
  case VO_PROP_DISCARD_FRAMES:
    ret = this->discard_frames;
    break;
    
  /*
   * handle XINE_PARAM_xxx properties (convert from driver's range)
   */
  case XINE_PARAM_VO_HUE:
  case XINE_PARAM_VO_SATURATION:
  case XINE_PARAM_VO_CONTRAST:
  case XINE_PARAM_VO_BRIGHTNESS: {
    int v, min_v, max_v, range_v;

    pthread_mutex_lock( &this->driver_lock );
    this->driver->get_property_min_max (this->driver,
					property & 0xffffff,
					&min_v, &max_v);

    v = this->driver->get_property (this->driver, property & 0xffffff);

    range_v = max_v - min_v;

    if (range_v > 0)
      ret = (v-min_v) * 65535 / range_v;
    else 
      ret = 0;
    pthread_mutex_unlock( &this->driver_lock );
  }
    break;
  
  default:
    pthread_mutex_lock( &this->driver_lock );
    ret = this->driver->get_property(this->driver, property & 0xffffff);
    pthread_mutex_unlock( &this->driver_lock );
  }
  return ret;
}

static int vo_set_property (xine_video_port_t *this_gen, int property, int value) {
  vos_t *this = (vos_t *) this_gen;
  int ret;

  switch (property) {
  
  case VO_PROP_DISCARD_FRAMES:
    /* recursive discard frames setting */
    pthread_mutex_lock(&this->display_img_buf_queue->mutex);
    if(value)
      this->discard_frames++;
    else
      this->discard_frames--;
    pthread_mutex_unlock(&this->display_img_buf_queue->mutex);
    ret = this->discard_frames;
    
    /* discard buffers here because we have no output thread */
    if (this->grab_only && this->discard_frames) {
      vo_frame_t *img;
      
      pthread_mutex_lock(&this->display_img_buf_queue->mutex);
  
      while ((img = this->display_img_buf_queue->first)) {
  
        lprintf ("flushing out frame\n");
  
        img = vo_remove_from_img_buf_queue_int (this->display_img_buf_queue, 1);
  
        vo_frame_dec_lock (img);
      }
      pthread_mutex_unlock(&this->display_img_buf_queue->mutex);
    }
    break;

  /*
   * handle XINE_PARAM_xxx properties (convert to driver's range)
   */
  case XINE_PARAM_VO_HUE:
  case XINE_PARAM_VO_SATURATION:
  case XINE_PARAM_VO_CONTRAST:
  case XINE_PARAM_VO_BRIGHTNESS:
    if (!this->grab_only) {
      int v, min_v, max_v, range_v;
  
      pthread_mutex_lock( &this->driver_lock );
      
      this->driver->get_property_min_max (this->driver,
  					property & 0xffffff,
  					&min_v, &max_v);
  
      range_v = max_v - min_v;
  
      v = (value * range_v) / 65535 + min_v;
  
      this->driver->set_property(this->driver, property & 0xffffff, v);
      pthread_mutex_unlock( &this->driver_lock );
      ret = value;
    } else
      ret = 0;
    break;
    
    
  default:
    if (!this->grab_only) {
      pthread_mutex_lock( &this->driver_lock );
      ret =  this->driver->set_property(this->driver, property & 0xffffff, value);
      pthread_mutex_unlock( &this->driver_lock );
    } else
      ret = 0;
  }

  return ret;
}


static int vo_status (xine_video_port_t *this_gen, xine_stream_t *stream,
                      int *width, int *height, int64_t *img_duration) {

  vos_t      *this = (vos_t *) this_gen;
  xine_stream_t *cur;
  int ret = 0;

  pthread_mutex_lock(&this->streams_lock);
  for (cur = xine_list_first_content(this->streams); cur;
       cur = xine_list_next_content(this->streams))
    if (cur == stream || !stream) {
      *width = this->current_width;
      *height = this->current_height;
      *img_duration = this->current_duration;
      ret = !!stream; /* return false for a NULL stream, true otherwise */
      break;
    }
  pthread_mutex_unlock(&this->streams_lock);
  
  return ret;
}


static void vo_free_img_buffers (xine_video_port_t *this_gen) {
  vos_t      *this = (vos_t *) this_gen;
  vo_frame_t *img;

  while (this->free_img_buf_queue->first) {
    img = vo_remove_from_img_buf_queue (this->free_img_buf_queue);
    img->dispose (img);
  }

  while (this->display_img_buf_queue->first) {
    img = vo_remove_from_img_buf_queue (this->display_img_buf_queue) ;
    img->dispose (img);
  }

  free (this->extra_info_base);
}

static void vo_exit (xine_video_port_t *this_gen) {

  vos_t      *this = (vos_t *) this_gen;

  lprintf ("vo_exit...\n");

  if (this->video_loop_running) {
    void *p;

    this->video_loop_running = 0;

    pthread_join (this->video_thread, &p);
  }

  vo_free_img_buffers (this_gen);

  this->driver->dispose (this->driver);

  lprintf ("vo_exit... done\n");

  if (this->overlay_source) {
    this->overlay_source->dispose (this->overlay_source);
  }
  
  xine_list_free(this->streams);
  pthread_mutex_destroy(&this->streams_lock);

  free (this->free_img_buf_queue);
  free (this->display_img_buf_queue);

  free (this);
}

static vo_frame_t *vo_get_last_frame (xine_video_port_t *this_gen) {
  vos_t      *this = (vos_t *) this_gen;
  return this->last_frame;
}

/*
 * overlay stuff 
 */

static video_overlay_manager_t *vo_get_overlay_manager (xine_video_port_t *this_gen) {
  vos_t      *this = (vos_t *) this_gen;
  return this->overlay_source;
}

static void vo_enable_overlay (xine_video_port_t *this_gen, int overlay_enabled) {
  vos_t      *this = (vos_t *) this_gen;
  
  if (overlay_enabled) {
    /* we always ENable ... */
    this->overlay_enabled = 1;
  } else {
    /* ... but we only actually DISable, if all associated streams have SPU off */
    xine_stream_t *stream;
    pthread_mutex_lock(&this->streams_lock);
    for (stream = xine_list_first_content(this->streams) ; stream ;
         stream = xine_list_next_content(this->streams)) {
      if (stream == XINE_ANON_STREAM || stream->spu_channel_user > -2) {
	pthread_mutex_unlock(&this->streams_lock);
	return;
      }
    }
    pthread_mutex_unlock(&this->streams_lock);
    this->overlay_enabled = 0;
  }
}

/*
 * Flush video_out fifo
 */
static void vo_flush (xine_video_port_t *this_gen) {
  vos_t      *this = (vos_t *) this_gen;
  vo_frame_t *img;

  if( this->video_loop_running ) {
    pthread_mutex_lock(&this->display_img_buf_queue->mutex);
    this->discard_frames++;
    pthread_mutex_unlock(&this->display_img_buf_queue->mutex);
    
    /* do not try this in paused mode */
    while(this->clock->speed != XINE_SPEED_PAUSE) {
      pthread_mutex_lock(&this->display_img_buf_queue->mutex);
      img = this->display_img_buf_queue->first;
      pthread_mutex_unlock(&this->display_img_buf_queue->mutex);
      if(!img)
        break;
      xine_usec_sleep (20000); /* pthread_cond_t could be used here */
    }
        
    pthread_mutex_lock(&this->display_img_buf_queue->mutex);
    this->discard_frames--;
    pthread_mutex_unlock(&this->display_img_buf_queue->mutex);
  }
}

xine_video_port_t *_x_vo_new_port (xine_t *xine, vo_driver_t *driver, int grabonly) {

  vos_t            *this;
  int               i;
  pthread_attr_t    pth_attrs;
  int		    err;
  int               num_frame_buffers;


  this = xine_xmalloc (sizeof (vos_t)) ;

  this->xine                  = xine;
  this->clock                 = xine->clock;
  this->driver                = driver;
  this->streams               = xine_list_new();
  
  pthread_mutex_init(&this->streams_lock, NULL);
  pthread_mutex_init(&this->driver_lock, NULL );
  
  this->vo.open                  = vo_open;
  this->vo.get_frame             = vo_get_frame;
  this->vo.get_last_frame        = vo_get_last_frame;
  this->vo.close                 = vo_close;
  this->vo.exit                  = vo_exit;
  this->vo.get_capabilities      = vo_get_capabilities;
  this->vo.enable_ovl            = vo_enable_overlay;
  this->vo.get_overlay_manager   = vo_get_overlay_manager;
  this->vo.flush                 = vo_flush;
  this->vo.get_property          = vo_get_property;
  this->vo.set_property          = vo_set_property;
  this->vo.status                = vo_status;
  this->vo.driver                = driver;

  this->num_frames_delivered  = 0;
  this->num_frames_skipped    = 0;
  this->num_frames_discarded  = 0;
  this->free_img_buf_queue    = vo_new_img_buf_queue ();
  this->display_img_buf_queue = vo_new_img_buf_queue ();
  this->video_loop_running    = 0;

  this->last_frame            = NULL;
  this->img_backup            = NULL;
  
  this->overlay_source        = _x_video_overlay_new_manager(xine);
  this->overlay_source->init (this->overlay_source);
  this->overlay_enabled       = 1;

  this->frame_drop_limit      = 3;
  this->frame_drop_cpt        = 0;

  num_frame_buffers = driver->get_property (driver, VO_PROP_MAX_NUM_FRAMES);

  if (!num_frame_buffers)
    num_frame_buffers = NUM_FRAME_BUFFERS; /* default */
  else if (num_frame_buffers<5) 
    num_frame_buffers = 5;

  this->extra_info_base = calloc (num_frame_buffers,
					  sizeof(extra_info_t));

  for (i=0; i<num_frame_buffers; i++) {
    vo_frame_t *img;

    img = driver->alloc_frame (driver) ;
    if (!img) break;

    img->id        = i;
    
    img->port      = &this->vo;
    img->free      = vo_frame_dec_lock;
    img->lock      = vo_frame_inc_lock;
    img->draw      = vo_frame_draw;

    img->extra_info = &this->extra_info_base[i];

    vo_append_to_img_buf_queue (this->free_img_buf_queue,
				img);
  }

  this->warn_skipped_threshold = 
    xine->config->register_num (xine->config, "video.warn_skipped_threshold", 10,
    _("percentage of skipped frames to tolerate"),
    _("When more than this percentage of frames are not shown, because they "
      "were not decoded in time, xine sends a notification."),
    20, NULL, NULL);
  this->warn_discarded_threshold = 
    xine->config->register_num (xine->config, "video.warn_discarded_threshold", 10,
    _("percentage of discarded frames to tolerate"),
    _("When more than this percentage of frames are not shown, because they "
      "were not scheduled for display in time, xine sends a notification."),
    20, NULL, NULL);


  if (grabonly) {

    this->video_loop_running   = 0;
    this->video_opened         = 0;
    this->grab_only            = 1;

  } else {

    /*
     * start video output thread
     *
     * this thread will alwys be running, displaying the
     * logo when "idle" thus making it possible to have
     * osd when not playing a stream
     */

    this->video_loop_running   = 1;
    this->video_opened         = 0;
    this->grab_only            = 0;
    
    pthread_attr_init(&pth_attrs);
    pthread_attr_setscope(&pth_attrs, PTHREAD_SCOPE_SYSTEM);
    
    if ((err = pthread_create (&this->video_thread,
			       &pth_attrs, video_out_loop, this)) != 0) {

      xprintf (this->xine, XINE_VERBOSITY_DEBUG, "video_out: can't create thread (%s)\n", strerror(err));
      /* FIXME: how does this happen ? */
      xprintf (this->xine, XINE_VERBOSITY_LOG,
	       _("video_out: sorry, this should not happen. please restart xine.\n"));
      _x_abort();
    }
    else
      xprintf(this->xine, XINE_VERBOSITY_DEBUG, "video_out: thread created\n");
    
    pthread_attr_destroy(&pth_attrs);
  }

  return &this->vo;
}
