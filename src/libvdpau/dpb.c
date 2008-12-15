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
#include "video_out.h"

struct decoded_picture* init_decoded_picture(struct nal_unit *src_nal,
    VdpVideoSurface surface, vo_frame_t *img)
{
  struct decoded_picture *pic = malloc(sizeof(struct decoded_picture));
  pic->nal = init_nal_unit();
  copy_nal_unit(pic->nal, src_nal);
  pic->used_for_reference = 0;
  pic->delayed_output = 0;
  pic->top_is_reference = pic->nal->slc->field_pic_flag
        ? (pic->nal->slc->bottom_field_flag ? 0 : 1) : 1;
  pic->bottom_is_reference = pic->nal->slc->field_pic_flag
        ? (pic->nal->slc->bottom_field_flag ? 1 : 0) : 1;
  pic->surface = surface;
  pic->img = img;
  pic->next = NULL;

  return pic;
}

void free_decoded_picture(struct decoded_picture *pic)
{
  pic->img->free(pic->img);
  free_nal_unit(pic->nal);
}

struct decoded_picture* dpb_get_next_out_picture(struct dpb *dpb)
{
  struct decoded_picture *pic = dpb->pictures;
  struct decoded_picture *outpic = NULL;

  if(dpb->used < MAX_DPB_SIZE)
    return NULL;

  if (pic != NULL)
    do {
      if (pic->delayed_output &&
          (outpic == NULL ||
              pic->nal->top_field_order_cnt < outpic->nal->top_field_order_cnt))
        outpic = pic;
    } while ((pic = pic->next) != NULL);

  if(outpic)
    printf("OUTPUT: %lld\n", outpic->img->pts);
  return outpic;
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

int dpb_set_unused_ref_picture_a(struct dpb *dpb, struct decoded_picture *refpic)
{
  struct decoded_picture *pic = dpb->pictures;
    if (pic != NULL)
      do {
        if (pic == refpic) {
          pic->used_for_reference = 0;
          if(!pic->delayed_output)
            dpb_remove_picture(dpb, pic);
          return 0;
        }
      } while ((pic = pic->next) != NULL);

    return -1;
}

int dpb_set_unused_ref_picture(struct dpb *dpb, uint32_t picnum)
{
  struct decoded_picture *pic = dpb->pictures;
  if (pic != NULL)
    do {
      if (pic->nal->curr_pic_num == picnum) {
        pic->used_for_reference = 0;
        if(!pic->delayed_output)
          dpb_remove_picture(dpb, pic);
        return 0;
      }
    } while ((pic = pic->next) != NULL);

  return -1;
}

int dpb_set_unused_ref_picture_byltpn(struct dpb *dpb, uint32_t longterm_picnum)
{
  struct decoded_picture *pic = dpb->pictures;
  if (pic != NULL)
    do {
      if (pic->nal->long_term_pic_num == longterm_picnum) {
        pic->used_for_reference = 0;
        if(!pic->delayed_output)
          dpb_remove_picture(dpb, pic);
        return 0;
      }
    } while ((pic = pic->next) != NULL);

  return -1;
}

int dpb_set_unused_ref_picture_bylidx(struct dpb *dpb, uint32_t longterm_idx)
{
  struct decoded_picture *pic = dpb->pictures;
  if (pic != NULL)
    do {
      if (pic->nal->long_term_frame_idx == longterm_idx) {
        pic->used_for_reference = 0;
        if(!pic->delayed_output)
          dpb_remove_picture(dpb, pic);
        return 0;
      }
    } while ((pic = pic->next) != NULL);

  return -1;
}

int dpb_set_unused_ref_picture_lidx_gt(struct dpb *dpb, uint32_t longterm_idx)
{
  struct decoded_picture *pic = dpb->pictures;
  if (pic != NULL)
    do {
      if (pic->nal->long_term_frame_idx >= longterm_idx) {
        pic->used_for_reference = 0;
        if(!pic->delayed_output) {
          struct decoded_picture *next_pic = pic->next;
          dpb_remove_picture(dpb, pic);
          pic = next_pic;
          continue;
        }
      }
    } while ((pic = pic->next) != NULL);

  return -1;
}


int dpb_set_output_picture(struct dpb *dpb, struct decoded_picture *outpic)
{
  struct decoded_picture *pic = dpb->pictures;
  if (pic != NULL)
    do {
      if (pic == outpic) {
        pic->delayed_output = 0;
        if(!pic->used_for_reference)
          dpb_remove_picture(dpb, pic);
        return 0;
      }
    } while ((pic = pic->next) != NULL);

  return -1;
}

int dpb_remove_picture(struct dpb *dpb, struct decoded_picture *rempic)
{
  struct decoded_picture *pic = dpb->pictures;
  struct decoded_picture *last_pic = NULL;

  if (pic != NULL)
    do {
      if (pic == rempic) {
        // FIXME: free the picture....

        if (last_pic != NULL)
          last_pic->next = pic->next;
        else
          dpb->pictures = pic->next;
        free_decoded_picture(pic);
        dpb->used--;
        return 0;
      }

      last_pic = pic;
    } while ((pic = pic->next) != NULL);

  return -1;
}

int dpb_remove_picture_by_picnum(struct dpb *dpb, uint32_t picnum)
{
  struct decoded_picture *pic = dpb->pictures;
  struct decoded_picture *last_pic = NULL;

  if (pic != NULL)
    do {
      if (pic->nal->curr_pic_num == picnum) {
        dpb_remove_picture(dpb, pic);
      }

      last_pic = pic;
    } while ((pic = pic->next) != NULL);

  return -1;
}

int dpb_add_picture(struct dpb *dpb, struct decoded_picture *pic, uint32_t num_ref_frames)
{
  int i = 0;
  struct decoded_picture *last_pic;

  pic->next = dpb->pictures;
  dpb->pictures = pic;
  dpb->used++;

  if(dpb->used > num_ref_frames) {
    do {
      if(pic->used_for_reference) {
        i++;
        if(i>num_ref_frames) {
          pic->used_for_reference = 0;
          if(!pic->delayed_output)
            dpb_remove_picture(dpb, pic);
          pic = last_pic;
        }
        last_pic = pic;
      }
    } while ((pic = pic->next) != NULL);
  }

  return 0;
}

int dpb_flush(struct dpb *dpb)
{
  struct decoded_picture *pic = dpb->pictures;

  if (pic != NULL)
    do {
      struct decoded_picture *next_pic = pic->next;
      dpb_set_unused_ref_picture_a(dpb, pic);
      pic = next_pic;
    } while (pic != NULL);

  printf("Flushed, used: %d\n", dpb->used);
  //dpb->pictures = NULL;
  //dpb->used = 0;

  return 0;
}

void fill_vdpau_reference_list(struct dpb *dpb, VdpReferenceFrameH264 *reflist)
{
  struct decoded_picture *pic = dpb->pictures;
  struct decoded_picture *last_pic = NULL;

  int i = 0;

  if (pic != NULL)
    do {
      if (pic->used_for_reference) {
        reflist[i].surface = pic->surface;
        reflist[i].is_long_term = pic->nal->used_for_long_term_ref;
        if(reflist[i].is_long_term)
          reflist[i].frame_idx = pic->nal->slc->frame_num; //pic->nal->long_term_frame_idx;
        else
          reflist[i].frame_idx = pic->nal->slc->frame_num; //pic->nal->curr_pic_num;
        reflist[i].top_is_reference = pic->top_is_reference; /*pic->nal->slc->field_pic_flag
            ? (pic->nal->slc->bottom_field_flag ? 0 : 1) : 1;*/
        reflist[i].bottom_is_reference = pic->bottom_is_reference; /*pic->nal->slc->field_pic_flag
            ? (pic->nal->slc->bottom_field_flag ? 1 : 0) : 1;*/
        reflist[i].field_order_cnt[0] = pic->nal->top_field_order_cnt;
        reflist[i].field_order_cnt[1] = pic->nal->bottom_field_order_cnt;
        i++;
      }
      last_pic = pic;
    } while ((pic = pic->next) != NULL && i < 16);

  // fill all other frames with invalid handles
  while(i < 16) {
    reflist[i].bottom_is_reference = VDP_FALSE;
    reflist[i].top_is_reference = VDP_FALSE;
    reflist[i].frame_idx = 0;
    reflist[i].is_long_term = VDP_FALSE;
    reflist[i].surface = VDP_INVALID_HANDLE;
    reflist[i].field_order_cnt[0] = 0;
    reflist[i].field_order_cnt[1] = 0;
    i++;
  }
}
