/*
 * nal.c
 *
 *  Created on: 07.12.2008
 *      Author: julian
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

  nal->sps = malloc(sizeof(struct seq_parameter_set_rbsp));
  nal->pps = malloc(sizeof(struct pic_parameter_set_rbsp));
  nal->slc = malloc(sizeof(struct slice_header));

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

  xine_fast_memcpy(dest->sps, src->sps, sizeof(struct seq_parameter_set_rbsp));
  xine_fast_memcpy(dest->pps, src->pps, sizeof(struct pic_parameter_set_rbsp));
  xine_fast_memcpy(dest->slc, src->slc, sizeof(struct slice_header));
}
