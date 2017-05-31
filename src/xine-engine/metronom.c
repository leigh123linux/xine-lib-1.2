/*
 * Copyright (C) 2000-2017 the xine project
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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <time.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <math.h>
#include <string.h>
#include <errno.h>

#define LOG_MODULE "metronom"
#define LOG_VERBOSE
/*
#define LOG
#define LOG_AUDIO
*/
#define METRONOM_INTERNAL
#define METRONOM_CLOCK_INTERNAL

#include <xine/xine_internal.h>
#include <xine/metronom.h>
#include <xine/xineutils.h>

#define MAX_AUDIO_DELTA        1600
#define AUDIO_SAMPLE_NUM      32768
#define WRAP_THRESHOLD       120000
#define MAX_NUM_WRAP_DIFF        10
#define MAX_SCR_PROVIDERS        10
#define VIDEO_DRIFT_TOLERANCE 45000
#define AUDIO_DRIFT_TOLERANCE 45000

/* metronom video modes */
#define VIDEO_PREDICTION_MODE     0      /* use pts + frame duration */
#define VIDEO_PTS_MODE            1      /* use only pts */

/* redefine abs as macro to handle 64-bit diffs.
   i guess llabs may not be available everywhere */
#define abs(x) ( ((x)<0) ? -(x) : (x) )

/*
 * ****************************************
 *   primary SCR plugin:
 *    unix System Clock Reference
 * ****************************************
 */

typedef struct unixscr_s {
  scr_plugin_t    scr;
  void           *mem_to_free;

  struct timeval  cur_time;
  int64_t         cur_pts;
  double          speed_factor;

  pthread_mutex_t  lock;

} unixscr_t;

static int unixscr_get_priority (scr_plugin_t *scr) {
  return 5; /* low priority */
}

/* Only call this when already mutex locked */
static void unixscr_set_pivot (unixscr_t *this) {

  struct   timeval tv;
  int64_t pts;
  double   pts_calc;

  xine_monotonic_clock(&tv, NULL);
  pts_calc = (tv.tv_sec  - this->cur_time.tv_sec) * this->speed_factor;
  pts_calc += (tv.tv_usec - this->cur_time.tv_usec) * this->speed_factor / 1e6;
  pts = this->cur_pts + pts_calc;

/* This next part introduces a one off inaccuracy
 * to the scr due to rounding tv to pts.
 */
  this->cur_time.tv_sec=tv.tv_sec;
  this->cur_time.tv_usec=tv.tv_usec;
  this->cur_pts=pts;

  return ;
}

static int unixscr_set_speed (scr_plugin_t *scr, int speed) {
  unixscr_t *this = (unixscr_t*) scr;

  pthread_mutex_lock (&this->lock);

  unixscr_set_pivot( this );
  this->speed_factor = (double) speed * 90000.0 / XINE_FINE_SPEED_NORMAL;

  pthread_mutex_unlock (&this->lock);

  return speed;
}

static void unixscr_adjust (scr_plugin_t *scr, int64_t vpts) {
  unixscr_t *this = (unixscr_t*) scr;
  struct   timeval tv;

  pthread_mutex_lock (&this->lock);

  xine_monotonic_clock(&tv, NULL);
  this->cur_time.tv_sec=tv.tv_sec;
  this->cur_time.tv_usec=tv.tv_usec;
  this->cur_pts = vpts;

  pthread_mutex_unlock (&this->lock);
}

static void unixscr_start (scr_plugin_t *scr, int64_t start_vpts) {
  unixscr_t *this = (unixscr_t*) scr;

  pthread_mutex_lock (&this->lock);

  xine_monotonic_clock(&this->cur_time, NULL);
  this->cur_pts = start_vpts;

  pthread_mutex_unlock (&this->lock);

  unixscr_set_speed (&this->scr, XINE_FINE_SPEED_NORMAL);
}

static int64_t unixscr_get_current (scr_plugin_t *scr) {
  unixscr_t *this = (unixscr_t*) scr;

  struct   timeval tv;
  int64_t pts;
  double   pts_calc;
  pthread_mutex_lock (&this->lock);

  xine_monotonic_clock(&tv, NULL);

  pts_calc = (tv.tv_sec  - this->cur_time.tv_sec) * this->speed_factor;
  pts_calc += (tv.tv_usec - this->cur_time.tv_usec) * this->speed_factor / 1e6;

  pts = this->cur_pts + pts_calc;

  pthread_mutex_unlock (&this->lock);

  return pts;
}

static void unixscr_exit (scr_plugin_t *scr) {
  unixscr_t *this = (unixscr_t*) scr;

  pthread_mutex_destroy (&this->lock);
  free (this->mem_to_free);
}

static scr_plugin_t *unixscr_init (void *this_gen) {
  unixscr_t *this = (unixscr_t *)this_gen;

  if (this) {
    this->mem_to_free = NULL;
  } else {
    this = calloc (1, sizeof (unixscr_t));
    if (!this)
      return NULL;
    this->mem_to_free = this;
  }

  this->scr.interface_version = 3;
  this->scr.get_priority      = unixscr_get_priority;
  this->scr.set_fine_speed    = unixscr_set_speed;
  this->scr.adjust            = unixscr_adjust;
  this->scr.start             = unixscr_start;
  this->scr.get_current       = unixscr_get_current;
  this->scr.exit              = unixscr_exit;

  pthread_mutex_init (&this->lock, NULL);

  unixscr_set_speed (&this->scr, XINE_SPEED_PAUSE);
  lprintf("xine-scr_init complete\n");

  return &this->scr;
}


/************************************************************************
* The master clock feature. It shall handle these basic cases:          *
* 1. A single system clock controls all timing.                         *
* 2. Some plugin is hard wired to use its own non-adjustable clock.     *
*    That clock is slightly faster or slower than system clock.         *
*    It will drift away over time, and wants xine to follow that drift. *
*    Such clock registers as high priority (> 5),                       *
*    and thus becomes the new master.                                   *
* 3. Some plugin uses its own drifting clock, but it is adjustable,     *
*    and wants xine to fix that drift.                                  *
*    Such clock registers as low priority (< 5).                        *
* In cases 2 and 3, we "sync" the masters time to all other clocks      *
* roughly every 5 seconds.                                              *
************************************************************************/

/* #$@! dont break existing API */
typedef struct {
  metronom_clock_t mct;
  unixscr_t        uscr;
  int              next_sync_pts; /* sync by API calls, STOP_PTS to disable */
  enum {
    SYNC_THREAD_NONE,             /* thread disabled by user, see above */
    SYNC_THREAD_OFF,              /* no clock to sync, or thread unavailable and -"- */
    SYNC_THREAD_RUNNING           /* self explaining */
  }                sync_thread_state;
  scr_plugin_t    *providers[MAX_SCR_PROVIDERS + 1];
} metronom_clock_private_t;

#define START_PTS 0
#define STOP_PTS ~0
#define MASK_PTS (1 << 19) /* 5.825 s */

static void metronom_start_clock (metronom_clock_t *this, int64_t pts) {
  metronom_clock_private_t *this_priv = (metronom_clock_private_t *)this;
  scr_plugin_t **r;

  lprintf("start_clock (at %" PRId64 ")\n", pts);

  if (this_priv->next_sync_pts != STOP_PTS)
    this_priv->next_sync_pts = (int)pts & MASK_PTS;

  pthread_mutex_lock (&this_priv->mct.lock);
  for (r = this_priv->providers; *r && (r < this_priv->providers + MAX_SCR_PROVIDERS); r++)
    (*r)->start (*r, pts);
  pthread_mutex_unlock (&this_priv->mct.lock);

  this_priv->mct.speed = XINE_FINE_SPEED_NORMAL;
}

static int64_t metronom_get_current_time (metronom_clock_t *this) {
  metronom_clock_private_t *this_priv = (metronom_clock_private_t *)this;
  int64_t pts = this_priv->mct.scr_master->get_current (this_priv->mct.scr_master);
  scr_plugin_t **r;

  /* sync not needed or done by separate thread */
  if (((int)pts & MASK_PTS) != this_priv->next_sync_pts)
    return pts;

  this_priv->next_sync_pts ^= MASK_PTS;

  pthread_mutex_lock (&this_priv->mct.lock);
  for (r = this_priv->providers; *r && (r < this_priv->providers + MAX_SCR_PROVIDERS); r++)
    if (*r != this_priv->mct.scr_master)
      (*r)->adjust (*r, pts);
  pthread_mutex_unlock (&this_priv->mct.lock);

  return pts;
}

static void metronom_stop_clock(metronom_clock_t *this) {
  metronom_clock_private_t *this_priv = (metronom_clock_private_t *)this;
  scr_plugin_t **r;

  pthread_mutex_lock (&this_priv->mct.lock);
  for (r = this_priv->providers; *r && (r < this_priv->providers + MAX_SCR_PROVIDERS); r++)
    (*r)->set_fine_speed (*r, XINE_SPEED_PAUSE);
  pthread_mutex_unlock (&this_priv->mct.lock);
}

static void metronom_resume_clock(metronom_clock_t *this) {
  metronom_clock_private_t *this_priv = (metronom_clock_private_t *)this;
  scr_plugin_t **r;

  pthread_mutex_lock (&this_priv->mct.lock);
  for (r = this_priv->providers; *r && (r < this_priv->providers + MAX_SCR_PROVIDERS); r++)
    (*r)->set_fine_speed (*r, XINE_FINE_SPEED_NORMAL);
  pthread_mutex_unlock (&this_priv->mct.lock);
}

static void metronom_adjust_clock(metronom_clock_t *this, int64_t desired_pts) {
  metronom_clock_private_t *this_priv = (metronom_clock_private_t *)this;

  if (this_priv->mct.scr_adjustable)
    this_priv->mct.scr_master->adjust (this_priv->mct.scr_master, desired_pts);
  if (this_priv->next_sync_pts != STOP_PTS)
    this_priv->next_sync_pts = (int)desired_pts & MASK_PTS;
}

static int metronom_set_speed (metronom_clock_t *this, int speed) {
  metronom_clock_private_t *this_priv = (metronom_clock_private_t *)this;
  scr_plugin_t **r;
  int            true_speed;

  true_speed = this_priv->mct.scr_master->set_fine_speed (this_priv->mct.scr_master, speed);

  this_priv->mct.speed = true_speed;

  pthread_mutex_lock (&this_priv->mct.lock);
  for (r = this_priv->providers; *r && (r < this_priv->providers + MAX_SCR_PROVIDERS); r++)
    (*r)->set_fine_speed (*r, true_speed);
  pthread_mutex_unlock (&this_priv->mct.lock);

  return true_speed;
}


static void metronom_set_audio_rate (metronom_t *this, int64_t pts_per_smpls) {
  pthread_mutex_lock (&this->lock);

  this->pts_per_smpls = pts_per_smpls;

  pthread_mutex_unlock (&this->lock);

  lprintf("%" PRId64 " pts per %d samples\n", pts_per_smpls, AUDIO_SAMPLE_NUM);
}

static int64_t metronom_got_spu_packet (metronom_t *this, int64_t pts) {
  int64_t vpts;

  pthread_mutex_lock (&this->lock);

  if (this->master) {
    this->master->set_option(this->master, METRONOM_LOCK, 1);

    this->vpts_offset = this->master->get_option(this->master, METRONOM_VPTS_OFFSET | METRONOM_NO_LOCK);
    this->spu_offset  = this->master->get_option(this->master, METRONOM_SPU_OFFSET | METRONOM_NO_LOCK);
  }

  vpts = pts + this->vpts_offset + this->spu_offset;

  /* no vpts going backwards please */
  if( vpts < this->spu_vpts )
    vpts = this->spu_vpts;

  this->spu_vpts = vpts;

  if (this->master) {
    this->master->set_option(this->master, METRONOM_LOCK, 0);
  }

  pthread_mutex_unlock (&this->lock);
  return vpts;
}

static void metronom_handle_discontinuity (metronom_t *this, int type,
                                           int64_t disc_off) {
  int64_t cur_time;

  /* video_vpts and audio_vpts adjustements */
  cur_time = this->xine->clock->get_current_time(this->xine->clock);

  switch (type) {
    /* When switching streams gaplessly, a paradox situation may happen:
     * Engine was very fast and filled output buffers with more than
     * this->prebuffer of yet to be played frames from the end of previous
     * stream. The DISC_STREAMSTART code below will then set back vpts
     * a few frames, and the engine will drop them later.
     * We could try to fix this by increasing this->prebuffer, but that
     * would cumulate over large playlists, and finally blow out queue
     * sizes.
     * Instead, we wait here a bit.
     */
    case DISC_GAPLESS:
      {
        int64_t t;
        int speed = this->xine->clock->speed;
        if (speed <= 0)
          break;
        pthread_mutex_lock (&this->lock);
        t = this->video_vpts > this->audio_vpts ? this->video_vpts : this->audio_vpts;
        t -= this->prebuffer + cur_time;
        pthread_mutex_unlock (&this->lock);
        if ((t <= 0) || (t > 90000))
          break;
        xprintf (this->xine, XINE_VERBOSITY_DEBUG,
          "metronom: gapless switch: wait %"PRId64" pts.\n", t);
        xine_usec_sleep ((int)((t * XINE_FINE_SPEED_NORMAL) / (speed * 90)) * 1000);
      }
      break;

    case DISC_STREAMSTART:
    case DISC_STREAMSEEK:
      this->video_vpts = this->prebuffer + cur_time;
      this->audio_vpts = this->video_vpts;
      this->audio_vpts_rmndr = 0;
      this->force_audio_jump = 1;
      this->force_video_jump = 1;
      this->video_drift = 0;
      xprintf (this->xine, XINE_VERBOSITY_DEBUG,
        "metronom: vpts adjusted with prebuffer to %" PRId64 ".\n", this->video_vpts);
      break;

    case DISC_ABSOLUTE:
    case DISC_RELATIVE:
      if (this->video_vpts < cur_time) {
        /* still frame */
        if (this->audio_vpts > cur_time) {
          /* still frame with audio */
          this->video_vpts = this->audio_vpts;
          xprintf (this->xine, XINE_VERBOSITY_DEBUG,
            "metronom: video vpts adjusted to audio vpts %" PRId64 ".\n", this->video_vpts);
        } else {
          /* still frame, no audio */
          this->video_vpts = this->prebuffer + cur_time;
          this->audio_vpts = this->video_vpts;
          this->audio_vpts_rmndr = 0;
          this->force_video_jump = 1;
          this->force_audio_jump = 1;
          this->video_drift = 0;
          xprintf (this->xine, XINE_VERBOSITY_DEBUG,
            "metronom: vpts adjusted with prebuffer to %" PRId64 ".\n",
	    this->video_vpts);
        }
      } else {
        /* video */
        if (this->audio_vpts < cur_time) {
          /* video, no sound */
          this->audio_vpts = this->video_vpts;
          this->audio_vpts_rmndr = 0;
          xprintf (this->xine, XINE_VERBOSITY_DEBUG,
            "metronom: audio vpts adjusted to video vpts %" PRId64 ".\n", this->video_vpts);
        } else {
          /* video + audio */
        }
      }
      break;
  }

  lprintf("video_vpts: %" PRId64 ", audio_vpts: %" PRId64 "\n", this->video_vpts, this->audio_vpts);

  /* vpts_offset adjustements */
  switch (type) {
  case DISC_STREAMSTART:
    lprintf("DISC_STREAMSTART\n");
    this->vpts_offset = this->video_vpts;
    break;
  case DISC_ABSOLUTE:
    lprintf("DISC_ABSOLUTE\n");
    this->vpts_offset = this->video_vpts - disc_off;
    break;
  case DISC_RELATIVE:
    lprintf("DISC_RELATIVE\n");
    this->vpts_offset = this->vpts_offset - disc_off;
    break;
  case DISC_STREAMSEEK:
    lprintf("DISC_STREAMSEEK\n");
    this->vpts_offset = this->video_vpts - disc_off;
    break;
  }

  this->last_video_pts = 0;
  this->last_audio_pts = 0;
}

static void metronom_handle_video_discontinuity (metronom_t *this, int type,
						 int64_t disc_off) {

  pthread_mutex_lock (&this->lock);

  if (this->master) {
    /* slaves are currently not allowed to set discontinuities */
    pthread_mutex_unlock(&this->lock);
    return;
  }

  this->video_discontinuity_count++;
  pthread_cond_signal (&this->video_discontinuity_reached);

  xprintf (this->xine, XINE_VERBOSITY_DEBUG,
    "metronom: video discontinuity #%d, type is %d, disc_off %" PRId64 ".\n",
    this->video_discontinuity_count, type, disc_off);

  if (this->have_audio) {
    while (this->audio_discontinuity_count <
	   this->video_discontinuity_count) {

      xprintf (this->xine, XINE_VERBOSITY_DEBUG,
        "metronom: waiting for audio discontinuity #%d...\n",
        this->video_discontinuity_count);

      pthread_cond_wait (&this->audio_discontinuity_reached, &this->lock);
    }
  }

  metronom_handle_discontinuity(this, type, disc_off);

  this->discontinuity_handled_count++;
  pthread_cond_signal (&this->video_discontinuity_reached);

  pthread_mutex_unlock (&this->lock);
}

static void metronom_got_video_frame (metronom_t *this, vo_frame_t *img) {

  int64_t vpts;
  int64_t pts = img->pts;
  int64_t diff;

  pthread_mutex_lock (&this->lock);

  if (this->master) {
    this->master->set_option(this->master, METRONOM_LOCK, 1);

    if (!this->discontinuity_handled_count) {
      /* we are not initialized yet */

      this->video_vpts = this->audio_vpts = this->master->get_option(this->master, METRONOM_VPTS | METRONOM_NO_LOCK);

      /* when being attached to the first master, do not drift into
       * his vpts values but adopt at once */
      this->force_audio_jump = 1;
      this->force_video_jump = 1;
      this->discontinuity_handled_count++;
    }

    this->vpts_offset = this->master->get_option(this->master, METRONOM_VPTS_OFFSET | METRONOM_NO_LOCK);
    this->av_offset   = this->master->get_option(this->master, METRONOM_AV_OFFSET | METRONOM_NO_LOCK);
  }

  lprintf("got_video_frame pts = %" PRId64 ", duration = %d\n", pts, img->duration);

  this->img_cpt++;

  /* 1000 fps usually means unknown or variable frame rate */
  if (img->duration > 90) {
    this->video_mode = VIDEO_PREDICTION_MODE;
    this->img_duration = img->duration;
  } else {
    /* will skip the whole predicted vpts stuff */
    this->video_mode = VIDEO_PTS_MODE;
  }

  if (pts && pts != this->last_video_pts) {

    if (!img->duration) {
      /* Compute the duration of previous frames using this formula:
       * duration = (curent_pts - last_pts) / (frame count between the 2 pts)
       * This duration will be used to predict the next frame vpts.
       */
      if (this->last_video_pts && this->img_cpt) {
        this->img_duration = (pts - this->last_video_pts) / this->img_cpt;
        lprintf("computed frame_duration = %" PRId64 "\n", this->img_duration );
      }
    }
    this->img_cpt = 0;
    this->last_video_pts = pts;


    /*
     * compare predicted (this->video_vpts) and given (pts+vpts_offset)
     * pts values - hopefully they will be the same
     * if not, for small diffs try to interpolate
     *         for big diffs: jump
     */

    vpts = pts + this->vpts_offset;

    if (this->video_mode == VIDEO_PREDICTION_MODE) {

      diff = this->video_vpts - vpts;

      lprintf("video diff is %" PRId64 " (predicted %" PRId64 ", given %" PRId64 ")\n", diff, this->video_vpts, vpts);


      if ((abs (diff) > VIDEO_DRIFT_TOLERANCE) || (this->force_video_jump)) {
        this->force_video_jump = 0;
        this->video_vpts       = vpts;
        this->video_drift      = 0;
        this->video_drift_step = 0;

        xprintf (this->xine, XINE_VERBOSITY_DEBUG,
          "metronom: video jump by %"PRId64" pts.\n", diff);

      } else {
        /* TJ. Drift into new value over the next 30 frames.
           Dont fall into the asymptote trap of bringing down step with remaining drift. */
        int64_t step;
        if (diff < 0) {
          step = (diff - 29) / 30;
          if (this->video_drift_step < step)
            step = this->video_drift_step < diff ? diff : this->video_drift_step;
        } else {
          step = (diff + 29) / 30;
          if (this->video_drift_step > step)
            step = this->video_drift_step > diff ? diff : this->video_drift_step;
        }
        this->video_drift      = diff;
        this->video_drift_step = step;
      }
    } else {
      /* VIDEO_PTS_MODE: do not use the predicted value */
      this->video_drift      = 0;
      this->video_drift_step = 0;
      this->video_vpts       = vpts;
    }
  }

  img->vpts = this->video_vpts + this->av_offset;

  /* We need to update this->video_vpts is both modes.
   * this->video_vpts is used as the next frame vpts if next frame pts=0
   */
  this->video_vpts += this->img_duration - this->video_drift_step;

  if (this->video_mode == VIDEO_PREDICTION_MODE) {
    lprintf("video vpts for %10"PRId64" : %10"PRId64" (duration:%d drift:%" PRId64 " step:%" PRId64 ")\n",
	  pts, this->video_vpts, img->duration, this->video_drift, this->video_drift_step );

    /* reset drift compensation if work is done after this frame */
    if (this->video_drift_step) {
      this->video_drift -= this->video_drift_step;
      if (this->video_drift_step < 0) {
        if (this->video_drift >= 0) {
          this->video_drift      = 0;
          this->video_drift_step = 0;
        }
      } else {
        if (this->video_drift <= 0) {
          this->video_drift      = 0;
          this->video_drift_step = 0;
        }
      }
    }
  }


  if (this->master) {
    this->master->set_option(this->master, METRONOM_LOCK, 0);
  }

  pthread_mutex_unlock (&this->lock);
  if (this->xine->verbosity == XINE_VERBOSITY_DEBUG + 1)
    xprintf (this->xine, XINE_VERBOSITY_DEBUG + 1, "metronom: video pts: %"PRId64":%04d ->  %"PRId64".\n",
      img->pts, (int)img->duration, img->vpts);
}

static void metronom_handle_audio_discontinuity (metronom_t *this, int type,
						 int64_t disc_off) {

  if (type == DISC_GAPLESS) {
    metronom_handle_discontinuity (this, type, disc_off);
    return;
  }

  pthread_mutex_lock (&this->lock);

  if (this->master) {
    /* slaves are currently not allowed to set discontinuities */
    pthread_mutex_unlock(&this->lock);
    return;
  }

  this->audio_discontinuity_count++;
  pthread_cond_signal (&this->audio_discontinuity_reached);

  xprintf (this->xine, XINE_VERBOSITY_DEBUG,
    "metronom: audio discontinuity #%d, type is %d, disc_off %" PRId64 ".\n",
    this->audio_discontinuity_count, type, disc_off);

  if (this->have_video) {

    /* next_vpts_offset, in_discontinuity is handled in expect_video_discontinuity */
    while ( this->audio_discontinuity_count >
            this->discontinuity_handled_count ) {

      xprintf (this->xine, XINE_VERBOSITY_DEBUG,
        "metronom: waiting for in_discontinuity update #%d...\n",
        this->audio_discontinuity_count);

      pthread_cond_wait (&this->video_discontinuity_reached, &this->lock);
    }
  } else {
    metronom_handle_discontinuity(this, type, disc_off);
  }

  this->audio_samples = 0;
  this->audio_drift_step = 0;

  pthread_mutex_unlock (&this->lock);
}

static int64_t metronom_got_audio_samples (metronom_t *this, int64_t pts,
					   int nsamples) {

  int64_t vpts;
  int64_t diff;

  lprintf("got %d audio samples, pts is %" PRId64 ", last pts = %" PRId64 "\n", nsamples, pts, this->last_audio_pts);
  lprintf("AUDIO pts from last= %" PRId64 "\n", pts-this->last_audio_pts);

  pthread_mutex_lock (&this->lock);

  if (this->master) {
    this->master->set_option(this->master, METRONOM_LOCK, 1);

    if (!this->discontinuity_handled_count) {
      /* we are not initialized yet */

      this->video_vpts = this->audio_vpts = this->master->get_option(this->master, METRONOM_VPTS | METRONOM_NO_LOCK);

      this->audio_vpts_rmndr = 0;
      /* when being attached to the first master, do not drift into
       * his vpts values but adopt at once */
      this->force_audio_jump = 1;
      this->force_video_jump = 1;
      this->discontinuity_handled_count++;
    }

    this->vpts_offset = this->master->get_option(this->master, METRONOM_VPTS_OFFSET | METRONOM_NO_LOCK);
  }

  if (pts && pts != this->last_audio_pts) {
    vpts = pts + this->vpts_offset;
    diff = this->audio_vpts - vpts;
    this->last_audio_pts = pts;

    /* compare predicted and given vpts */
    if((abs(diff) > AUDIO_DRIFT_TOLERANCE) || (this->force_audio_jump)) {
      this->force_audio_jump = 0;
      this->audio_vpts       = vpts;
      this->audio_vpts_rmndr = 0;
      this->audio_drift_step = 0;
      xprintf (this->xine, XINE_VERBOSITY_DEBUG, "metronom: audio jump, diff=%" PRId64 ".\n", diff);
    } else {
      if( this->audio_samples ) {
        /* calculate drift_step to recover vpts errors */
        lprintf("audio diff = %" PRId64 " \n", diff );

        diff *= AUDIO_SAMPLE_NUM;
        diff /= this->audio_samples * 4;

        /* drift_step is not allowed to change rate by more than 25% */
        if( diff > this->pts_per_smpls/4 )
          diff = this->pts_per_smpls/4;
        if( diff < -this->pts_per_smpls/4 )
          diff = -this->pts_per_smpls/4;

        this->audio_drift_step = diff;

        lprintf("audio_drift = %" PRId64 ", pts_per_smpls = %" PRId64 "\n", diff, this->pts_per_smpls);
      }
    }
    this->audio_samples = 0;
  }
  vpts = this->audio_vpts;

  /* drift here is caused by streams where nominal sample rate differs from
   * the rate of which pts increments. fixing the audio_vpts won't do us any
   * good because sound card won't play it faster or slower just because
   * we want. however, adding the error to the vpts_offset will force video
   * to change it's frame rate to keep in sync with us.
   *
   * Since we are using integer division below, it can happen that we lose
   * precision for the calculated duration in vpts for each audio buffer
   * (< 1 PTS, e.g. 0.25 PTS during playback of most DVDs with LPCM audio).
   * This would lead to a situation where the sound card actually needs
   * more time to play back the buffers, than the audio buffer's vpts field
   * indicates. This makes audio_out loop think we are in sync with the
   * soundcard, while we actually are not. So that's why there is the extra
   * modulo calculation, to keep track of the truncated, fractional part.
   */
  this->audio_vpts_rmndr += nsamples * this->pts_per_smpls % AUDIO_SAMPLE_NUM;
  this->audio_vpts       += nsamples * this->pts_per_smpls / AUDIO_SAMPLE_NUM;
  if (this->audio_vpts_rmndr >= AUDIO_SAMPLE_NUM) {
    this->audio_vpts       += 1;
    this->audio_vpts_rmndr -= AUDIO_SAMPLE_NUM;
  }
  this->audio_samples += nsamples;
  this->vpts_offset += nsamples * this->audio_drift_step / AUDIO_SAMPLE_NUM;

  lprintf("audio vpts for %10"PRId64" : %10"PRId64"\n", pts, vpts);

  if (this->master) {
    this->master->set_option(this->master, METRONOM_LOCK, 0);
  }

  pthread_mutex_unlock (&this->lock);

  return vpts;
}

static void metronom_set_option (metronom_t *this, int option, int64_t value) {

  if (option == METRONOM_LOCK) {
    if (value) {
      pthread_mutex_lock (&this->lock);
      if (this->master)
        this->master->set_option(this->master, option, value);
    } else {
      if (this->master)
        this->master->set_option(this->master, option, value);
      pthread_mutex_unlock (&this->lock);
    }
    return;
  }

  pthread_mutex_lock (&this->lock);

  if (this->master) {
    /* pass the option on to the master */
    this->master->set_option(this->master, option, value);
    pthread_mutex_unlock(&this->lock);
    return;
  }

  switch (option) {
  case METRONOM_AV_OFFSET:
    this->av_offset = value;
    xprintf (this->xine, XINE_VERBOSITY_LOG,
      "metronom: av_offset=%" PRId64 " pts.\n", this->av_offset);
    break;
  case METRONOM_SPU_OFFSET:
    this->spu_offset = value;
    xprintf (this->xine, XINE_VERBOSITY_LOG,
      "metronom: spu_offset=%" PRId64 " pts.\n", this->spu_offset);
    break;
  case METRONOM_ADJ_VPTS_OFFSET:
    this->audio_vpts      += value;
    this->audio_vpts_rmndr = 0;

    /* that message should be rare, please report otherwise.
     * when xine is in some sort of "steady state" hearing it
     * once in a while means a small sound card drift (or system
     * clock drift -- who knows?). nothing to worry about.
     */
    xprintf (this->xine, XINE_VERBOSITY_LOG,
      "metronom: fixing sound card drift by %" PRId64 " pts.\n", value);
    break;
  case METRONOM_PREBUFFER:
    this->prebuffer = value;
    xprintf (this->xine, XINE_VERBOSITY_LOG,
      "metronom: prebuffer=%" PRId64 " pts.\n", this->prebuffer);
    break;
  default:
    xprintf(this->xine, XINE_VERBOSITY_NONE,
      "metronom: unknown option in set_option: %d.\n", option);
  }

  pthread_mutex_unlock (&this->lock);
}

static void metronom_clock_set_option (metronom_clock_t *this,
					int option, int64_t value) {

  pthread_mutex_lock (&this->lock);

  switch (option) {
  case CLOCK_SCR_ADJUSTABLE:
    this->scr_adjustable = value;
    break;
  default:
    xprintf (this->xine, XINE_VERBOSITY_NONE,
      "metronom: unknown option in set_option: %d.\n", option);
  }

  pthread_mutex_unlock (&this->lock);
}

static int64_t metronom_get_option (metronom_t *this, int option) {

  int64_t result;
  int mutex_locked;

  if (option & METRONOM_NO_LOCK) {
    mutex_locked = 0;
  } else {
    pthread_mutex_lock (&this->lock);
    mutex_locked = 1;
  }

  if (this->master) {
    result = this->master->get_option(this->master, option);
    if (mutex_locked)
      pthread_mutex_unlock (&this->lock);
    return result;
  }

  option &= ~METRONOM_NO_LOCK;

  switch (option) {
  case METRONOM_AV_OFFSET:
    result = this->av_offset;
    break;
  case METRONOM_SPU_OFFSET:
    result = this->spu_offset;
    break;
  case METRONOM_FRAME_DURATION:
    result = this->img_duration;
    break;
  case METRONOM_VPTS_OFFSET:
    result = this->vpts_offset;
    break;
  case METRONOM_PREBUFFER:
    result = this->prebuffer;
    break;
  case METRONOM_VPTS:
      if (this->video_vpts > this->audio_vpts)
        result = this->video_vpts;
      else
        result = this->audio_vpts;
      break;
  default:
    result = 0;
    xprintf (this->xine, XINE_VERBOSITY_NONE,
      "metronom: unknown option in get_option: %d.\n", option);
    break;
  }

  if (mutex_locked) {
    pthread_mutex_unlock (&this->lock);
  }

  return result;
}

static int64_t metronom_clock_get_option (metronom_clock_t *this, int option) {
  switch (option) {
  case CLOCK_SCR_ADJUSTABLE:
    return this->scr_adjustable;
  }
  xprintf (this->xine, XINE_VERBOSITY_NONE,
    "metronom: unknown option in get_option: %d.\n", option);
  return 0;
}

static void metronom_set_master(metronom_t *this, metronom_t *master) {
  metronom_t *old_master = this->master;

  pthread_mutex_lock(&this->lock);
  /* someone might currently be copying values from the old master,
   * so we need his lock too */
  if (old_master)
    old_master->set_option(old_master, METRONOM_LOCK, 1);

  this->master = master;
  /* new master -> we have to reinit */
  this->discontinuity_handled_count = 0;

  if (old_master)
    old_master->set_option(old_master, METRONOM_LOCK, 0);
  pthread_mutex_unlock(&this->lock);
}

static scr_plugin_t* get_master_scr(metronom_clock_t *this) {
  metronom_clock_private_t *this_priv = (metronom_clock_private_t *)this;
  scr_plugin_t *found = NULL, **r;
  int maxprio = 0;

  /* find the SCR provider with the highest priority */
  for (r = this_priv->providers; *r && (r < this_priv->providers + MAX_SCR_PROVIDERS); r++) {
    int p = (*r)->get_priority (*r);
    if (maxprio < p) {
      found = *r;
      maxprio = p;
    }
  }
  if (!found) {
    xprintf (this_priv->mct.xine, XINE_VERBOSITY_NONE,
      "metronom: panic - no scr provider found!\n");
    return NULL;
  }
  return found;
}

static void *metronom_sync_loop (void *const this_gen) {
  metronom_clock_private_t *const this_priv = (metronom_clock_private_t *const)this_gen;

  struct timeval tv;
  struct timespec ts;
  scr_plugin_t **r;
  int64_t        pts;

  while (this_priv->mct.thread_running) {
    /* synchronise every 5 seconds */
    pthread_mutex_lock (&this_priv->mct.lock);

    pts = this_priv->mct.scr_master->get_current (this_priv->mct.scr_master);

    for (r = this_priv->providers; *r && (r < this_priv->providers + MAX_SCR_PROVIDERS); r++)
      if (*r != this_priv->mct.scr_master) (*r)->adjust (*r, pts);

    gettimeofday(&tv, NULL);
    ts.tv_sec  = tv.tv_sec + 5;
    ts.tv_nsec = tv.tv_usec * 1000;
    pthread_cond_timedwait (&this_priv->mct.cancel, &this_priv->mct.lock, &ts);

    pthread_mutex_unlock (&this_priv->mct.lock);
  }
  return NULL;
}

static void metronom_start_sync_thread (metronom_clock_private_t *this_priv) {
  int err;

  if (this_priv->sync_thread_state == SYNC_THREAD_NONE) {
    this_priv->next_sync_pts = START_PTS;
    return;
  }

  if (this_priv->sync_thread_state != SYNC_THREAD_OFF)
    return;

  pthread_cond_init  (&this_priv->mct.cancel, NULL);

  this_priv->mct.thread_running = 1;

  err = pthread_create (&this_priv->mct.sync_thread, NULL, metronom_sync_loop, &this_priv->mct);

  if (err) {
    xprintf (this_priv->mct.xine, XINE_VERBOSITY_NONE,
      "metronom: cannot create sync thread (%s).\n", strerror (err));
    this_priv->next_sync_pts = START_PTS;
  } else {
    this_priv->sync_thread_state = SYNC_THREAD_RUNNING;
    this_priv->next_sync_pts = STOP_PTS;
  }
}

static void metronom_stop_sync_thread (metronom_clock_private_t *this_priv) {

  if (this_priv->sync_thread_state == SYNC_THREAD_NONE) {
    this_priv->next_sync_pts = STOP_PTS;
    return;
  }

  if (this_priv->sync_thread_state != SYNC_THREAD_RUNNING)
    return;

  this_priv->mct.thread_running = 0;

  pthread_mutex_lock (&this_priv->mct.lock);
  pthread_cond_signal (&this_priv->mct.cancel);
  pthread_mutex_unlock (&this_priv->mct.lock);

  pthread_join (this_priv->mct.sync_thread, NULL);

  pthread_cond_destroy (&this_priv->mct.cancel);

  this_priv->sync_thread_state = SYNC_THREAD_OFF;
  this_priv->next_sync_pts = STOP_PTS;
}

static int metronom_register_scr (metronom_clock_t *this, scr_plugin_t *scr) {
  metronom_clock_private_t *this_priv = (metronom_clock_private_t *)this;
  scr_plugin_t **r;

  if (scr->interface_version != 3) {
    xprintf (this->xine, XINE_VERBOSITY_NONE,
      "metronom: wrong interface version for scr provider!\n");
    return -1;
  }

  if (this_priv->providers[0] && !this_priv->providers[1])
    metronom_start_sync_thread (this_priv);

  pthread_mutex_lock (&this_priv->mct.lock);
  for (r = this_priv->providers; *r && (r < this_priv->providers + MAX_SCR_PROVIDERS); r++) ;
  if (r >= this_priv->providers + MAX_SCR_PROVIDERS) {
    pthread_mutex_unlock (&this_priv->mct.lock);
    return -1; /* No free slot available */
  }

  scr->clock = &this_priv->mct;
  *r = scr;
  this_priv->mct.scr_master = get_master_scr (&this_priv->mct);
  pthread_mutex_unlock (&this_priv->mct.lock);
  return 0;
}

static void metronom_unregister_scr (metronom_clock_t *this, scr_plugin_t *scr) {
  metronom_clock_private_t *this_priv = (metronom_clock_private_t *)this;
  scr_plugin_t **found = NULL, **r;
  int64_t now;

  if (!scr)
    return;

  pthread_mutex_lock (&this_priv->mct.lock);
  /* never unregister scr_list[0]! */
  for (r = this_priv->providers + 1; *r && r < this_priv->providers + MAX_SCR_PROVIDERS; r++) {
    if (*r == scr)
      found = r;
  }
  if (!found) {
    pthread_mutex_unlock (&this_priv->mct.lock);
    return; /* Not found */
  }
  /* avoid holes in list */
  found[0] = r[-1];
  r[-1] = NULL;

  now = this_priv->mct.scr_master->get_current (this_priv->mct.scr_master);
  /* master could have been adjusted, others must follow now */
  for (r = this_priv->providers; *r && (r < this_priv->providers + MAX_SCR_PROVIDERS); r++)
    if (*r != this_priv->mct.scr_master)
      (*r)->adjust (*r, now);

  this_priv->mct.scr_master = get_master_scr (&this_priv->mct);
  pthread_mutex_unlock (&this_priv->mct.lock);

  if (this_priv->providers[0] && !this_priv->providers[1])
    metronom_stop_sync_thread (this_priv);
}

static void metronom_exit (metronom_t *this) {

  pthread_mutex_destroy (&this->lock);
  pthread_cond_destroy (&this->video_discontinuity_reached);
  pthread_cond_destroy (&this->audio_discontinuity_reached);

  free (this);
}

static void metronom_clock_exit (metronom_clock_t *this) {
  metronom_clock_private_t *this_priv = (metronom_clock_private_t *)this;
  scr_plugin_t **r;

  this_priv->mct.xine->config->unregister_callback (this_priv->mct.xine->config, "engine.use_metronom_sync_thread");

  metronom_stop_sync_thread (this_priv);

  pthread_mutex_lock (&this_priv->mct.lock);
  for (r = this_priv->providers; *r && (r < this_priv->providers + MAX_SCR_PROVIDERS); r++)
    (*r)->exit (*r);
  pthread_mutex_unlock (&this_priv->mct.lock);

  pthread_mutex_destroy (&this_priv->mct.lock);
  free (this_priv);
}


metronom_t * _x_metronom_init (int have_video, int have_audio, xine_t *xine) {

  metronom_t *this = calloc(1, sizeof (metronom_t));

  this->set_audio_rate             = metronom_set_audio_rate;
  this->got_video_frame            = metronom_got_video_frame;
  this->got_audio_samples          = metronom_got_audio_samples;
  this->got_spu_packet             = metronom_got_spu_packet;
  this->handle_audio_discontinuity = metronom_handle_audio_discontinuity;
  this->handle_video_discontinuity = metronom_handle_video_discontinuity;
  this->set_option                 = metronom_set_option;
  this->get_option                 = metronom_get_option;
  this->set_master                 = metronom_set_master;
  this->exit                       = metronom_exit;

  this->xine                       = xine;
  this->master                     = NULL;

  pthread_mutex_init (&this->lock, NULL);

  this->prebuffer                   = PREBUFFER_PTS_OFFSET;
  this->av_offset                   = 0;
  this->spu_offset                  = 0;
  this->vpts_offset                 = 0;

  /* initialize video stuff */

  this->video_vpts                  = this->prebuffer;
  this->video_drift                 = 0;
  this->video_drift_step            = 0;
  this->video_discontinuity_count   = 0;
  this->discontinuity_handled_count = 0;
  pthread_cond_init (&this->video_discontinuity_reached, NULL);
  this->img_duration              = 3000;
  this->img_cpt                   = 0;
  this->last_video_pts            = 0;
  this->last_audio_pts            = 0;


  /* initialize audio stuff */

  this->have_video                  = have_video;
  this->have_audio                  = have_audio;
  this->audio_vpts                  = this->prebuffer;
  this->audio_vpts_rmndr            = 0;
  this->audio_discontinuity_count   = 0;
  pthread_cond_init (&this->audio_discontinuity_reached, NULL);


  return this;
}


static void metronom_sync_hook (void *this_gen, xine_cfg_entry_t *entry) {
  metronom_clock_private_t *this_priv = (metronom_clock_private_t *)this_gen;

  if (entry->num_value) {
    if (this_priv->sync_thread_state != SYNC_THREAD_NONE)
      return;
    this_priv->sync_thread_state = SYNC_THREAD_OFF;
    this_priv->next_sync_pts = STOP_PTS;
    if (this_priv->providers[1])
      metronom_start_sync_thread (this_priv);
  } else {
    if (this_priv->sync_thread_state == SYNC_THREAD_NONE)
      return;
    metronom_stop_sync_thread (this_priv);
    this_priv->sync_thread_state = SYNC_THREAD_NONE;
    if (this_priv->providers[1])
      this_priv->next_sync_pts = START_PTS;
  }
}

metronom_clock_t *_x_metronom_clock_init(xine_t *xine)
{
  metronom_clock_private_t *this_priv = calloc (1, sizeof (metronom_clock_private_t));

  if (!this_priv)
    return NULL;

  this_priv->mct.set_option       = metronom_clock_set_option;
  this_priv->mct.get_option       = metronom_clock_get_option;
  this_priv->mct.start_clock      = metronom_start_clock;
  this_priv->mct.stop_clock       = metronom_stop_clock;
  this_priv->mct.resume_clock     = metronom_resume_clock;
  this_priv->mct.get_current_time = metronom_get_current_time;
  this_priv->mct.adjust_clock     = metronom_adjust_clock;
  this_priv->mct.set_fine_speed   = metronom_set_speed;
  this_priv->mct.register_scr     = metronom_register_scr;
  this_priv->mct.unregister_scr   = metronom_unregister_scr;
  this_priv->mct.exit             = metronom_clock_exit;

  this_priv->mct.xine             = xine;
  this_priv->mct.scr_adjustable   = 1;
  this_priv->mct.scr_list         = this_priv->providers;

  pthread_mutex_init (&this_priv->mct.lock, NULL);
  this_priv->mct.register_scr (&this_priv->mct, unixscr_init (&this_priv->uscr));

  this_priv->mct.thread_running   = 0;

  this_priv->next_sync_pts = STOP_PTS;

  if (this_priv->mct.xine->config->register_bool (this_priv->mct.xine->config,
    "engine.use_metronom_sync_thread", 0,
    _("Sync multiple clocks in a separate thread"),
    _("Enable this when there are problems with multiple (eg application supplied) clocks."),
    20, metronom_sync_hook, this_priv)) {
    this_priv->sync_thread_state  = SYNC_THREAD_OFF;
  } else {
    this_priv->sync_thread_state  = SYNC_THREAD_NONE;
  }

  return &this_priv->mct;
}
