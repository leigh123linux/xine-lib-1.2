/*
 * Copyright (C) 2000-2003 the xine project
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
 * $Id: scratch.c,v 1.24 2007/01/19 00:12:22 dgp85 Exp $
 *
 * top-level xine functions
 *
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h>
#include <stdarg.h>
#include <string.h>  /* For memset */

#define LOG_MODULE "scratch"
#define LOG_VERBOSE
/*
#define LOG
*/

#include "xineutils.h"
#include "scratch.h"

static void __attribute__((__format__(__printf__, 2, 0)))
  scratch_printf (scratch_buffer_t *this, const char *format, va_list argp)
{
  time_t t;
  struct tm tm;
  size_t l;

  pthread_mutex_lock (&this->lock);

  time (&t);
  localtime_r (&t, &tm);

  if ( ! this->lines[this->cur] )
    this->lines[this->cur] = xine_xmalloc(SCRATCH_LINE_LEN_MAX+1);
  if ( ! this->lines[this->cur] )
    return;

  strftime (this->lines[this->cur], SCRATCH_LINE_LEN_MAX, "%X: ", &tm);
  l = strlen (this->lines[this->cur]);
  vsnprintf (this->lines[this->cur] + l, SCRATCH_LINE_LEN_MAX - l, format, argp);

  lprintf ("printing format %s to line %d\n", format, this->cur);
  this->cur = (this->cur + 1) % this->num_lines;

  pthread_mutex_unlock (&this->lock);
}

static char **scratch_get_content (scratch_buffer_t *this) {
  int i, j;

  pthread_mutex_lock (&this->lock);

  for(i = 0, j = (this->cur - 1); i < this->num_lines; i++, j--) {

    if(j < 0)
      j = (this->num_lines - 1);

    free (this->ordered[i]);
    this->ordered[i] = this->lines[j] ? strdup (this->lines[j]) : NULL;
    lprintf ("line %d contains >%s<\n", i , this->lines[j]);
  }

  pthread_mutex_unlock (&this->lock);
  return this->ordered;

}

static void scratch_dispose (scratch_buffer_t *this) {
  int   i;
  
  pthread_mutex_lock (&this->lock);

  for(i = 0; i < this->num_lines; i++ ) {
    free(this->ordered[i]);
    free(this->lines[i]);
  }
  
  free (this->lines);
  free (this->ordered);

  pthread_mutex_unlock (&this->lock);
  pthread_mutex_destroy (&this->lock);

  free (this);
}

scratch_buffer_t *_x_new_scratch_buffer (int num_lines) {
  scratch_buffer_t *this;

  this = xine_xmalloc (sizeof (scratch_buffer_t));

  this->lines   = xine_xcalloc ((num_lines + 1), sizeof(char*));
  this->ordered = xine_xcalloc ((num_lines + 1), sizeof(char*));

  this->scratch_printf = scratch_printf;
  this->get_content    = scratch_get_content;
  this->dispose        = scratch_dispose;
  this->num_lines      = num_lines;
  this->cur            = 0;
  pthread_mutex_init (&this->lock, NULL);

  return this;
}
