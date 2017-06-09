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
 *
 *
 * contents:
 *
 * buffer_entry structure - serves as a transport encapsulation
 *   of the mpeg audio/video data through xine
 *
 * free buffer pool management routines
 *
 * FIFO buffer structures/routines
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <assert.h>

/********** logging **********/
#define LOG_MODULE "buffer"
#define LOG_VERBOSE
/*
#define LOG
*/

#include <xine/buffer.h>
#include <xine/xineutils.h>
#include <xine/xine_internal.h>

/* The large buffer feature.
 * If we have enough contigous memory, and if we can afford to hand if out,
 * provide an oversize item there. The buffers covering that extra space will
 * hide inside our buffer array, and buffer_pool_free () will reappear them
 * mysteriously later.
 * Small bufs are requested frequently, so we dont do a straightforward
 * heap manager. Instead, we keep bufs in pool sorted by address, and
 * be_ei_t.nbufs holds the count of contigous bufs when this is the
 * first of such a group.
 * Although not used by xine-lib-1.2 itself, API permits using bufs in a
 * different fifo than their pool origin. Thats why we test buf->source.
 * It is also possible to supply fully custom bufs. We detect these by
 * buf->free_buffer != buffer_pool_free.
 */

typedef struct {
  buf_element_t elem; /* needs to be first */
  int nbufs;          /* # of contigous bufs */
  extra_info_t  ei;
} be_ei_t;

/*
 * put a previously allocated buffer element back into the buffer pool
 */
static void buffer_pool_free (buf_element_t *element) {
  fifo_buffer_t *this = (fifo_buffer_t *) element->source;
  be_ei_t *newhead, *newtail, *nexthead;
  int n;

  pthread_mutex_lock (&this->buffer_pool_mutex);

  newhead = (be_ei_t *)element;
  n = newhead->nbufs;
  this->buffer_pool_num_free += n;
  if (this->buffer_pool_num_free > this->buffer_pool_capacity) {
    fprintf(stderr, _("xine-lib: buffer.c: There has been a fatal error: TOO MANY FREE's\n"));
    _x_abort();
  }

  /* we might be a new chunk */
  newtail = newhead;
  while (--n > 0) {
    newtail[0].elem.next = &newtail[1].elem;
    newtail++;
  }

  nexthead = (be_ei_t *)this->buffer_pool_top;
  if (!nexthead || (nexthead > newtail)) {
    /* add head */
    this->buffer_pool_top = &newhead->elem;
    newtail->elem.next = &nexthead->elem;
    /* merge with next chunk if no gap */
    if (newtail + 1 == nexthead)
      newhead->nbufs += nexthead->nbufs;
  } else {
    /* Keep the pool sorted, elem1 > elem2 implies elem1->mem > elem2->mem. */
    be_ei_t *prevhead, *prevtail;
    while (1) {
      prevhead = nexthead;
      prevtail = prevhead + prevhead->nbufs - 1;
      nexthead = (be_ei_t *)prevtail->elem.next;
      if (!nexthead || (nexthead > newtail))
        break;
    }
    prevtail->elem.next = &newhead->elem;
    newtail->elem.next = &nexthead->elem;
    /* merge with next chunk if no gap */
    if (newtail + 1 == nexthead)
      newhead->nbufs += nexthead->nbufs;
    /* merge with prev chunk if no gap */
    if (prevtail + 1 == newhead)
      prevhead->nbufs += newhead->nbufs;
  }
    
  pthread_cond_signal (&this->buffer_pool_cond_not_empty);

  pthread_mutex_unlock (&this->buffer_pool_mutex);
}

/*
 * allocate a buffer from buffer pool
 */

static buf_element_t *buffer_pool_size_alloc (fifo_buffer_t *this, size_t size) {

  int i, n;
  be_ei_t *buf;

  n = size ? ((int)size + this->buffer_pool_buf_size - 1) / this->buffer_pool_buf_size : 1;
  if (n > (this->buffer_pool_capacity >> 2))
    n = this->buffer_pool_capacity >> 2;
  if (n < 1)
    n = 1;

  pthread_mutex_lock (&this->buffer_pool_mutex);

  for (i = 0; this->alloc_cb[i]; i++)
    this->alloc_cb[i] (this, this->alloc_cb_data[i]);

  /* we always keep one free buffer for emergency situations like
   * decoder flushes that would need a buffer in buffer_pool_try_alloc() */
  while (this->buffer_pool_num_free < n + 2) {
    pthread_cond_wait (&this->buffer_pool_cond_not_empty, &this->buffer_pool_mutex);
  }

  buf = (be_ei_t *)this->buffer_pool_top;
  if (n == 1) {

    this->buffer_pool_top = buf->elem.next;
    i = buf->nbufs - 1;
    if (i > 0)
      buf[1].nbufs = i;
    this->buffer_pool_num_free--;

  } else {

    be_ei_t *beststart = buf, *bestprev = NULL, *bestnext = NULL, *prev = NULL, *next;
    int bestsize = 0, l;
    do {
      l = buf->nbufs;
      next = (be_ei_t *)buf[l - 1].elem.next;
      if (l >= n)
        break;
      if (l > bestsize) {
        bestsize = l;
        bestprev = prev;
        beststart = buf;
        bestnext = next;
      }
      prev = buf + l - 1;
      buf = next;
    } while (buf);
    if (!buf) {
      prev = bestprev;
      buf = beststart;
      next = bestnext;
      l = n = bestsize;
    }
    if (n < l) {
      next = buf + n;
      next->nbufs = l - n;
    }
    if (prev)
      prev->elem.next = &next->elem;
    else
      this->buffer_pool_top = &next->elem;
    this->buffer_pool_num_free -= n;

  }

  pthread_mutex_unlock (&this->buffer_pool_mutex);

  /* set sane values to the newly allocated buffer */
  buf->elem.content = buf->elem.mem; /* 99% of demuxers will want this */
  buf->elem.pts = 0;
  buf->elem.size = 0;
  buf->elem.max_size = n * this->buffer_pool_buf_size;
  buf->elem.decoder_flags = 0;
  buf->nbufs = n;
  memset (buf->elem.decoder_info, 0, sizeof (buf->elem.decoder_info));
  memset (buf->elem.decoder_info_ptr, 0, sizeof (buf->elem.decoder_info_ptr));
  _x_extra_info_reset (buf->elem.extra_info);

  return &buf->elem;
}

static buf_element_t *buffer_pool_alloc (fifo_buffer_t *this) {
  be_ei_t *buf;
  int i;

  pthread_mutex_lock (&this->buffer_pool_mutex);

  for(i = 0; this->alloc_cb[i]; i++)
    this->alloc_cb[i](this, this->alloc_cb_data[i]);

  /* we always keep one free buffer for emergency situations like
   * decoder flushes that would need a buffer in buffer_pool_try_alloc() */
  while (this->buffer_pool_num_free < 2) {
    pthread_cond_wait (&this->buffer_pool_cond_not_empty, &this->buffer_pool_mutex);
  }

  buf = (be_ei_t *)this->buffer_pool_top;
  this->buffer_pool_top = buf->elem.next;
  i = buf->nbufs - 1;
  if (i > 0)
    buf[1].nbufs = i;
  this->buffer_pool_num_free--;

  pthread_mutex_unlock (&this->buffer_pool_mutex);

  /* set sane values to the newly allocated buffer */
  buf->elem.content = buf->elem.mem; /* 99% of demuxers will want this */
  buf->elem.pts = 0;
  buf->elem.size = 0;
  buf->elem.max_size = this->buffer_pool_buf_size;
  buf->elem.decoder_flags = 0;
  buf->nbufs = 1;
  memset (buf->elem.decoder_info, 0, sizeof (buf->elem.decoder_info));
  memset (buf->elem.decoder_info_ptr, 0, sizeof (buf->elem.decoder_info_ptr));
  _x_extra_info_reset (buf->elem.extra_info);

  return &buf->elem;
}

/*
 * allocate a buffer from buffer pool - may fail if none is available
 */

static buf_element_t *buffer_pool_try_alloc (fifo_buffer_t *this) {
  be_ei_t *buf;
  int i;

  pthread_mutex_lock (&this->buffer_pool_mutex);
  buf = (be_ei_t *)this->buffer_pool_top;
  if (!buf) {
    pthread_mutex_unlock (&this->buffer_pool_mutex);
    return NULL;
  }

  this->buffer_pool_top = buf->elem.next;
  i = buf->nbufs - 1;
  if (i > 0)
    buf[1].nbufs = i;
  this->buffer_pool_num_free--;
  pthread_mutex_unlock (&this->buffer_pool_mutex);

  /* set sane values to the newly allocated buffer */
  buf->elem.content = buf->elem.mem; /* 99% of demuxers will want this */
  buf->elem.pts = 0;
  buf->elem.size = 0;
  buf->elem.max_size = this->buffer_pool_buf_size;
  buf->elem.decoder_flags = 0;
  buf->nbufs = 1;
  memset (buf->elem.decoder_info, 0, sizeof (buf->elem.decoder_info));
  memset (buf->elem.decoder_info_ptr, 0, sizeof (buf->elem.decoder_info_ptr));
  _x_extra_info_reset (buf->elem.extra_info);

  return &buf->elem;
}


/*
 * append buffer element to fifo buffer
 */
static void fifo_buffer_put (fifo_buffer_t *fifo, buf_element_t *element) {
  int i;

  pthread_mutex_lock (&fifo->mutex);

  if (element->decoder_flags & BUF_FLAG_MERGE) {
    be_ei_t *new = (be_ei_t *)element, *prev = (be_ei_t *)fifo->last;
    new->elem.decoder_flags &= ~BUF_FLAG_MERGE;
    if (prev && (prev + prev->nbufs == new)
      && (prev->elem.type == new->elem.type)
      && (prev->nbufs < (fifo->buffer_pool_capacity >> 3))) {
      fifo->fifo_size += new->nbufs;
      fifo->fifo_data_size += new->elem.size;
      prev->nbufs += new->nbufs;
      prev->elem.max_size += new->elem.max_size;
      prev->elem.size += new->elem.size;
      prev->elem.decoder_flags |= new->elem.decoder_flags;
      pthread_mutex_unlock (&fifo->mutex);
      return;
    }
  }

  for(i = 0; fifo->put_cb[i]; i++)
    fifo->put_cb[i](fifo, element, fifo->put_cb_data[i]);

  if (fifo->last)
    fifo->last->next = element;
  else
    fifo->first = element;

  fifo->last = element;
  element->next = NULL;

  if (element->free_buffer == buffer_pool_free) {
    be_ei_t *beei = (be_ei_t *)element;
    fifo->fifo_size += beei->nbufs;
  } else {
    fifo->fifo_size += 1;
  }
  fifo->fifo_data_size += element->size;

  pthread_cond_signal (&fifo->not_empty);

  pthread_mutex_unlock (&fifo->mutex);
}

/*
 * simulate append buffer element to fifo buffer
 */
static void dummy_fifo_buffer_put (fifo_buffer_t *fifo, buf_element_t *element) {
  int i;

  pthread_mutex_lock (&fifo->mutex);

  for(i = 0; fifo->put_cb[i]; i++)
    fifo->put_cb[i](fifo, element, fifo->put_cb_data[i]);

  pthread_mutex_unlock (&fifo->mutex);

  element->free_buffer(element);
}

/*
 * insert buffer element to fifo buffer (demuxers MUST NOT call this one)
 */
static void fifo_buffer_insert (fifo_buffer_t *fifo, buf_element_t *element) {
  pthread_mutex_lock (&fifo->mutex);

  element->next = fifo->first;
  fifo->first = element;

  if( !fifo->last )
    fifo->last = element;

  if (element->free_buffer == buffer_pool_free) {
    be_ei_t *beei = (be_ei_t *)element;
    fifo->fifo_size += beei->nbufs;
  } else {
    fifo->fifo_size += 1;
  }
  fifo->fifo_data_size += element->size;

  pthread_cond_signal (&fifo->not_empty);

  pthread_mutex_unlock (&fifo->mutex);
}

/*
 * insert buffer element to fifo buffer (demuxers MUST NOT call this one)
 */
static void dummy_fifo_buffer_insert (fifo_buffer_t *fifo, buf_element_t *element) {

  element->free_buffer(element);
}

/*
 * get element from fifo buffer
 */
static buf_element_t *fifo_buffer_get (fifo_buffer_t *fifo) {
  buf_element_t *buf;
  int i;

  pthread_mutex_lock (&fifo->mutex);

  while (fifo->first==NULL) {
    pthread_cond_wait (&fifo->not_empty, &fifo->mutex);
  }

  buf = fifo->first;

  fifo->first = fifo->first->next;
  if (fifo->first==NULL)
    fifo->last = NULL;

  if (buf->free_buffer == buffer_pool_free) {
    be_ei_t *beei = (be_ei_t *)buf;
    fifo->fifo_size -= beei->nbufs;
  } else {
    fifo->fifo_size -= 1;
  }
  fifo->fifo_data_size -= buf->size;

  for(i = 0; fifo->get_cb[i]; i++)
    fifo->get_cb[i](fifo, buf, fifo->get_cb_data[i]);

  pthread_mutex_unlock (&fifo->mutex);

  return buf;
}

/*
 * clear buffer (put all contained buffer elements back into buffer pool)
 */
static void fifo_buffer_clear (fifo_buffer_t *fifo) {
  be_ei_t *start;

  pthread_mutex_lock (&fifo->mutex);

  /* take out all at once */
  start = (be_ei_t *)fifo->first;
  fifo->first = fifo->last = NULL;
  fifo->fifo_size = 0;
  fifo->fifo_data_size = 0;

  while (start) {
    be_ei_t *buf, *next;
    int n;

    /* keep control bufs (flush, ...) */
    if ((start->elem.type & BUF_MAJOR_MASK) == BUF_CONTROL_BASE) {
      if (!fifo->first)
        fifo->first = &start->elem;
      else
        fifo->last->next = &start->elem;
      fifo->last = &start->elem;
      fifo->fifo_size += 1;
      fifo->fifo_data_size += start->elem.size;
      buf = (be_ei_t *)start->elem.next;
      start->elem.next = NULL;
      start = buf;
      continue;
    }

    /* free custom buf */
    if (start->elem.free_buffer != buffer_pool_free) {
      buf = (be_ei_t *)start->elem.next;
      start->elem.next = NULL;
      start->elem.free_buffer (&start->elem);
      start = buf;
      continue;
    }

    /* optimize: get contiguous chunk */
    buf = start;
    n = 0;
    while (1) {
      int i = buf->nbufs;
      next = (be_ei_t *)buf->elem.next;
      n += i;
      if (buf + i != next) /* includes next == NULL et al ;-) */
        break;
      if ((next->elem.type & BUF_MAJOR_MASK) == BUF_CONTROL_BASE)
        break;
      buf = next;
    }
    start->nbufs = n;
    start->elem.free_buffer (&start->elem);
    start = next;
  }

  /* printf("Free buffers after clear: %d\n", fifo->buffer_pool_num_free); */
  pthread_mutex_unlock (&fifo->mutex);
}

static void fifo_buffer_all_clear (fifo_buffer_t *fifo) {
  be_ei_t *start;

  pthread_mutex_lock (&fifo->mutex);

  /* take out all at once */
  start = (be_ei_t *)fifo->first;
  fifo->first = fifo->last = NULL;
  fifo->fifo_size = 0;
  fifo->fifo_data_size = 0;

  while (start) {
    be_ei_t *buf, *next;
    int n;

    /* free custom buf */
    if (start->elem.free_buffer != buffer_pool_free) {
      buf = (be_ei_t *)start->elem.next;
      start->elem.next = NULL;
      start->elem.free_buffer (&start->elem);
      start = buf;
      continue;
    }

    /* optimize: get contiguous chunk */
    buf = start;
    n = 0;
    while (1) {
      int i = buf->nbufs;
      next = (be_ei_t *)buf->elem.next;
      n += i;
      if (buf + i != next) /* includes next == NULL ;-) */
        break;
      buf = next;
    }
    /* free just sibling bufs */
    if (start->elem.source != (void *)fifo) {
      start->nbufs = n;
      start->elem.free_buffer (&start->elem);
    }
    start = next;
  }

  pthread_mutex_unlock (&fifo->mutex);
}

/*
 * Return the number of elements in the fifo buffer
 */
static int fifo_buffer_size (fifo_buffer_t *this) {
  int size;

  pthread_mutex_lock(&this->mutex);
  size = this->fifo_size;
  pthread_mutex_unlock(&this->mutex);

  return size;
}

/*
 * Return the amount of the data in the fifo buffer
 */
static uint32_t fifo_buffer_data_size (fifo_buffer_t *this) {
  uint32_t data_size;

  pthread_mutex_lock(&this->mutex);
  data_size = this->fifo_data_size;
  pthread_mutex_unlock(&this->mutex);

  return data_size;
}

/*
 * Return the number of free elements in the pool
 */
static int fifo_buffer_num_free (fifo_buffer_t *this) {
  int buffer_pool_num_free;

  pthread_mutex_lock(&this->mutex);
  buffer_pool_num_free = this->buffer_pool_num_free;
  pthread_mutex_unlock(&this->mutex);

  return buffer_pool_num_free;
}

/*
 * Destroy the buffer
 */
static void fifo_buffer_dispose (fifo_buffer_t *this) {
  fifo_buffer_all_clear (this);
  xine_free_aligned (this->buffer_pool_base);
  pthread_mutex_destroy(&this->mutex);
  pthread_cond_destroy(&this->not_empty);
  pthread_mutex_destroy(&this->buffer_pool_mutex);
  pthread_cond_destroy(&this->buffer_pool_cond_not_empty);
  free (this);
}

/*
 * Register an "alloc" callback
 */
static void fifo_register_alloc_cb (fifo_buffer_t *this,
                                    void (*cb)(fifo_buffer_t *this,
                                               void *data_cb),
                                    void *data_cb) {
  int i;

  pthread_mutex_lock(&this->mutex);
  for(i = 0; this->alloc_cb[i]; i++)
    ;
  if( i != BUF_MAX_CALLBACKS-1 ) {
    this->alloc_cb[i] = cb;
    this->alloc_cb_data[i] = data_cb;
    this->alloc_cb[i+1] = NULL;
  }
  pthread_mutex_unlock(&this->mutex);
}

/*
 * Register a "put" callback
 */
static void fifo_register_put_cb (fifo_buffer_t *this,
                                  void (*cb)(fifo_buffer_t *this,
                                             buf_element_t *buf,
                                             void *data_cb),
                                  void *data_cb) {
  int i;

  pthread_mutex_lock(&this->mutex);
  for(i = 0; this->put_cb[i]; i++)
    ;
  if( i != BUF_MAX_CALLBACKS-1 ) {
    this->put_cb[i] = cb;
    this->put_cb_data[i] = data_cb;
    this->put_cb[i+1] = NULL;
  }
  pthread_mutex_unlock(&this->mutex);
}

/*
 * Register a "get" callback
 */
static void fifo_register_get_cb (fifo_buffer_t *this,
                                  void (*cb)(fifo_buffer_t *this,
                                             buf_element_t *buf,
                                             void *data_cb),
                                  void *data_cb) {
  int i;

  pthread_mutex_lock(&this->mutex);
  for(i = 0; this->get_cb[i]; i++)
    ;
  if( i != BUF_MAX_CALLBACKS-1 ) {
    this->get_cb[i] = cb;
    this->get_cb_data[i] = data_cb;
    this->get_cb[i+1] = NULL;
  }
  pthread_mutex_unlock(&this->mutex);
}

/*
 * Unregister an "alloc" callback
 */
static void fifo_unregister_alloc_cb (fifo_buffer_t *this,
                                      void (*cb)(fifo_buffer_t *this,
                                                 void *data_cb) ) {
  int i,j;

  pthread_mutex_lock(&this->mutex);
  for(i = 0; this->alloc_cb[i]; i++) {
    if( this->alloc_cb[i] == cb ) {
      for(j = i; this->alloc_cb[j]; j++) {
        this->alloc_cb[j] = this->alloc_cb[j+1];
        this->alloc_cb_data[j] = this->alloc_cb_data[j+1];
      }
    }
  }
  pthread_mutex_unlock(&this->mutex);
}

/*
 * Unregister a "put" callback
 */
static void fifo_unregister_put_cb (fifo_buffer_t *this,
                                  void (*cb)(fifo_buffer_t *this,
                                             buf_element_t *buf,
                                             void *data_cb) ) {
  int i,j;

  pthread_mutex_lock(&this->mutex);
  for(i = 0; this->put_cb[i]; i++) {
    if( this->put_cb[i] == cb ) {
      for(j = i; this->put_cb[j]; j++) {
        this->put_cb[j] = this->put_cb[j+1];
        this->put_cb_data[j] = this->put_cb_data[j+1];
      }
    }
  }
  pthread_mutex_unlock(&this->mutex);
}

/*
 * Unregister a "get" callback
 */
static void fifo_unregister_get_cb (fifo_buffer_t *this,
                                  void (*cb)(fifo_buffer_t *this,
                                             buf_element_t *buf,
                                             void *data_cb) ) {
  int i,j;

  pthread_mutex_lock(&this->mutex);
  for(i = 0; this->get_cb[i]; i++) {
    if( this->get_cb[i] == cb ) {
      for(j = i; this->get_cb[j]; j++) {
        this->get_cb[j] = this->get_cb[j+1];
        this->get_cb_data[j] = this->get_cb_data[j+1];
      }
    }
  }
  pthread_mutex_unlock(&this->mutex);
}

/*
 * allocate and initialize new (empty) fifo buffer
 */
fifo_buffer_t *_x_fifo_buffer_new (int num_buffers, uint32_t buf_size) {

  fifo_buffer_t *this;
  int            i;
  unsigned char *multi_buffer;
  be_ei_t       *beei;

  this = calloc(1, sizeof(fifo_buffer_t));
  if (!this)
    return NULL;

  /* printf ("Allocating %d buffers of %ld bytes in one chunk\n", num_buffers, (long int) buf_size); */
  multi_buffer = xine_mallocz_aligned (num_buffers * (buf_size + sizeof (be_ei_t)));
  if (!multi_buffer) {
    free (this);
    return NULL;
  }

  this->first               = NULL;
  this->last                = NULL;
  this->fifo_size           = 0;
  this->put                 = fifo_buffer_put;
  this->insert              = fifo_buffer_insert;
  this->get                 = fifo_buffer_get;
  this->clear               = fifo_buffer_clear;
  this->size                = fifo_buffer_size;
  this->num_free            = fifo_buffer_num_free;
  this->data_size           = fifo_buffer_data_size;
  this->dispose             = fifo_buffer_dispose;
  this->register_alloc_cb   = fifo_register_alloc_cb;
  this->register_get_cb     = fifo_register_get_cb;
  this->register_put_cb     = fifo_register_put_cb;
  this->unregister_alloc_cb = fifo_unregister_alloc_cb;
  this->unregister_get_cb   = fifo_unregister_get_cb;
  this->unregister_put_cb   = fifo_unregister_put_cb;
  pthread_mutex_init (&this->mutex, NULL);
  pthread_cond_init (&this->not_empty, NULL);

  /* init buffer pool */

  pthread_mutex_init (&this->buffer_pool_mutex, NULL);
  pthread_cond_init (&this->buffer_pool_cond_not_empty, NULL);

  this->buffer_pool_num_free   =
  this->buffer_pool_capacity   = num_buffers;
  this->buffer_pool_buf_size   = buf_size;
  this->buffer_pool_alloc      = buffer_pool_alloc;
  this->buffer_pool_try_alloc  = buffer_pool_try_alloc;
  this->buffer_pool_size_alloc = buffer_pool_size_alloc;

  this->buffer_pool_base = multi_buffer;
  beei = (be_ei_t *)(multi_buffer + num_buffers * buf_size);
  this->buffer_pool_top  = &beei->elem;
  beei->nbufs = num_buffers;

  for (i = 0; i < num_buffers; i++) {
    beei->elem.mem         = multi_buffer;
    multi_buffer          += buf_size;
    beei->elem.max_size    = buf_size;
    beei->elem.free_buffer = buffer_pool_free;
    beei->elem.source      = this;
    beei->elem.extra_info  = &beei->ei;
    beei->elem.next        = &(beei + 1)->elem;
    beei++;
  }

  (beei - 1)->elem.next = NULL;

  this->alloc_cb[0]              = NULL;
  this->get_cb[0]                = NULL;
  this->put_cb[0]                = NULL;
  this->alloc_cb_data[0]         = NULL;
  this->get_cb_data[0]           = NULL;
  this->put_cb_data[0]           = NULL;
  return this;
}

/*
 * allocate and initialize new (empty) fifo buffer
 */
fifo_buffer_t *_x_dummy_fifo_buffer_new (int num_buffers, uint32_t buf_size) {

  fifo_buffer_t *this;

  this = _x_fifo_buffer_new(num_buffers, buf_size);
  this->put    = dummy_fifo_buffer_put;
  this->insert = dummy_fifo_buffer_insert;
  return this;
}
