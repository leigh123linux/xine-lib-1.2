/*
 * dpb.c
 *
 *  Created on: 07.12.2008
 *      Author: julian
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "dpb.h"

struct decoded_picture* init_decoded_picture(struct nal_unit *src_nal,
    VdpVideoSurface surface)
{
  struct decoded_picture *pic = malloc(sizeof(struct decoded_picture));
  pic->nal = init_nal_unit();
  copy_nal_unit(pic->nal, src_nal);
  pic->surface = surface;
  pic->next = NULL;

  return pic;
}

void free_decoded_picture(struct decoded_picture *pic)
{
  free_nal_unit(pic->nal);
}

struct decoded_picture* dpb_get_picture(struct dpb *dpb, uint32_t picnum)
{
  struct decoded_picture *pic = dpb->pictures;

  if (pic != NULL)
    do {
      if (pic->nal->curr_pic_num == picnum)
        return pic;
    } while ((pic = pic->next) != NULL);

  return NULL;
}

struct decoded_picture* dpb_get_picture_by_ltpn(struct dpb *dpb,
    uint32_t longterm_picnum)
{
  struct decoded_picture *pic = dpb->pictures;

  if (pic != NULL)
    do {
      if (pic->nal->long_term_pic_num == longterm_picnum)
        return pic;
    } while ((pic = pic->next) != NULL);

  return NULL;
}

struct decoded_picture* dpb_get_picture_by_ltidx(struct dpb *dpb,
    uint32_t longterm_idx)
{
  struct decoded_picture *pic = dpb->pictures;

  if (pic != NULL)
    do {
      if (pic->nal->long_term_frame_idx == longterm_idx)
        return pic;
    } while ((pic = pic->next) != NULL);

  return NULL;
}

int dpb_remove_picture(struct dpb *dpb, uint32_t picnum)
{
  struct decoded_picture *pic = dpb->pictures;
  struct decoded_picture *last_pic = NULL;

  if (pic != NULL)
    do {
      if (pic->nal->curr_pic_num == picnum) {
        // FIXME: free the picture....

        if (last_pic != NULL)
          last_pic->next = pic->next;
        else
          dpb->pictures = pic->next;

        free_decoded_picture(pic);
        return 0;
      }

      last_pic = pic;
    } while ((pic = pic->next) != NULL);

  return -1;
}

int dpb_remove_picture_by_ltpn(struct dpb *dpb, uint32_t longterm_picnum)
{
  struct decoded_picture *pic = dpb->pictures;
  struct decoded_picture *last_pic = NULL;

  if (pic != NULL)
    do {
      if (pic->nal->long_term_pic_num == longterm_picnum) {
        // FIXME: free the picture....

        if (last_pic != NULL)
          last_pic->next = pic->next;
        else
          dpb->pictures = pic->next;

        free_decoded_picture(pic);
        return 0;
      }

      last_pic = pic;
    } while ((pic = pic->next) != NULL);

  return -1;
}

int dpb_remove_picture_by_ltidx(struct dpb *dpb, uint32_t longterm_idx)
{
  struct decoded_picture *pic = dpb->pictures;
  struct decoded_picture *last_pic = NULL;

  if (pic != NULL)
    do {
      if (pic->nal->long_term_frame_idx == longterm_idx) {
        // FIXME: free the picture....

        if (last_pic != NULL)
          last_pic->next = pic->next;
        else
          dpb->pictures = pic->next;

        free_decoded_picture(pic);
        return 0;
      }

      last_pic = pic;
    } while ((pic = pic->next) != NULL);

  return -1;
}

int dpb_remove_ltidx_gt(struct dpb *dpb, uint32_t longterm_max)
{
  struct decoded_picture *pic = dpb->pictures;
  struct decoded_picture *last_pic = NULL;

  if (pic != NULL)
    do {
      if (pic->nal->long_term_frame_idx > longterm_max) {
        // FIXME: free the picture....
        if (last_pic != NULL)
          last_pic->next = pic->next;
        else
          dpb->pictures = pic->next;


        free_decoded_picture(pic);

        /* don't increase last_pic to current pic
         * in case we delete current pic */
        continue;
      }

      last_pic = pic;
    } while ((pic = pic->next) != NULL);

  return 0;
}

int dpb_add_picture(struct dpb *dpb, struct decoded_picture *pic)
{
  pic->next = dpb->pictures;
  dpb->pictures = pic;

  return 0;
}

int dpb_flush(struct dpb *dpb)
{
  struct decoded_picture *pic = dpb->pictures;
  struct decoded_picture *last_pic = NULL;

  if (pic != NULL)
    do {
      //FIXME: free the picture

      last_pic = pic;
    } while ((pic = pic->next) != NULL);

  dpb->pictures = NULL;

  return 0;
}

void fill_vdpau_reference_list(struct dpb *dpb, VdpReferenceFrameH264 *reflist)
{
  struct decoded_picture *pic = dpb->pictures;
  struct decoded_picture *last_pic = NULL;

  int i = 0;

  if (pic != NULL)
    do {
      if (pic->nal->nal_ref_idc != 0) {
        reflist[i].surface = pic->surface;
        reflist[i].is_long_term = pic->nal->used_for_long_term_ref;
        reflist[i].top_is_reference = pic->nal->slc->field_pic_flag
            ? (pic->nal->slc->bottom_field_flag ? 0 : 1) : 1;
        reflist[i].bottom_is_reference = pic->nal->slc->field_pic_flag
            ? (pic->nal->slc->bottom_field_flag ? 1 : 0) : 1;
        reflist[i].field_order_cnt[0] = pic->nal->top_field_order_cnt;
        reflist[i].field_order_cnt[1] = pic->nal->bottom_field_order_cnt;
        i++;
      }
      last_pic = pic;
    } while ((pic = pic->next) != NULL && i < 16);

  printf("Used ref-frames: %d\n", i);

  // fill all other frames with invalid handles
  while(i < 16) {
    reflist[i].bottom_is_reference = VDP_FALSE;
    reflist[i].top_is_reference = VDP_FALSE;
    reflist[i].frame_idx = VDP_INVALID_HANDLE;
    reflist[i].is_long_term = VDP_FALSE;
    reflist[i].surface = VDP_INVALID_HANDLE;
    reflist[i].field_order_cnt[0] = 0;
    reflist[i].field_order_cnt[1] = 0;
    i++;
  }
}
