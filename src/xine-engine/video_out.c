/*
 * Copyright (C) 2000-2018 the xine project
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
#include <pthread.h>
#include <inttypes.h>
#include <errno.h>
#include <sys/time.h>

#define XINE_ENABLE_EXPERIMENTAL_FEATURES

#define LOG_MODULE "video_out"
#define LOG_VERBOSE
/*
#define LOG
#define LOG_FLUSH
*/

#include <xine/xine_internal.h>
#include <xine/video_out.h>
#include <xine/metronom.h>
#include <xine/xineutils.h>
#include <yuv2rgb.h>

#include "xine_private.h"

#define NUM_FRAME_BUFFERS          15
/* 24/25/30 fps are most common, do these in a single wait */
#define MAX_USEC_TO_SLEEP       42000
#define DEFAULT_FRAME_DURATION   3000    /* 30 frames per second */

/* wait this delay if the first frame is still referenced */
#define FIRST_FRAME_POLL_DELAY   3000
#define FIRST_FRAME_MAX_POLL       10    /* poll n times at most */

/* experimental optimization: try to allocate frames from free queue
 * in the same format as requested (avoid unnecessary free/alloc in
 * vo driver). up to 25% less cpu load using deinterlace with film mode.
 */
#define EXPERIMENTAL_FRAME_QUEUE_OPTIMIZATION 1

static vo_frame_t * crop_frame( xine_video_port_t *this_gen, vo_frame_t *img );

typedef struct vos_grab_video_frame_s vos_grab_video_frame_t;
struct vos_grab_video_frame_s {
  xine_grab_video_frame_t grab_frame;

  vos_grab_video_frame_t *next;
  int finished;
  xine_video_port_t *video_port;
  vo_frame_t *vo_frame;
  yuv2rgb_factory_t *yuv2rgb_factory;
  yuv2rgb_t *yuv2rgb;
  int vo_width, vo_height;
  int grab_width, grab_height;
  int y_stride, uv_stride;
  int img_size;
  uint8_t *img;
};


typedef struct {
  vo_frame_t        *first;
  vo_frame_t       **add;
  int                num_buffers;
  int                num_buffers_max;

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

#define STREAMS_DEFAULT_SIZE 32
  int                       num_null_streams;
  int                       num_anon_streams;
  int                       num_streams;
  int                       streams_size;
  xine_stream_t           **streams, *streams_default[STREAMS_DEFAULT_SIZE];
  xine_rwlock_t             streams_lock;

  img_buf_fifo_t            free_img_buf_queue;
  img_buf_fifo_t            display_img_buf_queue;

  /* The flush protocol, protected by display_img_buf_queue.mutex. */
  int                       discard_frames;
  int                       flush_extra;
  int                       num_flush_waiters;
  pthread_cond_t            done_flushing;

  /* Optimization: keep video decoder from interfering with output timing.
   * Employ a separate mutex less frame queue private to the render thread.
   * Fetch the entire shared queue into it when flushing, or when running
   * down to less than 2 frames. This way we can still set frame.future_frame.
   * Also, do that after time critical stuff, if possible.
   */
  vo_frame_t               *ready_first;
  vo_frame_t              **ready_add;
  int                       ready_num;
  /* More render thread private stuff. */
  /* Output loop iterations, total and without a frame to display. */
  int                       wakeups_total;
  int                       wakeups_early;
  /* Snapshot of num_flush_waiters. */
  int                       need_flush_signal;
  /* Filler frame during forward seek. */
  vo_frame_t               *last_flushed;
  /* Wakeup time. */
  struct timespec           now;

  /* Get grab_lock when
   *  - accessing grab queue,
   *  - setting last_frame, and
   *  - reading last_frame from outside the render thread.
   */
  vo_frame_t               *last_frame;
  vos_grab_video_frame_t   *pending_grab_request;
  pthread_mutex_t           grab_lock;
  pthread_cond_t            grab_cond;

  uint32_t                  video_loop_running:1;
  uint32_t                  video_opened:1;

  uint32_t                  overlay_enabled:1;

  uint32_t                  warn_threshold_event_sent:1;

  /* do we true real-time output or is this a grab only instance ? */
  uint32_t                  grab_only:1;

  uint32_t                  redraw_needed:3;

  pthread_t                 video_thread;

  int                       num_frames_delivered;
  int                       num_frames_skipped;
  int                       num_frames_discarded;
  int                       num_frames_burst;

  /* threshold for sending XINE_EVENT_DROPPED_FRAMES */
  int                       warn_skipped_threshold;
  int                       warn_discarded_threshold;
  int                       warn_threshold_exceeded;

  /* pts value when decoder delivered last video frame */
  int64_t                   last_delivery_pts;

  video_overlay_manager_t  *overlay_source;

  extra_info_t             *extra_info_base; /* used to free mem chunk */

  int                       current_width, current_height;
  int64_t                   current_duration;

  int                       frame_drop_limit_max;
  int                       frame_drop_limit;
  int                       frame_drop_cpt;
  int                       frame_drop_suggested;

  int                       crop_left, crop_right, crop_top, crop_bottom;

  pthread_mutex_t           trigger_drawing_mutex;
  pthread_cond_t            trigger_drawing_cond;
  int                       trigger_drawing;
  int                       step;
  pthread_cond_t            done_stepping;

  int                       keyframe_mode;

  /* frame stream refs */
  vo_frame_t              **frames;
  xine_stream_t           **img_streams;

  /* frames usage stats */
  int                       frames_total;
  int                       frames_extref;
  int                       frames_peak_used;
} vos_t;


/********************************************************************
 * streams register.                                                *
 * Reading is way more speed relevant here.                         *
 *******************************************************************/

static void vo_streams_open (vos_t *this) {
#ifndef HAVE_ZERO_SAFE_MEM
  this->num_null_streams   = 0;
  this->num_anon_streams   = 0;
  this->num_streams        = 0;
  this->streams_default[0] = NULL;
#endif
  this->streams_size = STREAMS_DEFAULT_SIZE;
  this->streams      = &this->streams_default[0];
  xine_rwlock_init_default (&this->streams_lock);
}

static void vo_streams_close (vos_t *this) {
  xine_rwlock_destroy (&this->streams_lock);
  if (this->streams != &this->streams_default[0])
    free (this->streams);
#if 0 /* not yet needed */
  this->streams          = NULL;
  this->num_null_streams = 0;
  this->num_anon_streams = 0;
  this->num_streams      = 0;
  this->streams_size     = 0;
#endif
}

static void vo_streams_register (vos_t *this, xine_stream_t *s) {
  xine_rwlock_wrlock (&this->streams_lock);
  if (!s) {
    this->num_null_streams++;
  } else if (s == XINE_ANON_STREAM) {
    this->num_anon_streams++;
  } else do {
    xine_stream_t **a = this->streams;
    if (this->num_streams + 2 > this->streams_size) {
      xine_stream_t **n = malloc ((this->streams_size + 32) * sizeof (void *));
      if (!n)
        break;
      memcpy (n, a, this->streams_size * sizeof (void *));
      this->streams = n;
      if (a != &this->streams_default[0])
        free (a);
      a = n;
      this->streams_size += 32;
    }
    a[this->num_streams++] = s;
    a[this->num_streams] = NULL;
  } while (0);
  xine_rwlock_unlock (&this->streams_lock);
}

static void vo_streams_unregister (vos_t *this, xine_stream_t *s) {
  xine_rwlock_wrlock (&this->streams_lock);
  if (!s) {
    this->num_null_streams--;
  } else if (s == XINE_ANON_STREAM) {
    this->num_anon_streams--;
  } else {
    xine_stream_t **a = this->streams;
    while (*a && (*a != s))
      a++;
    if (*a) {
      do {
        a[0] = a[1];
        a++;
      } while (*a);
      this->num_streams--;
    }
  }
  xine_rwlock_unlock (&this->streams_lock);
}

/********************************************************************
 * reuse frame stream refs.                                         *
 * be the current owner of img when calling this.                   *
 *******************************************************************/

static void vo_reref (vos_t *this, vo_frame_t *img) {
  /* Paranoia? */
  if ((img->id >= 0) && (img->id < this->frames_total)) {
    xine_stream_t **s = this->img_streams + img->id;
    if (img->stream != *s) {
      if (*s)
        _x_refcounter_dec ((*s)->refcounter);
      if (img->stream)
        _x_refcounter_inc (img->stream->refcounter);
      *s = img->stream;
    }
  }
}

static void vo_unref_frame (vos_t *this, vo_frame_t *img) {
  img->stream = NULL;
  /* Paranoia? */
  if ((img->id >= 0) && (img->id < this->frames_total)) {
    xine_stream_t **s = this->img_streams + img->id;
    if (*s) {
      _x_refcounter_dec ((*s)->refcounter);
      *s = NULL;
    }
  }
}

static void vo_unref_list (vos_t *this, vo_frame_t *img) {
  while (img) {
    img->stream = NULL;
    /* Paranoia? */
    if ((img->id >= 0) && (img->id < this->frames_total)) {
      xine_stream_t **s = this->img_streams + img->id;
      if (*s) {
        _x_refcounter_dec ((*s)->refcounter);
        *s = NULL;
      }
    }
    img = img->next;
  }
}

static void vo_unref_all (vos_t *this) {
  vo_frame_t *img;
  int n = this->frames_total;
  pthread_mutex_lock (&this->free_img_buf_queue.mutex);
  for (img = this->free_img_buf_queue.first; img; img = img->next) {
    vo_unref_frame (this, img);
    n--;
  }
  pthread_mutex_unlock (&this->free_img_buf_queue.mutex);
  if (n > 0)
    xprintf (this->xine, XINE_VERBOSITY_DEBUG,
      "video_out: unref_all: %d frames still in use.\n", n);
}

static void vo_force_unref_all (vos_t *this) {
  vo_frame_t *img;
  pthread_mutex_lock (&this->free_img_buf_queue.mutex);
  for (img = this->free_img_buf_queue.first; img; img = img->next) {
    int i;
    vo_unref_frame (this, img);
    for (i = 0; i < this->frames_total; i++) {
      if (this->frames[i] == img) {
        this->frames[i] = NULL;
        break;
      }
    }
  }
  pthread_mutex_unlock (&this->free_img_buf_queue.mutex);
  {
    int i;
    for (i = 0; i < this->frames_total; i++) {
      if (this->frames[i]) {
        xprintf (this->xine, XINE_VERBOSITY_DEBUG,
          "video_out: BUG: frame #%d (%p) still in use (%d refs).\n",
          i, (void*)this->frames[i], this->frames[i]->lock_counter);
      }
    }
  }
}

/********************************************************************
 * frame queue (fifo)                                               *
 *******************************************************************/

static void vo_queue_open (img_buf_fifo_t *queue) {
#ifndef HAVE_ZERO_SAFE_MEM
  queue->first           = NULL;
  queue->num_buffers     = 0;
  queue->num_buffers_max = 0;
  queue->locked_for_read = 0;
#endif
  queue->add             = &queue->first;
  pthread_mutex_init (&queue->mutex, NULL);
  pthread_cond_init  (&queue->not_empty, NULL);
}

static void vo_queue_close (img_buf_fifo_t *queue) {
#if 0 /* not yet needed */
  queue->first           = NULL;
  queue->add             = &queue->first;
  queue->num_buffers     = 0;
  queue->num_buffers_max = 0;
  queue->locked_for_read = 0;
#endif
  pthread_mutex_destroy (&queue->mutex);
  pthread_cond_destroy  (&queue->not_empty);
}

static void vo_queue_dispose_all (img_buf_fifo_t *queue) {
  vo_frame_t *f;

  pthread_mutex_lock (&queue->mutex);
  f = queue->first;
  queue->first = NULL;
  queue->add   = &queue->first;
  queue->num_buffers = 0;
  pthread_mutex_unlock (&queue->mutex);

  while (f) {
    vo_frame_t *next = f->next;
    f->next = NULL;
    f->dispose (f);
    f = next;
  }
}

static void vo_queue_read_lock (img_buf_fifo_t *queue) {
  pthread_mutex_lock (&queue->mutex);
  queue->locked_for_read = 2;
  pthread_mutex_unlock (&queue->mutex);
}

static void vo_queue_read_unlock (img_buf_fifo_t *queue) {
  pthread_mutex_lock (&queue->mutex);
  queue->locked_for_read = 0;
  if (queue->first)
    pthread_cond_signal (&queue->not_empty);
  pthread_mutex_unlock (&queue->mutex);
}

static void vo_ticket_revoked (void *user_data, int flags) {
  vos_t *this = (vos_t *)user_data;
  const char *s1 = (flags & XINE_TICKET_FLAG_ATOMIC) ? " atomic" : "";
  const char *s2 = (flags & XINE_TICKET_FLAG_REWIRE) ? " port_rewire" : "";
  pthread_cond_signal (&this->free_img_buf_queue.not_empty);
  xprintf (this->xine, XINE_VERBOSITY_DEBUG, "video_out: port ticket revoked%s%s.\n", s1, s2);
}

static void vo_queue_append (img_buf_fifo_t *queue, vo_frame_t *img) {
  int n;

  /* img already enqueue? (serious leak) */
  _x_assert (img->next==NULL);

  pthread_mutex_lock (&queue->mutex);

  img->next = NULL;

  n = (queue->first ? queue->num_buffers : 0) + 1;
  *(queue->add) = img;
  queue->add    = &img->next;
  queue->num_buffers = n;
  if (queue->num_buffers_max < n)
    queue->num_buffers_max = n;

  if (n > queue->locked_for_read)
    pthread_cond_signal (&queue->not_empty);

  pthread_mutex_unlock (&queue->mutex);
}

static void vo_free_append_list (vos_t *this, vo_frame_t *img, vo_frame_t **add, int n) {
  if (!img)
    return;

  if (!this->num_streams)
    vo_unref_list (this, img);

  pthread_mutex_lock (&this->free_img_buf_queue.mutex);

  *(this->free_img_buf_queue.add) = img;
  this->free_img_buf_queue.add    = add;

  n += this->free_img_buf_queue.num_buffers;
  this->free_img_buf_queue.num_buffers = n;
  if (this->free_img_buf_queue.num_buffers_max < n)
    this->free_img_buf_queue.num_buffers_max = n;

  if (n > this->free_img_buf_queue.locked_for_read)
    pthread_cond_broadcast (&this->free_img_buf_queue.not_empty);
  pthread_mutex_unlock (&this->free_img_buf_queue.mutex);
}

static vo_frame_t *vo_queue_pop_int (img_buf_fifo_t *queue) {
  vo_frame_t *img;

  img = queue->first;
  queue->first = img->next;
  img->next = NULL;
  if (!queue->first) {
    queue->add = &queue->first;
    queue->num_buffers = 0;
  } else {
    queue->num_buffers--;
  }

  return img;
}

static int vo_frame_dec2_lock_int (vos_t *this, vo_frame_t *img);

static vo_frame_t *vo_get_unblock_frame (vos_t *this) {
  vo_frame_t *f, **add;
  /* Try 1: free queue reserve. */
  pthread_mutex_lock (&this->free_img_buf_queue.mutex);
  if (this->free_img_buf_queue.first) {
    f = vo_queue_pop_int (&this->free_img_buf_queue);
    pthread_mutex_unlock (&this->free_img_buf_queue.mutex);
    xprintf (this->xine, XINE_VERBOSITY_DEBUG, "video_out: got unblock frame from free queue.\n");
    return f;
  }
  pthread_mutex_unlock (&this->free_img_buf_queue.mutex);
  /* Try 2: shared display queue. */
  pthread_mutex_lock (&this->display_img_buf_queue.mutex);
  add = &this->display_img_buf_queue.first;
  while ((f = *add)) {
    if (f->lock_counter <= 2)
      break;
    add = &f->next;
  }
  if (f) {
    *add = f->next;
    /* f->next = NULL; vo_frame_dec2_lock_int () does this below */
    this->display_img_buf_queue.num_buffers--;
    if (!*add) {
      this->display_img_buf_queue.add = add;
      /* just a safety reset */
      if (!this->display_img_buf_queue.first)
        this->display_img_buf_queue.num_buffers = 0;
    }
    pthread_mutex_unlock (&this->display_img_buf_queue.mutex);
    vo_frame_dec2_lock_int (this, f);
    xprintf (this->xine, XINE_VERBOSITY_DEBUG, "video_out: got unblock frame from display queue.\n");
    return f;
  }
  pthread_mutex_unlock (&this->display_img_buf_queue.mutex);
  return NULL;
}

static vo_frame_t *vo_free_queue_get (vos_t *this,
  uint32_t width, uint32_t height, double ratio, int format, int flags) {
  vo_frame_t *img, **add;

  (void)flags;
  pthread_mutex_lock (&this->free_img_buf_queue.mutex);

  do {
    add = &this->free_img_buf_queue.first;
    if (this->free_img_buf_queue.num_buffers > this->free_img_buf_queue.locked_for_read) {
      img = *add;
#if EXPERIMENTAL_FRAME_QUEUE_OPTIMIZATION
      if (width && height) {
        /* try to obtain a frame with the same format first.
         * doing so may avoid unnecessary alloc/free's at the vo
         * driver, specially when using post plugins that change
         * format like the tvtime deinterlacer does.
         */
        int i = 0;
        while (img && 
          ((img->format != format) || (img->width != (int)width) ||
           (img->height != (int)height) || (img->ratio != ratio))) {
          add = &img->next;
          img = *add;
          i++;
        }

        if (!img) {
          if ((this->free_img_buf_queue.num_buffers == 1) && (this->free_img_buf_queue.num_buffers_max > 8)) {
            /* only a single frame on fifo with different
             * format -> ignore it (give another chance of a frame format hit)
             * only if we have a lot of buffers at all.
             */
            lprintf("frame format mismatch - will wait another frame\n");
          } else {
            /* we have just a limited number of buffers or at least 2 frames
             * on fifo but they don't match -> give up. return whatever we got.
             */
            add = &this->free_img_buf_queue.first;
            img = *add;
            lprintf("frame format miss (%d/%d)\n", i, this->free_img_buf_queue.num_buffers);
          }
        } else {
          /* good: format match! */
          lprintf("frame format hit (%d/%d)\n", i, this->free_img_buf_queue.num_buffers);
        }
      }
#endif
    } else {
      img = NULL;
    }
    if (!img) {
      if (this->xine->port_ticket->ticket_revoked) {
        pthread_mutex_unlock (&this->free_img_buf_queue.mutex);
        this->xine->port_ticket->renew (this->xine->port_ticket, 1);
        if (!(this->xine->port_ticket->ticket_revoked & XINE_TICKET_FLAG_REWIRE)) {
          pthread_mutex_lock (&this->free_img_buf_queue.mutex);
          continue;
        }
        /* O dear. Port rewire ahaed. Try unblocking with regular or emergency frame. */
        if (this->clock->speed == XINE_SPEED_PAUSE) {
          img = vo_get_unblock_frame (this);
          if (img)
            return img;
          xprintf (this->xine, XINE_VERBOSITY_DEBUG, "video_out: allow port rewire.\n");
          this->xine->port_ticket->renew (this->xine->port_ticket, XINE_TICKET_FLAG_REWIRE);
          pthread_mutex_lock (&this->free_img_buf_queue.mutex);
          continue;
        }
        pthread_mutex_lock (&this->free_img_buf_queue.mutex);
      }
      {
        struct timespec ts = {0, 0};
        xine_gettime (&ts);
        ts.tv_sec += 1;
        pthread_cond_timedwait (&this->free_img_buf_queue.not_empty, &this->free_img_buf_queue.mutex, &ts);
      }
    }
  } while (!img);

  if (img) {
    *add = img->next;
    img->next = NULL;
    this->free_img_buf_queue.num_buffers--;
    if (!*add) {
      this->free_img_buf_queue.add = add;
      /* just a safety reset */
      if (!this->free_img_buf_queue.first)
        this->free_img_buf_queue.num_buffers = 0;
    }
  }

  pthread_mutex_unlock (&this->free_img_buf_queue.mutex);
  return img;
}

static vo_frame_t *vo_free_get_dupl (vos_t *this, vo_frame_t *s) {
  vo_frame_t *img, **add;

  pthread_mutex_lock (&this->free_img_buf_queue.mutex);

  add = &this->free_img_buf_queue.first;
  while ((img = *add)) {
    if ((img->format == s->format) && (img->width == s->width)
      && (img->height == s->height) && (img->ratio == s->ratio))
      break;
    add = &img->next;
  }
  if (!img) {
    add = &this->free_img_buf_queue.first;
    img = *add;
  }

  if (img) {
    *add = img->next;
    img->next = NULL;
    this->free_img_buf_queue.num_buffers--;
    if (!*add) {
      this->free_img_buf_queue.add = add;
      /* just a safety reset */
      if (!this->free_img_buf_queue.first)
        this->free_img_buf_queue.num_buffers = 0;
    }
  }

  pthread_mutex_unlock (&this->free_img_buf_queue.mutex);

  return img;
}

#define ADD_READY_FRAMES \
  if (this->ready_num < 2) { \
    if (!this->ready_num || this->display_img_buf_queue.first) \
      vo_ready_refill (this); \
  }

static void vo_ready_refill (vos_t *this) {
  vo_frame_t *first, **add;

  pthread_mutex_lock (&this->display_img_buf_queue.mutex);
  first = this->display_img_buf_queue.first;
  if (!first) {
    this->flush_extra = this->ready_num;
    pthread_mutex_unlock (&this->display_img_buf_queue.mutex);
    return;
  }
  add = this->display_img_buf_queue.add;
  this->ready_num  += this->display_img_buf_queue.num_buffers;
  this->flush_extra = this->ready_num;
  this->display_img_buf_queue.first = NULL;
  this->display_img_buf_queue.add = &this->display_img_buf_queue.first;
  this->display_img_buf_queue.num_buffers = 0;
  pthread_mutex_unlock (&this->display_img_buf_queue.mutex);

  *(this->ready_add) = first;
  this->ready_add    = add;
}

static vo_frame_t *vo_ready_get_all (vos_t *this) {
  vo_frame_t *first;

  pthread_mutex_lock (&this->display_img_buf_queue.mutex);
  first = this->display_img_buf_queue.first;
  if (first) {
    this->display_img_buf_queue.first = NULL;
    this->display_img_buf_queue.add   = &this->display_img_buf_queue.first;
    this->display_img_buf_queue.num_buffers = 0;
  }
  this->flush_extra = 0;
  this->need_flush_signal = this->num_flush_waiters;
  pthread_mutex_unlock (&this->display_img_buf_queue.mutex);

  *(this->ready_add) = first;
  first = this->ready_first;
  this->ready_first = NULL;
  this->ready_add   = &this->ready_first;
  this->ready_num   = 0;
  return first;
}

static vo_frame_t *vo_ready_pop (vos_t *this) {
  vo_frame_t *img;

  img = this->ready_first;
  this->ready_first = img->next;
  img->next = NULL;
  if (!this->ready_first) {
    this->ready_add = &this->ready_first;
    this->ready_num = 0;
  } else {
    this->ready_num--;
  }

  return img;
}

static vo_frame_t *vo_ready_get_dupl (vos_t *this, vo_frame_t *s) {
  vo_frame_t *img, **add, **fadd = NULL;

  add = &this->ready_first;
  while ((img = *add)) {
    if ((img->lock_counter <= 2) && (img != s)) {
      if ((img->format == s->format) && (img->width == s->width)
        && (img->height == s->height) && (img->ratio == s->ratio))
        break;
      if (!fadd)
        fadd = add;
    }
    add = &img->next;
  }
  if (!img) {
    if (!fadd)
      return NULL;
    add = fadd;
    img = *add;
  }

  *add = img->next;
  img->next = NULL;
  this->ready_num--;
  if (!*add) {
    this->ready_add = add;
    /* just a safety reset */
    if (!this->ready_first)
      this->ready_num = 0;
  }

  return img;
}


/********************************************************************
 * frame lock_counter. Basic rule:                                  *
 * When queuing new frame, we add 2 refs.                           *
 * 1 for rendering, and 1 for still frame backup and rgb framegrab. *
 *******************************************************************/

static void vo_frame_inc2_lock (vo_frame_t *img) {
  int n;

  pthread_mutex_lock (&img->mutex);

  n = (img->lock_counter += 2);
  if ((n == 3) || (n == 4)) {
    vos_t *this = (vos_t *)img->port;
    if (this->frames_extref < this->frames_total)
      this->frames_extref++;
  }

  pthread_mutex_unlock (&img->mutex);
}

static void vo_frame_inc_lock (vo_frame_t *img) {

  pthread_mutex_lock (&img->mutex);

  img->lock_counter++;
  if (img->lock_counter == 3) {
    vos_t *this = (vos_t *)img->port;
    if (this->frames_extref < this->frames_total)
      this->frames_extref++;
  }

  pthread_mutex_unlock (&img->mutex);
}

static void vo_frame_dec_lock (vo_frame_t *img) {

  pthread_mutex_lock (&img->mutex);

  img->lock_counter--;
  if (!img->lock_counter) {
    vos_t *this = (vos_t *) img->port;
    if (!this->num_streams)
      vo_unref_frame (this, img);
    vo_queue_append (&this->free_img_buf_queue, img);
  } else
  if (img->lock_counter == 2) {
    vos_t *this = (vos_t *)img->port;
    if (this->frames_extref > 0)
      this->frames_extref--;
  }

  pthread_mutex_unlock (&img->mutex);
}

static int vo_frame_dec2_lock_int (vos_t *this, vo_frame_t *img) {
  int n;
  pthread_mutex_lock (&img->mutex);
  img->next = NULL;
  n = img->lock_counter - 2;
  if (n <= 0) /* "<=" yields better code than "<" there. */
    n = 0;
  else if ((n == 1) || (n == 2)) {
    if (this->frames_extref > 0)
      this->frames_extref--;
  }
  img->lock_counter = n;
  pthread_mutex_unlock (&img->mutex);
  return n;
}

static void vo_frame_dec2_lock (vos_t *this, vo_frame_t *img) {
  if (!vo_frame_dec2_lock_int (this, img)) {
    if (!this->num_streams)
      vo_unref_frame (this, img);
    vo_queue_append (&this->free_img_buf_queue, img);
  }
}


/********************************************************************
* Flush helpers.                                                    *
********************************************************************/

/* have this->display_img_buf_queue.mutex locked!! */
static void vo_wait_flush (vos_t *this) {
  this->num_flush_waiters++;
  pthread_mutex_lock (&this->trigger_drawing_mutex);
  this->trigger_drawing = 1;
  pthread_cond_signal (&this->trigger_drawing_cond);
  pthread_mutex_unlock (&this->trigger_drawing_mutex);
  while (this->flush_extra || this->display_img_buf_queue.first)
    pthread_cond_wait (&this->done_flushing, &this->display_img_buf_queue.mutex);
  this->num_flush_waiters--;
}

static void vo_list_flush (vos_t *this, vo_frame_t *f) {
  vo_frame_t *list = NULL, **add = &list;
  int n = 0;
  while (f) {
    vo_frame_t *next = f->next;
    if (!vo_frame_dec2_lock_int (this, f)) {
      *add = f;
      add = &f->next;
      n++;
    }
    f = next;
  }
  vo_free_append_list (this, list, add, n);
}

static void vo_manual_flush (vos_t *this) {
  vo_frame_t *f;
  pthread_mutex_lock (&this->display_img_buf_queue.mutex);
  f = this->display_img_buf_queue.first;
  this->display_img_buf_queue.first = NULL;
  this->display_img_buf_queue.add   = &this->display_img_buf_queue.first;
  this->display_img_buf_queue.num_buffers = 0;
  pthread_mutex_unlock (&this->display_img_buf_queue.mutex);
  vo_list_flush (this, f);
}


/********************************************************************
 * grabbing RGB images from displayed frames                        *
 *******************************************************************/

static void vo_dispose_grab_video_frame(xine_grab_video_frame_t *frame_gen)
{
  vos_grab_video_frame_t *frame = (vos_grab_video_frame_t *) frame_gen;

  if (frame->vo_frame)
    vo_frame_dec_lock(frame->vo_frame);

  if (frame->yuv2rgb)
    frame->yuv2rgb->dispose(frame->yuv2rgb);

  if (frame->yuv2rgb_factory)
    frame->yuv2rgb_factory->dispose(frame->yuv2rgb_factory);

  _x_freep(&frame->img);
  _x_freep(&frame->grab_frame.img);
  free(frame);
}


static int vo_grab_grab_video_frame (xine_grab_video_frame_t *frame_gen) {
  vos_grab_video_frame_t *frame = (vos_grab_video_frame_t *) frame_gen;
  vos_t *this = (vos_t *) frame->video_port;
  vo_frame_t *vo_frame;
  int format, y_stride, uv_stride;
  int width, height;
  uint8_t *base[3];

  if (frame->grab_frame.flags & XINE_GRAB_VIDEO_FRAME_FLAGS_WAIT_NEXT) {
    struct timespec ts = {0, 0};

    /* calculate absolute timeout time */
    xine_gettime (&ts);
    ts.tv_sec  +=  frame->grab_frame.timeout / 1000;
    ts.tv_nsec += (frame->grab_frame.timeout % 1000) * 1000000;
    if (ts.tv_nsec >= 1000000000) {
      ts.tv_sec += 1;
      ts.tv_nsec -= 1000000000;
    }

    pthread_mutex_lock(&this->grab_lock);

    /* insert grab request into grab queue */
    frame->next = this->pending_grab_request;
    this->pending_grab_request = frame;

    /* wait until our request is finished */
    frame->finished = 0;
    while (!frame->finished) {
      if (pthread_cond_timedwait(&this->grab_cond, &this->grab_lock, &ts) == ETIMEDOUT) {
        vos_grab_video_frame_t *prev = this->pending_grab_request;
        while (prev) {
          if (prev == frame) {
            this->pending_grab_request = frame->next;
            break;
          } else if (prev->next == frame) {
            prev->next = frame->next;
            break;
          }
          prev = prev->next;
        }
        frame->next = NULL;
        pthread_mutex_unlock(&this->grab_lock);
        return 1;   /* no frame available */
      }
    }

    pthread_mutex_unlock(&this->grab_lock);

    vo_frame = frame->vo_frame;
    frame->vo_frame = NULL;
    if (!vo_frame)
      return -1; /* error happened */
  } else {
    pthread_mutex_lock(&this->grab_lock);

    /* use last displayed frame */
    vo_frame = this->last_frame;
    if (!vo_frame) {
      pthread_mutex_unlock(&this->grab_lock);
      return 1;   /* no frame available */
    }
    if (vo_frame->format != XINE_IMGFMT_YV12 && vo_frame->format != XINE_IMGFMT_YUY2 && !vo_frame->proc_provide_standard_frame_data) {
      pthread_mutex_unlock(&this->grab_lock);
      return -1; /* error happened */
    }
    vo_frame_inc_lock(vo_frame);
    pthread_mutex_unlock(&this->grab_lock);
    frame->grab_frame.vpts = vo_frame->vpts;
  }

  width = vo_frame->width;
  height = vo_frame->height;

  if (vo_frame->format == XINE_IMGFMT_YV12 || vo_frame->format == XINE_IMGFMT_YUY2) {
    format = vo_frame->format;
    y_stride = vo_frame->pitches[0];
    uv_stride = vo_frame->pitches[1];
    base[0] = vo_frame->base[0];
    base[1] = vo_frame->base[1];
    base[2] = vo_frame->base[2];
  } else {
    /* retrieve standard format image data from output driver */
    xine_current_frame_data_t data;
    memset(&data, 0, sizeof(data));
    vo_frame->proc_provide_standard_frame_data(vo_frame, &data);
    if (data.img_size > frame->img_size) {
      free(frame->img);
      frame->img_size = data.img_size;
      frame->img = calloc(data.img_size, sizeof(uint8_t));
      if (!frame->img) {
        vo_frame_dec_lock(vo_frame);
        return -1; /* error happened */
      }
    }
    data.img = frame->img;
    vo_frame->proc_provide_standard_frame_data(vo_frame, &data);
    format = data.format;
    if (format == XINE_IMGFMT_YV12) {
      base[0] = data.img;
      base[1] = data.img + width * height;
      base[2] = data.img + width * height + ((width * height) >> 2);
      y_stride  = width;
      uv_stride = width >> 1;
    } else { // XINE_IMGFMT_YUY2
      base[0] = data.img;
      base[1] = NULL;
      base[2] = NULL;
      y_stride  = width * 2;
      uv_stride = 0;
    }
  }

  /* take cropping parameters into account */
  {
    int crop_left   = (vo_frame->crop_left   + frame->grab_frame.crop_left)  & ~1;
    int crop_right  = (vo_frame->crop_right  + frame->grab_frame.crop_right) & ~1;
    int crop_top    =  vo_frame->crop_top    + frame->grab_frame.crop_top;
    int crop_bottom =  vo_frame->crop_bottom + frame->grab_frame.crop_bottom;

    if (crop_left || crop_right || crop_top || crop_bottom) {
      if ((width - crop_left - crop_right) >= 8)
        width = width - crop_left - crop_right;
      else
        crop_left = crop_right = 0;

      if ((height - crop_top - crop_bottom) >= 8)
        height = height - crop_top - crop_bottom;
      else
        crop_top = crop_bottom = 0;

      if (format == XINE_IMGFMT_YV12) {
        size_t uv_offs;
        base[0] += crop_top * y_stride + crop_left;
        uv_offs = (crop_top >> 1) * uv_stride + (crop_left >> 1);
        base[1] += uv_offs;
        base[2] += uv_offs;
      } else { // XINE_IMGFMT_YUY2
        base[0] += crop_top * y_stride + crop_left * 2;
      }
    }
  }

  /* get pixel aspect ratio */
  {
    double sar = 1.0;
    {
      int sarw = vo_frame->width  - vo_frame->crop_left - vo_frame->crop_right;
      int sarh = vo_frame->height - vo_frame->crop_top  - vo_frame->crop_bottom;
      if ((vo_frame->ratio > 0.0) && (sarw > 0) && (sarh > 0))
        sar = vo_frame->ratio * sarh / sarw;
    }

    /* if caller does not specify frame size we return the actual size of grabbed frame */
    if ((frame->grab_frame.width <= 0) && (frame->grab_frame.height <= 0)) {
      if (sar > 1.0) {
        frame->grab_frame.width  = sar * width + 0.5;
        frame->grab_frame.height = height;
      } else {
        frame->grab_frame.width  = width;
        frame->grab_frame.height = (double)height / sar + 0.5;
      }
    } else if (frame->grab_frame.width <= 0)
      frame->grab_frame.width = frame->grab_frame.height * width * sar / height + 0.5;
    else if (frame->grab_frame.height <= 0)
      frame->grab_frame.height = (frame->grab_frame.width * height) / (sar * width) + 0.5;
  }

  /* allocate grab frame image buffer */
  if (frame->grab_frame.width != frame->grab_width || frame->grab_frame.height != frame->grab_height) {
    _x_freep(&frame->grab_frame.img);
  }
  if (frame->grab_frame.img == NULL) {
    frame->grab_frame.img = (uint8_t *) calloc(frame->grab_frame.width * frame->grab_frame.height, 3);
    if (frame->grab_frame.img == NULL) {
      vo_frame_dec_lock(vo_frame);
      return -1; /* error happened */
    }
  }

  /* initialize yuv2rgb factory */
  if (!frame->yuv2rgb_factory) {
    int cm = VO_GET_FLAGS_CM (vo_frame->flags);
    frame->yuv2rgb_factory = yuv2rgb_factory_init(MODE_24_RGB, 0, NULL);
    if (!frame->yuv2rgb_factory) {
      vo_frame_dec_lock(vo_frame);
      return -1; /* error happened */
    }
    if ((cm >> 1) == 2) /* color matrix undefined */
      cm = (cm & 1) |
        ((vo_frame->height - vo_frame->crop_top - vo_frame->crop_bottom >= 720) ||
         (vo_frame->width - vo_frame->crop_left - vo_frame->crop_right >= 1280) ? 2 : 10);
    else if ((cm >> 1) == 0) /* converted RGB source, always ITU 601 */
      cm = (cm & 1) | 10;
    frame->yuv2rgb_factory->set_csc_levels (frame->yuv2rgb_factory, 0, 128, 128, cm);
  }

  /* retrieve a yuv2rgb converter */
  if (!frame->yuv2rgb) {
    frame->yuv2rgb = frame->yuv2rgb_factory->create_converter(frame->yuv2rgb_factory);
    if (!frame->yuv2rgb) {
      vo_frame_dec_lock(vo_frame);
      return -1; /* error happened */
    }
  }

  /* configure yuv2rgb converter */
  if (width != frame->vo_width ||
        height != frame->vo_height ||
        frame->grab_frame.width != frame->grab_width ||
        frame->grab_frame.height != frame->grab_height ||
        y_stride != frame->y_stride ||
        uv_stride != frame->uv_stride) {
    frame->vo_width = width;
    frame->vo_height = height;
    frame->grab_width = frame->grab_frame.width;
    frame->grab_height = frame->grab_frame.height;
    frame->y_stride = y_stride;
    frame->uv_stride = uv_stride;
    frame->yuv2rgb->configure(frame->yuv2rgb, width, height, y_stride, uv_stride, frame->grab_width, frame->grab_height, frame->grab_width * 3);
  }

  /* convert YUV to RGB image taking possible scaling into account */
  if(format == XINE_IMGFMT_YV12)
    frame->yuv2rgb->yuv2rgb_fun(frame->yuv2rgb, frame->grab_frame.img, base[0], base[1], base[2]);
  else
    frame->yuv2rgb->yuy22rgb_fun(frame->yuv2rgb, frame->grab_frame.img, base[0]);

  vo_frame_dec_lock(vo_frame);
  return 0;
}


static xine_grab_video_frame_t *vo_new_grab_video_frame(xine_video_port_t *this_gen)
{
  vos_grab_video_frame_t *frame = calloc(1, sizeof(vos_grab_video_frame_t));
  if (frame) {
    frame->grab_frame.dispose = vo_dispose_grab_video_frame;
    frame->grab_frame.grab = vo_grab_grab_video_frame;
    frame->grab_frame.vpts = -1;
    frame->grab_frame.timeout = XINE_GRAB_VIDEO_FRAME_DEFAULT_TIMEOUT;
    frame->video_port = this_gen;
  }
  return (xine_grab_video_frame_t *)frame;
}


/* Use this after rendering a live frame (not for still frame duplicates). */
static void vo_grab_current_frame (vos_t *this, vo_frame_t *vo_frame, int64_t vpts)
{
  pthread_mutex_lock(&this->grab_lock);

  /* hold current frame for still frame generation and snapshot feature */
  if (this->last_frame)
    vo_frame_dec_lock(this->last_frame);
  this->last_frame = vo_frame;

  /* process grab queue */
  vos_grab_video_frame_t *frame = this->pending_grab_request;
  if (frame) {
    do {
      vos_grab_video_frame_t *next;
      if (frame->vo_frame)
        vo_frame_dec_lock(frame->vo_frame);
      frame->vo_frame = NULL;

      if (vo_frame->format == XINE_IMGFMT_YV12 || vo_frame->format == XINE_IMGFMT_YUY2 || vo_frame->proc_provide_standard_frame_data) {
        vo_frame_inc_lock(vo_frame);
        frame->vo_frame = vo_frame;
        frame->grab_frame.vpts = vpts;
      }

      frame->finished = 1;
      next = frame->next;
      frame->next = NULL;
      frame = next;
    } while (frame);

    this->pending_grab_request = NULL;
    pthread_cond_broadcast(&this->grab_cond);
  }

  pthread_mutex_unlock(&this->grab_lock);
}


/* call vo_driver->proc methods for the entire frame */
static void vo_frame_driver_proc(vo_frame_t *img)
{
  if (img->proc_frame) {
    img->proc_frame(img);
  }
  if (img->proc_called) return;

  if (img->proc_slice) {
    int height = img->height;
    uint8_t* src[3];

    switch (img->format) {
    case XINE_IMGFMT_YV12:
      src[0] = img->base[0];
      src[1] = img->base[1];
      src[2] = img->base[2];
      while ((height -= 16) > -16) {
        img->proc_slice(img, src);
        src[0] += 16 * img->pitches[0];
        src[1] +=  8 * img->pitches[1];
        src[2] +=  8 * img->pitches[2];
      }
      break;
    case XINE_IMGFMT_YUY2:
      src[0] = img->base[0];
      while ((height -= 16) > -16) {
        img->proc_slice(img, src);
        src[0] += 16 * img->pitches[0];
      }
      break;
    }
  }
}


/********************************************************************
 * called by video decoder:                                         *
 * get_frame  => alloc frame for rendering                          *
 * frame_draw => queue finished frame for display                   *
 * frame_free => frame no longer used as reference frame by decoder *
 *******************************************************************/

static vo_frame_t *vo_get_frame (xine_video_port_t *this_gen,
				 uint32_t width, uint32_t height,
				 double ratio, int format,
				 int flags) {

  vo_frame_t *img;
  vos_t      *this = (vos_t *) this_gen;

  lprintf ("get_frame (%d x %d)\n", width, height);

  while (1) {

    img = vo_free_queue_get (this, width, height, ratio, format, flags);

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
    img->crop_left      = 0;
    img->crop_right     = 0;
    img->crop_top       = 0;
    img->crop_bottom    = 0;
    img->overlay_offset_x = 0;
    img->overlay_offset_y = 0;
    img->stream         = NULL;

    _x_extra_info_reset ( img->extra_info );

    /* let driver ensure this image has the right format */

    this->driver->update_frame_format (this->driver, img, width, height,
                                       ratio, format, flags);

    pthread_mutex_unlock (&img->mutex);

    if (!width || img->width)
      break;

    xprintf (this->xine, XINE_VERBOSITY_LOG,
      _("video_out: found an unusable frame (%dx%d, format %0x08x) - no memory??\n"),
      width, height, img->format);
    img->lock_counter = 0;
    vo_queue_append (&this->free_img_buf_queue, img);

    /* check if we're allowed to return NULL */
    if (flags & VO_GET_FRAME_MAY_FAIL)
      return NULL;
  }

  /* update frame usage stats. No need to lock queues for that I guess :-) */
  {
    int frames_used;
    frames_used = this->frames_total;
    frames_used -= this->free_img_buf_queue.num_buffers;
    frames_used -= this->display_img_buf_queue.num_buffers;
    frames_used -= this->ready_num;
    frames_used += this->frames_extref;
    if (frames_used > this->frames_peak_used)
      this->frames_peak_used = frames_used;
  }

  lprintf ("get_frame (%d x %d) done\n", width, height);

  return img;
}

static int vo_frame_draw (vo_frame_t *img, xine_stream_t *stream) {

  vos_t         *this = (vos_t *) img->port;
  int            frames_to_skip, first_frame_flag = 0;

  img->stream = NULL;

  if (this->discard_frames) {
    /* Now that we have the auto gapless switch it should be safe to always drop here. */
    lprintf ("i'm in flush mode, not appending this frame to queue\n");
    return 0;
  }

  /* handle anonymous streams like NULL for easy checking */
  if (stream == XINE_ANON_STREAM) stream = NULL;

  if (stream) {
    first_frame_flag = stream->first_frame_flag;
    if (first_frame_flag >= 2) {
      /* Frame reordering and/or multithreaded deoders feature an initial delay.
       * Even worse: mpeg-ts does not seek to keyframes, as that would need quite some 
       * decoder knowledge. As a result, there often burst in many "bad" frames here quickly.
       * Dont let these mess up timing, generate high frames_to_skip values,
       * and kill the following keyframe seq with that.
       */
      this->last_delivery_pts = 0;
      if (img->bad_frame) {
        this->num_frames_burst++;
        return 0;
      }
      if (this->num_frames_burst) {
        xprintf (this->xine, XINE_VERBOSITY_DEBUG,
          "video_out: dropped %d bad frames after seek.\n", this->num_frames_burst);
        this->num_frames_burst = 0;
      }
    }
    img->stream = stream;
    _x_extra_info_merge( img->extra_info, stream->video_decoder_extra_info );
    stream->metronom->got_video_frame (stream->metronom, img);
#if 0
    if (FIXME: IS_KEYFRAME (img)) {
      if (this->keyframe_mode == 0) {
        if (!stream->index_array && stream->input_plugin && INPUT_IS_SEEKABLE (stream->input_plugin)) {
          xprintf (stream->xine, XINE_VERBOSITY_DEBUG,
            "video_out: no keyframe index found, lets do it from this side.\n");
          this->keyframe_mode = 1;
        } else {
          this->keyframe_mode = -1;
        }
      }
      if (this->keyframe_mode > 0) {
        xine_keyframes_entry_t entry;
        entry.msecs = img->extra_info->input_time;
        entry.normpos = img->extra_info->input_normpos;
        _x_keyframes_add (stream, &entry);
      }
    }
#endif
  }
  vo_reref (this, img);
  this->current_width = img->width;
  this->current_height = img->height;
  this->current_duration = img->duration;

  if (!this->grab_only) {

    img->extra_info->vpts = img->vpts;

    this->last_delivery_pts = this->clock->get_current_time (this->clock);

    lprintf ("got image at master vpts %" PRId64 ". vpts for picture is %" PRId64 " (pts was %" PRId64 ")\n",
      this->last_delivery_pts, img->vpts, img->pts);

    this->num_frames_delivered++;

    /* Frame dropping slow start:
     *   The engine starts to drop frames if there are less than frame_drop_limit
     *   frames in advance. There might be a problem just after a seek because
     *   there is no frame in advance yet.
     *   The following code increases progressively the frame_drop_limit (-2 -> 3)
     *   after a seek to give a chance to the engine to display the first frames
     *   smoothly before starting to drop frames if the decoder is really too
     *   slow.
     *   The above numbers are the result of frame_drop_limit_max beeing 3. They
     *   will be (-4 -> 1) when frame_drop_limit_max is only 1. This maximum value
     *   depends on the number of video buffers which the output device provides.
     */
    if (first_frame_flag >= 2)
      this->frame_drop_cpt = 10;

    if (this->frame_drop_cpt) {
      this->frame_drop_limit = this->frame_drop_limit_max - (this->frame_drop_cpt >> 1);
      this->frame_drop_cpt--;
    }

    /* do not skip decoding until output fifo frames are consumed */
    if (this->display_img_buf_queue.num_buffers + this->ready_num < this->frame_drop_limit) {
      int duration = img->duration > 0 ? img->duration : DEFAULT_FRAME_DURATION;
      frames_to_skip = (this->last_delivery_pts - img->vpts) / duration;
      frames_to_skip = (frames_to_skip + this->frame_drop_limit) * 2;
      if (frames_to_skip < 0)
        frames_to_skip = 0;
    } else {
      frames_to_skip = 0;
    }

    /* Do not drop frames immediately, but remember this as suggestion and give
     * decoder a further chance to supply frames.
     * This avoids unnecessary frame drops in situations where there is only
     * a very little number of image buffers, e. g. when using xxmc.
     */
    if (!frames_to_skip) {
      this->frame_drop_suggested = 0;
    } else {
      if (!this->frame_drop_suggested) {
        this->frame_drop_suggested = 1;
        frames_to_skip = 0;
      }
    }

    lprintf ("delivery diff : %" PRId64 ", current vpts is %" PRId64 ", %d frames to skip\n",
      img->vpts - this->last_delivery_pts, this->last_delivery_pts, frames_to_skip);

  } else {
    frames_to_skip = 0;
  }


  if (!img->bad_frame) {

    int img_already_locked = 0;

    /* add cropping requested by frontend */
    img->crop_left   = (img->crop_left + this->crop_left) & ~1;
    img->crop_right  = (img->crop_right + this->crop_right) & ~1;
    img->crop_top    += this->crop_top;
    img->crop_bottom += this->crop_bottom;

    /* perform cropping when vo driver does not support it */
    if( (img->crop_left || img->crop_top ||
         img->crop_right || img->crop_bottom) &&
        (this->grab_only ||
         !(this->driver->get_capabilities (this->driver) & VO_CAP_CROP)) ) {
      if (img->format == XINE_IMGFMT_YV12 || img->format == XINE_IMGFMT_YUY2) {
        img->overlay_offset_x -= img->crop_left;
        img->overlay_offset_y -= img->crop_top;
        img = crop_frame( img->port, img );
        img->lock_counter = 2;
        img_already_locked = 1;
      } else {
	/* noone knows how to crop this, so we can only ignore the cropping */
	img->crop_left   = 0;
	img->crop_top    = 0;
	img->crop_right  = 0;
	img->crop_bottom = 0;
      }
    }

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
    if (first_frame_flag >= 2) {
      /* We can always do the frame's native stream here.
       * We know its there, and we vo_reref()ed it above.
       */
      xine_stream_t *s = stream;
      pthread_mutex_lock (&s->first_frame_lock);
      if (s->first_frame_flag >= 2) {
        if ((s->first_frame_flag > 2) || this->grab_only) {
          s->first_frame_flag = 0;
          pthread_cond_broadcast (&s->first_frame_reached);
        } else {
          s->first_frame_flag = 1;
        }
        img->is_first = FIRST_FRAME_MAX_POLL;
        lprintf ("get_next_video_frame first_frame_reached\n");
      }
      pthread_mutex_unlock (&s->first_frame_lock);
    }
    /* avoid a complex deadlock situation caused by net_buf_control */
    if (!xine_rwlock_tryrdlock (&this->streams_lock)) {
      xine_stream_t **s;
      for (s = this->streams; *s; s++) {
        if (*s == stream)
          continue;
        /* a little speedup */
        if ((*s)->first_frame_flag < 2)
          continue;
        pthread_mutex_lock (&(*s)->first_frame_lock);
        if ((*s)->first_frame_flag >= 2) {
          if (((*s)->first_frame_flag > 2) || this->grab_only) {
            (*s)->first_frame_flag = 0;
            pthread_cond_broadcast (&(*s)->first_frame_reached);
          } else {
            (*s)->first_frame_flag = 1;
          }
          img->is_first = FIRST_FRAME_MAX_POLL;
          lprintf ("get_next_video_frame first_frame_reached\n");
        }
        pthread_mutex_unlock (&(*s)->first_frame_lock);
      }
      xine_rwlock_unlock (&this->streams_lock);
    }

    if (!img_already_locked)
      vo_frame_inc2_lock (img);
    vo_queue_append (&this->display_img_buf_queue, img);

    if (img->is_first && (this->display_img_buf_queue.first == img)) {
      /* wake up render thread */
      pthread_mutex_lock (&this->trigger_drawing_mutex);
      this->trigger_drawing = 1;
      pthread_cond_signal (&this->trigger_drawing_cond);
      pthread_mutex_unlock (&this->trigger_drawing_mutex);
    }

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

  if (this->num_frames_delivered == 200) {
    int send_event;
    xine_stream_t **it;

    /* 100 * n / num_frames_delivered */
    if (((this->num_frames_skipped >> 1)   > this->warn_skipped_threshold) ||
        ((this->num_frames_discarded >> 1) > this->warn_discarded_threshold))
      this->warn_threshold_exceeded++;
    else
      this->warn_threshold_exceeded = 0;

    /* make sure threshold has being consistently exceeded - 5 times in a row
     * (that is, this is not just a small burst of dropped frames).
     */
    send_event = (this->warn_threshold_exceeded == 5 &&
                  !this->warn_threshold_event_sent);
    this->warn_threshold_event_sent = send_event;

    xine_rwlock_rdlock (&this->streams_lock);
    for (it = this->streams; *it; it++) {
      int skipped, discarded;
      stream = *it;

      /* 1000 * n / num_frames_delivered */
      skipped   = 5 * this->num_frames_skipped;
      discarded = 5 * this->num_frames_discarded;
      _x_stream_info_set (stream, XINE_STREAM_INFO_SKIPPED_FRAMES, skipped);
      _x_stream_info_set (stream, XINE_STREAM_INFO_DISCARDED_FRAMES, discarded);

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
         data.skipped_frames = skipped;
         data.skipped_threshold = this->warn_skipped_threshold * 10;
         data.discarded_frames = discarded;
         data.discarded_threshold = this->warn_discarded_threshold * 10;
         xine_event_send(stream, &event);
      }
    }
    xine_rwlock_unlock (&this->streams_lock);


    if( this->num_frames_skipped || this->num_frames_discarded ) {
      xine_log(this->xine, XINE_LOG_MSG,
	       _("%d frames delivered, %d frames skipped, %d frames discarded\n"),
               200,
	       this->num_frames_skipped, this->num_frames_discarded);
    }

    this->num_frames_delivered = 0;
    this->num_frames_discarded = 0;
    this->num_frames_skipped   = 0;
  }

  return frames_to_skip;
}


/********************************************************************
 * video out loop related                                           *
 *******************************************************************/

/* duplicate_frame(): this function is used to keep playing frames
 * while video is still or player paused.
 *
 * frame allocation inside vo loop is dangerous:
 * we must never wait for a free frame -> deadlock condition.
 * to avoid deadlocks we don't use vo_free_queue_get ()
 * but vo_*_get_dupl () instead.
 */
static vo_frame_t *duplicate_frame (vos_t *this, vo_frame_t *img) {
  vo_frame_t *dupl;

  if (!img)
    return NULL;

  dupl = vo_free_get_dupl (this, img);
  if (!dupl) {
    /* OK we run out of free frames. Try to whistle back a frame already waiting for display.
       Search for one that is _not_ a DR1 reference frame that the decoder wants unchanged */
    vo_ready_refill (this);
    dupl = vo_ready_get_dupl (this, img);
    if (!dupl)
      return NULL;
  }

  pthread_mutex_lock (&dupl->mutex);
  dupl->lock_counter   = 1;
  dupl->width          = img->width;
  dupl->height         = img->height;
  dupl->ratio          = img->ratio;
  dupl->format         = img->format;
  dupl->flags          = img->flags | VO_BOTH_FIELDS;
  dupl->progressive_frame  = img->progressive_frame;
  dupl->repeat_first_field = img->repeat_first_field;
  dupl->top_field_first    = img->top_field_first;
  dupl->crop_left      = img->crop_left;
  dupl->crop_right     = img->crop_right;
  dupl->crop_top       = img->crop_top;
  dupl->crop_bottom    = img->crop_bottom;
  dupl->overlay_offset_x = img->overlay_offset_x;
  dupl->overlay_offset_y = img->overlay_offset_y;

  dupl->stream = img->stream;
  vo_reref (this, dupl);

  this->driver->update_frame_format (this->driver, dupl, dupl->width, dupl->height,
				     dupl->ratio, dupl->format, dupl->flags);

  pthread_mutex_unlock (&dupl->mutex);

  if (img->width && !dupl->width) {
    /* driver failed to set up render space */
    xprintf (this->xine, XINE_VERBOSITY_LOG,
      _("video_out: found an unusable frame (%dx%d, format %0x08x) - no memory??\n"),
      img->width, img->height, img->format);
    dupl->lock_counter = 0;
    vo_queue_append (&this->free_img_buf_queue, dupl);
    return NULL;
  }

  if (dupl->proc_duplicate_frame_data) {
    dupl->proc_duplicate_frame_data(dupl,img);
  } else {

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
  }

  dupl->bad_frame   = 0;
  dupl->pts         = 0;
  dupl->vpts        = 0;
  dupl->proc_called = 0;

  dupl->duration  = img->duration;
  dupl->is_first  = 0;

  /* extra info is thrown away, because it is not up to date */
  _x_extra_info_reset (dupl->extra_info);
  dupl->future_frame = NULL;

  /* delay frame processing for now, we might not even need it (eg. frame will be discarded) */
  /* vo_frame_driver_proc(dupl); */

  return dupl;
}

static void check_redraw_needed (vos_t *this, int64_t vpts) {

  if (this->overlay_source) {
    if( this->overlay_source->redraw_needed (this->overlay_source, vpts) )
      this->redraw_needed = 1;
  }

  /* calling the frontend's frame output hook (via driver->redraw_needed () here)
   * while flushing (xine_stop ()) may freeze.
   */
  if (this->discard_frames && (this->ready_first || this->display_img_buf_queue.first))
    return;

  if( this->driver->redraw_needed (this->driver) )
    this->redraw_needed = 1;
}

static vo_frame_t *next_frame (vos_t *this, int64_t *vpts) {

  vo_frame_t   *img;

  /* when flushing, drop everything now, and return latest "first" frame if any.
   * FIXME: when switching from movie to logo, somebody briefly flashes VO_PROP_DISCARD_FRAMES.
   * This kills up to num_frames from the end. For now, just forget the flush there.
   * That happens here automagically because we come here late ;-)
   * FIXED: by auto gapless switch.
   */
  if (this->discard_frames) {
    vo_frame_t *keep[2], *freelist = NULL, **add = &freelist;
    int n = 0, a = 0;
    keep[0] = NULL;
    keep[1] = this->last_flushed;
    /* Take out all at once, but keep decoder blocked for now. */
    img = vo_ready_get_all (this);
    /* Scan for stuff we want to keep. */
    while (img) {
      vo_frame_t *f = keep[(img->is_first <= 0)];
      n++;
      if (f) {
        if (!vo_frame_dec2_lock_int (this, f)) {
          *add = f;
          add = &f->next;
          a++;
        }
      }
      keep[(img->is_first <= 0)] = img;
      img = img->next;
    }
    this->last_flushed = keep[1];
    if (this->last_flushed) {
      this->last_flushed->next = NULL;
      if (!this->last_frame) { /* rare late setting */
        vo_frame_inc_lock (this->last_flushed);
        pthread_mutex_lock (&this->grab_lock);
        this->last_frame = this->last_flushed;
        pthread_mutex_unlock (&this->grab_lock);
      }
    }
    /* Override with first frame. */
    if (keep[0]) {
      keep[0]->vpts = *vpts;
      keep[0]->next = NULL;
      keep[0]->future_frame = NULL;
    }
    /* Free (almost) all at once. */
    *add = NULL;
    vo_free_append_list (this, freelist, add, a);
    /* Make sure clients dont miss this. */
    if (this->need_flush_signal) {
      pthread_mutex_lock (&this->display_img_buf_queue.mutex);
      pthread_cond_broadcast (&this->done_flushing);
      pthread_mutex_unlock (&this->display_img_buf_queue.mutex);
    }
    /* Report success. */
    if (n) {
      xprintf (this->xine, XINE_VERBOSITY_DEBUG,
        "video_out: flushed out %d frames (now=%"PRId64", discard=%d).\n",
        n, *vpts, this->discard_frames);
    }
    this->redraw_needed = 0;
    *vpts = 0;
    return keep[0];
  }

  ADD_READY_FRAMES;
  img = this->ready_first;

  while (img) {

    if (img->is_first > 0) {
#ifdef LOG_FLUSH
      printf ("video_out: first frame pts=%"PRId64", now=%"PRId64", discard=%d\n",
        img->vpts, *vpts, this->discard_frames);
#endif
      /* The user seek brake feature: display first frame after seek right now
       * (without "metronom prebuffering") if
       * - it is either no longer referenced by decoder or post layer, or
       * - it is due for display naturally, or
       * - we have waited too long for that to happen.
       * This shall do 3 things:
       * - User sees the effect of seek soon, and
       * - We dont decode too many frames in vain when there is a new seek, and
       * - We dont drop frames already decoded in time.
       * Finally, dont zero img->is_first so xine_play () gets woken up properly.
       */
      if ((img->lock_counter <= 2) || (img->vpts <= *vpts) || (img->is_first == 1)) {
        img->vpts = *vpts;
        *vpts = img->vpts + (img->duration ? img->duration : DEFAULT_FRAME_DURATION);
        break;
      }
      /* poll */
      lprintf ("frame still referenced %d times, is_first=%d\n", img->lock_counter, img->is_first);
      img->is_first--;
      *vpts += FIRST_FRAME_POLL_DELAY;
      /* At forward seek, fill the gap with last flushed frame if any. */
      if (this->last_flushed && img->pts && this->last_flushed->pts && (this->last_flushed->pts < img->pts)) {
        img = this->last_flushed;
        this->last_flushed = NULL;
        img->vpts = *vpts;
        *vpts = img->vpts + (img->duration ? img->duration : DEFAULT_FRAME_DURATION);
        return img;
      }
      this->wakeups_early++;
      return NULL;
    }

    {
      int64_t diff = *vpts - img->vpts, duration;
      if (diff < 0) {
        /* still too early for this frame */
        *vpts = img->vpts;
        this->wakeups_early++;
        return NULL;
      }
      duration = img->duration ? img->duration
        : (img->next ? img->next->vpts - img->vpts : DEFAULT_FRAME_DURATION);
      if (diff <= duration) {
        /* OK, show this one */
        *vpts = img->vpts + duration;
        break;
      }
      xine_log (this->xine, XINE_LOG_MSG,
        _("video_out: throwing away image with pts %" PRId64 " because it's too old (diff : %" PRId64 ").\n"),
        img->vpts, diff);
    }

    this->num_frames_discarded++;

    img = vo_ready_pop (this);

    if (img->stream) {
      pthread_mutex_lock (&img->stream->current_extra_info_lock);
      _x_extra_info_merge (img->stream->current_extra_info, img->extra_info);
      pthread_mutex_unlock (&img->stream->current_extra_info_lock);
    }

    ADD_READY_FRAMES;

    /* last frame? back it up for still frame creation */
    if (!this->ready_first) {
      pthread_mutex_lock (&this->grab_lock);
      if (this->last_frame) {
        lprintf ("overwriting frame backup\n");
        vo_frame_dec_lock (this->last_frame);
      }
      lprintf ("possible still frame (old)\n");
      this->last_frame = img;
      pthread_mutex_unlock (&this->grab_lock);
      vo_frame_dec_lock (img);
      /* wait 4 frames before drawing this one. this allow slower systems to recover. */
      this->redraw_needed = 4;
    } else {
      vo_frame_dec2_lock (this, img);
    }

    img = this->ready_first;
  }

  if (img) {
    /* remove frame from display queue and show it */
    img->future_frame = img->next;
    img = vo_ready_pop (this);
    /* we dont need that filler anymore */
    if (this->last_flushed) {
      vo_frame_dec2_lock (this, this->last_flushed);
      this->last_flushed = NULL;
    }
    return img;
  }
    
  lprintf ("no frame\n");
  check_redraw_needed (this, *vpts);
  *vpts = 0;
  return NULL;
}

static void overlay_and_display_frame (vos_t *this, vo_frame_t *img, int64_t vpts) {

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
    /* Always post first frame time to make frontend relative seek work. */
    if ((diff > 3000) || (diff<-300000) || (img->is_first > 0))
      _x_extra_info_merge( img->stream->current_extra_info, img->extra_info );
    pthread_mutex_unlock( &img->stream->current_extra_info_lock );
    /* First frame's native stream is the most common case.
     * Do it without streams lock.
     */
    if (img->is_first > 0) {
      xine_stream_t *s = img->stream;
      pthread_mutex_lock (&s->first_frame_lock);
      if (s->first_frame_flag) {
        s->first_frame_flag = 0;
        pthread_cond_broadcast (&s->first_frame_reached);
      }
      pthread_mutex_unlock (&s->first_frame_lock);
    }
  }

  /* xine_play() may be called from a thread that has the display device locked
   * (eg an X window event handler). If it is waiting for a frame we better wake
   * it up _before_ we start displaying, or the first 10 seconds of video are lost.
   */
  if (img->is_first > 0) {
    xine_stream_t **s;
    xine_rwlock_rdlock (&this->streams_lock);
    for (s = this->streams; *s; s++) {
      if (*s == img->stream)
        continue;
      pthread_mutex_lock (&(*s)->first_frame_lock);
      if ((*s)->first_frame_flag) {
        (*s)->first_frame_flag = 0;
        pthread_cond_broadcast (&(*s)->first_frame_reached);
      }
      pthread_mutex_unlock (&(*s)->first_frame_lock);
    }
    xine_rwlock_unlock (&this->streams_lock);
    /* Dont signal the same frame again. */
    img->is_first = -1;
  }

  /* calling the frontend's frame output hook (via driver->display_frame () here)
   * while flushing (xine_stop ()) may freeze.
   */
  if (this->discard_frames && (this->ready_first || this->display_img_buf_queue.first)) {
    img->free (img);
    this->redraw_needed = 0;
    return;
  }

  if (this->overlay_source) {
    this->overlay_source->multiple_overlay_blend (this->overlay_source,
						  vpts,
						  this->driver, img,
						  this->video_loop_running && this->overlay_enabled);
  }

  this->driver->display_frame (this->driver, img);

  this->redraw_needed = 0;
}

/* special loop for paused mode
 * needed to update screen due overlay changes, resize, window
 * movement, brightness adjusting etc.
 */
static void paused_loop( vos_t *this, int64_t vpts )
{
  /* prevent decoder thread from allocating too many new frames */
  vo_queue_read_lock (&this->free_img_buf_queue);

  while (this->clock->speed == XINE_SPEED_PAUSE && this->video_loop_running) {

    ADD_READY_FRAMES;

    /* set last_frame to play the same frame several times */
    if (!this->last_frame) {
      vo_frame_t *f;
      f = this->ready_first;
      if (f) {
        vo_frame_inc_lock (f);
        pthread_mutex_lock (&this->grab_lock);
        this->last_frame = f;
        pthread_mutex_unlock (&this->grab_lock);
        this->redraw_needed = 1;
      }
    }

    /* what a terrible HACK.
     * For single step mode, keep the engine paused all the time, as audio out
     * seems unable to pause that quickly. Instead, advance manually and nudge
     * that master clock. After this, audio out will get this message as well.
     */
    {
      vo_frame_t *f = NULL;

      if (this->step) {
        pthread_mutex_lock (&this->trigger_drawing_mutex);
        if (this->step) {
          if (this->ready_first) {
            f = vo_ready_pop (this);
            f->future_frame = this->ready_first;
            this->step = 0;
            pthread_cond_broadcast (&this->done_stepping);
          }
        }
        pthread_mutex_unlock (&this->trigger_drawing_mutex);

        if (f) {
          vpts = f->vpts;
          this->clock->adjust_clock (this->clock, vpts);
          xprintf (this->xine, XINE_VERBOSITY_DEBUG,
            "video_out: SINGLE_STEP: vpts %"PRId64".\n", vpts);
          overlay_and_display_frame (this, f, vpts);
          vo_grab_current_frame (this, f, vpts);
        }
      }

      /* refresh output */
      if (!f) {
        check_redraw_needed (this, vpts);
        if (this->redraw_needed) {
          f = duplicate_frame (this, this->last_frame);
          if (f) {
            f->vpts = vpts;
            overlay_and_display_frame (this, f, vpts);
          }
        }
      }
    }

    /* wait for 1/50s or wakeup */
    this->now.tv_nsec += 20000000;
    if (this->now.tv_nsec >= 1000000000) {
      /* resyncing the pause clock every second should be enough ;-) */
      xine_gettime (&this->now);
      this->now.tv_nsec += 20000000;
      if (this->now.tv_nsec >= 1000000000) {
        this->now.tv_sec++;
        this->now.tv_nsec -= 1000000000;
      }
    }
    pthread_mutex_lock (&this->trigger_drawing_mutex);
    if (!this->trigger_drawing) {
      struct timespec ts = this->now;
      pthread_cond_timedwait (&this->trigger_drawing_cond, &this->trigger_drawing_mutex, &ts);
    }
    if (this->trigger_drawing) {
      this->trigger_drawing = 0;
      this->redraw_needed = 1;
      /* no timeout, resync clock */
      this->now.tv_nsec = 990000000;
    }
    pthread_mutex_unlock (&this->trigger_drawing_mutex);

    /* flush when requested */
    if (this->discard_frames) {
      vo_frame_t *img = vo_ready_get_all (this);
      vo_list_flush (this, img);
      if (this->need_flush_signal) {
        pthread_mutex_lock (&this->display_img_buf_queue.mutex);
        pthread_cond_broadcast (&this->done_flushing);
        pthread_mutex_unlock (&this->display_img_buf_queue.mutex);
      }
    }
  }

  vo_queue_read_unlock (&this->free_img_buf_queue);
}

static void video_out_update_disable_flush_from_video_out(void *disable_decoder_flush_from_video_out, xine_cfg_entry_t *entry)
{
  *(int *)disable_decoder_flush_from_video_out = entry->num_value;
}

static void *video_out_loop (void *this_gen) {
  vos_t *this = (vos_t *) this_gen;
  int    disable_decoder_flush_from_video_out;

#ifndef WIN32
  errno = 0;
  if (nice(-2) == -1 && errno)
    xine_log(this->xine, XINE_LOG_MSG, "video_out: can't raise nice priority by 2: %s\n", strerror(errno));
#endif /* WIN32 */

  disable_decoder_flush_from_video_out = this->xine->config->register_bool(this->xine->config, "engine.decoder.disable_flush_from_video_out", 0,
      _("disable decoder flush from video out"),
      _("video out causes a decoder flush when video out runs out of frames for displaying,\n"
        "because the decoder hasn't deliverd new frames for quite a while.\n"
        "flushing the decoder causes decoding errors for images decoded after the flush.\n"
        "to avoid the decoding errors, decoder flush at video out should be disabled.\n\n"
        "WARNING: as the flush was introduced to fix some issues when playing DVD still images, it is\n"
        "likely that these issues may reappear in case they haven't been fixed differently meanwhile.\n"),
        20, video_out_update_disable_flush_from_video_out, &disable_decoder_flush_from_video_out);

  /*
   * here it is - the heart of xine (or rather: one of the hearts
   * of xine) : the video output loop
   */

  lprintf ("loop starting...\n");

  while ( this->video_loop_running ) {
    int64_t vpts, next_frame_vpts;
    int64_t usec_to_sleep;

    /* record current time as both speed dependent virtual presentation timestamp (vpts)
     * and absolute system time, and hope these are halfway in sync.
     */
    vpts = next_frame_vpts = this->clock->get_current_time (this->clock);
    xine_gettime (&this->now);
    lprintf ("loop iteration at %" PRId64 "\n", vpts);

    this->wakeups_total++;

    {
      /* find frame to display */
      vo_frame_t *img = next_frame (this, &next_frame_vpts);
      /* if we have found a frame, display it */
      if (img) {
        lprintf ("displaying frame (id=%d)\n", img->id);
        overlay_and_display_frame (this, img, vpts);
        vo_grab_current_frame (this, img, vpts);
      } else if (this->redraw_needed) {
        if (this->last_frame && (this->redraw_needed == 1)) {
          lprintf ("generating still frame (vpts = %" PRId64 ") \n", vpts);
          /* keep playing still frames */
          img = duplicate_frame (this, this->last_frame);
          if (img) {
            img->vpts = vpts;
            overlay_and_display_frame (this, img, vpts);
          }
        } else {
          lprintf ("no frame, but no backup frame\n");
          this->redraw_needed--;
        }
      }
    }

    /*
     * if we haven't heared from the decoder for some time
     * flush it
     * test display fifo empty to protect from deadlocks
     */

    if ((vpts - this->last_delivery_pts > 30000) &&
        !this->display_img_buf_queue.first && !this->ready_first) {
      if (this->last_delivery_pts && !disable_decoder_flush_from_video_out) {
        xine_stream_t **s;
        xine_rwlock_rdlock (&this->streams_lock);
        for (s = this->streams; *s; s++) {
          if ((*s)->video_decoder_plugin && (*s)->video_fifo) {
            buf_element_t *buf;
            lprintf ("flushing current video decoder plugin\n");
            buf = (*s)->video_fifo->buffer_pool_try_alloc ((*s)->video_fifo);
            if (buf) {
              buf->type = BUF_CONTROL_FLUSH_DECODER;
              (*s)->video_fifo->insert ((*s)->video_fifo, buf);
            }
          }
        }
        xine_rwlock_unlock (&this->streams_lock);
      }
      this->last_delivery_pts = vpts;
    }

    /* now the time critical stuff is done */
    ADD_READY_FRAMES;

    /*
     * wait until it's time to display next frame
     */

    lprintf ("next_frame_vpts is %" PRId64 "\n", next_frame_vpts);
    if ((next_frame_vpts - vpts) > 2 * 90000) {
      xprintf (this->xine, XINE_VERBOSITY_DEBUG,
      "video_out: vpts/clock error, next_vpts=%" PRId64 " cur_vpts=%" PRId64 "\n", next_frame_vpts, vpts);
      if (this->ready_first && this->ready_first->next) {
        int64_t d = this->ready_first->next->vpts - vpts;
        if ((d >= 0) && (d <= 2 * 90000)) {
          d = (d >> 1) + vpts;
          xprintf (this->xine, XINE_VERBOSITY_DEBUG,
            "video_out: looks like a missed decoder flush, fixing next_vpts to %" PRId64 ".\n", d);
          this->ready_first->vpts = d;
          next_frame_vpts = d;
        }
      }
    }

    /* get diff time for next iteration */
    if (next_frame_vpts && this->clock->speed > 0)
      usec_to_sleep = (next_frame_vpts - vpts) * 100 * XINE_FINE_SPEED_NORMAL / (9 * this->clock->speed);
    else
      /* we don't know when the next frame is due, only wait a little */
      usec_to_sleep = 20000;

    while (this->video_loop_running) {
      int timedout, wait;

      if (this->discard_frames && (this->ready_first || this->display_img_buf_queue.first))
        break;

      if (this->clock->speed == XINE_SPEED_PAUSE) {
        paused_loop (this, vpts);
        break;
      }

      /* limit usec_to_sleep to maintain responsiveness */
      wait = usec_to_sleep;
      if (wait <= 0)
        break;
      if (wait > MAX_USEC_TO_SLEEP)
        wait = MAX_USEC_TO_SLEEP;

      lprintf ("%d usec to sleep at master vpts %" PRId64 "\n", wait, vpts);

      /* next stop absolute time */
      this->now.tv_nsec += wait * 1000;
      if (this->now.tv_nsec >= 1000000000) {
        this->now.tv_sec++;
        this->now.tv_nsec -= 1000000000;
      }
      usec_to_sleep -= wait;

      timedout = 0;
      pthread_mutex_lock (&this->trigger_drawing_mutex);
      if (!this->trigger_drawing) {
        struct timespec abstime = this->now;
        timedout = pthread_cond_timedwait (&this->trigger_drawing_cond, &this->trigger_drawing_mutex, &abstime);
      }
      this->trigger_drawing = 0;
      pthread_mutex_unlock (&this->trigger_drawing_mutex);
      /* honor trigger update only when a backup img is available */
      if (!timedout && this->last_frame)
        break;
    }
  }

  /*
   * throw away undisplayed frames
   */

  {
    vo_frame_t *img = vo_ready_get_all (this);
    vo_list_flush (this, img);
  }

  /* dont let folks wait forever in vain */

  pthread_mutex_lock (&this->display_img_buf_queue.mutex);
  if (this->discard_frames)
    pthread_cond_broadcast (&this->done_flushing);
  pthread_mutex_unlock (&this->display_img_buf_queue.mutex);

  pthread_mutex_lock (&this->trigger_drawing_mutex);
  if (this->step) {
    this->step = 0;
    pthread_cond_broadcast (&this->done_stepping);
  }
  pthread_mutex_unlock (&this->trigger_drawing_mutex);

  if (this->last_flushed) {
    vo_frame_dec2_lock (this, this->last_flushed);
    this->last_flushed = NULL;
  }

  pthread_mutex_lock(&this->grab_lock);
  if (this->last_frame) {
    vo_frame_dec_lock( this->last_frame );
    this->last_frame = NULL;
  }
  pthread_mutex_unlock(&this->grab_lock);

  this->xine->config->unregister_callback(this->xine->config, "engine.decoder.disable_flush_from_video_out");

  return NULL;
}

/*
 * public function for video processing frontends to manually
 * consume video frames
 */

int xine_get_next_video_frame (xine_video_port_t *this_gen, xine_video_frame_t *frame) {
  vos_t *this = (vos_t *)this_gen;
  vo_frame_t *img;
  struct timespec now = {0, 990000000};

  pthread_mutex_lock (&this->display_img_buf_queue.mutex);

  while (!this->display_img_buf_queue.first) {
    {
      xine_stream_t *stream = this->streams[0];
      if (stream && (stream->video_fifo->fifo_size == 0)
        && (stream->demux_plugin->get_status(stream->demux_plugin) != DEMUX_OK)) {
        /* no further data can be expected here */
        pthread_mutex_unlock (&this->display_img_buf_queue.mutex);
        return 0;
      }
    }

    now.tv_nsec += 20000000;
    if (now.tv_nsec >= 1000000000) {
      xine_gettime (&now);
      now.tv_nsec += 20000000;
      if (now.tv_nsec >= 1000000000) {
        now.tv_sec++;
        now.tv_nsec -= 1000000000;
      }
    }
    {
      struct timespec ts = now;
      pthread_cond_timedwait (&this->display_img_buf_queue.not_empty, &this->display_img_buf_queue.mutex, &ts);
    }
  }

  /*
   * remove frame from display queue and return it
   */

  img = vo_queue_pop_int (&this->display_img_buf_queue);
  pthread_mutex_unlock(&this->display_img_buf_queue.mutex);

  frame->vpts         = img->vpts;
  frame->duration     = img->duration;
  frame->width        = img->width;
  frame->height       = img->height;
  frame->pos_stream   = img->extra_info->input_normpos;
  frame->pos_time     = img->extra_info->input_time;
  frame->frame_number = img->extra_info->frame_number;
  frame->aspect_ratio = img->ratio;
  frame->colorspace   = img->format;
  frame->data         = img->base[0];
  frame->xine_frame   = img;

  return 1;
}

void xine_free_video_frame (xine_video_port_t *port,
			    xine_video_frame_t *frame) {

  vo_frame_t *img = (vo_frame_t *) frame->xine_frame;
  vos_t *this = (vos_t *)img->port;

  (void)port;
  vo_frame_dec2_lock (this, img);
}


/********************************************************************
 * external API                                                     *
 *******************************************************************/

static uint32_t vo_get_capabilities (xine_video_port_t *this_gen) {
  vos_t      *this = (vos_t *) this_gen;
  return this->driver->get_capabilities (this->driver);
}

static void vo_open (xine_video_port_t *this_gen, xine_stream_t *stream) {

  vos_t      *this = (vos_t *) this_gen;

  xprintf (this->xine, XINE_VERBOSITY_DEBUG, "video_out: vo_open (%p)\n", (void*)stream);

  this->video_opened = 1;
  pthread_mutex_lock (&this->display_img_buf_queue.mutex);
  this->discard_frames = 0;
  pthread_mutex_unlock (&this->display_img_buf_queue.mutex);
  this->last_delivery_pts = 0;
  this->warn_threshold_event_sent = this->warn_threshold_exceeded = 0;
  if (!this->overlay_enabled && (stream == XINE_ANON_STREAM || stream == NULL || stream->spu_channel_user > -2))
    /* enable overlays if our new stream might want to show some */
    this->overlay_enabled = 1;

  vo_streams_register (this, stream);
}

static void vo_close (xine_video_port_t *this_gen, xine_stream_t *stream) {

  vos_t      *this = (vos_t *) this_gen;

  xprintf (this->xine, XINE_VERBOSITY_DEBUG, "video_out: vo_close (%p)\n", (void*)stream);

  vo_unref_all (this);

  /* this will make sure all hide events were processed */
  if (this->overlay_source)
    this->overlay_source->flush_events (this->overlay_source);

  this->video_opened = 0;

  /* unregister stream */
  vo_streams_unregister (this, stream);
}


static int vo_get_property (xine_video_port_t *this_gen, int property) {
  vos_t *this = (vos_t *) this_gen;
  int ret;

  switch (property) {
  case XINE_PARAM_VO_SINGLE_STEP:
    ret = 0;
    break;

  case VO_PROP_DISCARD_FRAMES:
    ret = this->discard_frames;
    break;

  case VO_PROP_BUFS_IN_FIFO:
    ret = this->video_loop_running ? this->display_img_buf_queue.num_buffers + this->ready_num : -1;
    break;

  case VO_PROP_BUFS_FREE:
    ret = this->video_loop_running ? this->free_img_buf_queue.num_buffers : -1;
    break;

  case VO_PROP_BUFS_TOTAL:
    ret = this->video_loop_running ? this->free_img_buf_queue.num_buffers_max : -1;
    break;

  case VO_PROP_NUM_STREAMS:
    xine_rwlock_rdlock (&this->streams_lock);
    ret = this->num_null_streams + this->num_anon_streams + this->num_streams;
    xine_rwlock_unlock (&this->streams_lock);
    break;

  /*
   * handle XINE_PARAM_xxx properties (convert from driver's range)
   */
  case XINE_PARAM_VO_CROP_LEFT:
    ret = this->crop_left;
    break;
  case XINE_PARAM_VO_CROP_RIGHT:
    ret = this->crop_right;
    break;
  case XINE_PARAM_VO_CROP_TOP:
    ret = this->crop_top;
    break;
  case XINE_PARAM_VO_CROP_BOTTOM:
    ret = this->crop_bottom;
    break;

  case XINE_PARAM_VO_SHARPNESS:
  case XINE_PARAM_VO_NOISE_REDUCTION:
  case XINE_PARAM_VO_HUE:
  case XINE_PARAM_VO_SATURATION:
  case XINE_PARAM_VO_CONTRAST:
  case XINE_PARAM_VO_BRIGHTNESS:
  case XINE_PARAM_VO_GAMMA:
   {
    int v, min_v, max_v, range_v;

    pthread_mutex_lock( &this->driver_lock );
    this->driver->get_property_min_max (this->driver,
					property & 0xffffff,
					&min_v, &max_v);

    v = this->driver->get_property (this->driver, property & 0xffffff);

    range_v = max_v - min_v + 1;

    if (range_v > 0)
      ret = ((v-min_v) * 65536 + 32768) / range_v;
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

  case XINE_PARAM_VO_SINGLE_STEP:
    ret = !!value;
    if (this->grab_only)
      break;
    /* xine_set_param () will (un)pause for us here to avoid ticket freeze. */
    pthread_mutex_lock (&this->trigger_drawing_mutex);
    this->step = ret;
    this->trigger_drawing = 0;
    pthread_cond_signal (&this->trigger_drawing_cond);
    if (ret) {
      struct timespec ts = {0, 0};
      xine_gettime (&ts);
      ts.tv_nsec += 500000000;
      if (ts.tv_nsec >= 1000000000) {
        ts.tv_sec++;
        ts.tv_nsec -= 1000000000;
      }
      if (pthread_cond_timedwait (&this->done_stepping, &this->trigger_drawing_mutex, &ts))
        ret = 0;
    }
    pthread_mutex_unlock (&this->trigger_drawing_mutex);
    break;

  case VO_PROP_DISCARD_FRAMES:
    /* recursive discard frames setting */
    if (value) {
      pthread_mutex_lock (&this->display_img_buf_queue.mutex);
      this->discard_frames++;
      pthread_mutex_unlock (&this->display_img_buf_queue.mutex);
    } else if (this->discard_frames) {
      pthread_mutex_lock (&this->display_img_buf_queue.mutex);
      if ((this->discard_frames == 1) && this->video_loop_running &&
        (this->flush_extra || this->display_img_buf_queue.first)) {
        /* Usually, render thread already did that in the meantime. Anyway, make sure display queue
           is empty, and more importantly, there are free frames for decoding when discard gets lifted. */
        vo_wait_flush (this);
      }
      this->discard_frames--;
      pthread_mutex_unlock (&this->display_img_buf_queue.mutex);
    } else
      xprintf (this->xine, XINE_VERBOSITY_DEBUG,
	       "vo_set_property: discard_frames is already zero\n");
    ret = this->discard_frames;

    /* discard buffers here because we have no output thread */
    if (this->grab_only && this->discard_frames)
      vo_manual_flush (this);
    break;

  /*
   * handle XINE_PARAM_xxx properties (convert to driver's range)
   */
  case XINE_PARAM_VO_CROP_LEFT:
    if( value < 0 )
      value = 0;
    ret = this->crop_left = value;
    break;
  case XINE_PARAM_VO_CROP_RIGHT:
    if( value < 0 )
      value = 0;
    ret = this->crop_right = value;
    break;
  case XINE_PARAM_VO_CROP_TOP:
    if( value < 0 )
      value = 0;
    ret = this->crop_top = value;
    break;
  case XINE_PARAM_VO_CROP_BOTTOM:
    if( value < 0 )
      value = 0;
    ret = this->crop_bottom = value;
    break;

  case XINE_PARAM_VO_SHARPNESS:
  case XINE_PARAM_VO_NOISE_REDUCTION:
  case XINE_PARAM_VO_HUE:
  case XINE_PARAM_VO_SATURATION:
  case XINE_PARAM_VO_CONTRAST:
  case XINE_PARAM_VO_BRIGHTNESS:
  case XINE_PARAM_VO_GAMMA:
    if (!this->grab_only) {
      int v, min_v, max_v, range_v;

      pthread_mutex_lock( &this->driver_lock );

      this->driver->get_property_min_max (this->driver,
					property & 0xffffff,
					&min_v, &max_v);

      range_v = max_v - min_v + 1;

      v = (value * range_v + (range_v/2)) / 65536 + min_v;

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

  if (!stream || (stream == XINE_ANON_STREAM)) {
    *width = this->current_width;
    *height = this->current_height;
    *img_duration = this->current_duration;
    return 0;
  }

  xine_rwlock_rdlock (&this->streams_lock);
  {
    xine_stream_t **s;
    for (s = this->streams; *s; s++) {
      if (*s == stream) {
        *width = this->current_width;
        *height = this->current_height;
        *img_duration = this->current_duration;
        xine_rwlock_unlock (&this->streams_lock);
        return 1;
      }
    }
  }
  xine_rwlock_unlock(&this->streams_lock);
  return 0;
}

static void vo_free_img_buffers (vos_t *this) {
  /* print frame usage stats */
  xprintf (this->xine, XINE_VERBOSITY_LOG,
    _("video_out: max frames used: %d of %d\n"),
    this->frames_peak_used, this->frames_total);
  xprintf (this->xine, XINE_VERBOSITY_LOG,
    _("video_out: early wakeups: %d of %d\n"),
    this->wakeups_early, this->wakeups_total);

  vo_queue_dispose_all (&this->free_img_buf_queue);
  vo_queue_dispose_all (&this->display_img_buf_queue);
}

static void vo_exit (xine_video_port_t *this_gen) {

  vos_t      *this = (vos_t *) this_gen;

  xprintf (this->xine, XINE_VERBOSITY_DEBUG, "video_out: exit.\n");

  this->xine->port_ticket->revoke_cb_unregister (this->xine->port_ticket, vo_ticket_revoked, this);
  
  if (this->video_loop_running) {
    void *p;

    this->video_loop_running = 0;

    pthread_join (this->video_thread, &p);
  }

  {
    int n = this->driver->set_property (this->driver, VO_PROP_DISCARD_FRAMES, -1);
    if (n > 0)
      xprintf (this->xine, XINE_VERBOSITY_DEBUG,
        "video_out: returned %d held frames from driver.\n", n);
  }

  vo_force_unref_all (this);
  vo_free_img_buffers (this);

  _x_free_video_driver(this->xine, &this->driver);

  xine_freep_aligned (&this->frames);

  if (this->overlay_source) {
    this->overlay_source->dispose (this->overlay_source);
  }

  vo_streams_close (this);

  vo_queue_close (&this->free_img_buf_queue);
  vo_queue_close (&this->display_img_buf_queue);

  pthread_cond_destroy(&this->done_stepping);
  pthread_cond_destroy(&this->trigger_drawing_cond);
  pthread_mutex_destroy(&this->trigger_drawing_mutex);

  pthread_mutex_destroy(&this->grab_lock);
  pthread_cond_destroy(&this->grab_cond);

  pthread_cond_destroy(&this->done_flushing);

  lprintf ("vo_exit... done\n");

  free (this);
}

static vo_frame_t *vo_get_last_frame (xine_video_port_t *this_gen) {
  vos_t      *this = (vos_t *) this_gen;
  vo_frame_t *last_frame;
  
  pthread_mutex_lock(&this->grab_lock);

  last_frame = this->last_frame;
  if (last_frame)
    vo_frame_inc_lock(last_frame);

  pthread_mutex_unlock(&this->grab_lock);

  return last_frame;
}

/* overlay stuff */

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
    xine_stream_t **s;
    xine_rwlock_rdlock (&this->streams_lock);
    if (this->num_anon_streams > 0) {
      xine_rwlock_unlock (&this->streams_lock);
      return;
    }
    for (s = this->streams; *s; s++) {
      if ((*s)->spu_channel_user > -2) {
        xine_rwlock_unlock (&this->streams_lock);
	return;
      }
    }
    xine_rwlock_unlock (&this->streams_lock);
    this->overlay_enabled = 0;
  }
}

/*
 * Flush video_out fifo
 */
static void vo_flush (xine_video_port_t *this_gen) {
  vos_t      *this = (vos_t *) this_gen;

  if (this->video_loop_running) {
    pthread_mutex_lock (&this->display_img_buf_queue.mutex);
    this->discard_frames++;
    vo_wait_flush (this);
    if (this->discard_frames > 0)
      this->discard_frames--;
    pthread_mutex_unlock (&this->display_img_buf_queue.mutex);
  } else {
    vo_manual_flush (this);
  }
}

static void vo_trigger_drawing (xine_video_port_t *this_gen) {
  vos_t      *this = (vos_t *) this_gen;

  pthread_mutex_lock (&this->trigger_drawing_mutex);
  this->trigger_drawing = 1;
  pthread_cond_signal (&this->trigger_drawing_cond);
  pthread_mutex_unlock (&this->trigger_drawing_mutex);
}

/* crop_frame() will allocate a new frame to copy in the given image
 * while cropping. maybe someday this will be an automatic post plugin.
 */
static vo_frame_t * crop_frame( xine_video_port_t *this_gen, vo_frame_t *img ) {

  vo_frame_t *dupl;

  dupl = vo_get_frame ( this_gen,
                        img->width - img->crop_left - img->crop_right,
                        img->height - img->crop_top - img->crop_bottom,
                        img->ratio, img->format, img->flags | VO_BOTH_FIELDS);

  dupl->progressive_frame  = img->progressive_frame;
  dupl->repeat_first_field = img->repeat_first_field;
  dupl->top_field_first    = img->top_field_first;
  dupl->overlay_offset_x   = img->overlay_offset_x;
  dupl->overlay_offset_y   = img->overlay_offset_y;

  switch (img->format) {
  case XINE_IMGFMT_YV12:
    yv12_to_yv12(
     /* Y */
      img->base[0] + img->crop_top * img->pitches[0] +
        img->crop_left, img->pitches[0],
      dupl->base[0], dupl->pitches[0],
     /* U */
      img->base[1] + img->crop_top/2 * img->pitches[1] +
        img->crop_left/2, img->pitches[1],
      dupl->base[1], dupl->pitches[1],
     /* V */
      img->base[2] + img->crop_top/2 * img->pitches[2] +
        img->crop_left/2, img->pitches[2],
      dupl->base[2], dupl->pitches[2],
     /* width x height */
      dupl->width, dupl->height);
    break;
  case XINE_IMGFMT_YUY2:
    yuy2_to_yuy2(
     /* src */
      img->base[0] + img->crop_top * img->pitches[0] +
        img->crop_left*2, img->pitches[0],
     /* dst */
      dupl->base[0], dupl->pitches[0],
     /* width x height */
      dupl->width, dupl->height);
    break;
  }

  dupl->bad_frame   = 0;
  dupl->pts         = img->pts;
  dupl->vpts        = img->vpts;
  dupl->proc_called = 0;

  dupl->duration  = img->duration;
  dupl->is_first  = img->is_first;

  dupl->stream    = img->stream;
  vo_reref ((vos_t *)this_gen, dupl);

  memcpy( dupl->extra_info, img->extra_info, sizeof(extra_info_t) );

  /* delay frame processing for now, we might not even need it (eg. frame will be discarded) */
  /* vo_frame_driver_proc(dupl); */

  return dupl;
}

xine_video_port_t *_x_vo_new_port (xine_t *xine, vo_driver_t *driver, int grabonly) {
  vos_t *this;
  int    num_frame_buffers;

  this = calloc(1, sizeof(vos_t)) ;
  if (!this)
    return NULL;
#ifndef HAVE_ZERO_SAFE_MEM
  /* Do these first, when compiler still knows "this" is all zeroed.
   * Let it optimize away this on most systems where clear mem
   * interpretes as 0, 0f or NULL safely.
   */
  this->num_frames_delivered  = 0;
  this->num_frames_skipped    = 0;
  this->num_frames_discarded  = 0;
  this->grab_only             = 0;
  this->video_opened          = 0;
  this->video_loop_running    = 0;
  this->trigger_drawing       = 0;
  this->step                  = 0;
  this->last_frame            = NULL;
  this->pending_grab_request  = NULL;
  this->frames_extref         = 0;
  this->frames_peak_used      = 0;
  this->frame_drop_cpt        = 0;
  this->frame_drop_suggested  = 0;
  this->discard_frames        = 0;
  this->flush_extra           = 0;
  this->num_flush_waiters     = 0;
  this->ready_first           = NULL;
  this->ready_num             = 0;
  this->need_flush_signal     = 0;
  this->last_flushed          = NULL;
#endif

  this->xine   = xine;
  this->clock  = xine->clock;
  this->driver = driver;

  this->vo.open                  = vo_open;
  this->vo.get_frame             = vo_get_frame;
  this->vo.get_last_frame        = vo_get_last_frame;
  this->vo.new_grab_video_frame  = vo_new_grab_video_frame;
  this->vo.close                 = vo_close;
  this->vo.exit                  = vo_exit;
  this->vo.get_capabilities      = vo_get_capabilities;
  this->vo.enable_ovl            = vo_enable_overlay;
  this->vo.get_overlay_manager   = vo_get_overlay_manager;
  this->vo.flush                 = vo_flush;
  this->vo.trigger_drawing       = vo_trigger_drawing;
  this->vo.get_property          = vo_get_property;
  this->vo.set_property          = vo_set_property;
  this->vo.status                = vo_status;
  this->vo.driver                = driver;

  /* default number of video frames from config */
  num_frame_buffers = xine->config->register_num (xine->config,
    "engine.buffers.video_num_frames",
    NUM_FRAME_BUFFERS, /* default */
    _("default number of video frames"),
    _("The default number of video frames to request "
      "from xine video out driver. Some drivers will "
      "override this setting with their own values."),
    20, NULL, NULL);

  /* check driver's limit and use the smaller value */
  {
    int i = driver->get_property (driver, VO_PROP_MAX_NUM_FRAMES);
    if (i && i < num_frame_buffers)
      num_frame_buffers = i;
  }

  /* we need at least 5 frames */
  if (num_frame_buffers<5)
    num_frame_buffers = 5;

  /* init frame usage stats */
  this->frames_total = num_frame_buffers;

  /* Choose a frame_drop_limit which matches num_frame_buffers.
   * xxmc for example supplies only 8 buffers. 2 are occupied by
   * MPEG2 decoding, further 2 for displaying and the remaining 4 can
   * hardly be filled all the time.
   * The below constants reserve buffers for decoding, displaying and
   * buffer fluctuation.
   * A frame_drop_limit_max below 1 will disable frame drops at all.
   */
  this->frame_drop_limit_max  = num_frame_buffers - 2 - 2 - 1;
  if (this->frame_drop_limit_max < 1)
    this->frame_drop_limit_max = 1;
  else if (this->frame_drop_limit_max > 3)
    this->frame_drop_limit_max = 3;

  this->frame_drop_limit      = this->frame_drop_limit_max;

  /* get some extra mem */
  {
    uint8_t *m = xine_mallocz_aligned (num_frame_buffers * (2 * sizeof (void *) + sizeof (extra_info_t)) + 32);
    if (!m) {
      free (this);
      return NULL;
    }
    this->frames = (vo_frame_t **)m;
    m += num_frame_buffers * sizeof (void *);
    this->img_streams = (xine_stream_t **)m;
    m += num_frame_buffers * sizeof (void *) + 31;
    m = (uint8_t *)((uintptr_t)m & ~(uintptr_t)31);
    this->extra_info_base = (extra_info_t *)m;
  }

  this->overlay_source = _x_video_overlay_new_manager (xine);
  if (this->overlay_source) {
    this->overlay_source->init (this->overlay_source);
    this->overlay_enabled = 1;
  }

  pthread_mutex_init (&this->driver_lock, NULL);
  pthread_mutex_init (&this->trigger_drawing_mutex, NULL);
  pthread_mutex_init (&this->grab_lock, NULL);

  pthread_cond_init (&this->grab_cond, NULL);
  pthread_cond_init (&this->trigger_drawing_cond, NULL);
  pthread_cond_init (&this->done_stepping, NULL);
  pthread_cond_init (&this->done_flushing, NULL);

  vo_streams_open (this);

  vo_queue_open (&this->free_img_buf_queue);
  vo_queue_open (&this->display_img_buf_queue);
  this->ready_add = &this->ready_first;

  /* nobody is listening yet, omit locking and signalling */
  {
    vo_frame_t **add = &this->free_img_buf_queue.first;
    int i;
    for (i = 0; i < num_frame_buffers; i++) {
      vo_frame_t *img = driver->alloc_frame (driver);
      if (!img)
        break;
      img->proc_duplicate_frame_data = NULL;
      img->id   = i;
      img->port = &this->vo;
      img->free = vo_frame_dec_lock;
      img->lock = vo_frame_inc_lock;
      img->draw = vo_frame_draw;
      img->extra_info = &this->extra_info_base[i];
      this->frames[i] = img;
      *add = img;
      add = &img->next;
    }
    *add = NULL;
    this->free_img_buf_queue.add = add;
    this->free_img_buf_queue.num_buffers     =
    this->free_img_buf_queue.num_buffers_max = i;
  }

  this->xine->port_ticket->revoke_cb_register (this->xine->port_ticket, vo_ticket_revoked, this);

  this->warn_skipped_threshold =
    xine->config->register_num (xine->config, "engine.performance.warn_skipped_threshold", 10,
    _("percentage of skipped frames to tolerate"),
    _("When more than this percentage of frames are not shown, because they "
      "were not decoded in time, xine sends a notification."),
    20, NULL, NULL);
  this->warn_discarded_threshold =
    xine->config->register_num (xine->config, "engine.performance.warn_discarded_threshold", 10,
    _("percentage of discarded frames to tolerate"),
    _("When more than this percentage of frames are not shown, because they "
      "were not scheduled for display in time, xine sends a notification."),
    20, NULL, NULL);

  if (grabonly) {

    this->grab_only = 1;

  } else {

    pthread_attr_t pth_attrs;
    int            err;
    /*
     * start video output thread
     *
     * this thread will alwys be running, displaying the
     * logo when "idle" thus making it possible to have
     * osd when not playing a stream
     */

    this->video_loop_running = 1;

    /* render thread needs no display queue signals */
    this->display_img_buf_queue.locked_for_read = 1000;

    pthread_attr_init(&pth_attrs);
#if defined(_POSIX_THREAD_PRIORITY_SCHEDULING) && (_POSIX_THREAD_PRIORITY_SCHEDULING > 0)
    pthread_attr_setscope(&pth_attrs, PTHREAD_SCOPE_SYSTEM);
#endif

    err = pthread_create (&this->video_thread, &pth_attrs, video_out_loop, this);
    pthread_attr_destroy(&pth_attrs);

    if (err != 0) {
      xprintf (this->xine, XINE_VERBOSITY_NONE, "video_out: can't create thread (%s)\n", strerror(err));
      /* FIXME: how does this happen ? */
      xprintf (this->xine, XINE_VERBOSITY_LOG,
	       _("video_out: sorry, this should not happen. please restart xine.\n"));
      _x_abort();
    }
    else
      xprintf(this->xine, XINE_VERBOSITY_DEBUG, "video_out: thread created\n");

  }

  return &this->vo;
}

