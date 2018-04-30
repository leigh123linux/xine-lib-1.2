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
 */

/*
 * top-level xine functions
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <fcntl.h>
#include <string.h>
#include <stdlib.h>
#include <pthread.h>
#include <stdarg.h>
#include <stdio.h>
#include <ctype.h>
#include <unistd.h>
#if defined (__linux__) || defined (__GLIBC__)
#include <endian.h>
#elif defined (__FreeBSD__)
#include <machine/endian.h>
#endif

#ifdef HAVE_SETLOCALE
#include <locale.h>
#endif

#include <basedir.h>

#define LOG_MODULE "xine"
#define LOG_VERBOSE
/*
#define LOG
#define DEBUG
*/

#define XINE_ENABLE_EXPERIMENTAL_FEATURES
#define METRONOM_INTERNAL
#define METRONOM_CLOCK_INTERNAL
#define POST_INTERNAL

#include <xine/xine_internal.h>
#include <xine/plugin_catalog.h>
#include <xine/audio_out.h>
#include <xine/video_out.h>
#include <xine/post.h>
#include <xine/demux.h>
#include <xine/buffer.h>
#include <xine/spu_decoder.h>
#include <xine/input_plugin.h>
#include <xine/metronom.h>
#include <xine/configfile.h>
#include <xine/osd.h>
#include <xine/spu.h>

#include <xine/xineutils.h>
#include <xine/compat.h>

#ifdef WIN32
#   include <fcntl.h>
#   include <winsock.h>
#endif /* WIN32 */

#include "xine_private.h"



static void mutex_cleanup (void *mutex) {
  pthread_mutex_unlock ((pthread_mutex_t *) mutex);
}

void _x_handle_stream_end (xine_stream_t *stream, int non_user) {

  if (stream->status == XINE_STATUS_QUIT)
    return;
  stream->status = XINE_STATUS_STOP;

  if (non_user) {
    /* frontends will not be interested in receiving this event
     * if they have called xine_stop explicitly, so only send
     * it if stream playback finished because of stream end reached
     */

    xine_event_t event;

    stream->finished_naturally = 1;

    event.data_length = 0;
    event.type        = XINE_EVENT_UI_PLAYBACK_FINISHED;

    xine_event_send (stream, &event);
  }
}

void _x_extra_info_reset( extra_info_t *extra_info ) {
  memset( extra_info, 0, sizeof(extra_info_t) );
}

void _x_extra_info_merge( extra_info_t *dst, extra_info_t *src ) {

  if (!src->invalid) {
    if( src->input_normpos )
      dst->input_normpos = src->input_normpos;

    if( src->input_time )
      dst->input_time = src->input_time;

    if( src->frame_number )
      dst->frame_number = src->frame_number;

    if( src->seek_count )
      dst->seek_count = src->seek_count;

    if( src->vpts )
      dst->vpts = src->vpts;
  }
}

typedef struct {
  xine_ticket_t   t;

  pthread_mutex_t lock;
  pthread_cond_t  issued;
  pthread_cond_t  revoked;
  xine_rwlock_t   port_rewiring_lock;

  int             tickets_granted;
  int             irrevocable_tickets;
  int             plain_renewers;

  int             rewirers;
  int             pending_revocations;
  int             atomic_revokers;
  pthread_t       atomic_revoker_thread;

  struct {
    int count;
    pthread_t holder;
  } *holder_threads;
  unsigned        holder_thread_count;

#define XINE_TICKET_MAX_CB 15
  xine_ticket_revoke_cb_t *revoke_callbacks[XINE_TICKET_MAX_CB + 1];
  void *revoke_cb_data[XINE_TICKET_MAX_CB + 1];
} xine_ticket_private_t;

static void ticket_revoke_cb_register (xine_ticket_t *tgen, xine_ticket_revoke_cb_t *cb, void *user_data) {
  xine_ticket_private_t *this = (xine_ticket_private_t *)tgen;
  int i;
  pthread_mutex_lock (&this->lock);
  for (i = 0; i < XINE_TICKET_MAX_CB; i++) {
    if (!this->revoke_callbacks[i]) {
      this->revoke_callbacks[i] = cb;
      this->revoke_cb_data[i]   = user_data;
      break;
    }
    if ((this->revoke_callbacks[i] == cb) && (this->revoke_cb_data[i] == user_data))
      break;
  }
  pthread_mutex_unlock (&this->lock);
}

static void ticket_revoke_cb_unregister (xine_ticket_t *tgen, xine_ticket_revoke_cb_t *cb, void *user_data) {
  xine_ticket_private_t *this = (xine_ticket_private_t *)tgen;
  int f;
  pthread_mutex_lock (&this->lock);
  for (f = 0; this->revoke_callbacks[f]; f++) {
    if ((this->revoke_callbacks[f] == cb) && (this->revoke_cb_data[f] == user_data)) {
      int l;
      for (l = f; this->revoke_callbacks[l + 1]; l++) ;
      if (f < l) {
        this->revoke_callbacks[f] = this->revoke_callbacks[l];
        this->revoke_cb_data[f]   = this->revoke_cb_data[l];
      }
      this->revoke_callbacks[l] = NULL;
      this->revoke_cb_data[l]   = NULL;
      break;
    }
  }
  pthread_mutex_unlock (&this->lock);
}

static int ticket_acquire_internal (xine_ticket_private_t *this, int irrevocable, int nonblocking) {
  pthread_t self = pthread_self ();
  int wait, i;

  pthread_mutex_lock (&this->lock);

  /* find ourselves in user list */
  for (i = 0; i < this->holder_thread_count; i++) {
    if (pthread_equal (this->holder_threads[i].holder, self))
      break;
  }

  if (i >= this->holder_thread_count) {
    /* not found */
    wait = this->pending_revocations
        && (this->atomic_revokers ? !pthread_equal (this->atomic_revoker_thread, self) : !irrevocable);
    if (nonblocking && wait) {
      /* not available immediately, bail out */
      pthread_mutex_unlock (&this->lock);
      return 0;
    }
    /* add */
    if (((i & 31) == 31) && (this->holder_threads[i].count == -1000)) {
      /* enlarge list */
      void *new = realloc (this->holder_threads, sizeof (*this->holder_threads) * (i + 33));
      if (!new) {
        pthread_mutex_unlock (&this->lock);
        return 0;
      }
      this->holder_threads = new;
      this->holder_threads[i + 32].count = -1000;
    }
    this->holder_threads[i].count = 1;
    this->holder_threads[i].holder = self;
    this->holder_thread_count = i + 1;
  } else {
    /* found, we already hold this */
    this->holder_threads[i].count++;
    wait = 0;
  }
    
  while (wait) {
    pthread_cond_wait (&this->issued, &this->lock);
    wait = this->pending_revocations
        && (this->atomic_revokers ? !pthread_equal (this->atomic_revoker_thread, self) : !irrevocable);
  }

  this->tickets_granted++;
  if (irrevocable)
    this->irrevocable_tickets++;

  pthread_mutex_unlock (&this->lock);
  return 1;
}

static int ticket_acquire_nonblocking(xine_ticket_t *tgen, int irrevocable) {
  xine_ticket_private_t *this = (xine_ticket_private_t *)tgen;
  return ticket_acquire_internal(this, irrevocable, 1);
}

static void ticket_acquire(xine_ticket_t *tgen, int irrevocable) {
  xine_ticket_private_t *this = (xine_ticket_private_t *)tgen;
  ticket_acquire_internal(this, irrevocable, 0);
}

static void ticket_release_internal (xine_ticket_private_t *this, int irrevocable) {
  pthread_t self = pthread_self ();
  int i;

  pthread_mutex_lock (&this->lock);

  /* find ourselves in user list */
  for (i = 0; i < this->holder_thread_count; i++) {
    if (pthread_equal (this->holder_threads[i].holder, self))
      break;
  }

  if (i >= this->holder_thread_count) {
    lprintf ("BUG! Ticket 0x%p released by a thread that never took it! Allowing code to continue\n", (void*)this);
    _x_assert (0);
  } else {
    this->holder_threads[i].count--;
    if (this->holder_threads[i].count == 0) {
      this->holder_thread_count--;
      if (i < this->holder_thread_count) {
        this->holder_threads[i].count  = this->holder_threads[this->holder_thread_count].count;
        this->holder_threads[i].holder = this->holder_threads[this->holder_thread_count].holder;
      }
    }
  }

  if ((this->irrevocable_tickets > 0) && irrevocable)
    this->irrevocable_tickets--;
  if (this->tickets_granted > 0)
    this->tickets_granted--;

  if (this->pending_revocations
    && (!this->tickets_granted || (this->rewirers && !this->irrevocable_tickets)))
    pthread_cond_broadcast(&this->revoked);

  pthread_mutex_unlock(&this->lock);
}

static void ticket_release_nonblocking (xine_ticket_t *tgen, int irrevocable) {
  xine_ticket_private_t *this = (xine_ticket_private_t *)tgen;
  ticket_release_internal (this, irrevocable);
}

static void ticket_release (xine_ticket_t *tgen, int irrevocable) {
  xine_ticket_private_t *this = (xine_ticket_private_t *)tgen;
  ticket_release_internal (this, irrevocable);
}

static void ticket_renew (xine_ticket_t *tgen, int irrevocable) {
  xine_ticket_private_t *this = (xine_ticket_private_t *)tgen;
  int i, grants;
  pthread_t self = pthread_self ();
  pthread_mutex_lock (&this->lock);
#if 0
  /* For performance, caller checks ticket_revoked without lock. 0 here is not really a bug. */
  _x_assert (this->ticket_revoked);
#endif
  /* Multithreaded decoders may well allocate frames outside the time window of the main
   * decoding call. Usually, we cannot make those hidden threads acquire and release
   * tickets properly. Blocking one of them may freeze main decoder with ticket held.
   * Lets just not make them wait, not messing with grant count.
   * There goes an optimization...
   */
  for (i = 0; i < this->holder_thread_count; i++) {
    if (pthread_equal (this->holder_threads[i].holder, self))
      break;
  }
  if (i >= this->holder_thread_count) {
    pthread_mutex_unlock (&this->lock);
    return;
  }
  /* If our thread still has saved self grants and calls this, restore them.
   * Assume later reissue by another thread (engine pause).
   */
  grants = this->holder_threads[i].count;
  if (grants >= 0x80000) {
    grants &= 0x7ffff;
    this->holder_threads[i].count = grants;
    this->tickets_granted += grants;
  }
  /* Lift _all_ our grants, avoid freeze when we have more than 1. */
  if (irrevocable & XINE_TICKET_FLAG_REWIRE) {
    /* allow everything */
    this->tickets_granted -= grants;
    if (!this->tickets_granted)
      pthread_cond_broadcast (&this->revoked);
    while (this->pending_revocations
        && (!this->irrevocable_tickets || !(irrevocable & ~XINE_TICKET_FLAG_REWIRE)))
      pthread_cond_wait (&this->issued, &this->lock);
    this->tickets_granted += grants;
  } else {
    /* fair wheather (not rewire) mode */
    this->plain_renewers++;
    this->tickets_granted -= grants;
    if (!this->tickets_granted)
      pthread_cond_broadcast (&this->revoked);
    while (this->pending_revocations
        && (!this->irrevocable_tickets || !irrevocable)
        && !this->rewirers)
      pthread_cond_wait (&this->issued, &this->lock);
    this->tickets_granted += grants;
    this->plain_renewers--;
  }

  pthread_mutex_unlock (&this->lock);
}

/* XINE_TICKET_FLAG_REWIRE implies XINE_TICKET_FLAG_ATOMIC. */
static void ticket_issue (xine_ticket_t *tgen, int flags) {
  xine_ticket_private_t *this = (xine_ticket_private_t *)tgen;
  pthread_t self = pthread_self ();
  int i;

  pthread_mutex_lock (&this->lock);

  if (this->pending_revocations > 0)
    this->pending_revocations--;
  if ((flags & XINE_TICKET_FLAG_REWIRE) && (this->rewirers > 0))
    this->rewirers--;
  if (flags & (XINE_TICKET_FLAG_REWIRE | XINE_TICKET_FLAG_ATOMIC)) {
    if (this->atomic_revokers > 0)
      this->atomic_revokers--;
    pthread_cond_broadcast (&this->revoked);
  }

  i = !this->pending_revocations ? 0
    : (this->rewirers ? (XINE_TICKET_FLAG_REWIRE | 1) : 1);
  if (this->t.ticket_revoked != i) {
    this->t.ticket_revoked = i;
    pthread_cond_broadcast (&this->issued);
  }

  /* HACK: restore self grants. */
  for (i = 0; i < this->holder_thread_count; i++) {
    if (pthread_equal (this->holder_threads[i].holder, self)) {
      int n = this->holder_threads[i].count;
      if (n >= 0x80000) {
        n -= 0x80000;
        this->holder_threads[i].count = n;
        if (n < 0x80000)
          this->tickets_granted += n;
      }
      break;
    }
  }

  pthread_mutex_unlock(&this->lock);
  if (flags & (XINE_TICKET_FLAG_REWIRE | XINE_TICKET_FLAG_ATOMIC))
    xine_rwlock_unlock (&this->port_rewiring_lock);
}

static void ticket_revoke (xine_ticket_t *tgen, int flags) {
  xine_ticket_private_t *this = (xine_ticket_private_t *)tgen;
  pthread_t self = pthread_self ();

  if (flags & (XINE_TICKET_FLAG_REWIRE | XINE_TICKET_FLAG_ATOMIC))
    xine_rwlock_wrlock (&this->port_rewiring_lock);
  pthread_mutex_lock(&this->lock);

  /* HACK: dont freeze on self grants, save them.
   * Also, nest revokes at bit 19. */
  {
    int i;
    for (i = 0; i < this->holder_thread_count; i++) {
      if (pthread_equal (this->holder_threads[i].holder, self)) {
        int n = this->holder_threads[i].count;
        if (n < 0x80000)
          this->tickets_granted -= n;
        this->holder_threads[i].count = n + 0x80000;
        break;
      }
    }
  }

  /* Set these early so release () / renew () see them. */
  this->pending_revocations++;

  /* No joke - see renew (). */
  if (flags & XINE_TICKET_FLAG_REWIRE) {
    this->rewirers++;
    if (this->plain_renewers)
      pthread_cond_broadcast (&this->issued);
  }

  /* New public status. */
  this->t.ticket_revoked = (this->rewirers ? (XINE_TICKET_FLAG_REWIRE | 1) : 1);

  if (this->atomic_revokers && pthread_equal (this->atomic_revoker_thread, self)) {
    /* Never wait for self. */
    if (flags & (XINE_TICKET_FLAG_REWIRE | XINE_TICKET_FLAG_ATOMIC))
      this->atomic_revokers++;
  } else if (this->tickets_granted || (this->rewirers && this->plain_renewers) || this->atomic_revokers) {
    /* Notify other holders. */
    int i, f = (this->rewirers ? XINE_TICKET_FLAG_REWIRE : 0)
             | (this->atomic_revokers ? XINE_TICKET_FLAG_ATOMIC : 0);
    for (i = 0; i < XINE_TICKET_MAX_CB; i++) {
      if (!this->revoke_callbacks[i])
        break;
      this->revoke_callbacks[i] (this->revoke_cb_data[i], f);
    }
    /* Wait for them to release/renew. */
    do {
      pthread_cond_wait (&this->revoked, &this->lock);
      this->t.ticket_revoked = (this->rewirers ? (XINE_TICKET_FLAG_REWIRE | 1) : 1);
    } while (this->tickets_granted || (this->rewirers && this->plain_renewers) || this->atomic_revokers);
    /* Its ours now. */
    if (flags & (XINE_TICKET_FLAG_REWIRE | XINE_TICKET_FLAG_ATOMIC)) {
      this->atomic_revoker_thread = self;
      this->atomic_revokers++;
    }
  }

  pthread_mutex_unlock(&this->lock);
}

static int lock_timeout (pthread_mutex_t *mutex, int ms_timeout) {
  if (ms_timeout == 0)
    return (0 == pthread_mutex_trylock (mutex));
  if (ms_timeout >= 0) {
    struct timespec abstime = {0, 0};
    xine_gettime (&abstime);
    abstime.tv_sec  +=  ms_timeout / 1000;
    abstime.tv_nsec += (ms_timeout % 1000) * 1000000;
    if (abstime.tv_nsec >= 1000000000) {
      abstime.tv_nsec -= 1000000000;
      abstime.tv_sec++;
    }
    return (0 == pthread_mutex_timedlock (mutex, &abstime));
  }
  pthread_mutex_lock (mutex);
  return 1;
}

static int ticket_lock_port_rewiring (xine_ticket_t *tgen, int ms_timeout) {
  xine_ticket_private_t *this = (xine_ticket_private_t *)tgen;
  if (ms_timeout == 0)
    return (0 == xine_rwlock_tryrdlock (&this->port_rewiring_lock));
  if (ms_timeout >= 0) {
    struct timespec abstime = {0, 0};
    xine_gettime (&abstime);
    abstime.tv_sec  +=  ms_timeout / 1000;
    abstime.tv_nsec += (ms_timeout % 1000) * 1000000;
    if (abstime.tv_nsec >= 1000000000) {
      abstime.tv_nsec -= 1000000000;
      abstime.tv_sec++;
    }
    return (0 == xine_rwlock_timedrdlock (&this->port_rewiring_lock, &abstime));
  }
  xine_rwlock_rdlock (&this->port_rewiring_lock);
  return 1;
}

static void ticket_unlock_port_rewiring (xine_ticket_t *tgen) {
  xine_ticket_private_t *this = (xine_ticket_private_t *)tgen;
  xine_rwlock_unlock (&this->port_rewiring_lock);
}

static void ticket_dispose (xine_ticket_t *tgen) {
  xine_ticket_private_t *this = (xine_ticket_private_t *)tgen;

  xine_rwlock_destroy   (&this->port_rewiring_lock);
  pthread_mutex_destroy (&this->lock);
  pthread_cond_destroy  (&this->issued);
  pthread_cond_destroy  (&this->revoked);

  free (this->holder_threads);
  free (this);
}

static xine_ticket_t *XINE_MALLOC ticket_init(void) {
  xine_ticket_private_t *port_ticket;

  port_ticket = calloc (1, sizeof (xine_ticket_private_t));
  if (!port_ticket)
    return NULL;
#ifndef HAVE_ZERO_SAFE_MEM
  port_ticket->t.ticket_revoked     = 0;
  port_ticket->holder_thread_count  = 0;
  port_ticket->tickets_granted      = 0;
  port_ticket->irrevocable_tickets  = 0;
  port_ticket->atomic_revokers      = 0;
  port_ticket->rewirers             = 0;
  port_ticket->plain_renewers       = 0;
  {
    int i;
    for (i = 0; i < XINE_TICKET_MAX_CB; i++) {
      port_ticket->revoke_callbacks[i] = NULL;
      port_ticket->revoke_cb_data[i]   = NULL;
    }
  }
#endif
  port_ticket->t.acquire_nonblocking  = ticket_acquire_nonblocking;
  port_ticket->t.acquire              = ticket_acquire;
  port_ticket->t.release_nonblocking  = ticket_release_nonblocking;
  port_ticket->t.release              = ticket_release;
  port_ticket->t.renew                = ticket_renew;
  port_ticket->t.revoke_cb_register   = ticket_revoke_cb_register;
  port_ticket->t.revoke_cb_unregister = ticket_revoke_cb_unregister;
  port_ticket->t.issue                = ticket_issue;
  port_ticket->t.revoke               = ticket_revoke;
  port_ticket->t.lock_port_rewiring   = ticket_lock_port_rewiring;
  port_ticket->t.unlock_port_rewiring = ticket_unlock_port_rewiring;
  port_ticket->t.dispose              = ticket_dispose;
  port_ticket->holder_threads         = malloc (32 * sizeof (*port_ticket->holder_threads));
  if (!port_ticket->holder_threads) {
    free (port_ticket);
    return NULL;
  }
  port_ticket->holder_threads[31].count = -1000;

  pthread_mutex_init       (&port_ticket->lock, NULL);
  xine_rwlock_init_default (&port_ticket->port_rewiring_lock);
  pthread_cond_init        (&port_ticket->issued, NULL);
  pthread_cond_init        (&port_ticket->revoked, NULL);

  return &port_ticket->t;
}

static void set_speed_internal (xine_stream_t *stream, int speed) {
  xine_t *xine = stream->xine;
  int old_speed = xine->clock->speed;
  int mode = 2 * (speed == XINE_SPEED_PAUSE) + (old_speed == XINE_SPEED_PAUSE);

  if (mode & 1) {
    if (mode & 2) {
      /* stay paused */
      return;
    } else {
      /* switch pause mode */
      if (speed == XINE_LIVE_PAUSE_ON) {
        if (xine->live_pause)
          return;
        pthread_mutex_lock (&xine->pause_mutex);
        if (!xine->live_pause) {
          xine->port_ticket->issue (xine->port_ticket, 0);
          xine->live_pause = 1;
        }
        pthread_mutex_unlock (&xine->pause_mutex);
        return;
      }
      if (speed == XINE_LIVE_PAUSE_OFF) {
        if (!xine->live_pause)
          return;
        pthread_mutex_lock (&xine->pause_mutex);
        if (xine->live_pause) {
          xine->port_ticket->revoke (xine->port_ticket, 0);
          xine->live_pause = 0;
        }
        pthread_mutex_unlock (&xine->pause_mutex);
        return;
      }
      /* unpause */
      pthread_mutex_lock (&xine->pause_mutex);
      if (xine->live_pause)
        xine->live_pause = 0;
      else
        /* all decoder and post threads may continue now */
        xine->port_ticket->issue (xine->port_ticket, 0);
      pthread_mutex_unlock (&xine->pause_mutex);
    }
  } else {
    if (mode & 2) {
      /* pause */
      /* get all decoder and post threads in a state where they agree to be blocked */
      xine->port_ticket->revoke (xine->port_ticket, 0);
      /* set master clock so audio_out loop can pause in a safe place */
      xine->clock->set_fine_speed (xine->clock, speed);
    } else {
      if ((speed == XINE_LIVE_PAUSE_ON) || (speed == XINE_LIVE_PAUSE_OFF))
        return;
      /* plain change */
      if (speed == old_speed)
        return;
    }
  }

  /* see coment on audio_out loop about audio_paused */
  if( stream->audio_out ) {
    xine->port_ticket->acquire(xine->port_ticket, 1);

    /* inform audio_out that speed has changed - he knows what to do */
    stream->audio_out->set_property (stream->audio_out, AO_PROP_CLOCK_SPEED, speed);

    xine->port_ticket->release(xine->port_ticket, 1);
  }

  if (mode < 2)
    /* master clock is set after resuming the audio device (audio_out loop may continue) */
    xine->clock->set_fine_speed (xine->clock, speed);
}


/* stream->ignore_speed_change must be set, when entering this function */
static void stop_internal (xine_stream_t *stream) {

  lprintf ("status before = %d\n", stream->status);

  if ( stream->status == XINE_STATUS_IDLE ||
       stream->status == XINE_STATUS_STOP ) {
    _x_demux_control_end(stream, 0);
    lprintf("ignored");
  } else {
    /* make sure we're not in "paused" state */
    set_speed_internal (stream, XINE_FINE_SPEED_NORMAL);

    /* Don't change status if we're quitting */
    if (stream->status != XINE_STATUS_QUIT)
      stream->status = XINE_STATUS_STOP;
  }
  /*
   * stop demux
   */
  if (stream->demux_plugin && stream->demux_thread_created) {
    lprintf ("stopping demux\n");
    _x_demux_stop_thread( stream );
    lprintf ("demux stopped\n");
  }
  lprintf ("done\n");
}

void xine_stop (xine_stream_t *stream) {

  pthread_mutex_lock (&stream->frontend_lock);
  pthread_cleanup_push (mutex_cleanup, (void *) &stream->frontend_lock);

  /* make sure that other threads cannot change the speed, especially pauseing the stream */
  pthread_mutex_lock(&stream->speed_change_lock);
  stream->ignore_speed_change = 1;
  pthread_mutex_unlock(&stream->speed_change_lock);

  stream->xine->port_ticket->acquire(stream->xine->port_ticket, 1);

  if (stream->audio_out)
    stream->audio_out->set_property(stream->audio_out, AO_PROP_DISCARD_BUFFERS, 1);
  if (stream->video_out)
    stream->video_out->set_property(stream->video_out, VO_PROP_DISCARD_FRAMES, 1);

  stop_internal (stream);

  if (stream->slave && (stream->slave_affection & XINE_MASTER_SLAVE_STOP))
    xine_stop(stream->slave);

  if (stream->video_out)
    stream->video_out->set_property(stream->video_out, VO_PROP_DISCARD_FRAMES, 0);
  if (stream->audio_out)
    stream->audio_out->set_property(stream->audio_out, AO_PROP_DISCARD_BUFFERS, 0);

  stream->xine->port_ticket->release(stream->xine->port_ticket, 1);
  stream->ignore_speed_change = 0;

  pthread_cleanup_pop (0);
  pthread_mutex_unlock (&stream->frontend_lock);
}


static void close_internal (xine_stream_t *stream) {

  int flush = !stream->gapless_switch && !stream->finished_naturally;

  if( stream->slave ) {
    xine_close( stream->slave );
    if( stream->slave_is_subtitle ) {
      xine_dispose(stream->slave);
      stream->slave = NULL;
      stream->slave_is_subtitle = 0;
    }
  }

  if (flush) {
    /* make sure that other threads cannot change the speed, especially pauseing the stream */
    pthread_mutex_lock(&stream->speed_change_lock);
    stream->ignore_speed_change = 1;
    pthread_mutex_unlock(&stream->speed_change_lock);

    stream->xine->port_ticket->acquire(stream->xine->port_ticket, 1);

    if (stream->audio_out)
      stream->audio_out->set_property(stream->audio_out, AO_PROP_DISCARD_BUFFERS, 1);
    if (stream->video_out)
      stream->video_out->set_property(stream->video_out, VO_PROP_DISCARD_FRAMES, 1);
  }

  stop_internal( stream );

  if (flush) {
    if (stream->video_out)
      stream->video_out->set_property(stream->video_out, VO_PROP_DISCARD_FRAMES, 0);
    if (stream->audio_out)
      stream->audio_out->set_property(stream->audio_out, AO_PROP_DISCARD_BUFFERS, 0);

    stream->xine->port_ticket->release(stream->xine->port_ticket, 1);
    stream->ignore_speed_change = 0;
  }

  if (stream->demux_plugin) {
    _x_free_demux_plugin(stream, stream->demux_plugin);
    stream->demux_plugin = NULL;
  }

  /*
   * close input plugin
   */

  if (stream->input_plugin) {
    _x_free_input_plugin(stream, stream->input_plugin);
    stream->input_plugin = NULL;
  }

  /*
   * reset / free meta info
   * XINE_STREAM_INFO_MAX is at least 99 but the info arrays are sparsely used.
   * Save a lot of mutex/free calls.
   */
  {
    int i;
    pthread_mutex_lock (&stream->info_mutex);
    for (i = 0; i < XINE_STREAM_INFO_MAX; i++)
      stream->stream_info_public[i] = stream->stream_info[i] = 0;
    pthread_mutex_unlock (&stream->info_mutex);
    pthread_mutex_lock (&stream->meta_mutex);
    for (i = 0; i < XINE_STREAM_INFO_MAX; i++) {
      if (stream->meta_info_public[i])
        free (stream->meta_info_public[i]), stream->meta_info_public[i] = NULL;
      if (stream->meta_info[i])
        free (stream->meta_info[i]), stream->meta_info[i] = NULL;
    }
    pthread_mutex_unlock (&stream->meta_mutex);
  }
  stream->audio_track_map_entries = 0;
  stream->spu_track_map_entries = 0;

  _x_keyframes_set (stream, NULL, 0);
}

void xine_close (xine_stream_t *stream) {

  pthread_mutex_lock (&stream->frontend_lock);
  pthread_cleanup_push (mutex_cleanup, (void *) &stream->frontend_lock);

  close_internal (stream);

  /*
   * set status to idle.
   * not putting this into close_internal because it is also called
   * by open_internal.
   */

  /* Don't change status if we're quitting */
  if (stream->status != XINE_STATUS_QUIT)
    stream->status = XINE_STATUS_IDLE;

  pthread_cleanup_pop (0);
  pthread_mutex_unlock (&stream->frontend_lock);
}

static int stream_rewire_audio(xine_post_out_t *output, void *data)
{
  xine_stream_t *stream = (xine_stream_t *)output->data;
  xine_audio_port_t *new_port = (xine_audio_port_t *)data, *old_port;
  uint32_t bits, rate;
  int mode;

  if (!data)
    return 0;

  stream->xine->port_ticket->revoke (stream->xine->port_ticket, XINE_TICKET_FLAG_REWIRE);
  /* just an optimization. Keep engine paused at rewire safe position for subsequent rewires. */
  set_speed_internal (stream, XINE_LIVE_PAUSE_OFF);

  old_port = stream->audio_out;
  _x_post_audio_port_ref (new_port);
  if (old_port->status (old_port, stream, &bits, &rate, &mode)) {
    /* register our stream at the new output port */
    (new_port->open) (new_port, stream, bits, rate, mode);
    old_port->close (old_port, stream);
  }
  stream->audio_out = new_port;
  _x_post_audio_port_unref (old_port);

  stream->xine->port_ticket->issue (stream->xine->port_ticket, XINE_TICKET_FLAG_REWIRE);

  return 1;
}

static int stream_rewire_video(xine_post_out_t *output, void *data)
{
  xine_stream_t *stream = (xine_stream_t *)output->data;
  xine_video_port_t *new_port = (xine_video_port_t *)data, *old_port;
  int64_t img_duration;
  int width, height;

  if (!data)
    return 0;

  stream->xine->port_ticket->revoke (stream->xine->port_ticket, XINE_TICKET_FLAG_REWIRE);
  /* just an optimization. Keep engine paused at rewire safe position for subsequent rewires. */
  set_speed_internal (stream, XINE_LIVE_PAUSE_OFF);

  old_port = stream->video_out;
  _x_post_video_port_ref (new_port);
  if (old_port->status (old_port, stream, &width, &height, &img_duration)) {
    /* register our stream at the new output port */
    (new_port->open) (new_port, stream);
    old_port->close (old_port, stream);
  }
  stream->video_out = new_port;
  _x_post_video_port_unref (old_port);

  stream->xine->port_ticket->issue (stream->xine->port_ticket, XINE_TICKET_FLAG_REWIRE);

  return 1;
}

static void xine_dispose_internal (xine_stream_t *stream);

typedef struct {
  xine_stream_t s;
  extra_info_t  ei[3];
} xine_stream_private_t;

xine_stream_t *xine_stream_new (xine_t *this, xine_audio_port_t *ao, xine_video_port_t *vo) {

  xine_stream_private_t *stream;
  pthread_mutexattr_t attr;

  xprintf (this, XINE_VERBOSITY_DEBUG, "xine_stream_new\n");

  /* create a new stream object */
  stream = calloc (1, sizeof (*stream));
  if (!stream)
    goto err_null;
#ifndef HAVE_ZERO_SAFE_MEM
  /* Do these first, when compiler still knows stream is all zeroed.
   * Let it optimize away this on most systems where clear mem
   * interpretes as 0, 0f or NULL safely.
   */
  stream->s.spu_decoder_plugin     = NULL;
  stream->s.audio_decoder_plugin   = NULL;
  stream->s.early_finish_event     = 0;
  stream->s.delay_finish_event     = 0;
  stream->s.gapless_switch         = 0;
  stream->s.keep_ao_driver_open    = 0;
  stream->s.video_channel          = 0;
  stream->s.video_decoder_plugin   = NULL;
  stream->s.header_count_audio     = 0;
  stream->s.header_count_video     = 0;
  stream->s.finished_count_audio   = 0;
  stream->s.finished_count_video   = 0;
  stream->s.err                    = 0;
  stream->s.broadcaster            = NULL;
  stream->s.index_array            = NULL;
  stream->s.slave                  = NULL;
  stream->s.slave_is_subtitle      = 0;
  {
    int i;
    for (i = 0; i < XINE_STREAM_INFO_MAX; i++) {
      stream->s.stream_info_public[i] = stream->s.stream_info[i] = 0;
      stream->s.meta_info_public[i]   = stream->s.meta_info[i]   = NULL;
    }
  }
#endif
  /* no need to memset again
  _x_extra_info_reset (&stream->ei[0]);
  _x_extra_info_reset (&stream->ei[1]);
  _x_extra_info_reset (&stream->ei[2]);
  */

  stream->s.current_extra_info       = &stream->ei[0];
  stream->s.audio_decoder_extra_info = &stream->ei[1];
  stream->s.video_decoder_extra_info = &stream->ei[2];

  stream->s.xine                = this;
  stream->s.status              = XINE_STATUS_IDLE;

  stream->s.video_source.name   = "video source";
  stream->s.video_source.type   = XINE_POST_DATA_VIDEO;
  stream->s.video_source.data   = &stream->s;
  stream->s.video_source.rewire = stream_rewire_video;

  stream->s.audio_source.name   = "audio source";
  stream->s.audio_source.type   = XINE_POST_DATA_AUDIO;
  stream->s.audio_source.data   = &stream->s;
  stream->s.audio_source.rewire = stream_rewire_audio;

  stream->s.spu_decoder_streamtype   = -1;
  stream->s.audio_out                = ao;
  stream->s.audio_channel_user       = -1;
  stream->s.audio_channel_auto       = -1;
  stream->s.audio_decoder_streamtype = -1;
  stream->s.spu_channel_auto         = -1;
  stream->s.spu_channel_letterbox    = -1;
  stream->s.spu_channel_pan_scan     = -1;
  stream->s.spu_channel_user         = -1;
  stream->s.spu_channel              = -1;
  /* Do not flush output when opening/closing yet unused streams (eg subtitle). */
  stream->s.finished_naturally       = 1;
  stream->s.video_out                = vo;
  stream->s.video_driver             = vo ? vo->driver : NULL;
  stream->s.video_decoder_streamtype = -1;
  /* initial master/slave */
  stream->s.master                   = &stream->s;

  /* event queues */
  stream->s.event_queues = xine_list_new ();
  if (!stream->s.event_queues)
    goto err_free;

  /* init mutexes and conditions */
  pthread_mutex_init (&stream->s.info_mutex, NULL);
  pthread_mutex_init (&stream->s.meta_mutex, NULL);
  pthread_mutex_init (&stream->s.demux_lock, NULL);
  pthread_mutex_init (&stream->s.demux_action_lock, NULL);
  pthread_mutex_init (&stream->s.demux_mutex, NULL);
  pthread_cond_init  (&stream->s.demux_resume, NULL);
  pthread_mutex_init (&stream->s.event_queues_lock, NULL);
  pthread_mutex_init (&stream->s.counter_lock, NULL);
  pthread_cond_init  (&stream->s.counter_changed, NULL);
  pthread_mutex_init (&stream->s.first_frame_lock, NULL);
  pthread_cond_init  (&stream->s.first_frame_reached, NULL);
  pthread_mutex_init (&stream->s.current_extra_info_lock, NULL);
  pthread_mutex_init (&stream->s.speed_change_lock, NULL);
  pthread_mutex_init (&stream->s.index_mutex, NULL);

  /* warning: frontend_lock is a recursive mutex. it must NOT be
   * used with neither pthread_cond_wait() or pthread_cond_timedwait()
   */
  pthread_mutexattr_init(&attr);
  pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_RECURSIVE);
  pthread_mutex_init (&stream->s.frontend_lock, &attr);
  pthread_mutexattr_destroy(&attr);

  pthread_mutex_lock (&this->streams_lock);

  /* create a metronom */
  stream->s.metronom = _x_metronom_init ( (vo != NULL), (ao != NULL), this);
  if (!stream->s.metronom)
    goto err_mutex;

  /* alloc fifos, init and start decoder threads */
  if (!_x_video_decoder_init (&stream->s))
    goto err_metronom;

  if (!_x_audio_decoder_init (&stream->s))
    goto err_video;

  /* osd */
  if (vo) {
    _x_spu_misc_init (this);
    stream->s.osd_renderer = _x_osd_renderer_init (&stream->s);
  } else
    stream->s.osd_renderer = NULL;

  /* create a reference counter */
  stream->s.refcounter = _x_new_refcounter (&stream->s, (refcounter_destructor)xine_dispose_internal);
  if (!stream->s.refcounter)
    goto err_audio;

  /* register stream */
  xine_list_push_back (this->streams, &stream->s);
  pthread_mutex_unlock (&this->streams_lock);
  return &stream->s;

  err_audio:
  _x_audio_decoder_shutdown (&stream->s);

  err_video:
  _x_video_decoder_shutdown (&stream->s);

  err_metronom:
  stream->s.metronom->exit (stream->s.metronom);

  err_mutex:
  pthread_mutex_unlock  (&this->streams_lock);
  pthread_mutex_destroy (&stream->s.frontend_lock);
  pthread_mutex_destroy (&stream->s.index_mutex);
  pthread_mutex_destroy (&stream->s.speed_change_lock);
  pthread_mutex_destroy (&stream->s.current_extra_info_lock);
  pthread_cond_destroy  (&stream->s.first_frame_reached);
  pthread_mutex_destroy (&stream->s.first_frame_lock);
  pthread_cond_destroy  (&stream->s.counter_changed);
  pthread_mutex_destroy (&stream->s.counter_lock);
  pthread_mutex_destroy (&stream->s.event_queues_lock);
  pthread_cond_destroy  (&stream->s.demux_resume);
  pthread_mutex_destroy (&stream->s.demux_mutex);
  pthread_mutex_destroy (&stream->s.demux_action_lock);
  pthread_mutex_destroy (&stream->s.demux_lock);
  pthread_mutex_destroy (&stream->s.meta_mutex);
  pthread_mutex_destroy (&stream->s.info_mutex);
  xine_list_delete      (stream->s.event_queues);

  err_free:
  free (stream);

  err_null:
  return NULL;
}

void _x_mrl_unescape (char *mrl) {
  static const uint8_t tab_unhex[256] = {
#define nn 128
    nn,nn,nn,nn,nn,nn,nn,nn,nn,nn,nn,nn,nn,nn,nn,nn,
    nn,nn,nn,nn,nn,nn,nn,nn,nn,nn,nn,nn,nn,nn,nn,nn,
    nn,nn,nn,nn,nn,nn,nn,nn,nn,nn,nn,nn,nn,nn,nn,nn,
     0, 1, 2, 3, 4, 5, 6, 7, 8, 9,nn,nn,nn,nn,nn,nn,
    nn,10,11,12,13,14,15,nn,nn,nn,nn,nn,nn,nn,nn,nn,
    nn,nn,nn,nn,nn,nn,nn,nn,nn,nn,nn,nn,nn,nn,nn,nn,
    nn,10,11,12,13,14,15,nn,nn,nn,nn,nn,nn,nn,nn,nn,
    nn,nn,nn,nn,nn,nn,nn,nn,nn,nn,nn,nn,nn,nn,nn,nn,
    nn,nn,nn,nn,nn,nn,nn,nn,nn,nn,nn,nn,nn,nn,nn,nn,
    nn,nn,nn,nn,nn,nn,nn,nn,nn,nn,nn,nn,nn,nn,nn,nn,
    nn,nn,nn,nn,nn,nn,nn,nn,nn,nn,nn,nn,nn,nn,nn,nn,
    nn,nn,nn,nn,nn,nn,nn,nn,nn,nn,nn,nn,nn,nn,nn,nn,
    nn,nn,nn,nn,nn,nn,nn,nn,nn,nn,nn,nn,nn,nn,nn,nn,
    nn,nn,nn,nn,nn,nn,nn,nn,nn,nn,nn,nn,nn,nn,nn,nn,
    nn,nn,nn,nn,nn,nn,nn,nn,nn,nn,nn,nn,nn,nn,nn,nn,
    nn,nn,nn,nn,nn,nn,nn,nn,nn,nn,nn,nn,nn,nn,nn,nn
  };
  const uint8_t *p = (const uint8_t *)mrl;
  uint8_t *q;
  /* dont touch until first %xx */
  while (*p) {
    if (*p == '%') break;
    p++;
  }
  if (!*p)
    return;
  /* now really unescape */
  /* q = (uint8_t *)p ?? */
  q = (uint8_t *)mrl + (p - (const uint8_t *)mrl);
  while (1) {
    uint8_t z = *p++;
    if (z == '%') {
      uint8_t h = tab_unhex[*p];
      if (!(h & nn)) {
        z = h;
        p++;
        h = tab_unhex[*p];
        if (!(h & nn)) {
          z = (z << 4) | h;
          p++;
        }
      } else if (*p == '%') {
        p++;
      }
    }
    *q++ = z;
    if (!z) break;
  }
#undef nn
}

char *_x_mrl_remove_auth(const char *mrl_in)
{
  char *mrl = strdup(mrl_in);
  char *auth, *p, *at, *host_end;

  /* parse protocol */
  if (!(p = strchr(mrl, ':'))) {
    /* no protocol means plain filename */
    return mrl;
  }

  p++; /* skip ':' */
  if (*p == '/') p++;
  if (*p == '/') p++;

  /* authorization (user[:pass]@hostname) */
  auth = p;
  host_end = strchr(p, '/');
  while ((at = strchr(p, '@')) && at < host_end) {
    p = at + 1; /* skip '@' */
  }

  if (p != auth) {
    while (p[-1]) {
      *auth++ = *p++;
    }
  }

  return mrl;
}

void _x_flush_events_queues (xine_stream_t *stream) {

  xine_list_iterator_t ite;

  pthread_mutex_lock (&stream->event_queues_lock);

  /* No events queue? */
  for (ite = xine_list_front (stream->event_queues);
       ite; ite = xine_list_next (stream->event_queues, ite)) {
    xine_event_queue_t *queue = xine_list_get_value(stream->event_queues, ite);
    pthread_mutex_lock (&queue->lock);
    pthread_mutex_unlock (&stream->event_queues_lock);

    /* we might have been called from the very same function that
     * processes events, therefore waiting here would cause deadlock.
     * check only queues with listener threads which are not
     * currently executing their callback functions.
     */
    if (queue->listener_thread != NULL && !queue->callback_running) {
      while (!xine_list_empty (queue->events)) {
        pthread_cond_wait (&queue->events_processed, &queue->lock);
      }
    }

    pthread_mutex_unlock (&queue->lock);
    pthread_mutex_lock (&stream->event_queues_lock);
  }

  pthread_mutex_unlock (&stream->event_queues_lock);
}

/* 0x01 (end), 0x02 (alpha), 0x04 (alnum - + .), 0x08 (:), 0x10 (;), 0x20 (#)  */
static const uint8_t tab_parse[256] = {
   1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
   0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
   0, 0, 0,32, 0, 0, 0, 0, 0, 0, 0, 4, 0, 4, 4, 0,
   4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 8,16, 0, 0, 0, 0,
   0, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6,
   6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 0, 0, 0, 0, 0,
   0, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6,
   6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 0, 0, 0, 0, 0,
   0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
   0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
   0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
   0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
   0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
   0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
   0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
   0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
};

/* aka "does path have a protocol prefix" */
static inline int _x_path_looks_like_mrl (const char *path) {
  const uint8_t *p = (const uint8_t *)path;
  if (!(tab_parse[*p++] & 0x02))
    return 0;
  while (tab_parse[*p++] & 0x04) ;
  return (p[-1] == ':') && (p[0] == '/');
}

static int open_internal (xine_stream_t *stream, const char *mrl) {

  static const uint8_t tab_tolower[256] = {
      0,  1,  2,  3,  4,  5,  6,  7,  8,  9, 10, 11, 12, 13, 14, 15,
     16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31,
    ' ','!','"','#','$','%','&','\'','(',')','*','+',',','-','.','/',
    '0','1','2','3','4','5','6','7','8','9',':',';','<','=','>','?',
    '@','a','b','c','d','e','f','g','h','i','j','k','l','m','n','o',
    'p','q','r','s','t','u','v','w','x','y','z','[','\\',']','^','_',
    '`','a','b','c','d','e','f','g','h','i','j','k','l','m','n','o',
    'p','q','r','s','t','u','v','w','x','y','z','{','|','}','~',127,
    128,129,130,131,132,133,134,135,136,137,138,139,140,141,142,143,
    144,145,146,147,148,149,150,151,152,153,154,155,156,157,158,159,
    160,161,162,163,164,165,166,167,168,169,170,171,172,173,174,175,
    176,177,178,179,180,181,182,183,184,185,186,187,188,189,190,191,
    192,193,194,195,196,197,198,199,200,201,202,203,204,205,206,207,
    208,209,210,211,212,213,214,215,216,217,218,219,220,221,222,223,
    224,225,226,227,228,229,230,231,232,233,234,235,236,237,238,239,
    240,241,242,243,244,245,246,247,248,249,250,251,252,253,254,255
  };

  uint8_t *buf, *name, *args;
  int no_cache = 0;

  if (!mrl) {
    xprintf (stream->xine, XINE_VERBOSITY_LOG, _("xine: error while parsing mrl\n"));
    stream->err = XINE_ERROR_MALFORMED_MRL;
    if (stream->status != XINE_STATUS_IDLE)
      stream->status = XINE_STATUS_STOP;
    return 0;
  }

  lprintf ("opening MRL '%s'...\n", mrl);

  /*
   * stop engine if necessary
   */

  close_internal (stream);

  lprintf ("engine should be stopped now\n");

  /*
   * look for a stream_setup in MRL and try finding an input plugin
   */
  buf = malloc (32 + strlen (mrl) + 32);
  if (!buf)
    return 0;
  name = buf + 32;
  args = NULL;
  {
    const uint8_t *p = (const uint8_t *)mrl;
    uint8_t *prot = NULL, *q = name, z;
    /* test protocol prefix */
    if (tab_parse[*p] & 0x02) {
      while (tab_parse[z = *p] & 0x04) p++, *q++ = z;
      if ((q > name) && (z == ':') && (p[1] == '/')) prot = name;
    }
    if (prot) {
      /* split off args at first hash */
      while (!(tab_parse[z = *p] & 0x21)) p++, *q++ = z;
      *q = 0;
      if (z == '#') {
        p++;
        args = ++q;
        while ((*q++ = *p++) != 0) ;
      }
    } else {
      /* raw filename, may contain any number of hashes */
      while (1) {
        struct stat s;
        while (!(tab_parse[z = *p] & 0x21)) p++, *q++ = z;
        *q = 0;
        /* no need to stat when no hashes found */
        if (!args && !z) break;
        if (!stat ((const char *)name, &s)) {
          args = NULL;
          /* no general break yet, beware "/foo/#bar.flv" */
        }
        if (!z) break;
        p++, *q++ = z;
        args = q;
      }
      if (args) args[-1] = 0;
    }
  }
    
  {
    /*
     * find an input plugin
     */
    stream->input_plugin = _x_find_input_plugin (stream, (const char *)name);

    if ( stream->input_plugin ) {
      int res;
      input_class_t *input_class = stream->input_plugin->input_class;

      xine_log (stream->xine, XINE_LOG_MSG, _("xine: found input plugin  : %s\n"),
                dgettext(input_class->text_domain ? input_class->text_domain : XINE_TEXTDOMAIN,
                         input_class->description));
      if (stream->input_plugin->input_class->eject_media)
        stream->eject_class = stream->input_plugin->input_class;
      _x_meta_info_set_utf8(stream, XINE_META_INFO_INPUT_PLUGIN,
			    stream->input_plugin->input_class->identifier);

      res = (stream->input_plugin->open) (stream->input_plugin);
      switch(res) {
      case 1: /* Open successfull */
	break;
      case -1: /* Open unsuccessfull, but correct plugin */
	stream->err = XINE_ERROR_INPUT_FAILED;
	_x_flush_events_queues (stream);
        free (buf);
	return 0;
      default:
	xine_log (stream->xine, XINE_LOG_MSG, _("xine: input plugin cannot open MRL [%s]\n"),mrl);
        _x_free_input_plugin(stream, stream->input_plugin);
	stream->input_plugin = NULL;
	stream->err = XINE_ERROR_INPUT_FAILED;
      }
    }
  }

  if (!stream->input_plugin) {
    xine_log (stream->xine, XINE_LOG_MSG, _("xine: cannot find input plugin for MRL [%s]\n"),mrl);
    stream->err = XINE_ERROR_NO_INPUT_PLUGIN;
    _x_flush_events_queues (stream);
    free (buf);
    return 0;
  }

  if (args) {
    uint8_t *entry = NULL;

    while (*args) {
      /* Turn "WhatAGreat:Bullshit[;]" into key="whatagreat" entry="WhatAGreat" value="Bullshit". */
      uint8_t *key = buf, *value = NULL;
      if (entry)
        xine_log (stream->xine, XINE_LOG_MSG, _("xine: no value for \"%s\", ignoring.\n"), entry);
      entry = args;
      {
        uint8_t *spos = args + 31, sval = *spos, *q = key, z;
        *spos = 0;
        while (!(tab_parse[z = *args] & 0x19)) *q++ = tab_tolower[z], args++;
        *q = 0;
        *spos = sval;
        while (!(tab_parse[z = *args] & 0x19)) args++;
        if (z == ':') {
          *args++ = 0;
          value = args;
          while (!(tab_parse[z = *args] & 0x11)) args++;
        }
        if (z == ';')
          *args++ = 0;
      }

      if (!memcmp (key, "demux", 6)) {
        if (value) {
          /* demuxer specified by name */
          char *demux_name = (char *)value;
          _x_mrl_unescape (demux_name);
	  if (!(stream->demux_plugin = _x_find_demux_plugin_by_name(stream, demux_name, stream->input_plugin))) {
	    xine_log(stream->xine, XINE_LOG_MSG, _("xine: specified demuxer %s failed to start\n"), demux_name);
	    stream->err = XINE_ERROR_NO_DEMUX_PLUGIN;
	    stream->status = XINE_STATUS_IDLE;
            free (buf);
	    return 0;
	  }

	  _x_meta_info_set_utf8(stream, XINE_META_INFO_SYSTEMLAYER,
				stream->demux_plugin->demux_class->identifier);
          entry = NULL;
        }
	continue;
      }

      if (!memcmp (key, "save", 5)) {
        if (value) {
          /* filename to save */
          char *filename = (char *)value;
	  input_plugin_t *input_saver;

          _x_mrl_unescape (filename);

	  xine_log(stream->xine, XINE_LOG_MSG, _("xine: join rip input plugin\n"));
	  input_saver = _x_rip_plugin_get_instance (stream, filename);

	  if( input_saver ) {
	    stream->input_plugin = input_saver;
	  } else {
	    xprintf(stream->xine, XINE_VERBOSITY_LOG, _("xine: error opening rip input plugin instance\n"));
	    stream->err = XINE_ERROR_MALFORMED_MRL;
	    stream->status = XINE_STATUS_IDLE;
            free (buf);
	    return 0;
	  }
          entry = NULL;
        }
	continue;
      }

      if (!memcmp (key, "lastdemuxprobe", 15)) {
        if (value) {
          /* all demuxers will be probed before the specified one */
          char *demux_name = (char *)value;
          _x_mrl_unescape (demux_name);
	  if (!(stream->demux_plugin = _x_find_demux_plugin_last_probe(stream, demux_name, stream->input_plugin))) {
	    xine_log(stream->xine, XINE_LOG_MSG, _("xine: last_probed demuxer %s failed to start\n"), demux_name);
	    stream->err = XINE_ERROR_NO_DEMUX_PLUGIN;
	    stream->status = XINE_STATUS_IDLE;
            free (buf);
	    return 0;
	  }
	  lprintf ("demux and input plugin found\n");

	  _x_meta_info_set_utf8(stream, XINE_META_INFO_SYSTEMLAYER,
				stream->demux_plugin->demux_class->identifier);
          entry = NULL;
        }
	continue;
      }

      if (!memcmp (key, "novideo", 8)) {
        _x_stream_info_set (stream, XINE_STREAM_INFO_IGNORE_VIDEO, 1);
        xprintf (stream->xine, XINE_VERBOSITY_LOG, _("ignoring video\n"));
        entry = NULL;
	continue;
      }

      if (!memcmp (key, "noaudio", 8)) {
        _x_stream_info_set (stream, XINE_STREAM_INFO_IGNORE_AUDIO, 1);
        xprintf (stream->xine, XINE_VERBOSITY_LOG, _("ignoring audio\n"));
        entry = NULL;
	continue;
      }

      if (!memcmp (key, "nospu", 6)) {
        _x_stream_info_set (stream, XINE_STREAM_INFO_IGNORE_SPU, 1);
        xprintf (stream->xine, XINE_VERBOSITY_LOG, _("ignoring subpicture\n"));
        entry = NULL;
	continue;
      }

      if (!memcmp (key, "nocache", 8)) {
        no_cache = 1;
        xprintf (stream->xine, XINE_VERBOSITY_LOG, _("input cache plugin disabled\n"));
        entry = NULL;
	continue;
      }

      if (!memcmp (key, "volume", 7)) {
        if (value) {
          char *volume = (char *)value;
          _x_mrl_unescape (volume);
	  xine_set_param(stream, XINE_PARAM_AUDIO_VOLUME, atoi(volume));
          entry = NULL;
        }
	continue;
      }

      if (!memcmp (key, "compression", 12)) {
        if (value) {
          char *compression = (char *)value;
          _x_mrl_unescape (compression);
	  xine_set_param(stream, XINE_PARAM_AUDIO_COMPR_LEVEL, atoi(compression));
          entry = NULL;
        }
	continue;
      }

      if (!memcmp (key, "subtitle", 9)) {
        if (value) {
          char *subtitle_mrl = (char *)value;
	  /* unescape for xine_open() if the MRL looks like a raw pathname */
	  if (!_x_path_looks_like_mrl(subtitle_mrl))
	    _x_mrl_unescape(subtitle_mrl);
	  stream->slave = xine_stream_new (stream->xine, NULL, stream->video_out );
	  stream->slave_affection = XINE_MASTER_SLAVE_PLAY | XINE_MASTER_SLAVE_STOP;
	  if( xine_open( stream->slave, subtitle_mrl ) ) {
	    xprintf (stream->xine, XINE_VERBOSITY_LOG, _("subtitle mrl opened '%s'\n"), subtitle_mrl);
	    stream->slave->master = stream;
	    stream->slave_is_subtitle = 1;
	  } else {
	    xprintf(stream->xine, XINE_VERBOSITY_LOG, _("xine: error opening subtitle mrl\n"));
	    xine_dispose( stream->slave );
	    stream->slave = NULL;
	  }
          entry = NULL;
        }
	continue;
      }

      if (value) {
        /* when we got here, the stream setup parameter must be a config entry */
        int retval;
        value[-1] = ':';
        _x_mrl_unescape ((char *)entry);
        retval = _x_config_change_opt (stream->xine->config, (char *)entry);
	if (retval <= 0) {
          value[-1] = 0;
	  if (retval == 0) {
            /* the option not found */
            xine_log (stream->xine, XINE_LOG_MSG, _("xine: unknown config option \"%s\", ignoring.\n"), entry);
	  } else {
            /* not permitted to change from MRL */
            xine_log (stream->xine, XINE_LOG_MSG, _("xine: changing option '%s' from MRL isn't permitted\n"), entry);
	  }
	}
        entry = NULL;
      }
    }
    if (entry)
      xine_log (stream->xine, XINE_LOG_MSG, _("xine: no value for \"%s\", ignoring.\n"), entry);
  }

  free (buf);

  no_cache = no_cache || (stream->input_plugin->get_capabilities(stream->input_plugin) & INPUT_CAP_NO_CACHE);
  if( !no_cache )
    /* enable buffered input plugin (request optimizer) */
    stream->input_plugin = _x_cache_plugin_get_instance(stream);

  /* Let the plugin request a specific demuxer (if the user hasn't).
   * This overrides find-by-content & find-by-extension.
   */
  if (!stream->demux_plugin)
  {
    char *default_demux = NULL;
    stream->input_plugin->get_optional_data (stream->input_plugin, &default_demux, INPUT_OPTIONAL_DATA_DEMUXER);
    if (default_demux)
    {
      stream->demux_plugin = _x_find_demux_plugin_by_name (stream, default_demux, stream->input_plugin);
      if (stream->demux_plugin)
      {
        lprintf ("demux and input plugin found\n");
        _x_meta_info_set_utf8 (stream, XINE_META_INFO_SYSTEMLAYER,
                               stream->demux_plugin->demux_class->identifier);
      }
      else
        xine_log (stream->xine, XINE_LOG_MSG, _("xine: couldn't load plugin-specified demux %s for >%s<\n"), default_demux, mrl);
    }
  }

  if (!stream->demux_plugin) {

    /*
     * find a demux plugin
     */
    if (!(stream->demux_plugin = _x_find_demux_plugin (stream, stream->input_plugin))) {
      xine_log (stream->xine, XINE_LOG_MSG, _("xine: couldn't find demux for >%s<\n"), mrl);
      stream->err = XINE_ERROR_NO_DEMUX_PLUGIN;

      stream->status = XINE_STATUS_IDLE;

      /* force the engine to unregister fifo callbacks */
      _x_demux_control_nop(stream, BUF_FLAG_END_STREAM);

      return 0;
    }
    lprintf ("demux and input plugin found\n");

    _x_meta_info_set_utf8(stream, XINE_META_INFO_SYSTEMLAYER,
			  stream->demux_plugin->demux_class->identifier);
  }

  {
    demux_class_t *demux_class = stream->demux_plugin->demux_class;
    xine_log (stream->xine, XINE_LOG_MSG, _("xine: found demuxer plugin: %s\n"),
              dgettext(demux_class->text_domain ? demux_class->text_domain : XINE_TEXTDOMAIN,
                       demux_class->description));
  }

  _x_extra_info_reset( stream->current_extra_info );
  _x_extra_info_reset( stream->video_decoder_extra_info );
  _x_extra_info_reset( stream->audio_decoder_extra_info );

  /* assume handled for now. we will only know for sure after trying
   * to init decoders (which should happen when headers are sent)
   */
  _x_stream_info_set(stream, XINE_STREAM_INFO_VIDEO_HANDLED, 1);
  _x_stream_info_set(stream, XINE_STREAM_INFO_AUDIO_HANDLED, 1);

  /*
   * send and decode headers
   */

  stream->demux_plugin->send_headers (stream->demux_plugin);

  if (stream->demux_plugin->get_status(stream->demux_plugin) != DEMUX_OK) {
    if (stream->demux_plugin->get_status(stream->demux_plugin) == DEMUX_FINISHED) {
      xine_log (stream->xine, XINE_LOG_MSG, _("xine: demuxer is already done. that was fast!\n"));
    } else {
      xine_log (stream->xine, XINE_LOG_MSG, _("xine: demuxer failed to start\n"));
    }

    _x_free_demux_plugin(stream, stream->demux_plugin);
    stream->demux_plugin = NULL;

    xprintf (stream->xine, XINE_VERBOSITY_DEBUG, "demux disposed\n");

    _x_free_input_plugin(stream, stream->input_plugin);
    stream->input_plugin = NULL;
    stream->err = XINE_ERROR_NO_DEMUX_PLUGIN;

    stream->status = XINE_STATUS_IDLE;

    xprintf (stream->xine, XINE_VERBOSITY_DEBUG, "return from\n");
    return 0;
  }

  _x_demux_control_headers_done (stream);

  stream->status = XINE_STATUS_STOP;

  lprintf ("done\n");
  return 1;
}

int xine_open (xine_stream_t *stream, const char *mrl) {

  int ret;

  pthread_mutex_lock (&stream->frontend_lock);
  pthread_cleanup_push (mutex_cleanup, (void *) &stream->frontend_lock);

  lprintf ("open MRL:%s\n", mrl);

  ret = open_internal (stream, mrl);

  pthread_cleanup_pop (0);
  pthread_mutex_unlock (&stream->frontend_lock);

  return ret;
}

static void wait_first_frame (xine_stream_t *stream) {
  if (stream->video_decoder_plugin) {
    pthread_mutex_lock (&stream->first_frame_lock);
    if (stream->first_frame_flag > 0) {
      struct timespec ts = {0, 0};
      xine_gettime (&ts);
      ts.tv_sec += 10;
      pthread_cond_timedwait(&stream->first_frame_reached, &stream->first_frame_lock, &ts);
    }
    pthread_mutex_unlock (&stream->first_frame_lock);
  }
}

static int play_internal (xine_stream_t *stream, int start_pos, int start_time) {

  int        flush;
  int        first_frame_flag = 3;
  int        demux_status;
  int        demux_thread_running;

  xprintf (stream->xine, XINE_VERBOSITY_DEBUG, "xine_play\n");

  if (!stream->demux_plugin) {
    xine_log (stream->xine, XINE_LOG_MSG, _("xine_play: no demux available\n"));
    stream->err = XINE_ERROR_NO_DEMUX_PLUGIN;

    return 0;
  }

  if (start_pos || start_time) {
    stream->finished_naturally = 0;
    first_frame_flag = 2;
  }
  flush = (stream->master == stream) && !stream->gapless_switch && !stream->finished_naturally;
  if (!flush)
    xprintf (stream->xine, XINE_VERBOSITY_DEBUG, "xine_play: using gapless switch\n");

  /* hint demuxer thread we want to interrupt it */
  _x_action_raise(stream);

  /* set normal speed */
  if (_x_get_speed(stream) != XINE_SPEED_NORMAL)
    set_speed_internal (stream, XINE_FINE_SPEED_NORMAL);

  /* ignore speed changes (net_buf_ctrl causes deadlocks while seeking ...) */
  pthread_mutex_lock(&stream->speed_change_lock);
  stream->ignore_speed_change = 1;
  pthread_mutex_unlock(&stream->speed_change_lock);

  stream->xine->port_ticket->acquire(stream->xine->port_ticket, 1);

  /* only flush/discard output ports on master streams */
  if (flush) {
    /* discard audio/video buffers to get engine going and take the lock faster */
    if (stream->audio_out)
      stream->audio_out->set_property(stream->audio_out, AO_PROP_DISCARD_BUFFERS, 1);
    if (stream->video_out)
      stream->video_out->set_property(stream->video_out, VO_PROP_DISCARD_FRAMES, 1);
  }

  pthread_mutex_lock( &stream->demux_lock );
  /* demux_lock taken. now demuxer is suspended */
  _x_action_lower(stream);
  pthread_cond_signal(&stream->demux_resume);

  /* set normal speed again (now that demuxer/input pair is suspended)
   * some input plugin may have changed speed by itself, we must ensure
   * the engine is not paused.
   */
  if (_x_get_speed(stream) != XINE_SPEED_NORMAL)
    set_speed_internal (stream, XINE_FINE_SPEED_NORMAL);

  /*
   * start/seek demux
   */

  /* seek to new position (no data is sent to decoders yet) */
  demux_status = stream->demux_plugin->seek (stream->demux_plugin,
					     start_pos, start_time,
					     stream->demux_thread_running);

  /* only flush/discard output ports on master streams */
  if (flush) {
    if (stream->audio_out)
      stream->audio_out->set_property(stream->audio_out, AO_PROP_DISCARD_BUFFERS, 0);
    if (stream->video_out)
      stream->video_out->set_property(stream->video_out, VO_PROP_DISCARD_FRAMES, 0);
  } else {
    stream->metronom->handle_audio_discontinuity (stream->metronom, DISC_GAPLESS, 0);
  }

  stream->xine->port_ticket->release(stream->xine->port_ticket, 1);
  pthread_mutex_lock(&stream->speed_change_lock);
  stream->ignore_speed_change = 0;
  pthread_mutex_unlock(&stream->speed_change_lock);

  /* before resuming the demuxer, set first_frame_flag */
  pthread_mutex_lock (&stream->first_frame_lock);
  stream->first_frame_flag = first_frame_flag;
  pthread_mutex_unlock (&stream->first_frame_lock);

  /* before resuming the demuxer, reset current position information */
  pthread_mutex_lock( &stream->current_extra_info_lock );
  _x_extra_info_reset( stream->current_extra_info );
  pthread_mutex_unlock( &stream->current_extra_info_lock );

  demux_thread_running = stream->demux_thread_running;

  /* now resume demuxer thread if it is running already */
  pthread_mutex_unlock( &stream->demux_lock );

  if (demux_status != DEMUX_OK) {
    xine_log (stream->xine, XINE_LOG_MSG, _("xine_play: demux failed to start\n"));

    stream->err = XINE_ERROR_DEMUX_FAILED;
    pthread_mutex_lock (&stream->first_frame_lock);
    stream->first_frame_flag = 0;
    // no need to signal: wait is done only in this function.
    pthread_mutex_unlock (&stream->first_frame_lock);
    return 0;

  } else {
    if (!demux_thread_running) {
      _x_demux_start_thread( stream );
      stream->status = XINE_STATUS_PLAY;
    }
    stream->finished_naturally = 0;
  }


  /* Wait until the first frame produced is displayed
   * see video_out.c
   */
  wait_first_frame (stream);

  xprintf (stream->xine, XINE_VERBOSITY_DEBUG, "play_internal ...done\n");

  return 1;
}

int xine_play (xine_stream_t *stream, int start_pos, int start_time) {

  int ret;

  pthread_mutex_lock (&stream->frontend_lock);
  pthread_cleanup_push (mutex_cleanup, (void *) &stream->frontend_lock);

  stream->delay_finish_event = 0;

  ret = play_internal (stream, start_pos, start_time);
  if( stream->slave && (stream->slave_affection & XINE_MASTER_SLAVE_PLAY) )
    xine_play (stream->slave, start_pos, start_time);

  stream->gapless_switch = 0;

  pthread_cleanup_pop (0);
  pthread_mutex_unlock (&stream->frontend_lock);

  return ret;
}

int xine_eject (xine_stream_t *stream) {

  int status;

  if (!stream->eject_class)
    return 0;

  pthread_mutex_lock (&stream->frontend_lock);
  pthread_cleanup_push (mutex_cleanup, (void *) &stream->frontend_lock);

  status = 0;
  /* only eject, if we are stopped OR a different input plugin is playing */
  if (stream->eject_class && stream->eject_class->eject_media &&
      ((stream->status == XINE_STATUS_STOP) ||
      stream->eject_class != stream->input_plugin->input_class)) {

    status = stream->eject_class->eject_media (stream->eject_class);
  }

  pthread_cleanup_pop (0);
  pthread_mutex_unlock (&stream->frontend_lock);

  return status;
}

static void xine_dispose_internal (xine_stream_t *stream) {

  xine_t *xine = stream->xine;
  xine_list_iterator_t *ite;

  lprintf("stream: %p\n", (void*)stream);

  pthread_mutex_lock (&xine->streams_lock);
  ite = xine_list_find (xine->streams, stream);
  if (ite)
    xine_list_remove (xine->streams, ite);
  /* keep xine instance open for this */
  stream->metronom->exit (stream->metronom);
  pthread_mutex_unlock (&xine->streams_lock);

  pthread_mutex_destroy (&stream->frontend_lock);
  pthread_mutex_destroy (&stream->index_mutex);
  pthread_mutex_destroy (&stream->speed_change_lock);
  pthread_mutex_destroy (&stream->current_extra_info_lock);
  pthread_cond_destroy  (&stream->first_frame_reached);
  pthread_mutex_destroy (&stream->first_frame_lock);
  pthread_cond_destroy  (&stream->counter_changed);
  pthread_mutex_destroy (&stream->counter_lock);
  pthread_mutex_destroy (&stream->event_queues_lock);
  pthread_cond_destroy  (&stream->demux_resume);
  pthread_mutex_destroy (&stream->demux_mutex);
  pthread_mutex_destroy (&stream->demux_action_lock);
  pthread_mutex_destroy (&stream->demux_lock);
  pthread_mutex_destroy (&stream->meta_mutex);
  pthread_mutex_destroy (&stream->info_mutex);

  xine_list_delete(stream->event_queues);

  _x_refcounter_dispose(stream->refcounter);

  free (stream->index_array);
  free (stream);
}

void xine_dispose (xine_stream_t *stream) {
  /* decrease the reference counter
   * if there is no more reference on this stream, the xine_dispose_internal
   * function is called
   */
  xprintf (stream->xine, XINE_VERBOSITY_DEBUG, "xine_dispose\n");
  stream->status = XINE_STATUS_QUIT;

  xine_close(stream);

  if( stream->master != stream ) {
    stream->master->slave = NULL;
  }
  if( stream->slave && stream->slave->master == stream ) {
    stream->slave->master = NULL;
  }

  if(stream->broadcaster)
    _x_close_broadcaster(stream->broadcaster);

  /* Demuxers mpeg, mpeg_block and yuv_frames may send video pool buffers
   * to audio fifo. Fifos should be empty at this point. For safety,
   * shut down audio first anyway.
   */
  xprintf (stream->xine, XINE_VERBOSITY_DEBUG, "shutdown audio\n");
  _x_audio_decoder_shutdown (stream);

  xprintf (stream->xine, XINE_VERBOSITY_DEBUG, "shutdown video\n");
  _x_video_decoder_shutdown (stream);

  if (stream->osd_renderer)
    stream->osd_renderer->close( stream->osd_renderer );

  /* Remove the reference that the stream was created with. */
  _x_refcounter_dec(stream->refcounter);
}

#ifdef WIN32
static int xine_wsa_users = 0;
#endif

void xine_exit (xine_t *this) {
  if (this->streams) {
    int n = 10;
    /* XXX: streams kill themselves via their refcounter hook. */
    while (n--) {
      xine_stream_t *stream = NULL;
      xine_list_iterator_t *ite;

      pthread_mutex_lock (&this->streams_lock);
      ite = xine_list_front (this->streams);
      while (ite) {
        stream = xine_list_get_value (this->streams, ite);
        if (stream && (stream != XINE_ANON_STREAM))
          break;
        ite = xine_list_next (this->streams, ite);
      }
      if (!ite) {
        pthread_mutex_unlock (&this->streams_lock);
        break;
      }
      /* stream->refcounter->lock might be taken already */
      {
        int i = stream->refcounter->count;
        pthread_mutex_unlock (&this->streams_lock);
        xprintf (this, XINE_VERBOSITY_LOG,
                 "xine_exit: BUG: stream %p still open (%d refs), waiting.\n",
                 (void*)stream, i);
      }
      if (n) {
        xine_usec_sleep (50000);
      } else {
#ifdef FORCE_STREAM_SHUTDOWN
        /* might raise even more heap damage, disabled for now */
        xprintf (this, XINE_VERBOSITY_LOG,
                 "xine_exit: closing stream %p.\n",
                 (void*)stream);
        stream->refcounter->count = 1;
        xine_dispose (stream);
        n = 1;
#endif
      }
    }
    xine_list_delete (this->streams);
    pthread_mutex_destroy (&this->streams_lock);
  }

  xprintf (this, XINE_VERBOSITY_DEBUG, "xine_exit: bye!\n");

  _x_dispose_plugins (this);

  if(this->clock)
    this->clock->exit (this->clock);

  if(this->config)
    this->config->dispose(this->config);

  if(this->port_ticket)
    this->port_ticket->dispose(this->port_ticket);

  pthread_mutex_destroy (&this->pause_mutex);

  {
    int i;
    for (i = 0; i < XINE_LOG_NUM; i++)
      if (this->log_buffers[i])
        this->log_buffers[i]->dispose (this->log_buffers[i]);
  }
  pthread_mutex_destroy(&this->log_lock);

#if defined(WIN32)
  if (xine_wsa_users) {
    if (--xine_wsa_users == 0)
      WSACleanup ();
  }
#endif

  xdgWipeHandle(&this->basedir_handle);

  free (this);
}

xine_t *xine_new (void) {
  xine_t      *this;

  this = calloc(1, sizeof (xine_t));
  if (!this)
    return NULL;
#ifndef HAVE_ZERO_SAFE_MEM
  this->plugin_catalog = NULL;
  this->save_path      = NULL;
  this->streams        = NULL;
  this->clock          = NULL;
  this->port_ticket    = NULL;
#endif

#ifdef ENABLE_NLS
  /*
   * i18n
   */

  bindtextdomain(XINE_TEXTDOMAIN, XINE_LOCALEDIR);
#endif

  /*
   * config
   */

  this->config = _x_config_init ();
  if (!this->config) {
    free(this);
    return NULL;
  }

  /*
   * log buffers
   */
  memset(this->log_buffers, 0, sizeof(this->log_buffers));
  pthread_mutex_init (&this->log_lock, NULL);

  this->live_pause = 0;
  pthread_mutex_init (&this->pause_mutex, NULL);


#ifdef WIN32
  if (!xine_wsa_users) {
    /* WinSock Library Init. */
    WSADATA Data;
    int i_err = WSAStartup (MAKEWORD (1, 1), &Data);
    if (i_err) {
      fprintf (stderr, "error: can't initiate WinSocks, error %i\n", i_err);
    } else {
      xine_wsa_users++;
    }
  } else {
    xine_wsa_users++;
  }
#endif /* WIN32 */

  this->verbosity = XINE_VERBOSITY_NONE;

  return this;
}

void xine_engine_set_param(xine_t *this, int param, int value) {

  if(this) {
    switch(param) {

    case XINE_ENGINE_PARAM_VERBOSITY:
      this->verbosity = value;
      break;

    default:
      lprintf("Unknown parameter %d\n", param);
      break;
    }
  }
}

int xine_engine_get_param(xine_t *this, int param) {

  if(this) {
    switch(param) {

    case XINE_ENGINE_PARAM_VERBOSITY:
      return this->verbosity;
      break;

    default:
      lprintf("Unknown parameter %d\n", param);
      break;
    }
  }
  return -1;
}

static void config_demux_strategy_cb (void *this_gen, xine_cfg_entry_t *entry) {
  xine_t *this = (xine_t *)this_gen;

  this->demux_strategy = entry->num_value;
}

static void config_save_cb (void *this_gen, xine_cfg_entry_t *entry) {
  xine_t *this = (xine_t *)this_gen;
  char homedir_trail_slash[strlen(xine_get_homedir()) + 2];

  sprintf(homedir_trail_slash, "%s/", xine_get_homedir());
  if (entry->str_value[0] &&
      (entry->str_value[0] != '/' || strstr(entry->str_value, "/.") ||
       strcmp(entry->str_value, xine_get_homedir()) == 0 ||
       strcmp(entry->str_value, homedir_trail_slash) == 0)) {
    xine_stream_t *stream;
    xine_list_iterator_t ite;

    xine_log(this, XINE_LOG_MSG,
	     _("xine: The specified save_dir \"%s\" might be a security risk.\n"), entry->str_value);

    pthread_mutex_lock(&this->streams_lock);
    if ( (ite = xine_list_front(this->streams)) ) {
      stream = xine_list_get_value(this->streams, ite);
      _x_message(stream, XINE_MSG_SECURITY, _("The specified save_dir might be a security risk."), NULL);
    }
    pthread_mutex_unlock(&this->streams_lock);
  }

  this->save_path = entry->str_value;
}

void xine_set_flags (xine_t *this, int flags)
{
  this->flags = flags;
}

void xine_init (xine_t *this) {
  static const char *const demux_strategies[] = {"default", "reverse", "content",
						 "extension", NULL};

  /* First of all, initialise libxdg-basedir as it's used by plugins. */
  setenv ("HOME", xine_get_homedir (), 0); /* libxdg-basedir needs $HOME */
  xdgInitHandle(&this->basedir_handle);

  /* debug verbosity override */
  {
    int v = 0;
    const uint8_t *s = (const uint8_t *)getenv ("LIBXINE_VERBOSITY"), *t = s;
    uint8_t z;
    if (s) {
      while ((z = *s++ ^ '0') < 10)
        v = 10 * v + z;
      if (s > t + 1)
        this->verbosity = v;
    }
  }

  /*
   * locks
   */
  pthread_mutex_init (&this->streams_lock, NULL);

  /* initialize color conversion tables and functions */
  init_yuv_conversion();

  /* probe for optimized memcpy or config setting */
  xine_probe_fast_memcpy (this);

  /*
   * plugins
   */
  XINE_PROFILE(_x_scan_plugins(this));

#ifdef HAVE_SETLOCALE
  if (!setlocale(LC_CTYPE, ""))
    xprintf(this, XINE_VERBOSITY_LOG, _("xine: locale not supported by C library\n"));
#endif

  /*
   * content detection strategy
   */
  this->demux_strategy  = this->config->register_enum (
      this->config, "engine.demux.strategy", 0,
      (char **)demux_strategies,
      _("media format detection strategy"),
      _("xine offers various methods to detect the media format of input to play. "
	"The individual values are:\n\n"
	"default\n"
	"First try to detect by content, then by file name extension.\n\n"
	"reverse\n"
	"First try to detect by file name extension, then by content.\n\n"
	"content\n"
	"Detect by content only.\n\n"
	"extension\n"
	"Detect by file name extension only.\n"),
      20, config_demux_strategy_cb, this);

  /*
   * save directory
   */
  this->save_path  = this->config->register_filename (
      this->config,
      "media.capture.save_dir", "", XINE_CONFIG_STRING_IS_DIRECTORY_NAME,
      _("directory for saving streams"),
      _("When using the stream save feature, files will be written only into this directory.\n"
	"This setting is security critical, because when changed to a different directory, xine "
	"can be used to fill files in it with arbitrary content. So you should be careful that "
	"the directory you specify is robust against any content in any file."),
      XINE_CONFIG_SECURITY, config_save_cb, this);

  /*
   * implicit configuration changes
   */
  this->config->register_bool(this->config,
      "misc.implicit_config", 0,
      _("allow implicit changes to the configuration (e.g. by MRL)"),
      _("If enabled, you allow xine to change your configuration without "
	"explicit actions from your side. For example configuration changes "
	"demanded by MRLs or embedded into playlist will be executed.\n"
	"This setting is security critcal, because xine can receive MRLs or "
	"playlists from untrusted remote sources. If you allow them to "
	"arbitrarily change your configuration, you might end with a totally "
	"messed up xine."),
      XINE_CONFIG_SECURITY, NULL, this);

  /*
   * timeout for network I/O to avoid freezes
   */
  this->config->register_num(this->config,
      "media.network.timeout", 30,
      _("Timeout for network stream reading (in seconds)"),
      _("Specifies the timeout when reading from network streams, in seconds. "
	"Too low values might stop streaming when the source is slow or the "
	"bandwidth is occupied, too high values will freeze the player if the "
	"connection is lost."),
      0, NULL, this);

  /*
   * keep track of all opened streams
   */
  this->streams = xine_list_new();

  /*
   * start metronom clock
   */

  this->clock = _x_metronom_clock_init(this);

  this->clock->start_clock (this->clock, 0);

  /*
   * tickets
   */
  this->port_ticket = ticket_init();
}

void _x_select_spu_channel (xine_stream_t *stream, int channel) {

  pthread_mutex_lock (&stream->frontend_lock);
  stream->spu_channel_user = (channel >= -2 ? channel : -2);

  stream->xine->port_ticket->acquire(stream->xine->port_ticket, 1);

  switch (stream->spu_channel_user) {
  case -2:
    stream->spu_channel = -1;
    if(stream->video_out)
      stream->video_out->enable_ovl (stream->video_out, 0);
    break;
  case -1:
    stream->spu_channel = stream->spu_channel_auto;
    if(stream->video_out)
      stream->video_out->enable_ovl (stream->video_out, 1);
    break;
  default:
    stream->spu_channel = stream->spu_channel_user;
    if(stream->video_out)
      stream->video_out->enable_ovl (stream->video_out, 1);
  }
  lprintf("set to %d\n",stream->spu_channel);

  stream->xine->port_ticket->release(stream->xine->port_ticket, 1);

  pthread_mutex_unlock (&stream->frontend_lock);
}

static int get_current_position (xine_stream_t *stream) {

  int pos;

  pthread_mutex_lock (&stream->frontend_lock);

  if (!stream->input_plugin) {
    lprintf ("no input source\n");
    pthread_mutex_unlock (&stream->frontend_lock);
    return -1;
  }

  if ( (!stream->video_decoder_plugin && !stream->audio_decoder_plugin) ) {
    if( _x_stream_info_get(stream, XINE_STREAM_INFO_HAS_VIDEO) )
      _x_extra_info_merge( stream->current_extra_info, stream->video_decoder_extra_info );
    else
      _x_extra_info_merge( stream->current_extra_info, stream->audio_decoder_extra_info );
  }

  if ( stream->current_extra_info->seek_count != stream->video_seek_count ) {
    pthread_mutex_unlock (&stream->frontend_lock);
    return -1; /* position not yet known */
  }

  pthread_mutex_lock( &stream->current_extra_info_lock );
  pos = stream->current_extra_info->input_normpos;
  pthread_mutex_unlock( &stream->current_extra_info_lock );

  pthread_mutex_unlock (&stream->frontend_lock);

  return pos;
}

void _x_get_current_info (xine_stream_t *stream, extra_info_t *extra_info, int size) {

  pthread_mutex_lock( &stream->current_extra_info_lock );
  memcpy( extra_info, stream->current_extra_info, size );
  pthread_mutex_unlock( &stream->current_extra_info_lock );
}


int xine_get_status (xine_stream_t *stream) {
  return stream->status;
}

/*
 * trick play
 */

void _x_set_fine_speed (xine_stream_t *stream, int speed) {
  pthread_mutex_lock(&stream->speed_change_lock);

  if (!stream->ignore_speed_change)
  {
    if (speed <= XINE_SPEED_PAUSE)
      speed = XINE_SPEED_PAUSE;

    xprintf (stream->xine, XINE_VERBOSITY_DEBUG, "set_speed %d\n", speed);
    set_speed_internal (stream, speed);

    if (stream->slave && (stream->slave_affection & XINE_MASTER_SLAVE_SPEED))
      set_speed_internal (stream->slave, speed);
  }
  pthread_mutex_unlock(&stream->speed_change_lock);
}

int _x_get_fine_speed (xine_stream_t *stream) {
  return stream->xine->clock->speed;
}

void _x_set_speed (xine_stream_t *stream, int speed) {

  if (speed > XINE_SPEED_FAST_4)
    speed = XINE_SPEED_FAST_4;

  _x_set_fine_speed (stream, speed * XINE_FINE_SPEED_NORMAL / XINE_SPEED_NORMAL);
}

int _x_get_speed (xine_stream_t *stream) {
  int speed = _x_get_fine_speed (stream);

  /*
   * ensure compatibility with old API, only valid XINE_SPEED_xxx
   * constants are allowed. XINE_SPEED_NORMAL may only be returned
   * if speed is exactly XINE_FINE_SPEED_NORMAL.
   */

  if( speed <= XINE_SPEED_PAUSE )
    return XINE_SPEED_PAUSE;
  if( speed <= XINE_SPEED_SLOW_4 * XINE_FINE_SPEED_NORMAL / XINE_SPEED_NORMAL )
    return XINE_SPEED_SLOW_4;
  if( speed < XINE_FINE_SPEED_NORMAL )
    return XINE_SPEED_SLOW_2;
  if( speed == XINE_FINE_SPEED_NORMAL )
    return XINE_SPEED_NORMAL;
  if( speed <= XINE_SPEED_FAST_2 * XINE_FINE_SPEED_NORMAL / XINE_SPEED_NORMAL )
    return XINE_SPEED_FAST_2;
  return XINE_SPEED_FAST_4;
}


/*
 * time measurement / seek
 */

static int get_stream_length (xine_stream_t *stream) {

  /* pthread_mutex_lock( &stream->demux_lock ); */

  if (stream->demux_plugin) {
    int len = stream->demux_plugin->get_stream_length (stream->demux_plugin);
    /* pthread_mutex_unlock( &stream->demux_lock ); */

    return len;
  }

  /* pthread_mutex_unlock( &stream->demux_lock ); */

  return 0;
}

int xine_get_pos_length (xine_stream_t *stream, int *pos_stream,
			 int *pos_time, int *length_time) {

  int pos = get_current_position (stream); /* force updating extra_info */

  if (pos == -1)
    return 0;

  if (pos_stream)
    *pos_stream  = pos;
  if (pos_time) {
    pthread_mutex_lock( &stream->current_extra_info_lock );
    *pos_time    = stream->current_extra_info->input_time;
    pthread_mutex_unlock( &stream->current_extra_info_lock );
  }
  if (length_time)
    *length_time = get_stream_length (stream);

  return 1;
}

static int _x_get_current_frame_data (xine_stream_t *stream,
				      xine_current_frame_data_t *data,
				      int flags, int img_size_unknown) {

  vo_frame_t *frame;
  size_t required_size = 0;

  stream->xine->port_ticket->acquire(stream->xine->port_ticket, 1);
  frame = stream->video_out->get_last_frame (stream->video_out);
  stream->xine->port_ticket->release(stream->xine->port_ticket, 1);

  if (!frame) {
    data->img_size = 0;
    return 0;
  }

  data->width       = frame->width;
  data->height      = frame->height;
  data->crop_left   = frame->crop_left;
  data->crop_right  = frame->crop_right;
  data->crop_top    = frame->crop_top;
  data->crop_bottom = frame->crop_bottom;

  data->ratio_code = 10000.0 * frame->ratio;
  /* make ratio_code backward compatible */
#define RATIO_LIKE(a, b)  ((b) - 1 <= (a) && (a) <= 1 + (b))
  if (RATIO_LIKE(data->ratio_code, 10000))
    data->ratio_code = XINE_VO_ASPECT_SQUARE;
  else if (RATIO_LIKE(data->ratio_code, 13333))
    data->ratio_code = XINE_VO_ASPECT_4_3;
  else if (RATIO_LIKE(data->ratio_code, 17778))
    data->ratio_code = XINE_VO_ASPECT_ANAMORPHIC;
  else if (RATIO_LIKE(data->ratio_code, 21100))
    data->ratio_code = XINE_VO_ASPECT_DVB;

  data->format     = frame->format;
  data->interlaced = frame->progressive_frame ? 0 : (2 - frame->top_field_first);

  switch (frame->format) {

  default:
    if (frame->proc_provide_standard_frame_data) {
      uint8_t *img = data->img;
      size_t img_size = data->img_size;
      data->img = 0;
      data->img_size = 0;

      /* ask frame implementation for required img buffer size */
      frame->proc_provide_standard_frame_data(frame, data);
      required_size = data->img_size;

      data->img = img;
      data->img_size = img_size;
      break;
    }

    if (!data->img && !(flags & XINE_FRAME_DATA_ALLOCATE_IMG))
      break; /* not interested in image data */

    xprintf (stream->xine, XINE_VERBOSITY_DEBUG,
	     "xine: error, snapshot function not implemented for format 0x%x\n", frame->format);
    /* fall through and provide "green" YV12 image */
    data->format = XINE_IMGFMT_YV12;
    /* fall through */
  case XINE_IMGFMT_YV12:
    required_size = frame->width * frame->height
                  + ((frame->width + 1) / 2) * ((frame->height + 1) / 2)
                  + ((frame->width + 1) / 2) * ((frame->height + 1) / 2);
    break;

  case XINE_IMGFMT_YUY2:
    required_size = frame->width * frame->height
                  + ((frame->width + 1) / 2) * frame->height
                  + ((frame->width + 1) / 2) * frame->height;
    break;

  }

  if (flags & XINE_FRAME_DATA_ALLOCATE_IMG) {
    /* return allocated buffer size */
    data->img_size = required_size;
    /* allocate img or fail */
    if (!(data->img = calloc(1, required_size))) {
      frame->free(frame);
      return 0;
    }
  } else {
    /* fail if supplied buffer is to small */
    if (data->img && !img_size_unknown && data->img_size < required_size) {
      data->img_size = required_size;
      frame->free(frame);
      return 0;
    }
    /* return used buffer size */
    data->img_size = required_size;
  }

  if (data->img) {
    switch (frame->format) {

    case XINE_IMGFMT_YV12:
      yv12_to_yv12(
       /* Y */
        frame->base[0], frame->pitches[0],
        data->img, frame->width,
       /* U */
        frame->base[1], frame->pitches[1],
        data->img+frame->width*frame->height, frame->width/2,
       /* V */
        frame->base[2], frame->pitches[2],
        data->img+frame->width*frame->height+frame->width*frame->height/4, frame->width/2,
       /* width x height */
        frame->width, frame->height);
      break;

    case XINE_IMGFMT_YUY2:
      yuy2_to_yuy2(
       /* src */
        frame->base[0], frame->pitches[0],
       /* dst */
        data->img, frame->width*2,
       /* width x height */
        frame->width, frame->height);
      break;

    default:
      if (frame->proc_provide_standard_frame_data)
        frame->proc_provide_standard_frame_data(frame, data);
      else if (!(flags & XINE_FRAME_DATA_ALLOCATE_IMG))
        memset(data->img, 0, data->img_size);
    }
  }

  frame->free(frame);
  return 1;
}

int xine_get_current_frame_data (xine_stream_t *stream,
				 xine_current_frame_data_t *data,
				 int flags) {

  return _x_get_current_frame_data(stream, data, flags, 0);
}

int xine_get_current_frame_alloc (xine_stream_t *stream, int *width, int *height,
				  int *ratio_code, int *format,
				  uint8_t **img, int *img_size) {

  int result;
  xine_current_frame_data_t data;

  memset(&data, 0, sizeof (data));

  result = _x_get_current_frame_data(stream, &data, img ? XINE_FRAME_DATA_ALLOCATE_IMG : 0, 0);
  if (width)      *width      = data.width;
  if (height)     *height     = data.height;
  if (ratio_code) *ratio_code = data.ratio_code;
  if (format)     *format     = data.format;
  if (img_size)   *img_size   = data.img_size;
  if (img)        *img        = data.img;
  return result;
}

int xine_get_current_frame_s (xine_stream_t *stream, int *width, int *height,
				int *ratio_code, int *format,
				uint8_t *img, int *img_size) {
  int result;
  xine_current_frame_data_t data;

  memset(&data, 0, sizeof (data));
  data.img = img;
  if (img_size)
    data.img_size = *img_size;

  result = _x_get_current_frame_data(stream, &data, 0, 0);
  if (width)      *width      = data.width;
  if (height)     *height     = data.height;
  if (ratio_code) *ratio_code = data.ratio_code;
  if (format)     *format     = data.format;
  if (img_size)   *img_size   = data.img_size;
  return result;
}

int xine_get_current_frame (xine_stream_t *stream, int *width, int *height,
			    int *ratio_code, int *format,
			    uint8_t *img) {
  int result;
  xine_current_frame_data_t data;

  memset(&data, 0, sizeof (data));
  data.img = img;

  result = _x_get_current_frame_data(stream, &data, 0, 1);
  if (width)      *width      = data.width;
  if (height)     *height     = data.height;
  if (ratio_code) *ratio_code = data.ratio_code;
  if (format)     *format     = data.format;
  return result;
}

xine_grab_video_frame_t* xine_new_grab_video_frame (xine_stream_t *stream) {
  xine_grab_video_frame_t *frame;

  if (stream->video_out->driver->new_grab_video_frame)
    frame = stream->video_out->driver->new_grab_video_frame(stream->video_out->driver);
  else
    frame = stream->video_out->new_grab_video_frame(stream->video_out);

  return frame;
}

int xine_get_spu_lang (xine_stream_t *stream, int channel, char *lang) {

  /* Ask the demuxer first (e.g. TS extracts this information from
   * the stream)
   **/
  if (stream->demux_plugin) {
    if (stream->demux_plugin->get_capabilities (stream->demux_plugin) & DEMUX_CAP_SPULANG) {
      /* pass the channel number to the plugin in the data field */
      memcpy(lang, &channel, sizeof(channel));
      if (stream->demux_plugin->get_optional_data (stream->demux_plugin, lang,
	  DEMUX_OPTIONAL_DATA_SPULANG) == DEMUX_OPTIONAL_SUCCESS)
        return 1;
    }
  }

  /* No match, check with input plugin instead (e.g. DVD gets this
   * info from the IFO).
   **/
  if (stream->input_plugin) {
    if (stream->input_plugin->get_capabilities (stream->input_plugin) & INPUT_CAP_SPULANG) {
      /* pass the channel number to the plugin in the data field */
      memcpy(lang, &channel, sizeof(channel));
      if (stream->input_plugin->get_optional_data (stream->input_plugin, lang,
	  INPUT_OPTIONAL_DATA_SPULANG) == INPUT_OPTIONAL_SUCCESS)
        return 1;
    }
  }

  return 0;
}

int xine_get_audio_lang (xine_stream_t *stream, int channel, char *lang) {

  if (stream->demux_plugin) {
    if (stream->demux_plugin->get_capabilities (stream->demux_plugin) & DEMUX_CAP_AUDIOLANG) {
      /* pass the channel number to the plugin in the data field */
      memcpy(lang, &channel, sizeof(channel));
      if (stream->demux_plugin->get_optional_data (stream->demux_plugin, lang,
	  DEMUX_OPTIONAL_DATA_AUDIOLANG) == DEMUX_OPTIONAL_SUCCESS)
        return 1;
    }
  }

  if (stream->input_plugin) {
    if (stream->input_plugin->get_capabilities (stream->input_plugin) & INPUT_CAP_AUDIOLANG) {
      /* pass the channel number to the plugin in the data field */
      memcpy(lang, &channel, sizeof(channel));
      if (stream->input_plugin->get_optional_data (stream->input_plugin, lang,
	  INPUT_OPTIONAL_DATA_AUDIOLANG) == INPUT_OPTIONAL_SUCCESS)
        return 1;
    }
  }

  return 0;
}

int _x_get_spu_channel (xine_stream_t *stream) {
  return stream->spu_channel_user;
}

int _x_get_video_streamtype (xine_stream_t *stream) {
  return stream->video_decoder_streamtype;
}

/*
 * log functions
 */
int xine_get_log_section_count (xine_t *this) {
  return XINE_LOG_NUM;
}

const char *const *xine_get_log_names (xine_t *this) {
  static const char *log_sections[XINE_LOG_NUM + 1];

  log_sections[XINE_LOG_MSG]      = _("messages");
  log_sections[XINE_LOG_PLUGIN]   = _("plugin");
  log_sections[XINE_LOG_TRACE]    = _("trace");
  log_sections[XINE_LOG_NUM]      = NULL;

  return log_sections;
}

static inline void check_log_alloc (xine_t *this, int buf)
{
  if ( this->log_buffers[buf] )
    return;

  pthread_mutex_lock (&this->log_lock);

  if ( ! this->log_buffers[buf] )
    this->log_buffers[buf] = _x_new_scratch_buffer(150);

  pthread_mutex_unlock (&this->log_lock);
}

void xine_log (xine_t *this, int buf, const char *format, ...) {
  va_list argp;
  char    buffer[SCRATCH_LINE_LEN_MAX];

  check_log_alloc (this, buf);

  va_start (argp, format);
  this->log_buffers[buf]->scratch_printf (this->log_buffers[buf], format, argp);
  va_end(argp);

  if(this->verbosity) {
    va_start(argp, format);
    vsnprintf(buffer, SCRATCH_LINE_LEN_MAX, format, argp);
    printf("%s", buffer);
    va_end (argp);
  }

  if (this->log_cb)
    this->log_cb (this->log_cb_user_data, buf);
}

void xine_vlog(xine_t *this, int buf, const char *format,
                va_list args)
{
  check_log_alloc (this, buf);

  this->log_buffers[buf]->scratch_printf(this->log_buffers[buf], format, args);

  if (this->log_cb)
    this->log_cb (this->log_cb_user_data, buf);
}

char *const *xine_get_log (xine_t *this, int buf) {

  if(buf >= XINE_LOG_NUM)
    return NULL;

  if ( this->log_buffers[buf] )
    return this->log_buffers[buf]->get_content (this->log_buffers[buf]);
  else
    return NULL;
}

void xine_register_log_cb (xine_t *this, xine_log_cb_t cb, void *user_data) {
  this->log_cb = cb;
  this->log_cb_user_data = user_data;
}

int xine_get_error (xine_stream_t *stream) {
  return stream->err;
}

int xine_stream_master_slave(xine_stream_t *master, xine_stream_t *slave,
                         int affection) {
  master->slave = slave;
  master->slave_affection = affection;
  /* respect transitivity: if our designated master already has a master
   * of its own, we point to this master's master; if our master is a
   * standalone stream, its master pointer will point to itself */
  slave->master = master->master;
  return 1;
}

int _x_query_buffer_usage(xine_stream_t *stream, int *num_video_buffers, int *num_audio_buffers, int *num_video_frames, int *num_audio_frames)
{
  int ticket_acquired = -1;

  if (num_video_buffers)
    *num_video_buffers = (stream->video_fifo ? stream->video_fifo->size(stream->video_fifo) : 0);

  if (num_audio_buffers)
    *num_audio_buffers = (stream->audio_fifo ? stream->audio_fifo->size(stream->audio_fifo) : 0);

  if ((num_video_frames && stream->video_out)
    || (num_audio_frames && stream->audio_out)) {

    ticket_acquired = stream->xine->port_ticket->acquire_nonblocking(stream->xine->port_ticket, 1);
  }

  if (num_video_frames)
    *num_video_frames = ((ticket_acquired && stream->video_out) ? stream->video_out->get_property(stream->video_out, VO_PROP_BUFS_IN_FIFO) : 0);

  if (num_audio_frames)
    *num_audio_frames = ((ticket_acquired && stream->audio_out) ? stream->audio_out->get_property(stream->audio_out, AO_PROP_BUFS_IN_FIFO) : 0);

  if (ticket_acquired > 0)
    stream->xine->port_ticket->release_nonblocking(stream->xine->port_ticket, 1);

  return ticket_acquired != 0;
}

static void _x_query_buffers_fix_data(xine_query_buffers_data_t *data)
{
  if (data->total < 0)
    data->total = 0;

  if (data->ready < 0)
    data->ready = 0;

  if (data->avail < 0)
    data->avail = 0;

  /* fix race condition of not filling data atomically */
  if (data->ready + data->avail > data->total)
    data->avail = data->total - data->ready;
}

int _x_query_buffers(xine_stream_t *stream, xine_query_buffers_t *query)
{
  int ticket_acquired = -1;

  memset(query, 0, sizeof (*query));

  if (stream->video_fifo)
  {
    query->vi.total = stream->video_fifo->buffer_pool_capacity;
    query->vi.ready = stream->video_fifo->size(stream->video_fifo);
    query->vi.avail = stream->video_fifo->num_free(stream->video_fifo);
    _x_query_buffers_fix_data(&query->vi);
  }

  if (stream->audio_fifo)
  {
    query->ai.total = stream->audio_fifo->buffer_pool_capacity;
    query->ai.ready = stream->audio_fifo->size(stream->audio_fifo);
    query->ai.avail = stream->audio_fifo->num_free(stream->audio_fifo);
    _x_query_buffers_fix_data(&query->ai);
  }

  if (stream->video_out || stream->audio_out)
    ticket_acquired = stream->xine->port_ticket->acquire_nonblocking(stream->xine->port_ticket, 1);

  if (ticket_acquired > 0)
  {
    if (stream->video_out)
    {
      query->vo.total = stream->video_out->get_property(stream->video_out, VO_PROP_BUFS_TOTAL);
      query->vo.ready = stream->video_out->get_property(stream->video_out, VO_PROP_BUFS_IN_FIFO);
      query->vo.avail = stream->video_out->get_property(stream->video_out, VO_PROP_BUFS_FREE);
    }

    if (stream->audio_out)
    {
      query->ao.total = stream->audio_out->get_property(stream->audio_out, AO_PROP_BUFS_TOTAL);
      query->ao.ready = stream->audio_out->get_property(stream->audio_out, AO_PROP_BUFS_IN_FIFO);
      query->ao.avail = stream->audio_out->get_property(stream->audio_out, AO_PROP_BUFS_FREE);
    }

    stream->xine->port_ticket->release_nonblocking(stream->xine->port_ticket, 1);
  }

  return ticket_acquired != 0;
}

int _x_lock_port_rewiring(xine_t *xine, int ms_timeout)
{
  return xine->port_ticket->lock_port_rewiring(xine->port_ticket, ms_timeout);
}

void _x_unlock_port_rewiring(xine_t *xine)
{
  xine->port_ticket->unlock_port_rewiring(xine->port_ticket);
}

int _x_lock_frontend(xine_stream_t *stream, int ms_to_time_out) {
  return lock_timeout (&stream->frontend_lock, ms_to_time_out);
}

void _x_unlock_frontend(xine_stream_t *stream)
{
  pthread_mutex_unlock(&stream->frontend_lock);
}

int _x_query_unprocessed_osd_events(xine_stream_t *stream)
{
  video_overlay_manager_t *ovl;
  int redraw_needed;

  if (!stream->xine->port_ticket->acquire_nonblocking(stream->xine->port_ticket, 1))
    return -1;

  ovl = stream->video_out->get_overlay_manager(stream->video_out);
  redraw_needed = ovl->redraw_needed(ovl, 0);

  if (redraw_needed)
    stream->video_out->trigger_drawing(stream->video_out);

  stream->xine->port_ticket->release_nonblocking(stream->xine->port_ticket, 1);

  return redraw_needed;
}

int _x_demux_seek(xine_stream_t *stream, off_t start_pos, int start_time, int playing)
{
  if (!stream->demux_plugin)
    return -1;
  return stream->demux_plugin->seek(stream->demux_plugin, start_pos, start_time, playing);
}

int _x_continue_stream_processing(xine_stream_t *stream)
{
  return stream->status != XINE_STATUS_STOP
    && stream->status != XINE_STATUS_QUIT;
}

void _x_trigger_relaxed_frame_drop_mode(xine_stream_t *stream)
{
  pthread_mutex_lock (&stream->first_frame_lock);
  stream->first_frame_flag = 2;
  pthread_mutex_unlock (&stream->first_frame_lock);
}

void _x_reset_relaxed_frame_drop_mode(xine_stream_t *stream)
{
  pthread_mutex_lock (&stream->first_frame_lock);
  stream->first_frame_flag = 1;
  pthread_mutex_unlock (&stream->first_frame_lock);
}


#define KF_BITS 10
#define KF_SIZE (1 << KF_BITS)
#define KF_MASK (KF_SIZE - 1)

int xine_keyframes_find (xine_stream_t *stream, xine_keyframes_entry_t *pos, int offs) {
  if (!stream || (stream == XINE_ANON_STREAM) || !pos)
    return 2;

  pthread_mutex_lock (&stream->index_mutex);
  if (!stream->index_array || !stream->index_used) {
    pthread_mutex_unlock (&stream->index_mutex);
    return 2;
  }
  /* binary search the index */
  {
    xine_keyframes_entry_t *t = stream->index_array;
    int d, a = 0, e = stream->index_used, m = e >> 1, l;
    if ((pos->normpos > 0) && (pos->normpos < 0x10000)) {
      do {
        d = t[m].normpos - pos->normpos;
        if (d == 0)
          break;
        if (d > 0)
          e = m;
        else
          a = m;
        l = m;
        m = (a + e) >> 1;
      } while (m != l);
      if ((offs == 0) && (m + 1 < stream->index_used) &&
        (pos->normpos >= ((t[m].normpos + t[m + 1].normpos) >> 1)))
        m++;
    } else {
      do {
        d = t[m].msecs - pos->msecs;
        if (d == 0)
          break;
        if (d > 0)
          e = m;
        else
          a = m;
        l = m;
        m = (a + e) >> 1;
      } while (m != l);
      if ((offs == 0) && (m + 1 < stream->index_used) &&
        (pos->msecs >= ((t[m].msecs + t[m + 1].msecs) >> 1)))
        m++;
    }
    e = 0;
    if ((offs < 0) && (d != 0))
      offs++;
    m += offs;
    if (m < 0) {
      m = 0;
      e = 1;
    } else if (m >= stream->index_used) {
      m = stream->index_used - 1;
      e = 1;
    }
    *pos = t[m];
    pthread_mutex_unlock (&stream->index_mutex);
    return e;
  }
}

int _x_keyframes_add (xine_stream_t *stream, xine_keyframes_entry_t *pos) {
  xine_keyframes_entry_t *t;
  pthread_mutex_lock (&stream->index_mutex);
  /* first ever entry */
  t = stream->index_array;
  if (!t) {
    t = calloc (KF_SIZE, sizeof (*t));
    if (!t) {
      pthread_mutex_unlock (&stream->index_mutex);
      return -1;
    }
    t[0] = *pos;
    stream->index_array = t;
    stream->index_lastadd = 0;
    stream->index_used = 1;
    stream->index_size = KF_SIZE;
    pthread_mutex_unlock (&stream->index_mutex);
    xprintf (stream->xine, XINE_VERBOSITY_DEBUG,
      "keyframes: build index while playing.\n");
    return 0;
  }
  /* enlarge buf */
  if (stream->index_used + 1 >= stream->index_size) {
    t = realloc (stream->index_array, (stream->index_size + KF_SIZE) * sizeof (*t));
    if (!t) {
      pthread_mutex_unlock (&stream->index_mutex);
      return -1;
    }
    stream->index_array = t;
    stream->index_size += KF_SIZE;
  }
  /* binary search seek target */
  {
    /* fast detect the most common "progressive" case */
    int d, a = 0, m = stream->index_lastadd, l, e = stream->index_used;
    if (m + 1 < e)
      m++;
    do {
      d = t[m].msecs - pos->msecs;
      if (abs (d) < 10) {
        t[m] = *pos; /* already known */
        pthread_mutex_unlock (&stream->index_mutex);
        return m;
      }
      if (d > 0)
        e = m;
      else
        a = m;
      l = m;
      m = (a + e) >> 1;
    } while (m != l);
    if (d < 0)
      m++;
    if (m < stream->index_used) /* insert */
      memmove (&t[m + 1], &t[m], (stream->index_used - m) * sizeof (*t));
    stream->index_used++;
    stream->index_lastadd = m;
    t[m] = *pos;
    pthread_mutex_unlock (&stream->index_mutex);
    return m;
  }
}

xine_keyframes_entry_t *xine_keyframes_get (xine_stream_t *stream, int *size) {
  xine_keyframes_entry_t *ret;
  if (!stream || (stream == XINE_ANON_STREAM) || !size)
    return NULL;
  pthread_mutex_lock (&stream->index_mutex);
  if (stream->index_array && stream->index_used) {
    ret = malloc (stream->index_used * sizeof (xine_keyframes_entry_t));
    if (ret) {
      memcpy (ret, stream->index_array, stream->index_used * sizeof (xine_keyframes_entry_t));
      *size = stream->index_used;
    }
  } else {
    ret = NULL;
    *size = 0;
  }
  pthread_mutex_unlock (&stream->index_mutex);
  return ret;
}

int _x_keyframes_set (xine_stream_t *stream, xine_keyframes_entry_t *list, int size) {
  int n = (size + KF_MASK) & ~KF_MASK;
  pthread_mutex_lock (&stream->index_mutex);
  if (stream->index_array) {
    xprintf (stream->xine, XINE_VERBOSITY_DEBUG,
      "keyframes: deleting index.\n");
    free (stream->index_array);
  }
  stream->index_lastadd = 0;
  stream->index_array = (list && (n > 0)) ? malloc (n * sizeof (xine_keyframes_entry_t)) : NULL;
  if (!stream->index_array) {
    stream->index_used = 0;
    stream->index_size = 0;
    pthread_mutex_unlock (&stream->index_mutex);
    return 1;
  }
  memcpy (stream->index_array, list, size * sizeof (xine_keyframes_entry_t));
  stream->index_used = size;
  stream->index_size = n;
  n -= size;
  if (n > 0)
    memset (stream->index_array + size, 0, n * sizeof (xine_keyframes_entry_t));
  pthread_mutex_unlock (&stream->index_mutex);
  xprintf (stream->xine, XINE_VERBOSITY_DEBUG,
    "keyframes: got %d of them.\n", stream->index_used);
  return 0;
}

