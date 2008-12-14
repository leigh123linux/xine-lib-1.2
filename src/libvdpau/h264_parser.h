#ifndef NAL_PARSER_H_
#define NAL_PARSER_H_

#include <stdlib.h>

#include "xine_internal.h"
#include "nal.h"
#include "dpb.h"

#define MAX_FRAME_SIZE  1024*1024

struct nal_parser {
    uint8_t buf[MAX_FRAME_SIZE];
    uint32_t buf_len;

    /* prebuf is used to store the currently
     * processed nal unit */
    uint8_t prebuf[MAX_FRAME_SIZE];
    uint32_t prebuf_len;
    uint32_t next_nal_position;
    uint8_t incomplete_nal;

    uint8_t found_sps;
    uint8_t found_pps;
    uint8_t last_nal_res;

    uint8_t is_idr;

    int field; /* 0=top, 1=bottom, -1=both */
    int slice;
    int slice_cnt;

    uint8_t have_top;
    uint8_t have_frame;

    struct nal_unit *nal0;
    struct nal_unit *nal1;
    struct nal_unit *current_nal;
    struct nal_unit *last_nal;

    uint8_t cpb_dpb_delays_present_flag;

    uint32_t pic_order_cnt_lsb;
    uint32_t pic_order_cnt_msb;
    uint32_t prev_pic_order_cnt_lsb;
    uint32_t prev_pic_order_cnt_msb;

    /* this is dpb used for reference frame
     * heading to vdpau + unordered frames
     */
    struct dpb dpb;
};

int parse_nal(uint8_t *buf, int buf_len, struct nal_parser *parser);

int seek_for_nal(uint8_t *buf, int buf_len);

struct nal_parser* init_parser();
void free_parser(struct nal_parser *parser);
int parse_frame(struct nal_parser *parser, uint8_t *inbuf, int inbuf_len,
                uint8_t **ret_buf, uint32_t *ret_len, uint32_t *ret_slice_cnt);

#endif
