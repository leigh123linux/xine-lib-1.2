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
 * dpb.h: Decoder Picture Buffer
 */

#ifndef DPB_H_
#define DPB_H_

#define MAX_DPB_SIZE 16

#include "nal.h"
#include "video_out.h"

struct decoded_picture {
  VdpVideoSurface surface;
  vo_frame_t *img; /* this is the image we block, to make sure
                    * the surface is not double-used */
  struct nal_unit *nal;

  uint8_t used_for_reference;
  uint8_t top_is_reference;
  uint8_t bottom_is_reference;

  uint8_t delayed_output;

  struct decoded_picture *next;
};

/* Decoded Picture Buffer */
struct dpb {
  struct decoded_picture *pictures;

  uint32_t num_ref_frames;
  uint32_t used;
};

struct decoded_picture* init_decoded_picture(struct nal_unit *src_nal,
    VdpVideoSurface surface, vo_frame_t *img);
void free_decoded_picture(struct decoded_picture *pic);

struct decoded_picture* dpb_get_next_out_picture(struct dpb *dpb);

struct decoded_picture* dpb_get_picture(struct dpb *dpb, uint32_t picnum);
struct decoded_picture* dpb_get_picture_by_ltpn(struct dpb *dpb, uint32_t longterm_picnum);
struct decoded_picture* dpb_get_picture_by_ltidx(struct dpb *dpb, uint32_t longterm_idx);

int dpb_set_unused_ref_picture(struct dpb *dpb, uint32_t picnum);
int dpb_set_unused_ref_picture_a(struct dpb *dpb, struct decoded_picture *refpic);
int dpb_set_unused_ref_picture_byltpn(struct dpb *dpb, uint32_t longterm_picnum);
int dpb_set_unused_ref_picture_bylidx(struct dpb *dpb, uint32_t longterm_idx);
int dpb_set_unused_ref_picture_lidx_gt(struct dpb *dpb, uint32_t longterm_idx);

int dpb_set_output_picture(struct dpb *dpb, struct decoded_picture *outpic);

int dpb_remove_picture(struct dpb *dpb, struct decoded_picture *rempic);
int dpb_add_picture(struct dpb *dpb, struct decoded_picture *pic, uint32_t num_ref_frames);
int dpb_flush(struct dpb *dpb);
void dpb_free_all( struct dpb *dpb );
void dpb_clear_all_pts( struct dpb *dpb );

int fill_vdpau_reference_list(struct dpb *dpb, VdpReferenceFrameH264 *reflist);

#endif /* DPB_H_ */
