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
 * $Id:
 *
 * Event handling functions
 *
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "xine_internal.h"

int xine_register_event_listener(xine_t *this, event_listener_t listener) {
  /* Ensure the listener is non-NULL */
  if(listener == NULL) {
    return 0;
  }

  /* Check we hava a slot free */
  if(this->num_event_listeners < XINE_MAX_EVENT_LISTENERS) {
    this->event_listeners[this->num_event_listeners++] = listener;
    return 1;
  } 

  return 0;
}

void xine_send_event(xine_t *this, event_t *event, void *data) {
  uint16_t i;
  
  /* Itterate through all event handlers */
  for(i=0; i < this->num_event_listeners; i++) {
    (this->event_listeners[i]) (this, event, data);
  }
}

int xine_remove_event_listener(xine_t *this, event_listener_t listener) {
  uint16_t i, found;

  found = 1; i = 0;

  /* Attempt to find the listener */
  while((found == 1) && (i < this->num_event_listeners)) {
    if(this->event_listeners[i] == listener) {
      /* Set found flag */
      found = 0;

      this->event_listeners[i] = NULL;

      /* If possible, move the last listener to the hole thats left */
      if(this->num_event_listeners > 1) {
	this->event_listeners[i] = this->event_listeners[this->num_event_listeners - 1];
	this->event_listeners[this->num_event_listeners - 1] = NULL;
      }

      this->num_event_listeners --;
    }

    i++;
  }

  return found;
}
