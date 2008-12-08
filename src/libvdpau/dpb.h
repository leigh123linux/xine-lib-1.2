/*
 * dpb.h
 *
 *  Created on: 06.12.2008
 *      Author: julian
 */

#ifndef DPB_H_
#define DPB_H_

#include "nal.h"
#include "video_out.h"

struct decoded_picture {
  VdpVideoSurface surface;
  vo_frame_t *img; /* this is the image we block, to make sure
                    * the surface is not double-used */
  struct nal_unit *nal;

  struct decoded_picture *next;
};

/* Decoded Picture Buffer */
struct dpb {
  struct decoded_picture *pictures;
  uint32_t used;
};

struct decoded_picture* init_decoded_picture(struct nal_unit *src_nal,
    VdpVideoSurface surface, vo_frame_t *img);
void free_decoded_picture(struct decoded_picture *pic);

struct decoded_picture* dpb_get_picture(struct dpb *dpb, uint32_t picnum);
struct decoded_picture* dpb_get_picture_by_ltpn(struct dpb *dpb, uint32_t longterm_picnum);
struct decoded_picture* dpb_get_picture_by_ltidx(struct dpb *dpb, uint32_t longterm_idx);
int dpb_remove_picture(struct dpb *dpb, uint32_t picnum);
int dpb_remove_picture_by_ltpn(struct dpb *dpb, uint32_t longterm_picnum);
int dpb_remove_picture_by_ltidx(struct dpb *dpb, uint32_t longterm_idx);
int dpb_remove_ltidx_gt(struct dpb *dpb, uint32_t longterm_max);
int dpb_add_picture(struct dpb *dpb, struct decoded_picture *pic, uint32_t num_ref_frames);
int dpb_flush(struct dpb *dpb);

void fill_vdpau_reference_list(struct dpb *dpb, VdpReferenceFrameH264 *reflist);

#endif /* DPB_H_ */
