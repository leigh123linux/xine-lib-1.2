/*
 * decode.c
 * Copyright (C) 2000-2003 Michel Lespinasse <walken@zoy.org>
 * Copyright (C) 1999-2000 Aaron Holtzman <aholtzma@ess.engr.uvic.ca>
 *
 * This file is part of mpeg2dec, a free MPEG-2 video stream decoder.
 * See http://libmpeg2.sourceforge.net/ for updates.
 *
 * mpeg2dec is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * mpeg2dec is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include "config.h"

#include <stdio.h>  /* For testing printf */
#include <string.h>	/* memcmp/memset, try to remove */
#include <stdlib.h>
#include <inttypes.h>

#include "../include/mpeg2.h"
#include "mpeg2_internal.h"
#include "../include/convert.h"

static int mpeg2_accels = 0;

#define BUFFER_SIZE (1194 * 1024)

const mpeg2_info_t * mpeg2_info (mpeg2dec_t * mpeg2dec)
{
    return &(mpeg2dec->info);
}

static inline int skip_chunk (mpeg2dec_t * mpeg2dec, int bytes)
{
    uint8_t * current;
    uint32_t shift;
    uint8_t * chunk_ptr;
    uint8_t * limit;
    uint8_t byte;

    if (!bytes)
	return 0;

    current = mpeg2dec->buf_start;
    shift = mpeg2dec->shift;
    chunk_ptr = mpeg2dec->chunk_ptr;
    limit = current + bytes;

    do {
	byte = *current++;
	if (shift == 0x00000100) {
	    int skipped;

	    mpeg2dec->shift = 0xffffff00;
	    skipped = current - mpeg2dec->buf_start;
	    mpeg2dec->buf_start = current;
	    return skipped;
	}
	shift = (shift | byte) << 8;
    } while (current < limit);

    mpeg2dec->shift = shift;
    mpeg2dec->buf_start = current;
    return 0;
}

static inline int copy_chunk (mpeg2dec_t * mpeg2dec, int bytes)
{
    uint8_t * current;
    uint32_t shift;
    uint8_t * chunk_ptr;
    uint8_t * limit;
    uint8_t byte;

    if (!bytes)
	return 0;

    current = mpeg2dec->buf_start;
    shift = mpeg2dec->shift;
    chunk_ptr = mpeg2dec->chunk_ptr;
    limit = current + bytes;

    do {
	byte = *current++;
	if (shift == 0x00000100) {
	    int copied;

	    mpeg2dec->shift = 0xffffff00;
	    mpeg2dec->chunk_ptr = chunk_ptr + 1;
	    copied = current - mpeg2dec->buf_start;
	    mpeg2dec->buf_start = current;
	    return copied;
	}
	shift = (shift | byte) << 8;
	*chunk_ptr++ = byte;
    } while (current < limit);

    mpeg2dec->shift = shift;
    mpeg2dec->buf_start = current;
    return 0;
}

void mpeg2_buffer (mpeg2dec_t * mpeg2dec, uint8_t * start, uint8_t * end)
{
    mpeg2dec->buf_start = start;
    mpeg2dec->buf_end = end;
}

int mpeg2_getpos (mpeg2dec_t * mpeg2dec)
{
    return mpeg2dec->buf_end - mpeg2dec->buf_start;
}

static inline mpeg2_state_t seek_chunk (mpeg2dec_t * mpeg2dec)
{
    int size, skipped;

    size = mpeg2dec->buf_end - mpeg2dec->buf_start;
    skipped = skip_chunk (mpeg2dec, size);
    if (!skipped) {
	mpeg2dec->bytes_since_pts += size;
	return STATE_BUFFER;
    }
    mpeg2dec->bytes_since_pts += skipped;
    mpeg2dec->code = mpeg2dec->buf_start[-1];
    return (mpeg2_state_t)-1;
}

static mpeg2_state_t seek_header (mpeg2dec_t * mpeg2dec)
{
    while (mpeg2dec->code != 0xb3 &&
	   ((mpeg2dec->code != 0xb7 && mpeg2dec->code != 0xb8 &&
	     mpeg2dec->code) || mpeg2dec->sequence.width == (unsigned)-1))
	if (seek_chunk (mpeg2dec) == STATE_BUFFER)
	    return STATE_BUFFER;
    mpeg2dec->chunk_start = mpeg2dec->chunk_ptr = mpeg2dec->chunk_buffer;
    return (mpeg2dec->code ? mpeg2_parse_header (mpeg2dec) :
	    mpeg2_header_picture_start (mpeg2dec));
}

mpeg2_state_t mpeg2_seek_sequence (mpeg2dec_t * mpeg2dec)
{
    mpeg2dec->sequence.width = (unsigned)-1;
    return seek_header (mpeg2dec);
}

#define RECEIVED(code,state) (((state) << 8) + (code))

mpeg2_state_t mpeg2_parse (mpeg2dec_t * mpeg2dec)
{
    int size_buffer, size_chunk, copied;

    printf("mpeg2dec-lib:decode.c:CODE=%x\n",mpeg2dec->code);
    printf("mpeg2_parse:mpeg2dec->fbuf[0]=%p",mpeg2dec->fbuf[0]);
    if (mpeg2dec->fbuf[0]) printf(", img=%p\n", mpeg2dec->fbuf[0]->id);
    else printf("\n");
    printf("mpeg2_parse:mpeg2dec->fbuf[1]=%p",mpeg2dec->fbuf[1]);
    if (mpeg2dec->fbuf[1]) printf(", img=%p\n", mpeg2dec->fbuf[1]->id);
    else printf("\n");
    printf("mpeg2_parse:mpeg2dec->fbuf[2]=%p",mpeg2dec->fbuf[2]);
    if (mpeg2dec->fbuf[2]) printf(", img=%p\n", mpeg2dec->fbuf[2]->id);
    else printf("\n");
    printf("mpeg2_parse:mpeg2dec->decoder->pictures[0]=%p\n",&mpeg2dec->pictures[0]);
    printf("mpeg2_parse:mpeg2dec->decoder->pictures[1]=%p\n",&mpeg2dec->pictures[1]);
    printf("mpeg2_parse:mpeg2dec->decoder->pictures[2]=%p\n",&mpeg2dec->pictures[2]);
    printf("mpeg2_parse:mpeg2dec->decoder->pictures[3]=%p\n",&mpeg2dec->pictures[3]);
    if (mpeg2dec->action) {
	mpeg2_state_t state;

	state = mpeg2dec->action (mpeg2dec);
	if ((int)state >= 0)
	    return state;
    }

    while (1) {
	while ((unsigned) (mpeg2dec->code - mpeg2dec->first_decode_slice) <
	       mpeg2dec->nb_decode_slices) {
	    size_buffer = mpeg2dec->buf_end - mpeg2dec->buf_start;
	    size_chunk = (mpeg2dec->chunk_buffer + BUFFER_SIZE -
			  mpeg2dec->chunk_ptr);
	    if (size_buffer <= size_chunk) {
		copied = copy_chunk (mpeg2dec, size_buffer);
		if (!copied) {
		    mpeg2dec->bytes_since_pts += size_buffer;
		    mpeg2dec->chunk_ptr += size_buffer;
		    return STATE_BUFFER;
		}
	    } else {
		copied = copy_chunk (mpeg2dec, size_chunk);
		if (!copied) {
		    /* filled the chunk buffer without finding a start code */
		    mpeg2dec->bytes_since_pts += size_chunk;
		    mpeg2dec->action = seek_chunk;
                    printf("mpeg2dec:action = mpeg2_seek_chunk\n");
		    return STATE_INVALID;
		}
	    }
	    mpeg2dec->bytes_since_pts += copied;

	    mpeg2_slice (&(mpeg2dec->decoder), mpeg2dec->code,
			 mpeg2dec->chunk_start);
	    mpeg2dec->code = mpeg2dec->buf_start[-1];
	    mpeg2dec->chunk_ptr = mpeg2dec->chunk_start;
	}
	if ((unsigned) (mpeg2dec->code - 1) >= 0xb0 - 1)
	    break;
	if (seek_chunk (mpeg2dec) == STATE_BUFFER)
	    return STATE_BUFFER;
    }
    printf("mpeg2dec-lib:decode.c:CODE2=%x\n",mpeg2dec->code);
    printf("mpeg2_parse:mpeg2dec->fbuf[0]=%p\n",mpeg2dec->fbuf[0]);
    printf("mpeg2_parse:mpeg2dec->fbuf[1]=%p\n",mpeg2dec->fbuf[1]);
    printf("mpeg2_parse:mpeg2dec->fbuf[2]=%p\n",mpeg2dec->fbuf[2]);
    switch (mpeg2dec->code) {
    case 0x00:
	mpeg2dec->action = mpeg2_header_picture_start;
        printf("mpeg2dec:action = mpeg2_header_picture_start\n");
        printf("mpeg2dec:returning state = %d\n", mpeg2dec->state);
    if (mpeg2dec->state == STATE_SLICE) {
      printf("mpeg2dec:slicing info0\n");
      mpeg2dec->info.current_picture = mpeg2dec->info.current_picture_2nd = NULL;
      mpeg2dec->info.display_picture = mpeg2dec->info.display_picture_2nd = NULL;
      mpeg2dec->info.current_fbuf = mpeg2dec->info.display_fbuf = mpeg2dec->info.discard_fbuf = NULL;
      printf("mpeg2dec:reset_info in CODE\n");
      mpeg2dec->info.user_data = NULL;
      mpeg2dec->info.user_data_len = 0;

      mpeg2dec->info.current_fbuf = mpeg2dec->fbuf[0];
      if (mpeg2dec->decoder.coding_type == B_TYPE) {
        mpeg2dec->info.display_fbuf = mpeg2dec->fbuf[0];
        mpeg2dec->info.discard_fbuf = mpeg2dec->fbuf[0];
        mpeg2dec->fbuf[0]=0;
        printf("mpeg2dec:mpeg_parse:discard_fbuf=fbuf[0]\n");
      } else {
        mpeg2dec->info.display_fbuf = mpeg2dec->fbuf[1];
        mpeg2dec->info.discard_fbuf = mpeg2dec->fbuf[2];
        mpeg2dec->fbuf[2]=0;
        printf("mpeg2dec:mpeg_parse:discard_fbuf=fbuf[2]\n");
      }
    }

	return mpeg2dec->state;
    case 0xb7:
	mpeg2dec->action = mpeg2_header_end;
        printf("mpeg2dec:action = mpeg2_header_end\n");
	break;
    case 0xb3:
    case 0xb8:
	mpeg2dec->action = mpeg2_parse_header;
        printf("mpeg2dec:action = mpeg2_parse_header\n");
	break;
    default:
	mpeg2dec->action = seek_chunk;
        printf("mpeg2dec:action = seek_chunk\n");
        printf("mpeg2dec:returning state = %d\n", mpeg2dec->state);
	return STATE_INVALID;
    }
    if (mpeg2dec->state == STATE_SLICE) {
      printf("mpeg2dec:slicing info\n");
      mpeg2dec->info.current_picture = mpeg2dec->info.current_picture_2nd = NULL;
      mpeg2dec->info.display_picture = mpeg2dec->info.display_picture_2nd = NULL;
      mpeg2dec->info.current_fbuf = mpeg2dec->info.display_fbuf = mpeg2dec->info.discard_fbuf = NULL;
      printf("mpeg2dec:reset_info in CODE\n");
      mpeg2dec->info.user_data = NULL;
      mpeg2dec->info.user_data_len = 0;

      mpeg2dec->info.current_fbuf = mpeg2dec->fbuf[0];
      if (mpeg2dec->decoder.coding_type == B_TYPE) {
        mpeg2dec->info.display_fbuf = mpeg2dec->fbuf[0];
        mpeg2dec->info.discard_fbuf = mpeg2dec->fbuf[0];
        mpeg2dec->fbuf[0]=0;
        printf("mpeg2dec:mpeg_parse:discard_fbuf=fbuf[0]\n");
      } else {
        mpeg2dec->info.display_fbuf = mpeg2dec->fbuf[1];
        mpeg2dec->info.discard_fbuf = mpeg2dec->fbuf[2];
        mpeg2dec->fbuf[2]=0;
        printf("mpeg2dec:mpeg_parse:discard_fbuf=fbuf[2]\n");
      }
    }



    printf("mpeg2dec:returning state = %d\n", mpeg2dec->state);
    return (mpeg2dec->state == STATE_SLICE) ? STATE_SLICE : STATE_INVALID;
}

mpeg2_state_t mpeg2_parse_header (mpeg2dec_t * mpeg2dec)
{
    static int (* process_header[]) (mpeg2dec_t * mpeg2dec) = {
	mpeg2_header_picture, mpeg2_header_extension, mpeg2_header_user_data,
	mpeg2_header_sequence, NULL, NULL, NULL, NULL, mpeg2_header_gop
    };
    int size_buffer, size_chunk, copied;

    mpeg2dec->action = mpeg2_parse_header;
    printf("mpeg2dec:action = mpeg2_parse_header\n");
    while (1) {
	size_buffer = mpeg2dec->buf_end - mpeg2dec->buf_start;
	size_chunk = (mpeg2dec->chunk_buffer + BUFFER_SIZE -
		      mpeg2dec->chunk_ptr);
	if (size_buffer <= size_chunk) {
	    copied = copy_chunk (mpeg2dec, size_buffer);
	    if (!copied) {
		mpeg2dec->bytes_since_pts += size_buffer;
		mpeg2dec->chunk_ptr += size_buffer;
		return STATE_BUFFER;
	    }
	} else {
	    copied = copy_chunk (mpeg2dec, size_chunk);
	    if (!copied) {
		/* filled the chunk buffer without finding a start code */
		mpeg2dec->bytes_since_pts += size_chunk;
		mpeg2dec->code = 0xb4;
		mpeg2dec->action = seek_header;
                printf("mpeg2dec:action = seek_header\n");
		return STATE_INVALID;
	    }
	}
	mpeg2dec->bytes_since_pts += copied;

	if (process_header[mpeg2dec->code & 0x0b] (mpeg2dec)) {
	    mpeg2dec->code = mpeg2dec->buf_start[-1];
	    mpeg2dec->action = seek_header;
            printf("mpeg2dec:action = seek_header\n");
	    return STATE_INVALID;
	}

	mpeg2dec->code = mpeg2dec->buf_start[-1];
        printf("mpeg2dec:CODE3=%x, state=%x\n",mpeg2dec->code, mpeg2dec->state);
	switch (RECEIVED (mpeg2dec->code, mpeg2dec->state)) {

	/* state transition after a sequence header */
	case RECEIVED (0x00, STATE_SEQUENCE):
	    mpeg2dec->action = mpeg2_header_picture_start;
            printf("mpeg2dec:action = mpeg2_header_picture_start\n");
	case RECEIVED (0xb8, STATE_SEQUENCE):
	    mpeg2_header_sequence_finalize (mpeg2dec);
	    break;

	/* other legal state transitions */
	case RECEIVED (0x00, STATE_GOP):
	    mpeg2dec->action = mpeg2_header_picture_start;
            printf("mpeg2dec:action = mpeg2_header_picture_start\n");
	    break;
	case RECEIVED (0x01, STATE_PICTURE):
	case RECEIVED (0x01, STATE_PICTURE_2ND):
	    mpeg2_header_matrix_finalize (mpeg2dec);
	    mpeg2dec->action = mpeg2_header_slice_start;
            printf("mpeg2dec:action = mpeg2_header_slice_start\n");
	    break;

	/* legal headers within a given state */
	case RECEIVED (0xb2, STATE_SEQUENCE):
	case RECEIVED (0xb2, STATE_GOP):
	case RECEIVED (0xb2, STATE_PICTURE):
	case RECEIVED (0xb2, STATE_PICTURE_2ND):
	case RECEIVED (0xb5, STATE_SEQUENCE):
	case RECEIVED (0xb5, STATE_PICTURE):
	case RECEIVED (0xb5, STATE_PICTURE_2ND):
	    mpeg2dec->chunk_ptr = mpeg2dec->chunk_start;
	    continue;

	default:
	    mpeg2dec->action = seek_header;
            printf("mpeg2dec:action = seek_header\n");
	    return STATE_INVALID;
	}

	mpeg2dec->chunk_start = mpeg2dec->chunk_ptr = mpeg2dec->chunk_buffer;
	return mpeg2dec->state;
    }
}

void mpeg2_convert (mpeg2dec_t * mpeg2dec,
		    void (* convert) (int, int, uint32_t, void *,
				      struct convert_init_s *), void * arg)
{
    convert_init_t convert_init;
    int size;

    convert_init.id = NULL;
    convert (mpeg2dec->decoder.width, mpeg2dec->decoder.height,
	     mpeg2_accels, arg, &convert_init);
    if (convert_init.id_size) {
	convert_init.id = mpeg2dec->convert_id =
	    mpeg2_malloc (convert_init.id_size, ALLOC_CONVERT_ID);
	convert (mpeg2dec->decoder.width, mpeg2dec->decoder.height,
		 mpeg2_accels, arg, &convert_init);
    }
    mpeg2dec->convert_size[0] = size = convert_init.buf_size[0];
    mpeg2dec->convert_size[1] = size += convert_init.buf_size[1];
    mpeg2dec->convert_size[2] = size += convert_init.buf_size[2];
    mpeg2dec->convert_start = convert_init.start;
    mpeg2dec->convert_copy = convert_init.copy;

    size = mpeg2dec->decoder.width * mpeg2dec->decoder.height >> 2;
    mpeg2dec->yuv_buf[0][0] = (uint8_t *) mpeg2_malloc (6 * size, ALLOC_YUV);
    mpeg2dec->yuv_buf[0][1] = mpeg2dec->yuv_buf[0][0] + 4 * size;
    mpeg2dec->yuv_buf[0][2] = mpeg2dec->yuv_buf[0][0] + 5 * size;
    mpeg2dec->yuv_buf[1][0] = (uint8_t *) mpeg2_malloc (6 * size, ALLOC_YUV);
    mpeg2dec->yuv_buf[1][1] = mpeg2dec->yuv_buf[1][0] + 4 * size;
    mpeg2dec->yuv_buf[1][2] = mpeg2dec->yuv_buf[1][0] + 5 * size;
    size = mpeg2dec->decoder.width * 8;
    mpeg2dec->yuv_buf[2][0] = (uint8_t *) mpeg2_malloc (6 * size, ALLOC_YUV);
    mpeg2dec->yuv_buf[2][1] = mpeg2dec->yuv_buf[2][0] + 4 * size;
    mpeg2dec->yuv_buf[2][2] = mpeg2dec->yuv_buf[2][0] + 5 * size;
}

void mpeg2_set_buf (mpeg2dec_t * mpeg2dec, uint8_t * buf[3], void * id)
{
    mpeg2_fbuf_t * fbuf;

    if (mpeg2dec->custom_fbuf) {
	mpeg2_set_fbuf (mpeg2dec, mpeg2dec->decoder.coding_type);
	fbuf = mpeg2dec->fbuf[0];
	if (mpeg2dec->state == STATE_SEQUENCE) {
	    mpeg2dec->fbuf[2] = mpeg2dec->fbuf[1];
	    mpeg2dec->fbuf[1] = mpeg2dec->fbuf[0];
	}
    } else {
	fbuf = &(mpeg2dec->fbuf_alloc[mpeg2dec->alloc_index].fbuf);
	mpeg2dec->alloc_index_user = ++mpeg2dec->alloc_index;
    }
    fbuf->buf[0] = buf[0];
    fbuf->buf[1] = buf[1];
    fbuf->buf[2] = buf[2];
    fbuf->id = id;
}

void mpeg2_custom_fbuf (mpeg2dec_t * mpeg2dec, int custom_fbuf)
{
    mpeg2dec->custom_fbuf = custom_fbuf;
    mpeg2dec->fbuf[0] = NULL;
    mpeg2dec->fbuf[1] = NULL;
    mpeg2dec->fbuf[2] = NULL;

}

void mpeg2_skip (mpeg2dec_t * mpeg2dec, int skip)
{
    mpeg2dec->first_decode_slice = 1;
    mpeg2dec->nb_decode_slices = skip ? 0 : (0xb0 - 1);
}

void mpeg2_slice_region (mpeg2dec_t * mpeg2dec, int start, int end)
{
    start = (start < 1) ? 1 : (start > 0xb0) ? 0xb0 : start;
    end = (end < start) ? start : (end > 0xb0) ? 0xb0 : end;
    mpeg2dec->first_decode_slice = start;
    mpeg2dec->nb_decode_slices = end - start;
}

void mpeg2_pts (mpeg2dec_t * mpeg2dec, uint32_t pts)
{
    mpeg2dec->pts_previous = mpeg2dec->pts_current;
    mpeg2dec->pts_current = pts;
    mpeg2dec->num_pts++;
    mpeg2dec->bytes_since_pts = 0;
}

uint32_t mpeg2_accel (uint32_t accel)
{
    if (!mpeg2_accels) {
	if (accel & MPEG2_ACCEL_DETECT)
	    accel |= mpeg2_detect_accel ();
	mpeg2_accels = accel |= MPEG2_ACCEL_DETECT;
	mpeg2_cpu_state_init (accel);
	mpeg2_idct_init (accel);
	mpeg2_mc_init (accel);
    }
    return mpeg2_accels & ~MPEG2_ACCEL_DETECT;
}

mpeg2dec_t * mpeg2_init (void)
{
    mpeg2dec_t * mpeg2dec;

    mpeg2_accel (MPEG2_ACCEL_DETECT);

    mpeg2dec = (mpeg2dec_t *) mpeg2_malloc (sizeof (mpeg2dec_t),
					    ALLOC_MPEG2DEC);
    if (mpeg2dec == NULL)
	return NULL;

    memset (mpeg2dec, 0, sizeof (mpeg2dec_t));

    mpeg2dec->chunk_buffer = (uint8_t *) mpeg2_malloc (BUFFER_SIZE + 4,
						       ALLOC_CHUNK);

    mpeg2dec->shift = 0xffffff00;
    mpeg2dec->action = mpeg2_seek_sequence;
    printf("mpeg2dec:action = mpeg2_seek_sequence\n");
    mpeg2dec->code = 0xb4;
    mpeg2dec->first_decode_slice = 1;
    mpeg2dec->nb_decode_slices = 0xb0 - 1;
    mpeg2dec->convert_id = NULL;

    /* initialize substructures */
    printf("mpeg2_init:mpeg2dec->fbuf[0]=%p\n",mpeg2dec->fbuf[0]);
    printf("mpeg2_init:mpeg2dec->fbuf[1]=%p\n",mpeg2dec->fbuf[1]);
    printf("mpeg2_init:mpeg2dec->fbuf[2]=%p\n",mpeg2dec->fbuf[2]);
    mpeg2_header_state_init (mpeg2dec);
    printf("mpeg2_init:mpeg2dec->fbuf[0]=%p\n",mpeg2dec->fbuf[0]);
    printf("mpeg2_init:mpeg2dec->fbuf[1]=%p\n",mpeg2dec->fbuf[1]);
    printf("mpeg2_init:mpeg2dec->fbuf[2]=%p\n",mpeg2dec->fbuf[2]);
    return mpeg2dec;
}

void mpeg2_close (mpeg2dec_t * mpeg2dec)
{
    int i;

    /* static uint8_t finalizer[] = {0,0,1,0xb4}; */
    /* mpeg2_decode_data (mpeg2dec, finalizer, finalizer+4); */

    mpeg2_free (mpeg2dec->chunk_buffer);
    if (!mpeg2dec->custom_fbuf)
	for (i = mpeg2dec->alloc_index_user; i < mpeg2dec->alloc_index; i++)
	    mpeg2_free (mpeg2dec->fbuf_alloc[i].fbuf.buf[0]);
    if (mpeg2dec->convert_start)
	for (i = 0; i < 3; i++)
	    mpeg2_free (mpeg2dec->yuv_buf[i][0]);
    if (mpeg2dec->convert_id)
	mpeg2_free (mpeg2dec->convert_id);
    mpeg2_free (mpeg2dec);
}
