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
 * Event handling functions
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <pthread.h>
#include <sys/time.h>

#include <xine/xine_internal.h>

xine_event_t *xine_event_get  (xine_event_queue_t *queue) {

  xine_event_t *event;
  xine_list_iterator_t ite;

  pthread_mutex_lock (&queue->lock);
  ite = NULL;
  event = xine_list_next_value (queue->events, &ite);
  /* XXX: Do NULL events really intend to plug the queue ?? */
  if (event)
    xine_list_remove (queue->events, ite);
  pthread_mutex_unlock (&queue->lock);

  return event;
}

static xine_event_t *xine_event_wait_locked (xine_event_queue_t *queue) {

  xine_event_t  *event;
  xine_list_iterator_t ite;

  while ( !(ite = xine_list_front (queue->events)) ) {
    pthread_cond_wait (&queue->new_event, &queue->lock);
  }

  event = xine_list_get_value (queue->events, ite);

  xine_list_remove (queue->events, ite);

  return event;
}

xine_event_t *xine_event_wait (xine_event_queue_t *queue) {

  xine_event_t  *event;

  pthread_mutex_lock (&queue->lock);
  event = xine_event_wait_locked(queue);
  pthread_mutex_unlock (&queue->lock);

  return event;
}

void xine_event_free (xine_event_t *event) {
  _x_freep (&event->data);
  free (event);
}

void xine_event_send (xine_stream_t *stream, const xine_event_t *event) {

  xine_list_iterator_t ite;
  xine_event_queue_t *queue;

  pthread_mutex_lock (&stream->event_queues_lock);

  ite = NULL;
  while ((queue = xine_list_next_value (stream->event_queues, &ite))) {
    xine_event_t *cevent;

    cevent = malloc (sizeof (xine_event_t));
    if (!cevent)
      continue;

    cevent->type        = event->type;
    cevent->stream      = stream;
    cevent->data_length = event->data_length;
    if ((event->data_length > 0) && (event->data) ) {
      cevent->data = malloc (event->data_length);
      memcpy (cevent->data, event->data, event->data_length);
    } else {
      cevent->data = NULL;
    }
    gettimeofday (&cevent->tv, NULL);

    pthread_mutex_lock (&queue->lock);
    xine_list_push_back (queue->events, cevent);
    pthread_cond_signal (&queue->new_event);
    pthread_mutex_unlock (&queue->lock);
  }

  pthread_mutex_unlock (&stream->event_queues_lock);
}


xine_event_queue_t *xine_event_new_queue (xine_stream_t *stream) {

  xine_event_queue_t *queue;

  _x_refcounter_inc(stream->refcounter);

  queue = malloc (sizeof (xine_event_queue_t));

  pthread_mutex_init (&queue->lock, NULL);
  pthread_cond_init (&queue->new_event, NULL);
  pthread_cond_init (&queue->events_processed, NULL);
  queue->events = xine_list_new ();
  queue->stream = stream;
  queue->listener_thread = NULL;
  queue->callback_running = 0;

  pthread_mutex_lock (&stream->event_queues_lock);
  xine_list_push_back (stream->event_queues, queue);
  pthread_mutex_unlock (&stream->event_queues_lock);

  return queue;
}

void xine_event_dispose_queue (xine_event_queue_t *queue) {

  xine_stream_t        *stream = queue->stream;
  xine_event_t         *event;
  xine_event_t         *qevent;
  xine_event_queue_t   *q;
  xine_list_iterator_t  ite;

  pthread_mutex_lock (&stream->event_queues_lock);

  q = NULL;
  ite = NULL;
  while ((q = xine_list_next_value (stream->event_queues, &ite))) {
    if (q == queue)
      break;
  }

  if (q != queue) {
    xprintf (stream->xine, XINE_VERBOSITY_DEBUG, "events: tried to dispose queue which is not in list\n");

    pthread_mutex_unlock (&stream->event_queues_lock);
    return;
  }

  xine_list_remove (stream->event_queues, ite);
  pthread_mutex_unlock (&stream->event_queues_lock);

  /*
   * send quit event
   */
  qevent = (xine_event_t *)malloc(sizeof(xine_event_t));

  qevent->type        = XINE_EVENT_QUIT;
  qevent->stream      = stream;
  qevent->data        = NULL;
  qevent->data_length = 0;
  gettimeofday (&qevent->tv, NULL);

  pthread_mutex_lock (&queue->lock);
  xine_list_push_back (queue->events, qevent);
  pthread_cond_signal (&queue->new_event);
  pthread_mutex_unlock (&queue->lock);

  /*
   * join listener thread, if any
   */

  if (queue->listener_thread) {
    void *p;
    pthread_join (*queue->listener_thread, &p);
    _x_freep (&queue->listener_thread);
  }

  _x_refcounter_dec(stream->refcounter);

  /*
   * clean up pending events
   */

  while ( (event = xine_event_get (queue)) ) {
    xine_event_free (event);
  }
  xine_list_delete(queue->events);

  pthread_mutex_destroy(&queue->lock);
  pthread_cond_destroy(&queue->new_event);
  pthread_cond_destroy(&queue->events_processed);

  free (queue);
}


static void *listener_loop (void *queue_gen) {

  xine_event_queue_t *queue = (xine_event_queue_t *) queue_gen;
  int running = 1;

  pthread_mutex_lock (&queue->lock);

  while (running) {

    xine_event_t *event;

    event = xine_event_wait_locked (queue);

    if (event->type == XINE_EVENT_QUIT)
      running = 0;

    queue->callback_running = 1;

    pthread_mutex_unlock (&queue->lock);

    queue->callback (queue->user_data, event);
    xine_event_free (event);

    pthread_mutex_lock (&queue->lock);

    queue->callback_running = 0;

    if (xine_list_empty (queue->events)) {
      pthread_cond_signal (&queue->events_processed);
    }
  }

  pthread_mutex_unlock (&queue->lock);
  return NULL;
}


void xine_event_create_listener_thread (xine_event_queue_t *queue,
					xine_event_listener_cb_t callback,
					void *user_data) {
  int err;

  queue->listener_thread = malloc (sizeof (pthread_t));
  queue->callback        = callback;
  queue->user_data       = user_data;

  if ((err = pthread_create (queue->listener_thread,
			     NULL, listener_loop, queue)) != 0) {
    xprintf (queue->stream->xine, XINE_VERBOSITY_NONE,
	     "events: can't create new thread (%s)\n", strerror(err));
    _x_abort();
  }
}
