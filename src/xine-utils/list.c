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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdlib.h>
#include <xine/attributes.h>
#include <xine/list.h>
#include <xine/xineutils.h> /* dlist_*, dnode_* */

#define MIN_CHUNK_SIZE    32
#define MAX_CHUNK_SIZE 65536

typedef struct _xine_list_elem_s _xine_list_elem_t;
typedef struct _xine_list_chunk_s _xine_list_chunk_t;
/* typedef struct xine_list_s xine_list_t; */

struct _xine_list_elem_s {
  dnode_t             node;
/*_xine_list_chunk_t *chunk;*/
  void               *value;
};
  
struct _xine_list_chunk_s {
  _xine_list_chunk_t *next;
/*_xine_list_t *list;*/
  uint32_t max_elems;
  uint32_t first_unused;
  _xine_list_elem_t elems[1];
};

struct xine_list_s {
  dlist_t used;
  dlist_t free;
  _xine_list_chunk_t *chunks;
  uint32_t size;
  _xine_list_chunk_t first_chunk;
};

static _xine_list_chunk_t *XINE_MALLOC _xine_list_chunk_new (xine_list_t *list, uint32_t size) {
  _xine_list_chunk_t *chunk;
  chunk = malloc (sizeof (*chunk) + (size - 1) * sizeof (_xine_list_elem_t));
  if (!chunk)
    return NULL;
/*chunk->list = list;*/
  chunk->max_elems = size;
  chunk->first_unused = 0;
  chunk->next = list->chunks;
  list->chunks = chunk;
  return chunk;
}

xine_list_t *XINE_MALLOC xine_list_new (void) {
  xine_list_t *list;
  list = malloc (sizeof (*list) + (MIN_CHUNK_SIZE - 1) * sizeof (_xine_list_elem_t));
  if (!list)
    return NULL;
  DLIST_INIT (&list->used);
  DLIST_INIT (&list->free);
  list->size = 0;
/*list->first_chunk.list = list;*/
  list->first_chunk.max_elems = MIN_CHUNK_SIZE;
  list->first_chunk.first_unused = 0;
  list->first_chunk.next = NULL;
  list->chunks = &list->first_chunk;
  return list;
}

static void _xine_list_reset (xine_list_t *list) {
  _xine_list_chunk_t *chunk;
  chunk = list->chunks;
  while (chunk != &list->first_chunk) {
    _xine_list_chunk_t *next = chunk->next;
    free (chunk);
    chunk = next;
  }
  list->size = 0;
  list->first_chunk.first_unused = 0;
  DLIST_INIT (&list->used);
  DLIST_INIT (&list->free);
  list->chunks = &list->first_chunk;
}

void xine_list_clear (xine_list_t *list) {
  if (list)
    _xine_list_reset (list);
}

void xine_list_delete (xine_list_t *list) {
  if (list) {
    _xine_list_reset (list);
    free (list);
  }
}

static _xine_list_elem_t *_xine_list_elem_new (xine_list_t *list) {
  _xine_list_elem_t *elem;
  _xine_list_chunk_t *chunk;
  uint32_t n;

  if (!DLIST_IS_EMPTY (&list->free)) {
    elem = (_xine_list_elem_t *)list->free.head;
    DLIST_REMOVE (&elem->node);
    return elem;
  }

  chunk = list->chunks;
  if (chunk->first_unused < chunk->max_elems) {
    elem = &chunk->elems[0] + chunk->first_unused;
    chunk->first_unused++;
  /*elem->chunk = chunk;*/
    return elem;
  }

  n = chunk->max_elems * 2;
  if (n > MAX_CHUNK_SIZE)
    n = MAX_CHUNK_SIZE;
  chunk = _xine_list_chunk_new (list, n);
  if (!chunk)
    return NULL;
  elem = &chunk->elems[0];
  chunk->first_unused = 1;
/* elem->chunk = new_chunk;*/
  return elem;
}

unsigned int xine_list_size (xine_list_t *list) {
  return list ? list->size : 0;
}

unsigned int xine_list_empty (xine_list_t *list) {
  return list ? (list->size == 0) : 1;
}

xine_list_iterator_t xine_list_front (xine_list_t *list) {
  return list && list->size ? list->used.head : NULL;
}

xine_list_iterator_t xine_list_back(xine_list_t *list) {
  return list && list->size ? list->used.tail : NULL;
}

void xine_list_push_back(xine_list_t *list, void *value) {
  _xine_list_elem_t *new_elem;

  if (!list)
    return;
  new_elem = _xine_list_elem_new (list);
  if (!new_elem)
    return;

  new_elem->value = value;
  DLIST_ADD_TAIL (&new_elem->node, &list->used);
  list->size++;
}

void xine_list_push_front(xine_list_t *list, void *value) {
  _xine_list_elem_t *new_elem;

  if (!list)
    return;
  new_elem = _xine_list_elem_new (list);
  if (!new_elem)
    return;

  new_elem->value = value;
  DLIST_ADD_HEAD (&new_elem->node, &list->used);
  list->size++;
}

xine_list_iterator_t xine_list_next (xine_list_t *list, xine_list_iterator_t ite) {
  _xine_list_elem_t *elem = ite;

  elem = elem ? (_xine_list_elem_t *)elem->node.next : (_xine_list_elem_t *)list->used.head;
  return elem->node.next ? elem : NULL;
}

void *xine_list_next_value (xine_list_t *list, xine_list_iterator_t *ite) {
  _xine_list_elem_t *elem = *ite;
  if (elem) {
    elem = (_xine_list_elem_t *)elem->node.next;
  } else if (list) {
    elem = (_xine_list_elem_t *)list->used.head;
  } else {
    *ite = NULL;
    return NULL;
  }
  if (!elem->node.next) {
    *ite = NULL;
    return NULL;
  }
  *ite = elem;
  return elem->value;
}

xine_list_iterator_t xine_list_prev (xine_list_t *list, xine_list_iterator_t ite) {
  _xine_list_elem_t *elem = ite;

  elem = elem ? (_xine_list_elem_t *)elem->node.prev : (_xine_list_elem_t *)list->used.tail;
  return elem->node.prev ? elem : NULL;
}

void *xine_list_get_value (xine_list_t *list, xine_list_iterator_t ite) {
  _xine_list_elem_t *elem = ite;
  (void)list;
  return elem ? elem->value : NULL;
}

void xine_list_remove (xine_list_t *list, xine_list_iterator_t position) {
  _xine_list_elem_t *elem = position;

  if (list && elem) {
    DLIST_REMOVE (&elem->node);
    DLIST_ADD_TAIL (&elem->node, &list->free);
    list->size--;
  }
}

xine_list_iterator_t xine_list_insert (xine_list_t *list, xine_list_iterator_t position, void *value) {
  _xine_list_elem_t *new_elem, *elem = position;

  if (!list)
    return NULL;
  new_elem = _xine_list_elem_new (list);
  if (!new_elem)
    return NULL;

  new_elem->value = value;
  if (!elem) {
    DLIST_ADD_TAIL (&new_elem->node, &list->used);
  } else {
    DLIST_INSERT (&new_elem->node, &elem->node);
  }
  list->size++;
  return new_elem;
}

xine_list_iterator_t xine_list_find (xine_list_t *list, void *value) {
  _xine_list_elem_t *elem;

  if (!list)
    return NULL;
  for (elem = (_xine_list_elem_t *)list->used.head; elem->node.next; elem = (_xine_list_elem_t *)elem->node.next) {
    if (elem->value == value)
      return elem;
  }
  return NULL;
}

