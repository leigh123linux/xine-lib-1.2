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
** $Id: pns.h,v 1.1 2002/07/14 23:43:01 miguelfreitas Exp $
**/

#ifndef __PNS_H__
#define __PNS_H__

#ifdef __cplusplus
extern "C" {
#endif

#include "syntax.h"
#include "common.h"

#define NOISE_OFFSET 90
#define MEAN_NRG 1.537228e+18 /* (2^31)^2 / 3 */


void pns_decode(ic_stream *ics, real_t *spec, uint16_t frame_len);

static INLINE int32_t random2();
static void gen_rand_vector(real_t *spec, uint16_t scale_factor, uint16_t size);

static INLINE uint8_t is_noise(ic_stream *ics, uint8_t group, uint8_t sfb)
{
    if (ics->sfb_cb[group][sfb] == NOISE_HCB)
        return 1;
    return 0;
}

#ifdef __cplusplus
}
#endif
#endif
