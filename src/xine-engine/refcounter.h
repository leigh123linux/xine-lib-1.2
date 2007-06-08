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
 * $Id: refcounter.h,v 1.2 2006/09/26 05:19:49 dgp85 Exp $
 *
 */
#ifndef HAVE_REFCOUNTER_H
#define HAVE_REFCOUNTER_H

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <pthread.h>

typedef struct {
  pthread_mutex_t   lock;
  void*             object;               /* referenced object */
  void            (*destructor)(void *);  /* object destructor */
  int               count;
} refcounter_t;

typedef void (*refcounter_destructor)(void*);

refcounter_t* _x_new_refcounter(void *object, refcounter_destructor destructor) XINE_PROTECTED;

int _x_refcounter_inc(refcounter_t *refcounter) XINE_PROTECTED;

int _x_refcounter_dec(refcounter_t *refcounter) XINE_PROTECTED;

void _x_refcounter_dispose(refcounter_t *refcounter) XINE_PROTECTED;

#endif /* HAVE_REFCOUNTER_H */
