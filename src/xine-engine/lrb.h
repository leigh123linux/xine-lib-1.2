/*
 * Copyright (C) 2001-2003 the xine project
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
 * $Id: lrb.h,v 1.4 2003/12/09 00:02:36 f1rmb Exp $
 *
 * lrb : limited ring buffer
 * used for temporal buffer, limited to n elements
 *
 */

#ifndef HAVE_LRB_H
#define HAVE_LRB_H

#ifdef XINE_COMPILE
#  include "buffer.h"
#else
#  include <xine/buffer.h>
#endif

typedef struct {

  int   max_num_entries;
  int   cur_num_entries;

  buf_element_t *newest, *oldest;
  fifo_buffer_t *fifo;

} lrb_t;


lrb_t *lrb_new (int max_num_entries,
		fifo_buffer_t *fifo) ;

void lrb_drop (lrb_t *this) ;

void lrb_add (lrb_t *this, buf_element_t *buf) ;

void lrb_feedback (lrb_t *this, fifo_buffer_t *fifo) ;

void lrb_flush (lrb_t *this) ;

#endif
