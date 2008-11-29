#ifndef NAL_PARSER_H_
#define NAL_PARSER_H_

#include <stdlib.h>

#include "xine_internal.h"

enum nal_unit_types {
    NAL_UNSPECIFIED = 0,
    NAL_SLICE,
    NAL_PART_A,
    NAL_PART_B,
    NAL_PART_C,
    NAL_SLICE_IDR,
    NAL_SEI,
    NAL_SPS,
    NAL_PPS,
    NAL_AU_DELIMITER,
    NAL_END_OF_SEQUENCE,
    NAL_END_OF_STREAM,
    NAL_FILLER_DATA,
    NAL_SPS_EXT
};

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


struct nal_unit {
    uint8_t     nal_ref_idc;    // 0x03
    uint8_t     nal_unit_type;  // 0x1f

    struct seq_parameter_set_rbsp   *sps;
    struct pic_parameter_set_rbsp   *pps;
    struct slice_header             *slc;
};

struct seq_parameter_set_rbsp {
    uint8_t     profile_idc;            // 0xff
    uint8_t     constraint_setN_flag;   // 0x0f
    uint8_t     level_idc;              // 0xff
    uint32_t    seq_parameter_set_id;
    uint32_t    chroma_format_idc;
    uint8_t     residual_colour_transform_flag;  // 0x01
    uint32_t    bit_depth_luma_minus8;
    uint32_t    bit_depth_chroma_minus8;
    uint8_t     qpprime_y_zero_transform_bypass_flag;
    uint8_t     seq_scaling_matrix_present_flag;

    /* if(seq_scaling_matrix_present_flag) */
	{
		uint8_t 	seq_scaling_list_present_flag[8];

		uint8_t		scaling_lists_4x4[6][16];
		uint8_t		scaling_lists_8x8[2][64];
	}

    uint32_t    log2_max_frame_num_minus4;
    uint32_t    pic_order_cnt_type;
    // if pic_order_cnt_type==0
    uint32_t    log2_max_pic_order_cnt_lsb_minus4;
    // else
    uint8_t     delta_pic_order_always_zero_flag;
    int32_t     offset_for_non_ref_pic;
    int32_t     offset_for_top_to_bottom_field;
    uint8_t     num_ref_frames_in_pic_order_cnt_cycle;
    int32_t     offset_for_ref_frame[256];
    // TODO: some more ignored here
    uint32_t    num_ref_frames;
    uint8_t     gaps_in_frame_num_value_allowed_flag;
    /*uint32_t    pic_width_in_mbs_minus1;
    uint32_t    pic_height_in_map_units_minus1;*/
    uint32_t    pic_width;
    uint32_t    pic_height;
    uint8_t     frame_mbs_only_flag;
    uint8_t     mb_adaptive_frame_field_flag;
    uint8_t     direct_8x8_inference_flag;
    uint8_t     frame_cropping_flag;
    uint32_t    frame_crop_left_offset;
    uint32_t    frame_crop_right_offset;
    uint32_t    frame_crop_top_offset;
    uint32_t    frame_crop_bottom_offset;
    uint8_t     vui_parameters_present_flag;
    // TODO: add vui_parameters, rtbsp_trailing_bits

};

struct pic_parameter_set_rbsp {
    uint32_t    pic_parameter_set_id;
    uint32_t    seq_parameter_set_id;
    uint8_t     entropy_coding_mode_flag;
    uint8_t     pic_order_present_flag;

    uint32_t	num_slice_groups_minus1;

    /* num_slice_groups_minus1 > 0 */
    {
        uint32_t	slice_group_map_type;

		/* slice_group_map_type == 1 */
		{
			uint32_t	run_length_minus1[64];
		}

		/* slice_group_map_type == 2 */
		{
			uint32_t	top_left[64];
			uint32_t	bottom_right[64];
		}

		/* slice_group_map_type == 3,4,5 */
		{
			uint8_t		slice_group_change_direction_flag;
			uint32_t	slice_group_change_rate_minus1;
		}

		/* slice_group_map_type == 6 */
		{
			uint32_t	pic_size_in_map_units_minus1;
			uint8_t		slice_group_id[64];
		}
    }

    uint32_t	num_ref_idx_l0_active_minus1;
    uint32_t	num_ref_idx_l1_active_minus1;
    uint8_t		weighted_pred_flag;
    uint8_t		weighted_bipred_idc;
    int32_t		pic_init_qp_minus26;
    int32_t		pic_init_qs_minus26;
    int32_t		chroma_qp_index_offset;
    uint8_t		deblocking_filter_control_present_flag;
    uint8_t		constrained_intra_pred_flag;
    uint8_t		redundant_pic_cnt_present_flag;

    /* if(more_rbsp_data) */
    uint8_t		transform_8x8_mode_flag;
    uint8_t		pic_scaling_matrix_present_flag;

    /* if(pic_scaling_matrix_present_flag) */
    {
    	uint8_t 	pic_scaling_list_present_flag[8];

    	uint8_t		scaling_lists_4x4[6][16];
    	uint8_t		scaling_lists_8x8[2][64];

    	int32_t		second_chroma_qp_index_offset;
    }
};

struct slice_header {
    uint32_t    first_mb_in_slice;
    uint32_t    slice_type;
    uint32_t    pic_parameter_set_id;
    uint32_t    frame_num;
    int8_t      field_pic_flag;
    int8_t      bottom_field_flag;
    uint32_t    idr_pic_id;

    /* sps->pic_order_cnt_type == 0 */
    uint32_t    pic_order_cnt_lsb;
    int32_t     delta_pic_order_cnt_bottom;
    /* sps->pic_order_cnt_type == 1 && !sps->delta_pic_order_always_zero_flag */
    int32_t     delta_pic_order_cnt[2];

    /* not needed for packetizing */
    /*int32_t     redundant_pic_cnt;
    uint8_t     direct_spatial_mv_pred_flag;*/

};


#define MAX_FRAME_SIZE  1024*1024

struct nal_parser {
    uint8_t buf[MAX_FRAME_SIZE];
    int buf_len;
    int found_sps;
    int found_pps;
    int last_nal_res;
    int field; // 0=top, 1=bottom, -1=both
    int slice;
    int have_top;
    int have_frame;
    struct nal_unit *nal0;
    struct nal_unit *nal1;
    struct nal_unit *current_nal;
    struct nal_unit *last_nal;
};

int parse_nal(uint8_t *buf, int buf_len, struct nal_parser *parser);

int seek_for_nal(uint8_t *buf, int buf_len);

struct nal_parser* init_parser();
void free_parser(struct nal_parser *parser);
int parse_frame(struct nal_parser *parser, uint8_t *inbuf, int inbuf_len,
                uint8_t **ret_buf, int *ret_len);

#endif
