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
 * Sorted array which grows automatically when you add elements.
 * A binary search is used to find the position of a new element.
 *
 * Example:
 *   Let's create de comparison method for integers:
 *
 *     int int_comparator(void *a, void *b) {
 *       if ((int)a < (int)b) {
 *         return -1;
 *       } else if ((int)a == (int)b) {
 *         return 0;
 *       } else {
 *        return 1;
 *       }
 *     }
 *
 *   Create a sorted array for integers:
 *     xine_sarray_t *sarray = xine_sarray_new(10, int_comparator);
 *
 *   Add elements:
 *     xine_sarray_add(sarray, (void*)4);
 *     xine_sarray_add(sarray, (void*)28);
 *     xine_sarray_add(sarray, (void*)7);
 *
 *   Find an element:
 *     int pos = xine_sarray_binary_search(sarray, (void*)7);
 *     if (pos >= 0)
 *       FOUND
 *     else
 *       NOT FOUND
 *
 *   Delete the array:
 *     xine_sarray_delete(sarray);
 */
#ifndef XINE_SORTED_ARRAY_H
#define XINE_SORTED_ARRAY_H

#include <stddef.h> /* size_t */
#include <xine/attributes.h>

/* Array type */
typedef struct xine_sarray_s xine_sarray_t;

/* Array element comparator */
typedef int (*xine_sarray_comparator_t)(void*, void*);

/* Constructor */
xine_sarray_t *xine_sarray_new(size_t initial_size, xine_sarray_comparator_t comparator) XINE_MALLOC XINE_PROTECTED;

/* Destructor */
void xine_sarray_delete(xine_sarray_t *sarray) XINE_PROTECTED;

/* Set mode */
/* For both add() and binary_search(): if there are multiple matching indices,
 * select 1 of them randomly. That is, the order of matches does not matter, just be fast. */
#define XINE_SARRAY_MODE_DEFAULT 0x00000000
/* select the first match. */
#define XINE_SARRAY_MODE_FIRST   0x80000000
/* select the last match (+ 1 if found). */
#define XINE_SARRAY_MODE_LAST    0x40000000
/* For add(): if there is a match already, dont add another copy,
 * and return ~(found_index) instead. */
#define XINE_SARRAY_MODE_UNIQUE  0x20000000
void xine_sarray_set_mode (xine_sarray_t *sarray, unsigned int mode) XINE_PROTECTED;

/* Returns the number of element stored in the array */
size_t xine_sarray_size(const xine_sarray_t *sarray) XINE_PROTECTED;

/* Removes all elements from an array */
void xine_sarray_clear(xine_sarray_t *sarray) XINE_PROTECTED;

/* Adds the element into the array
   Returns the insertion position */
int xine_sarray_add(xine_sarray_t *sarray, void *value) XINE_PROTECTED;

/* Removes one element from an array at the position specified */
void xine_sarray_remove(xine_sarray_t *sarray, unsigned int position) XINE_PROTECTED;

/* Removes one element from an array by user pointer.
 * Return the index it was found at, or ~0. */
int xine_sarray_remove_ptr (xine_sarray_t *sarray, void *ptr) XINE_PROTECTED;

/* Get the element at the position specified */
void *xine_sarray_get(xine_sarray_t *sarray, unsigned int position) XINE_PROTECTED;

/* Returns the index of the search key, if it is contained in the list.
   Otherwise, (-(insertion point) - 1) or ~(insertion point).
   The insertion point is defined as the point at which the key would be
   inserted into the array. */
int xine_sarray_binary_search(xine_sarray_t *sarray, void *key) XINE_PROTECTED;

#endif

