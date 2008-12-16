/*
 * Copyright (C) 2008 Julian Scheel
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
 * nal.c: nal-structure utility functions
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "nal.h"
#include "xine_internal.h"

struct nal_unit* init_nal_unit()
{
  struct nal_unit *nal = malloc(sizeof(struct nal_unit));
  memset(nal, 0x00, sizeof(struct nal_unit));

  /*nal->sps = malloc(sizeof(struct seq_parameter_set_rbsp));
  memset(nal->sps, 0x00, sizeof(struct seq_parameter_set_rbsp));
  nal->pps = malloc(sizeof(struct pic_parameter_set_rbsp));
  memset(nal->pps, 0x00, sizeof(struct pic_parameter_set_rbsp));
  nal->slc = malloc(sizeof(struct slice_header));
  memset(nal->slc, 0x00, sizeof(struct slice_header));*/

  return nal;
}

void free_nal_unit(struct nal_unit *nal)
{
  free(nal->sps);
  free(nal->pps);
  free(nal->slc);
  free(nal);
}

void copy_nal_unit(struct nal_unit *dest, struct nal_unit *src)
{
  /* size without pps, sps and slc units: */
  int size = sizeof(struct nal_unit) - sizeof(struct seq_parameter_set_rbsp*)
      - sizeof(struct pic_parameter_set_rbsp*) - sizeof(struct slice_header*);

  xine_fast_memcpy(dest, src, size);

  if(!dest->sps)
    dest->sps = malloc(sizeof(struct seq_parameter_set_rbsp));

  if(!dest->pps)
    dest->pps = malloc(sizeof(struct pic_parameter_set_rbsp));

  if(!dest->slc)
    dest->slc = malloc(sizeof(struct slice_header));

  xine_fast_memcpy(dest->sps, src->sps, sizeof(struct seq_parameter_set_rbsp));
  xine_fast_memcpy(dest->pps, src->pps, sizeof(struct pic_parameter_set_rbsp));
  xine_fast_memcpy(dest->slc, src->slc, sizeof(struct slice_header));
}
