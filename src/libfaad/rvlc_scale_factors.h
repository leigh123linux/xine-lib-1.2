/*
** FAAD - Freeware Advanced Audio Decoder
** Copyright (C) 2002 M. Bakker
**  
** This program is free software; you can redistribute it and/or modify
** it under the terms of the GNU General Public License as published by
** the Free Software Foundation; either version 2 of the License, or
** (at your option) any later version.
** 
** This program is distributed in the hope that it will be useful,
** but WITHOUT ANY WARRANTY; without even the implied warranty of
** MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
** GNU General Public License for more details.
** 
** You should have received a copy of the GNU General Public License
** along with this program; if not, write to the Free Software 
** Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
**
** $Id: rvlc_scale_factors.h,v 1.1 2002/08/09 22:36:36 miguelfreitas Exp $
**/

#ifndef __RVLC_SCF_H__
#define __RVLC_SCF_H__

#ifdef __cplusplus
extern "C" {
#endif

typedef struct
{
    int8_t index;
    uint8_t len;
    uint32_t cw;
} rvlc_huff_table;


#define ESC_VAL 7


uint8_t rvlc_scale_factor_data(ic_stream *ics, bitfile *ld);
uint8_t rvlc_decode_scale_factors(ic_stream *ics, bitfile *ld);

static uint8_t rvlc_decode_sf_forward(ic_stream *ics,
                                      bitfile *ld_sf,
                                      bitfile *ld_esc);
static int8_t rvlc_huffman_sf(bitfile *ld_sf,
                              bitfile *ld_esc);
static int8_t rvlc_huffman_esc(bitfile *ld_esc);


#ifdef __cplusplus
}
#endif
#endif
