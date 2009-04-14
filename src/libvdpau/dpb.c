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
 * dpb.c: Implementing Decoded Picture Buffer
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "dpb.h"
#include "nal.h"
#include "video_out.h"

struct decoded_picture* init_decoded_picture(struct nal_unit *src_nal,
    VdpVideoSurface surface, vo_frame_t *img)
{
  struct decoded_picture *pic = calloc(1, sizeof(struct decoded_picture));
  pic->nal = init_nal_unit();
  copy_nal_unit(pic->nal, src_nal);
  pic->top_is_reference = pic->nal->slc->field_pic_flag
        ? (pic->nal->slc->bottom_field_flag ? 0 : 1) : 1;
  pic->bottom_is_reference = pic->nal->slc->field_pic_flag
        ? (pic->nal->slc->bottom_field_flag ? 1 : 0) : 1;
  pic->surface = surface;
  pic->img = img;

  return pic;
}

void free_decoded_picture(struct decoded_picture *pic)
{
  pic->img->free(pic->img);
  free_nal_unit(pic->nal);
  free(pic);
}

struct decoded_picture* dpb_get_next_out_picture(struct dpb *dpb, int do_flush)
{
  struct decoded_picture *pic = dpb->pictures;
  struct decoded_picture *outpic = NULL;

  if(!do_flush && dpb->used < MAX_DPB_SIZE)
    return NULL;

  if (pic != NULL)
    do {
      if (pic->delayed_output &&
          (outpic == NULL ||
              (pic->nal->top_field_order_cnt <= outpic->nal->top_field_order_cnt &&
                  pic->nal->bottom_field_order_cnt <= outpic->nal->bottom_field_order_cnt)||
              (outpic->nal->top_field_order_cnt < 0 && pic->nal->top_field_order_cnt > 0 &&
                  outpic->nal->bottom_field_order_cnt < 0 && pic->nal->bottom_field_order_cnt > 0)||
              outpic->nal->nal_unit_type == NAL_SLICE_IDR))
        outpic = pic;
    } while ((pic = pic->next) != NULL);

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

static int dpb_remove_picture_by_img(struct dpb *dpb, vo_frame_t *remimg)
{
  struct decoded_picture *pic = dpb->pictures;
  struct decoded_picture *last_pic = NULL;

  if (pic != NULL)
    do {
      if (pic->img == remimg) {
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
  pic->img->lock(pic->img);
  if (0 == dpb_remove_picture_by_img(dpb, pic->img))
    fprintf(stderr, "broken stream: current img was already in dpb -- freed it\n");
  else
    pic->img->free(pic->img);

  int i = 0;
  struct decoded_picture *last_pic = dpb->pictures;

  pic->next = dpb->pictures;
  dpb->pictures = pic;
  dpb->num_ref_frames = num_ref_frames;
  dpb->used++;

  if(pic != NULL && dpb->used > num_ref_frames) {
    do {
      if(pic->used_for_reference) {
        i++;
        if(i>num_ref_frames) {
          pic->used_for_reference = 0;
          if(pic == dpb->pictures)
            last_pic = pic->next;

          if(!pic->delayed_output) {
            dpb_remove_picture(dpb, pic);
          }
          pic = last_pic;
          if(pic == dpb->pictures)
            continue;
        }
        last_pic = pic;
      }
    } while (pic != NULL && (pic = pic->next) != NULL);
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

  //printf("Flushed, used: %d\n", dpb->used);

  return 0;
}

void dpb_free_all( struct dpb *dpb )
{
  struct decoded_picture *pic = dpb->pictures;

  if (pic != NULL)
    do {
      struct decoded_picture *next_pic = pic->next;
      free_decoded_picture(pic);
      --dpb->used;
      pic = next_pic;
    } while (pic != NULL);

  printf("dpb_free_all, used: %d\n", dpb->used);
  dpb->pictures = NULL;
}

void dpb_clear_all_pts( struct dpb *dpb )
{
  struct decoded_picture *pic = dpb->pictures;

  while (pic != NULL) {
    pic->img->pts = 0;
    pic = pic->next;
  }
}

int fill_vdpau_reference_list(struct dpb *dpb, VdpReferenceFrameH264 *reflist)
{
  struct decoded_picture *pic = dpb->pictures;
  struct decoded_picture *last_pic = NULL;

  int i = 0;
  int used_refframes = 0;

  if (pic != NULL)
    do {
      if (pic->used_for_reference) {
        reflist[i].surface = pic->surface;
        reflist[i].is_long_term = pic->nal->used_for_long_term_ref;
        if(reflist[i].is_long_term)
          reflist[i].frame_idx = pic->nal->slc->frame_num;
        else
          reflist[i].frame_idx = pic->nal->slc->frame_num;
        reflist[i].top_is_reference = pic->top_is_reference;
        reflist[i].bottom_is_reference = pic->bottom_is_reference;
        reflist[i].field_order_cnt[0] = pic->nal->top_field_order_cnt;
        reflist[i].field_order_cnt[1] = pic->nal->bottom_field_order_cnt;
        i++;
      }
      last_pic = pic;
    } while ((pic = pic->next) != NULL && i < 16);

  used_refframes = i;

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

  return used_refframes;
}
