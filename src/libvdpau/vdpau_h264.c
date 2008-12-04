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
 * foovideo.c: This is a reference video decoder for the xine multimedia
 * player. It really works too! It will output frames of packed YUY2 data
 * where each byte in the map is the same value, which is 3 larger than the
 * value from the last frame. This creates a slowly rotating solid color
 * frame when the frames are played in succession.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>
#include <vdpau/vdpau.h>

#include "xine_internal.h"
#include "video_out.h"
#include "buffer.h"
#include "xineutils.h"
#include "bswap.h"
#include "accel_vdpau.h"
#include "nal_parser.h"

#define VIDEOBUFSIZE 128*1024

typedef struct {
  video_decoder_class_t   decoder_class;
} vdpau_h264_class_t;

typedef struct vdpau_h264_decoder_s {
  video_decoder_t   video_decoder;  /* parent video decoder structure */

  vdpau_h264_class_t *class;
  xine_stream_t    *stream;

  /* these are traditional variables in a video decoder object */
  uint64_t          video_step;  /* frame duration in pts units */
  int               decoder_started;  /* current decoder status */
  int               decoder_initialized; /* vdpau init state */
  int               skipframes;

  unsigned char    *buf;         /* the accumulated buffer data */
  int               bufsize;     /* the maximum size of buf */
  int               size;        /* the current size of buf */

  int               width;       /* the width of a video frame */
  int               height;      /* the height of a video frame */
  double            ratio;       /* the width to height ratio */


  struct nal_parser *nal_parser;  /* h264 nal parser. extracts stream data for vdpau */

  VdpDecoder        decoder;

  VdpDecoderProfile profile;
  VdpPictureInfoH264 vdp_picture_info;
  vdpau_accel_t     *vdpau_accel;

  VdpReferenceFrameH264 reference_frames[16];
  int reference_frames_used;

  xine_t            *xine;

} vdpau_h264_decoder_t;

/**************************************************************************
 * vdpau_h264 specific decode functions
 *************************************************************************/

/**************************************************************************
 * xine video plugin functions
 *************************************************************************/


static inline void dump_pictureinfo_h264(VdpPictureInfoH264 *pic)
{
  printf("slice_count: %d\n", pic->slice_count);
  printf("field_order_cnt[0]: %d\n", pic->field_order_cnt[0]);
  printf("field_order_cnt[1]: %d\n", pic->field_order_cnt[1]);
  printf("is_reference: %d\n", pic->is_reference);
  printf("frame_num: %d\n", pic->frame_num);
  printf("field_pic_flag: %d\n", pic->field_pic_flag);
  printf("bottom_field_flag: %d\n", pic->bottom_field_flag);
  printf("num_ref_frames: %d\n", pic->num_ref_frames);
  printf("mb_adaptive_frame_field_flag: %d\n", pic->mb_adaptive_frame_field_flag);
  printf("constrained_intra_pred_flag: %d\n", pic->constrained_intra_pred_flag);
  printf("weighted_pred_flag: %d\n", pic->weighted_pred_flag);
  printf("weighted_bipred_idc: %d\n", pic->weighted_bipred_idc);
  printf("frame_mbs_only_flag: %d\n", pic->frame_mbs_only_flag);
  printf("transform_8x8_mode_flag: %d\n", pic->transform_8x8_mode_flag);
  printf("chroma_qp_index_offset: %d\n", pic->chroma_qp_index_offset);
  printf("second_chroma_qp_index_offset: %d\n", pic->second_chroma_qp_index_offset);
  printf("pic_init_qp_minus26: %d\n", pic->pic_init_qp_minus26);
  printf("num_ref_idx_l0_active_minus1: %d\n", pic->num_ref_idx_l0_active_minus1);
  printf("num_ref_idx_l1_active_minus1: %d\n", pic->num_ref_idx_l1_active_minus1);
  printf("log2_max_frame_num_minus4: %d\n", pic->log2_max_frame_num_minus4);
  printf("pic_order_cnt_type: %d\n", pic->pic_order_cnt_type);
  printf("log2_max_pic_order_cnt_lsb_minus4: %d\n", pic->log2_max_pic_order_cnt_lsb_minus4);
  printf("delta_pic_order_always_zero_flag: %d\n", pic->delta_pic_order_always_zero_flag);
  printf("direct_8x8_inference_flag: %d\n", pic->direct_8x8_inference_flag);
  printf("entropy_coding_mode_flag: %d\n", pic->entropy_coding_mode_flag);
  printf("pic_order_present_flag: %d\n", pic->pic_order_present_flag);
  printf("deblocking_filter_control_present_flag: %d\n", pic->deblocking_filter_control_present_flag);
  printf("redundant_pic_cnt_present_flag: %d\n", pic->redundant_pic_cnt_present_flag);

  int i, j;
  for(i = 0; i < 6; i++) {
    printf("scalint_list4x4[%d]: ", i);
    for(j = 0; j < 16; j++) {
      printf("[%d] ", pic->scaling_lists_4x4[i][j]);
      if(j%8 == 0)
        printf("\n");
    }
    printf("\n");
  }
  for(i = 0; i < 2; i++) {
    printf("scalint_list4x4[%d]: ", i);
    for(j = 0; j < 64; j++) {
      printf("[%d] ", pic->scaling_lists_4x4[i][j]);
      if(j%8 == 0)
        printf("\n");
    }
    printf("\n");
  }
  /*memcpy(pic.scaling_lists_4x4, pps->scaling_lists_4x4, 6*16);
  memcpy(pic.scaling_lists_8x8, pps->scaling_lists_8x8, 2*64);
  memcpy(pic.referenceFrames, this->reference_frames, sizeof(this->reference_frames));*/

}

/*
 * This function receives a buffer of data from the demuxer layer and
 * figures out how to handle it based on its header flags.
 */
static void vdpau_h264_decode_data (video_decoder_t *this_gen,
  buf_element_t *buf) {

  vdpau_h264_decoder_t *this = (vdpau_h264_decoder_t *) this_gen;

  VdpBitstreamBuffer vdp_buffer;
  vdp_buffer.struct_version = VDP_BITSTREAM_BUFFER_VERSION;

  vo_frame_t *img; /* video out frame */

  /* a video decoder does not care about this flag (?) */
  if (buf->decoder_flags & BUF_FLAG_PREVIEW)
    return;

  if (buf->decoder_flags & BUF_FLAG_FRAMERATE) {
    this->video_step = buf->decoder_info[0];
    _x_stream_info_set(this->stream, XINE_STREAM_INFO_FRAME_DURATION, this->video_step);
  }

  if (buf->decoder_flags & BUF_FLAG_STDHEADER) { /* need to initialize */
    return;
  } else {

    /* parse the first nal packages to retrieve profile type */
    int len = 0;
    uint32_t slice_count;

    while(len < buf->size) {
      len += parse_frame(this->nal_parser, buf->content + len, buf->size - len,
          (void*)&vdp_buffer.bitstream, &vdp_buffer.bitstream_bytes, &slice_count);

      if(!this->decoder_initialized &&
          this->nal_parser->current_nal->sps != NULL) {

        this->width = this->nal_parser->current_nal->sps->pic_width;
        this->height = this->nal_parser->current_nal->sps->pic_height;

        /* FIXME: ratio should be calculated in some other way to
         * support anamorph codings...
         */
        this->ratio = (double)this->width / (double)this->height;

        switch(this->nal_parser->current_nal->sps->profile_idc) {
          case 100:
            this->profile = VDP_DECODER_PROFILE_H264_HIGH;
            break;
          case 77:
            this->profile = VDP_DECODER_PROFILE_H264_MAIN;
            break;
          case 66:
          default:
            this->profile = VDP_DECODER_PROFILE_H264_BASELINE;
            break;
        }

        /* get the vdpau context from vo */
        (this->stream->video_out->open) (this->stream->video_out, this->stream);
        img = this->stream->video_out->get_frame (this->stream->video_out,
                                          this->width, this->height,
                                          this->ratio,
                                          XINE_IMGFMT_VDPAU, VO_BOTH_FIELDS);

         this->vdpau_accel = (vdpau_accel_t*)img->accel_data;

         /*VdpBool is_supported;
         uint32_t max_level, max_references, max_width, max_height;*/
         xprintf(this->xine, XINE_VERBOSITY_LOG,
             "Create decoder: vdp_device: %d, profile: %d, res: %dx%d\n",
             this->vdpau_accel->vdp_device, this->profile, this->width, this->height);

         VdpStatus status = this->vdpau_accel->vdp_decoder_create(this->vdpau_accel->vdp_device,
             this->profile, this->width, this->height, &this->decoder);

         if(status != VDP_STATUS_OK)
           xprintf(this->xine, XINE_VERBOSITY_LOG, "vdpau_h264: ERROR: VdpDecoderCreate returned status != OK (%d)\n", status);
         else
           this->decoder_initialized = 1;

         img->free(img);
         img = NULL;
      }

      if(this->decoder_initialized) {
        if(vdp_buffer.bitstream_bytes > 0 &&
            this->nal_parser->current_nal->slc != NULL &&
            this->nal_parser->current_nal->sps != NULL &&
            this->nal_parser->current_nal->pps != NULL) {

          struct pic_parameter_set_rbsp *pps = this->nal_parser->current_nal->pps;
          struct seq_parameter_set_rbsp *sps = this->nal_parser->current_nal->sps;
          struct slice_header *slc = this->nal_parser->current_nal->slc;

          /* go and decode a frame */
          VdpPictureInfoH264 pic;

          pic.slice_count = slice_count;
          pic.field_order_cnt[0] = this->nal_parser->top_field_order_cnt;
          pic.field_order_cnt[1] = this->nal_parser->bottom_field_order_cnt;
          pic.is_reference =
            (this->nal_parser->current_nal->nal_ref_idc != 0) ? VDP_TRUE : VDP_FALSE;
          pic.frame_num = slc->frame_num;
          pic.field_pic_flag = slc->field_pic_flag;
          pic.bottom_field_flag = slc->bottom_field_flag;
          pic.num_ref_frames = sps->num_ref_frames;
          pic.mb_adaptive_frame_field_flag = sps->mb_adaptive_frame_field_flag;
          pic.constrained_intra_pred_flag = pps->constrained_intra_pred_flag;
          pic.weighted_pred_flag = pps->weighted_pred_flag;
          pic.weighted_bipred_idc = pps->weighted_bipred_idc;
          pic.frame_mbs_only_flag = sps->frame_mbs_only_flag;
          pic.transform_8x8_mode_flag = pps->transform_8x8_mode_flag;
          pic.chroma_qp_index_offset = pps->chroma_qp_index_offset;
          pic.second_chroma_qp_index_offset = pps->second_chroma_qp_index_offset;
          pic.pic_init_qp_minus26 = pps->pic_init_qp_minus26;
          pic.num_ref_idx_l0_active_minus1 = pps->num_ref_idx_l0_active_minus1;
          pic.num_ref_idx_l1_active_minus1 = pps->num_ref_idx_l1_active_minus1;
          pic.log2_max_frame_num_minus4 = sps->log2_max_frame_num_minus4;
          pic.pic_order_cnt_type = sps->pic_order_cnt_type;
          pic.log2_max_pic_order_cnt_lsb_minus4 = sps->log2_max_pic_order_cnt_lsb_minus4;
          pic.delta_pic_order_always_zero_flag = sps->delta_pic_order_always_zero_flag;
          pic.direct_8x8_inference_flag = sps->direct_8x8_inference_flag;
          pic.entropy_coding_mode_flag = pps->entropy_coding_mode_flag;
          pic.pic_order_present_flag = pps->pic_order_present_flag;
          pic.deblocking_filter_control_present_flag = pps->deblocking_filter_control_present_flag;
          pic.redundant_pic_cnt_present_flag = pps->redundant_pic_cnt_present_flag;
          memcpy(pic.scaling_lists_4x4, pps->scaling_lists_4x4, sizeof(pic.scaling_lists_4x4));
          memcpy(pic.scaling_lists_8x8, pps->scaling_lists_8x8, sizeof(pic.scaling_lists_8x8));
          memcpy(pic.referenceFrames, this->reference_frames, sizeof(pic.referenceFrames));

          if(this->decoder_started || pic.is_reference) {
            this->nal_parser->is_idr = 0;
            if(!this->decoder_started)
              this->decoder_started = 1;

            //dump_pictureinfo_h264(&pic);

            int i;
            printf("Decode data: \n");
            for(i = 0; i < ((vdp_buffer.bitstream_bytes < 20) ? vdp_buffer.bitstream_bytes : 20); i++) {
              printf("%02x ", ((uint8_t*)vdp_buffer.bitstream)[i]);
              if((i+1) % 10 == 0)
                printf("\n");
            }
            printf("\n...\n");
            for(i = vdp_buffer.bitstream_bytes - 20; i < vdp_buffer.bitstream_bytes; i++) {
              printf("%02x ", ((uint8_t*)vdp_buffer.bitstream)[i]);
              if((i+1) % 10 == 0)
                printf("\n");
            }


            if(img == NULL) {
              img = this->stream->video_out->get_frame (this->stream->video_out,
                                                        this->width, this->height,
                                                        this->ratio,
                                                        XINE_IMGFMT_VDPAU, VO_BOTH_FIELDS);
              this->vdpau_accel = (vdpau_accel_t*)img->accel_data;
            }

            VdpVideoSurface surface = this->vdpau_accel->surface;

            if(surface == VDP_INVALID_HANDLE) {
              VdpStatus status = this->vdpau_accel->vdp_video_surface_create(this->vdpau_accel->vdp_device,
                  VDP_CHROMA_TYPE_420, this->width, this->height,
                  &surface);
              this->vdpau_accel->surface = surface;
              if(status != VDP_STATUS_OK)
                xprintf(this->xine, XINE_VERBOSITY_LOG, "vdpau_h264: Surface creation failed\n");
            }

            printf("Decode: NUM: %d, REF: %d, BYTES: %d, PTS: %lld\n", pic.frame_num, pic.is_reference, vdp_buffer.bitstream_bytes, buf->pts);
            VdpStatus status = this->vdpau_accel->vdp_decoder_render(this->decoder,
                surface, (VdpPictureInfo*)&pic, 1, &vdp_buffer);

            if(status != VDP_STATUS_OK)
              xprintf(this->xine, XINE_VERBOSITY_LOG, "vdpau_h264: Decoder failure: %d\n", status);
            else {
              printf("DECODING SUCCESS\n");

              img->duration  = this->video_step;
              img->pts       = buf->pts;
              img->bad_frame = 0;

              img->draw(img, this->stream);
              img->free(img);
              img = NULL;
            }

            //this->vdpau_accel->vdp_video_surface_destroy(surface);
          }
        }
      }
    }
  }
}

/*
 * This function is called when xine needs to flush the system.
 */
static void vdpau_h264_flush (video_decoder_t *this_gen) {
}

/*
 * This function resets the video decoder.
 */
static void vdpau_h264_reset (video_decoder_t *this_gen) {
  vdpau_h264_decoder_t *this = (vdpau_h264_decoder_t *) this_gen;

  this->size = 0;
}

/*
 * The decoder should forget any stored pts values here.
 */
static void vdpau_h264_discontinuity (video_decoder_t *this_gen) {
  vdpau_h264_decoder_t *this = (vdpau_h264_decoder_t *) this_gen;

}

/*
 * This function frees the video decoder instance allocated to the decoder.
 */
static void vdpau_h264_dispose (video_decoder_t *this_gen) {

  vdpau_h264_decoder_t *this = (vdpau_h264_decoder_t *) this_gen;

  if (this->buf) {
    free (this->buf);
    this->buf = NULL;
  }

  if (this->decoder_initialized) {
    this->decoder_initialized = 0;
    this->stream->video_out->close(this->stream->video_out, this->stream);
  }

  free (this_gen);
}

/*
 * This function allocates, initializes, and returns a private video
 * decoder structure.
 */
static video_decoder_t *open_plugin (video_decoder_class_t *class_gen, xine_stream_t *stream) {

  vdpau_h264_decoder_t  *this ;

  this = (vdpau_h264_decoder_t *) calloc(1, sizeof(vdpau_h264_decoder_t));

  /* the videoout must be vdpau-capable to support this decoder */
  /*if ( !(stream->video_driver->get_capabilities(stream->video_driver) & VO_CAP_VDPAU_H264) )
	  return NULL;*/

  this->video_decoder.decode_data         = vdpau_h264_decode_data;
  this->video_decoder.flush               = vdpau_h264_flush;
  this->video_decoder.reset               = vdpau_h264_reset;
  this->video_decoder.discontinuity       = vdpau_h264_discontinuity;
  this->video_decoder.dispose             = vdpau_h264_dispose;
  this->size                              = 0;

  this->stream                            = stream;
  this->xine                              = stream->xine;
  this->class                             = (vdpau_h264_class_t *) class_gen;

  this->decoder_started    = 0;
  this->decoder_initialized = 0;
  this->nal_parser = init_parser();
  this->buf           = NULL;

  memset(this->reference_frames, VDP_INVALID_HANDLE, sizeof(this->reference_frames));
  this->reference_frames_used = 0;

  return &this->video_decoder;
}

/*
 * This function returns a brief string that describes (usually with the
 * decoder's most basic name) the video decoder plugin.
 */
static char *get_identifier (video_decoder_class_t *this) {
  return "vdpau_h264";
}

/*
 * This function returns a slightly longer string describing the video
 * decoder plugin.
 */
static char *get_description (video_decoder_class_t *this) {
  return "vdpau_h264: h264 decoder plugin using VDPAU hardware decoding.\n"
	  "Must be used along with video_out_vdpau.";
}

/*
 * This function frees the video decoder class and any other memory that was
 * allocated.
 */
static void dispose_class (video_decoder_class_t *this) {
  free (this);
}

/*
 * This function allocates a private video decoder class and initializes
 * the class's member functions.
 */
static void *init_plugin (xine_t *xine, void *data) {

  vdpau_h264_class_t *this;

  this = (vdpau_h264_class_t *) calloc(1, sizeof(vdpau_h264_class_t));

  this->decoder_class.open_plugin     = open_plugin;
  this->decoder_class.get_identifier  = get_identifier;
  this->decoder_class.get_description = get_description;
  this->decoder_class.dispose         = dispose_class;

  return this;
}

/*
 * This is a list of all of the internal xine video buffer types that
 * this decoder is able to handle. Check src/xine-engine/buffer.h for a
 * list of valid buffer types (and add a new one if the one you need does
 * not exist). Terminate the list with a 0.
 */
static const uint32_t video_types[] = {
  /* BUF_VIDEO_FOOVIDEO, */
  BUF_VIDEO_H264,
  0
};

/*
 * This data structure combines the list of supported xine buffer types and
 * the priority that the plugin should be given with respect to other
 * plugins that handle the same buffer type. A plugin with priority (n+1)
 * will be used instead of a plugin with priority (n).
 */
static const decoder_info_t dec_info_video = {
  video_types,         /* supported types */
  7                    /* priority        */
};

/*
 * The plugin catalog entry. This is the only information that this plugin
 * will export to the public.
 */
const plugin_info_t xine_plugin_info[] EXPORTED = {
  /* { type, API, "name", version, special_info, init_function } */
  { PLUGIN_VIDEO_DECODER, 18, "vdpau_h264", XINE_VERSION_CODE, &dec_info_video, init_plugin },
  { PLUGIN_NONE, 0, "", 0, NULL, NULL }
};
