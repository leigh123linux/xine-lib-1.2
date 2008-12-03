#include <stdio.h>
#include <stdlib.h>
#include <math.h>

#include "nal_parser.h"

/* default scaling_lists according to Table 7-2 */
uint8_t default_4x4_intra[16] =
  { 6, 13, 13, 20, 20, 20, 28, 28, 28, 28, 32, 32, 32, 37, 37, 42 };

uint8_t default_4x4_inter[16] =
  { 10, 14, 14, 20, 20, 20, 24, 24, 24, 24, 27, 27, 27, 30, 30, 34};

uint8_t default_8x8_intra[64] =
  { 6, 10, 10, 13, 11, 13, 16, 16, 16, 16, 18, 18, 18, 18, 18, 32,
    23, 23, 24, 24, 24, 25, 25, 25, 25, 25, 25, 25, 27, 27, 27, 27,
    27, 27, 27, 27, 29, 29, 29, 29, 29, 29, 29, 31, 31, 31, 31, 31,
    31, 33, 33, 33, 33, 33, 36, 36, 36, 36, 38, 38, 38, 40, 40, 42 };

uint8_t default_8x8_inter[64] =
  { 9, 13, 13, 15, 13, 15, 17, 17, 17, 17, 19, 19, 19, 19, 19, 21,
    21, 21, 21, 21, 21, 22, 22, 22, 22, 22, 22, 22, 24, 24, 24, 24,
    24, 24, 24, 24, 25, 25, 25, 25, 25, 25, 25, 27, 27, 27, 27, 27,
    27, 28, 28, 28, 28, 28, 30, 30, 30, 30, 32, 32, 32, 33, 33, 35 };


struct buf_reader {
    uint8_t *buf;
    uint8_t *cur_pos;
    int len;
    int cur_offset;
};

static inline uint32_t read_bits(struct buf_reader *buf, int len);
uint32_t read_exp_golomb(struct buf_reader *buf);
int32_t read_exp_golomb_s(struct buf_reader *buf);

void calculate_pic_order(struct nal_parser *parser);
void skip_scaling_list(struct buf_reader *buf, int size);
void parse_scaling_list(struct buf_reader *buf, uint8_t *scaling_list, int length, int index);
int parse_nal_header(struct buf_reader *buf, struct nal_unit *nal);
uint8_t parse_sps(struct buf_reader *buf, struct seq_parameter_set_rbsp *sps);
void parse_vui_parameters(struct buf_reader *buf, struct seq_parameter_set_rbsp *sps);
void parse_hrd_parameters(struct buf_reader *buf, struct hrd_parameters *hrd);
uint8_t parse_pps(struct buf_reader *buf, struct pic_parameter_set_rbsp *pps);
uint8_t parse_slice_header(struct buf_reader *buf, struct nal_unit *nal);
void parse_ref_pic_list_reordering(struct buf_reader *buf, struct nal_unit *nal);
void parse_pred_weight_table(struct buf_reader *buf, struct nal_unit *nal);
void parse_dec_ref_pic_marking(struct buf_reader *buf, struct nal_unit *nal);

static void decode_nal(uint8_t **ret, int *len_ret, uint8_t *buf, int buf_len)
{
    uint8_t *end = &buf[buf_len];
    uint8_t *pos = malloc(buf_len);

    *ret = pos;
    while(buf < end) {
        if(buf < end - 3 && buf[0] == 0x00 && buf[1] == 0x00 &&
           buf[2] == 0x03) {

            *pos++ = 0x00;
            *pos++ = 0x00;

            buf += 3;
            continue;
        }
        *pos++ = *buf++;
    }

    *len_ret = pos - *ret;
}

/*uint32_t read_bits(struct buf_reader *buf, int len)
{
    uint32_t bits = 0x00;
    int i, j;
    for(i=0, j=0; i<len; i++) {
        while(buf->cur_offset >= 8) {
            buf->cur_pos++;
            buf->cur_offset -= 8;
        }
        uint8_t bit = (*buf->cur_pos >> (7 - buf->cur_offset)) & 0x01;
        bits |= ((uint32_t)bit) << i;
        buf->cur_offset++;
    }
printf("ret: 0x%08x\n", bits);
    return bits;
}*/

static inline uint32_t read_bits (struct buf_reader *buf, int len)
{
    static uint32_t i_mask[33] =
    {  0x00,
       0x01,      0x03,      0x07,      0x0f,
       0x1f,      0x3f,      0x7f,      0xff,
       0x1ff,     0x3ff,     0x7ff,     0xfff,
       0x1fff,    0x3fff,    0x7fff,    0xffff,
       0x1ffff,   0x3ffff,   0x7ffff,   0xfffff,
       0x1fffff,  0x3fffff,  0x7fffff,  0xffffff,
       0x1ffffff, 0x3ffffff, 0x7ffffff, 0xfffffff,
       0x1fffffff,0x3fffffff,0x7fffffff,0xffffffff};

    int i_shr;
    uint32_t bits = 0;

    while(len > 0 && (buf->cur_pos - buf->buf) < buf->len) {
        if((i_shr = buf->cur_offset-len) >= 0) {
            bits |= (*buf->cur_pos >> i_shr)&i_mask[len];
            buf->cur_offset -= len;
            if(buf->cur_offset == 0) {
                buf->cur_pos++;
                buf->cur_offset = 8;
            }
            return bits;
        } else {
            bits |= (*buf->cur_pos & i_mask[buf->cur_offset]) << -i_shr;
            len -= buf->cur_offset;
            buf->cur_pos++;
            buf->cur_offset = 8;
        }
    }
    return bits;
}

/* determines if following bits are rtsb_trailing_bits */
static inline uint8_t rbsp_trailing_bits(struct buf_reader *buf)
{
	// store the offset and pos in buffer
	// to revert this afterwards.
	int last_offset;
	uint8_t *last_pos;

	uint8_t rbsp_trailing_bits = 1;

	last_offset = buf->cur_offset;
	last_pos = buf->cur_pos;

	if(read_bits(buf, 1) == 1)
	{
		while(buf->cur_offset != 8)
			if(read_bits(buf, 1) == 1)
				rbsp_trailing_bits = 0;
	}

	// revert buffer
	buf->cur_offset = last_offset;
	buf->cur_pos = last_pos;

	return rbsp_trailing_bits;
}

uint32_t read_exp_golomb(struct buf_reader *buf)
{
    int leading_zero_bits = 0;

    while(read_bits(buf, 1) == 0 && leading_zero_bits < 32)
        leading_zero_bits++;

    uint32_t code = (1<<leading_zero_bits) - 1 + read_bits(buf, leading_zero_bits);
    return code;
}

int32_t read_exp_golomb_s(struct buf_reader *buf)
{
    uint32_t ue = read_exp_golomb(buf);
    int32_t code = ue&0x01 ? (ue+1)/2 : -(ue/2);
    return code;
}

int parse_nal_header(struct buf_reader *buf, struct nal_unit *nal)
{
    if(buf->len < 1)
        return -1;
    int ret = -1;

    nal->nal_ref_idc = (buf->buf[0] >> 5) & 0x03;
    nal->nal_unit_type = buf->buf[0] & 0x1f;

    buf->cur_pos = buf->buf + 1;
    //printf("NAL: %d\n", nal->nal_unit_type);

    struct buf_reader ibuf;
    ibuf.cur_offset = 8;

    switch(nal->nal_unit_type) {
        case NAL_SPS:
            decode_nal(&ibuf.buf, &ibuf.len, buf->cur_pos, buf->len-1);
            ibuf.cur_pos = ibuf.buf;
            if(!nal->sps)
                nal->sps = malloc(sizeof(struct seq_parameter_set_rbsp));
            else
                memset(nal->sps, 0x00, sizeof(struct seq_parameter_set_rbsp));

            parse_sps(&ibuf, nal->sps);
            free(ibuf.buf);
            ret = NAL_SPS;
            break;
        case NAL_PPS:
            if(!nal->pps)
                nal->pps = malloc(sizeof(struct pic_parameter_set_rbsp));
            else
                memset(nal->pps, 0x00, sizeof(struct pic_parameter_set_rbsp));

            parse_pps(buf, nal->pps);
            ret = NAL_PPS;
            break;
        case NAL_SLICE:
        case NAL_PART_A:
        case NAL_PART_B:
        case NAL_PART_C:
        case NAL_SLICE_IDR:
            if(nal->sps && nal->pps) {
                if(!nal->slc)
                    nal->slc = malloc(sizeof(struct slice_header));
                else
                    memset(nal->slc, 0x00, sizeof(struct slice_header));

                parse_slice_header(buf, nal);
                ret = nal->nal_unit_type;
            }
            break;
        default:
            ret = nal->nal_unit_type;
            break;
    }

    return ret;
}

void calculate_pic_order(struct nal_parser *parser)
{
  return;


  struct nal_unit *nal = parser->current_nal;
  struct dpb *dpb = parser->dpb;

  struct seq_parameter_set_rbsp *sps = nal->sps;
  struct pic_parameter_set_rbsp *pps = nal->pps;
  struct slice_header *slc = nal->slc;
  if(!sps || !pps)
      return;

  uint32_t max_frame_num = pow(2, sps->log2_max_frame_num_minus4+4);
  if(dpb->max_frame_num == 0)
    dpb->max_frame_num = max_frame_num;

  if(dpb->max_frame_num != max_frame_num && dpb->max_frame_num != 0)
    printf("ERROR, FIXME, max_frame_num changed");

  /* calculate frame_num based stuff */
  if(nal->nal_unit_type == NAL_SLICE_IDR) {
    dpb->prev_ref_frame_number = 0;
  } else {
    // FIXME: understand p92 in h264 spec
  }

  if(slc->frame_num != dpb->prev_ref_frame_number) {
    memset(dpb->non_existing_pictures, 0, 32);
    int i = 0;
    dpb->unused_short_term_frame_num = (dpb->prev_ref_frame_number + 1) % dpb->max_frame_num;
    dpb->non_existing_pictures[i] = dpb->unused_short_term_frame_num;
    i++;

    while(dpb->unused_short_term_frame_num != slc->frame_num) {
      dpb->unused_short_term_frame_num = (dpb->unused_short_term_frame_num + 1) % dpb->max_frame_num;
      dpb->non_existing_pictures[i] = dpb->unused_short_term_frame_num;
      i++;
    }
  }

  if(sps->pic_order_cnt_type == 0) {
    if(nal->nal_unit_type == NAL_SLICE_IDR) {
      parser->prev_pic_order_cnt_lsb = 0;
      parser->prev_pic_order_cnt_msb = 0;
    } else {

    }
  }

}

void skip_scaling_list(struct buf_reader *buf, int size)
{
    int i;
    for(i = 0; i < size; i++) {
        read_exp_golomb_s(buf);
    }
}

void parse_scaling_list(struct buf_reader *buf, uint8_t *scaling_list, int length, int index)
{
	int last_scale = 8;
	int next_scale = 8;
	int32_t delta_scale;
	uint8_t use_default_scaling_matrix_flag = 0;
	int i;

	for(i = 0; i < length; i++) {
		if(next_scale != 0) {
			delta_scale = read_exp_golomb_s(buf);
			next_scale = (last_scale + delta_scale + 256) % 256;
			if(i == 0 && next_scale == 0) {
				use_default_scaling_matrix_flag = 1;
				break;
			}
		}
		scaling_list[i] = (next_scale == 0) ? last_scale : next_scale;
		last_scale = scaling_list[i];
	}

	if(use_default_scaling_matrix_flag) {
		switch(index) {
			case 0:
			case 1:
			case 2:
				memcpy(scaling_list, default_4x4_intra, length);
				break;
			case 3:
			case 4:
			case 5:
				memcpy(scaling_list, default_4x4_inter, length);
				break;
			case 6:
				memcpy(scaling_list, default_8x8_intra, length);
				break;
			case 7:
				memcpy(scaling_list, default_8x8_inter, length);
				break;
		}
	}
}

uint8_t parse_sps(struct buf_reader *buf, struct seq_parameter_set_rbsp *sps)
{
    sps->profile_idc = buf->buf[0];
    sps->constraint_setN_flag = (buf->buf[1] >> 4) & 0x0f;
    sps->level_idc = buf->buf[2];

    buf->cur_pos = buf->buf+3;
    sps->seq_parameter_set_id = read_exp_golomb(buf);
    if(sps->profile_idc == 100 || sps->profile_idc == 110 ||
       sps->profile_idc == 122 || sps->profile_idc == 144) {
        sps->chroma_format_idc = read_exp_golomb(buf);
        if(sps->chroma_format_idc == 3) {
            sps->residual_colour_transform_flag = read_bits(buf, 1);
        }

        sps->bit_depth_luma_minus8 = read_exp_golomb(buf);
        sps->bit_depth_chroma_minus8 = read_exp_golomb(buf);
        sps->qpprime_y_zero_transform_bypass_flag = read_bits(buf, 1);
        sps->seq_scaling_matrix_present_flag = read_bits(buf, 1);
        if(sps->seq_scaling_matrix_present_flag) {
        	int i;
			for(i = 0; i < 8; i++) {
				sps->seq_scaling_list_present_flag[i] = read_bits(buf, 1);

				if(sps->seq_scaling_list_present_flag[i]) {
					if(i < 6)
						parse_scaling_list(buf, sps->scaling_lists_4x4[i], 16, i);
					else
						parse_scaling_list(buf, sps->scaling_lists_8x8[i-6], 64, i);
				}
			}
        }
    }

    sps->log2_max_frame_num_minus4 = read_exp_golomb(buf);

    sps->pic_order_cnt_type = read_exp_golomb(buf);
    if(!sps->pic_order_cnt_type)
        sps->log2_max_pic_order_cnt_lsb_minus4 = read_exp_golomb(buf);
    else {
        sps->delta_pic_order_always_zero_flag = read_bits(buf, 1);
        sps->offset_for_non_ref_pic = read_exp_golomb_s(buf);
        sps->offset_for_top_to_bottom_field = read_exp_golomb_s(buf);
        sps->num_ref_frames_in_pic_order_cnt_cycle = read_exp_golomb(buf);
        int i;
        for(i=0; i<sps->num_ref_frames_in_pic_order_cnt_cycle; i++) {
            sps->offset_for_ref_frame[i] = read_exp_golomb_s(buf);
        }
    }
    sps->num_ref_frames = read_exp_golomb(buf);
    sps->gaps_in_frame_num_value_allowed_flag = read_bits(buf, 1);

    /*sps->pic_width_in_mbs_minus1 = read_exp_golomb(buf);
    sps->pic_height_in_map_units_minus1 = read_exp_golomb(buf);*/
    sps->pic_width = 16 * (read_exp_golomb(buf) + 1);
    sps->pic_height = 16 * (read_exp_golomb(buf) + 1);

    sps->frame_mbs_only_flag = read_bits(buf, 1);

    /* compute the height correctly even for interlaced material */
    sps->pic_height = (2-sps->frame_mbs_only_flag) * sps->pic_height;
    if(sps->pic_height == 1088)
      sps->pic_height = 1080;

    if(!sps->frame_mbs_only_flag)
        sps->mb_adaptive_frame_field_flag = read_bits(buf, 1);

    sps->direct_8x8_inference_flag = read_bits(buf, 1);
    sps->frame_cropping_flag = read_bits(buf, 1);
    if(sps->frame_cropping_flag) {
        sps->frame_crop_left_offset = read_exp_golomb(buf);
        sps->frame_crop_right_offset = read_exp_golomb(buf);
        sps->frame_crop_top_offset = read_exp_golomb(buf);
        sps->frame_crop_bottom_offset = read_exp_golomb(buf);
    }
    sps->vui_parameters_present_flag = read_bits(buf, 1);
    if(sps->vui_parameters_present_flag) {
        parse_vui_parameters(buf, sps);
    }

    return 0;
}

void parse_vui_parameters(struct buf_reader *buf, struct seq_parameter_set_rbsp *sps)
{
  sps->vui_parameters.aspect_ration_info_present_flag = read_bits(buf, 1);
  if(sps->vui_parameters.aspect_ration_info_present_flag == 1) {
    sps->vui_parameters.aspect_ratio_idc = read_bits(buf, 8);
    if(sps->vui_parameters.aspect_ratio_idc == ASPECT_RESERVED) {
      sps->vui_parameters.sar_width = read_bits(buf, 16);
      sps->vui_parameters.sar_height = read_bits(buf, 16);
    }
  }

  sps->vui_parameters.overscan_info_present_flag = read_bits(buf, 1);
  if(sps->vui_parameters.overscan_info_present_flag) {
    sps->vui_parameters.overscan_appropriate_flag = read_bits(buf, 1);
  }

  sps->vui_parameters.video_signal_type_present_flag = read_bits(buf, 1);
  if(sps->vui_parameters.video_signal_type_present_flag) {
    sps->vui_parameters.video_format = read_bits(buf, 3);
    sps->vui_parameters.video_full_range_flag = read_bits(buf, 1);
    sps->vui_parameters.colour_description_present = read_bits(buf, 1);
    if(sps->vui_parameters.colour_description_present) {
      sps->vui_parameters.colour_primaries = read_bits(buf, 8);
      sps->vui_parameters.transfer_characteristics = read_bits(buf, 8);
      sps->vui_parameters.matrix_coefficients = read_bits(buf, 8);
    }
  }

  sps->vui_parameters.chroma_loc_info_present_flag = read_bits(buf, 1);
  if(sps->vui_parameters.chroma_loc_info_present_flag) {
    sps->vui_parameters.chroma_sample_loc_type_top_field = read_exp_golomb(buf);
    sps->vui_parameters.chroma_sample_loc_type_bottom_field = read_exp_golomb(buf);
  }

  sps->vui_parameters.timing_info_present_flag = read_bits(buf, 1);
  if(sps->vui_parameters.timing_info_present_flag) {
    sps->vui_parameters.num_units_in_tick = read_bits(buf, 32);
    sps->vui_parameters.time_scale = read_bits(buf, 32);
    sps->vui_parameters.fixed_frame_rate_flag = read_bits(buf, 1);
  }

  sps->vui_parameters.nal_hrd_parameters_present_flag = read_bits(buf, 1);
  if(sps->vui_parameters.nal_hrd_parameters_present_flag)
    parse_hrd_parameters(buf, &sps->vui_parameters.nal_hrd_parameters);

  sps->vui_parameters.vc1_hrd_parameters_present_flag = read_bits(buf, 1);
  if(sps->vui_parameters.vc1_hrd_parameters_present_flag)
    parse_hrd_parameters(buf, &sps->vui_parameters.vc1_hrd_parameters);

  if(sps->vui_parameters.nal_hrd_parameters_present_flag ||
      sps->vui_parameters.vc1_hrd_parameters_present_flag)
    sps->vui_parameters.low_delay_hrd_flag = read_bits(buf, 1);

  sps->vui_parameters.pic_struct_present_flag = read_bits(buf, 1);
  sps->vui_parameters.bitstream_restriction_flag = read_bits(buf, 1);

  if(sps->vui_parameters.bitstream_restriction_flag) {
    sps->vui_parameters.motion_vectors_over_pic_boundaries = read_bits(buf, 1);
    sps->vui_parameters.max_bytes_per_pic_denom = read_exp_golomb(buf);
    sps->vui_parameters.max_bits_per_mb_denom = read_exp_golomb(buf);
    sps->vui_parameters.log2_max_mv_length_horizontal = read_exp_golomb(buf);
    sps->vui_parameters.log2_max_mv_length_vertical = read_exp_golomb(buf);
    sps->vui_parameters.num_reorder_frames = read_exp_golomb(buf);
    sps->vui_parameters.max_dec_frame_buffering = read_exp_golomb(buf);
  }
}

void parse_hrd_parameters(struct buf_reader *buf, struct hrd_parameters *hrd)
{
  hrd->cpb_cnt_minus1 = read_exp_golomb(buf);
  hrd->bit_rate_scale = read_bits(buf, 4);
  hrd->cpb_size_scale = read_bits(buf, 4);

  int i;
  for(i = 0; i <= hrd->cpb_cnt_minus1; i++) {
    hrd->bit_rate_value_minus1[i] = read_exp_golomb(buf);
    hrd->cpb_size_value_minus1[i] = read_exp_golomb(buf);
    hrd->cbr_flag[i] = read_bits(buf, 1);
  }

  hrd->initial_cpb_removal_delay_length_minus1 = read_bits(buf, 5);
  hrd->cpb_removal_delay_length_minus1 = read_bits(buf, 5);
  hrd->dpb_output_delay_length_minus1 = read_bits(buf, 5);
  hrd->time_offset_length = read_bits(buf, 5);
}

uint8_t parse_pps(struct buf_reader *buf, struct pic_parameter_set_rbsp *pps)
{
    pps->pic_parameter_set_id = read_exp_golomb(buf);
    pps->seq_parameter_set_id = read_exp_golomb(buf);
    pps->entropy_coding_mode_flag = read_bits(buf, 1);
    pps->pic_order_present_flag = read_bits(buf, 1);

    pps->num_slice_groups_minus1 = read_exp_golomb(buf);
    if(pps->num_slice_groups_minus1 > 0) {
    	pps->slice_group_map_type = read_exp_golomb(buf);
    	if(pps->slice_group_map_type == 0) {
    		int i_group;
    		for(i_group = 0; i_group <= pps->num_slice_groups_minus1; i_group++) {
    			if(i_group < 64)
    				pps->run_length_minus1[i_group] = read_exp_golomb(buf);
    			else { // FIXME: skips if more than 64 groups exist
    				fprintf(stderr, "Error: Only 64 slice_groups are supported\n");
    				read_exp_golomb(buf);
    			}
    		}
    	} else if(pps->slice_group_map_type == 3 ||
    			pps->slice_group_map_type == 4 ||
    			pps->slice_group_map_type == 5) {
    		pps->slice_group_change_direction_flag = read_bits(buf, 1);
    		pps->slice_group_change_rate_minus1 = read_exp_golomb(buf);
    	} else if(pps->slice_group_map_type == 6) {
    		pps->pic_size_in_map_units_minus1 = read_exp_golomb(buf);
    		int i_group;
    		for(i_group = 0; i_group <= pps->num_slice_groups_minus1; i_group++) {
    			pps->slice_group_id[i_group] =
    				read_bits(buf, ceil(log(pps->num_slice_groups_minus1 + 1)));
    		}
    	}
    }

    pps->num_ref_idx_l0_active_minus1 = read_exp_golomb(buf);
    pps->num_ref_idx_l1_active_minus1 = read_exp_golomb(buf);
    pps->weighted_pred_flag = read_bits(buf, 1);
    pps->weighted_bipred_idc = read_bits(buf, 2);
    pps->pic_init_qp_minus26 = read_exp_golomb_s(buf);
    pps->pic_init_qs_minus26 = read_exp_golomb_s(buf);
    pps->chroma_qp_index_offset = read_exp_golomb_s(buf);
    pps->deblocking_filter_control_present_flag = read_bits(buf, 1);
    pps->constrained_intra_pred_flag = read_bits(buf, 1);
    pps->redundant_pic_cnt_present_flag = read_bits(buf, 1);

    if(!rbsp_trailing_bits(buf)) {
		pps->transform_8x8_mode_flag = read_bits(buf, 1);
		pps->pic_scaling_matrix_present_flag = read_bits(buf, 1);
		if(pps->pic_scaling_matrix_present_flag) {
			int i;
			for(i = 0; i < 6 + 2 * pps->transform_8x8_mode_flag; i++) {
				pps->pic_scaling_list_present_flag[i] = read_bits(buf, 1);

				if(pps->pic_scaling_list_present_flag[i]) {
					if(i < 6)
						parse_scaling_list(buf, pps->scaling_lists_4x4[i], 16, i);
					else
						parse_scaling_list(buf, pps->scaling_lists_8x8[i-6], 64, i);
				}
			}
		}

		pps->second_chroma_qp_index_offset = read_exp_golomb_s(buf);
    }

    return 0;
}

uint8_t parse_slice_header(struct buf_reader *buf, struct nal_unit *nal)
{
    struct seq_parameter_set_rbsp *sps = nal->sps;
    struct pic_parameter_set_rbsp *pps = nal->pps;
    struct slice_header *slc = nal->slc;
    if(!sps || !pps)
        return -1;

    slc->first_mb_in_slice = read_exp_golomb(buf);
    /* we do some parsing on the slice type, because the list is doubled */
    slc->slice_type = slice_type(read_exp_golomb(buf));
    slc->pic_parameter_set_id = read_exp_golomb(buf);
    slc->frame_num = read_bits(buf, sps->log2_max_frame_num_minus4 + 4);
    if(!sps->frame_mbs_only_flag) {
        slc->field_pic_flag = read_bits(buf, 1);
        if(slc->field_pic_flag)
            slc->bottom_field_flag = read_bits(buf, 1);
        else
            slc->bottom_field_flag = -1;
    } else {
        slc->field_pic_flag = 0;
        slc->bottom_field_flag = -1;
    }

    if(nal->nal_unit_type == NAL_SLICE_IDR)
        slc->idr_pic_id = read_exp_golomb(buf);

    if(!sps->pic_order_cnt_type) {
        slc->pic_order_cnt_lsb = read_bits(buf, sps->log2_max_pic_order_cnt_lsb_minus4 + 4);
        if(pps->pic_order_present_flag && !slc->field_pic_flag)
            slc->delta_pic_order_cnt_bottom = read_exp_golomb_s(buf);
    } else if (sps->pic_order_cnt_type == 1) {
        slc->delta_pic_order_cnt[0] = read_exp_golomb_s(buf);
        if(pps->pic_order_present_flag && !slc->field_pic_flag)
            slc->delta_pic_order_cnt[1] = read_exp_golomb_s(buf);
    }

    if(pps->redundant_pic_cnt_present_flag == 1) {
      slc->redundant_pic_cnt = read_exp_golomb(buf);
    }

    if(slc->slice_type == SLICE_B)
      slc->direct_spatial_mv_pred_flag = read_bits(buf, 1);

    if(slc->slice_type == SLICE_P ||
        slc->slice_type == SLICE_SP ||
        slc->slice_type == SLICE_B) {
      slc->num_ref_idx_active_override_flag = read_bits(buf, 1);

      if(slc->num_ref_idx_active_override_flag == 1) {
        slc->num_ref_idx_l0_active_minus1 = read_exp_golomb(buf);

        if(slc->slice_type == SLICE_B) {
          slc->num_ref_idx_l1_active_minus1 = read_exp_golomb(buf);
        }
      }
    }

    /* --- ref_pic_list_reordering --- */
    parse_ref_pic_list_reordering(buf, nal);

    /* --- pred_weight_table --- */
    if((pps->weighted_pred_flag &&
        (slc->slice_type == SLICE_P || slc->slice_type == SLICE_SP)) ||
        (pps->weighted_bipred_idc == 1 && slc->slice_type == SLICE_B)) {
      parse_pred_weight_table(buf, nal);
    }

    /* --- dec_ref_pic_marking --- */
    if(nal->nal_ref_idc != 0)
      parse_dec_ref_pic_marking(buf, nal);

    return 0;
}

void parse_ref_pic_list_reordering(struct buf_reader *buf, struct nal_unit *nal)
{
  struct seq_parameter_set_rbsp *sps = nal->sps;
  struct pic_parameter_set_rbsp *pps = nal->pps;
  struct slice_header *slc = nal->slc;
  if(!sps || !pps)
      return;

  if(slc->slice_type != SLICE_I && slc->slice_type != SLICE_SI) {
    slc->ref_pic_list_reordering.ref_pic_list_reordering_flag_l0 = read_bits(buf, 1);

    if(slc->ref_pic_list_reordering.ref_pic_list_reordering_flag_l0 == 1) {
      do {
        slc->ref_pic_list_reordering.reordering_of_pic_nums_idc = read_exp_golomb(buf);

        if(slc->ref_pic_list_reordering.reordering_of_pic_nums_idc == 0 ||
            slc->ref_pic_list_reordering.reordering_of_pic_nums_idc == 1) {
          slc->ref_pic_list_reordering.abs_diff_pic_num_minus1 = read_exp_golomb(buf);
        } else if (slc->ref_pic_list_reordering.reordering_of_pic_nums_idc == 2) {
          slc->ref_pic_list_reordering.long_term_pic_num = read_exp_golomb(buf);
        }
      } while (slc->ref_pic_list_reordering.reordering_of_pic_nums_idc != 3);
    }
  }

  if(slc->slice_type == SLICE_B) {
    slc->ref_pic_list_reordering.ref_pic_list_reordering_flag_l1 = read_bits(buf, 1);

    if(slc->ref_pic_list_reordering.ref_pic_list_reordering_flag_l1 == 1) {
      do {
        slc->ref_pic_list_reordering.reordering_of_pic_nums_idc = read_exp_golomb(buf);

        if(slc->ref_pic_list_reordering.reordering_of_pic_nums_idc == 0 ||
            slc->ref_pic_list_reordering.reordering_of_pic_nums_idc == 1) {
          slc->ref_pic_list_reordering.abs_diff_pic_num_minus1 = read_exp_golomb(buf);
        } else if (slc->ref_pic_list_reordering.reordering_of_pic_nums_idc == 2) {
          slc->ref_pic_list_reordering.long_term_pic_num = read_exp_golomb(buf);
        }
      } while (slc->ref_pic_list_reordering.reordering_of_pic_nums_idc != 3);
    }
  }
}

void parse_pred_weight_table(struct buf_reader *buf, struct nal_unit *nal)
{
  struct seq_parameter_set_rbsp *sps = nal->sps;
  struct pic_parameter_set_rbsp *pps = nal->pps;
  struct slice_header *slc = nal->slc;
  if(!sps || !pps)
      return;

  nal->slc->pred_weight_table.luma_log2_weight_denom = read_exp_golomb(buf);

  if(sps->chroma_format_idc != 0)
    nal->slc->pred_weight_table.chroma_log2_weight_denom = read_exp_golomb(buf);

  int i;
  for(i = 0; i <= pps->num_ref_idx_l0_active_minus1; i++) {
    uint8_t luma_weight_l0_flag = read_bits(buf, 1);

    if(luma_weight_l0_flag == 1 ) {
      nal->slc->pred_weight_table.luma_weight_l0[i] = read_exp_golomb_s(buf);
      nal->slc->pred_weight_table.luma_offset_l0[i] = read_exp_golomb_s(buf);
    }

    if(sps->chroma_format_idc != 0) {
      uint8_t chroma_weight_l0_flag = read_bits(buf, 1);

      if(chroma_weight_l0_flag == 1 ) {
        int j;
        for(j = 0; j < 2 ; j++) {
          nal->slc->pred_weight_table.chroma_weight_l0[i][j] = read_exp_golomb_s(buf);
          nal->slc->pred_weight_table.chroma_offset_l0[i][j] = read_exp_golomb_s(buf);
        }
      }
    }
  }

  if(slc->slice_type == SLICE_B) {
    for(i = 0; i <= pps->num_ref_idx_l1_active_minus1; i++) {
      uint8_t luma_weight_l1_flag = read_bits(buf, 1);

      if(luma_weight_l1_flag == 1 ) {
        nal->slc->pred_weight_table.luma_weight_l1[i] = read_exp_golomb_s(buf);
        nal->slc->pred_weight_table.luma_offset_l1[i] = read_exp_golomb_s(buf);
      }

      if(sps->chroma_format_idc != 0) {
        uint8_t chroma_weight_l1_flag = read_bits(buf, 1);

        if(chroma_weight_l1_flag == 1 ) {
          int j;
          for(j = 0; j < 2 ; j++) {
            nal->slc->pred_weight_table.chroma_weight_l1[i][j] = read_exp_golomb_s(buf);
            nal->slc->pred_weight_table.chroma_offset_l1[i][j] = read_exp_golomb_s(buf);
          }
        }
      }
    }
  }
}

void parse_dec_ref_pic_marking(struct buf_reader *buf, struct nal_unit *nal)
{
  struct seq_parameter_set_rbsp *sps = nal->sps;
  struct pic_parameter_set_rbsp *pps = nal->pps;
  struct slice_header *slc = nal->slc;
  if(!sps || !pps)
      return;

  if(nal->nal_unit_type == NAL_SLICE_IDR) {
    slc->dec_ref_pic_marking.no_output_of_prior_pics_flag = read_bits(buf, 1);
    slc->dec_ref_pic_marking.long_term_reference_flag = read_bits(buf, 1);
  } else {
    slc->dec_ref_pic_marking.adaptive_ref_pic_marking_mode_flag = read_bits(buf, 1);

    if(slc->dec_ref_pic_marking.adaptive_ref_pic_marking_mode_flag) {
      do {
        slc->dec_ref_pic_marking.memory_management_control_operation = read_exp_golomb(buf);

        if(slc->dec_ref_pic_marking.memory_management_control_operation == 1 ||
            slc->dec_ref_pic_marking.memory_management_control_operation == 3)
          slc->dec_ref_pic_marking.difference_of_pic_nums_minus1 = read_exp_golomb(buf);

        if(slc->dec_ref_pic_marking.memory_management_control_operation == 2)
          slc->dec_ref_pic_marking.long_term_pic_num = read_exp_golomb(buf);

        if(slc->dec_ref_pic_marking.memory_management_control_operation == 3 ||
            slc->dec_ref_pic_marking.memory_management_control_operation == 6)
          slc->dec_ref_pic_marking.long_term_frame_idx = read_exp_golomb(buf);

        if(slc->dec_ref_pic_marking.memory_management_control_operation == 4)
          slc->dec_ref_pic_marking.max_long_term_frame_idx_plus1 = read_exp_golomb(buf);
      } while(slc->dec_ref_pic_marking.memory_management_control_operation != 0);
    }
  }
}


/* ----------------- NAL parser ----------------- */

struct nal_parser* init_parser()
{
    struct nal_parser *parser = malloc(sizeof(struct nal_parser));
    memset(parser->buf, 0x00, MAX_FRAME_SIZE);
    parser->buf_len = 0;
    parser->found_sps = 0;
    parser->found_pps = 0;
    parser->nal0 = malloc(sizeof(struct nal_unit));
    memset(parser->nal0, 0x00, sizeof(struct nal_unit));
    parser->nal1 = malloc(sizeof(struct nal_unit));
    memset(parser->nal1, 0x00, sizeof(struct nal_unit));
    parser->current_nal = parser->nal0;
    parser->last_nal = parser->nal1;

    parser->last_nal_res = 0;
    parser->is_idr = 0;
    parser->slice = 0;
    parser->slice_cnt = 0;
    parser->field = -1;
    parser->have_top = 0;

    return parser;
}

void free_parser(struct nal_parser *parser)
{
    free(parser->nal0);
    free(parser->nal1);
    free(parser);
}

int parse_frame(struct nal_parser *parser, uint8_t *inbuf, int inbuf_len,
                uint8_t **ret_buf, uint32_t *ret_len, uint32_t *ret_slice_cnt)
{
    int next_nal;
    int parsed_len = 0;
    int search_offset = 0;

    while((next_nal = seek_for_nal(inbuf+search_offset, inbuf_len-parsed_len)) >= 0) {
        // save buffer up to the nal-start
        if(parser->buf_len + next_nal + search_offset > MAX_FRAME_SIZE) {
            printf("buf underrun!!\n");
            *ret_len = 0;
            *ret_buf = NULL;
            return parsed_len;
        }
        //if(parser->last_nal_res != 1) {
            xine_fast_memcpy(&parser->buf[parser->buf_len], inbuf, next_nal+search_offset);
            parser->buf_len += next_nal+search_offset;
        //}
        inbuf += next_nal+search_offset;
        parsed_len += next_nal+search_offset;

        if((parser->last_nal_res = parse_nal(inbuf+4, inbuf_len-parsed_len, parser)) == 1
            && parser->buf_len>0) {
            // parse_nal returned 1 --> detected a frame_boundary
            // do the extended parsing stuff...
            calculate_pic_order(parser);

            *ret_buf = malloc(parser->buf_len);
            xine_fast_memcpy(*ret_buf, parser->buf, parser->buf_len);
            *ret_len = parser->buf_len;
            *ret_slice_cnt = parser->slice_cnt;

            //memset(parser->buf, 0x00, parser->buf_len);
            parser->buf_len = 0;
            parser->last_nal_res = 0;
            parser->slice_cnt = 0;
            return parsed_len;
        }

        search_offset = 4;
    }

    // no further NAL found, copy the rest of the stream
    // into the buffer
//    if(parser->last_nal_res != 1) {
        xine_fast_memcpy(&parser->buf[parser->buf_len], inbuf, inbuf_len-parsed_len);
        parser->buf_len += inbuf_len-parsed_len;
//    }

    parsed_len += (inbuf_len-parsed_len);
    *ret_len = 0;
    *ret_buf = NULL;

    return parsed_len;
}

int parse_nal(uint8_t *buf, int buf_len, struct nal_parser *parser)
{
    struct buf_reader bufr;

    bufr.buf = buf;
    bufr.cur_pos = buf;
    bufr.cur_offset = 8;
    bufr.len = buf_len;

    struct nal_unit *nal = parser->current_nal;
    struct nal_unit *last_nal = parser->last_nal;

    int res = parse_nal_header(&bufr, nal);
    printf("type: %d\n", res);
    if(res == NAL_SLICE_IDR)
      parser->is_idr = 1;

    if(res >= NAL_SLICE && res <= NAL_SLICE_IDR) {
        // now detect if it's a new frame!
        int ret = 0;
        if(nal->slc->field_pic_flag == 1)
            parser->field = nal->slc->bottom_field_flag;
        else {
            parser->have_top = 1;
            parser->field = -1;
        }

        if(nal->slc->field_pic_flag == 1 && nal->slc->bottom_field_flag == 0)
            parser->have_top = 1;

        parser->slice = 1;

        if(nal->slc == NULL || last_nal->slc == NULL) {
          printf("A\n");
            ret = 1;
        }
        if(nal->slc && last_nal->slc &&
           (nal->slc->frame_num != last_nal->slc->frame_num)) {
          printf("B\n");
            ret = 1;
        }
        if(nal->slc && last_nal->slc &&
           (nal->slc->pic_parameter_set_id != last_nal->slc->pic_parameter_set_id)) {
          printf("C\n");
            ret = 1;
        }
        if(nal->slc && last_nal->slc &&
           (nal->slc->field_pic_flag != last_nal->slc->field_pic_flag)) {
          printf("D\n");
            ret = 1;
        }
        if(nal->slc && last_nal->slc &&
           (nal->slc->bottom_field_flag != -1 &&
                last_nal->slc->bottom_field_flag != -1 &&
                nal->slc->bottom_field_flag != last_nal->slc->bottom_field_flag)) {
          printf("E\n");
            ret = 1;
        }
        if(nal->nal_ref_idc != last_nal->nal_ref_idc &&
                (nal->nal_ref_idc == 0 || last_nal->nal_ref_idc == 0)) {
          printf("F\n");
            ret = 1;
        }
        if(nal->sps && nal->slc && last_nal->slc &&
           (nal->sps->pic_order_cnt_type == 0 && last_nal->sps->pic_order_cnt_type == 0 &&
                (nal->slc->pic_order_cnt_lsb != last_nal->slc->pic_order_cnt_lsb ||
                 nal->slc->delta_pic_order_cnt_bottom != last_nal->slc->delta_pic_order_cnt_bottom))) {
          printf("G\n");
            ret = 1;
        }
        if(nal->slc && last_nal->slc &&
           (nal->sps->pic_order_cnt_type == 1 && last_nal->sps->pic_order_cnt_type == 1 &&
                (nal->slc->delta_pic_order_cnt[0] != last_nal->slc->delta_pic_order_cnt[0] ||
                nal->slc->delta_pic_order_cnt[1] != last_nal->slc->delta_pic_order_cnt[1]))) {
          printf("H\n");
            ret = 1;
        }
        if(nal->nal_unit_type != last_nal->nal_unit_type &&
                (nal->nal_unit_type == 5 || last_nal->nal_unit_type == 5)) {
          printf("I\n");
            ret = 1;
        }
        if(nal->slc && last_nal->slc &&
           (nal->nal_unit_type == 5 && last_nal->nal_unit_type == 5 &&
                nal->slc->idr_pic_id != last_nal->slc->idr_pic_id)) {
          printf("J\n");
            ret = 1;
        }

        if(parser->current_nal == parser->nal0) {
            parser->current_nal = parser->nal1;
            parser->last_nal = parser->nal0;
        }
        else {
            parser->current_nal = parser->nal0;
            parser->last_nal = parser->nal1;
        }

        if(parser->current_nal->sps == NULL)
            parser->current_nal->sps = parser->last_nal->sps;
        if(parser->current_nal->pps == NULL)
            parser->current_nal->pps = parser->last_nal->pps;

        /* increase the slice_cnt until a new frame is detected */
        if(!ret)
          parser->slice_cnt++;

        return ret;
    } else if(res == NAL_PPS || res == NAL_SPS) {
        return 0;
    } else if (res == NAL_AU_DELIMITER || res == NAL_SEI ||
               (res >= 13 && res <= 18)) {
        //printf("New Frame\n");
        return 0;
    }

    return 1;
}

int seek_for_nal(uint8_t *buf, int buf_len)
{
    int i;
    for(i=0; i<buf_len-3; i++) {
        if(buf[i] == 0x00 && buf[i+1] == 0x00 &&
           buf[i+2] == 0x00 && buf[i+3] == 0x01) {
            //printf("found nal at: %d\n", i);
            return i;
           }
    }

    return -1;
}
