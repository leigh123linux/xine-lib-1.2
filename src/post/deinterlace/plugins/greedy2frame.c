/**
 * Copyright (c) 2000 John Adcock, Tom Barry, Steve Grimm  All rights reserved.
 * mmx.h port copyright (c) 2002 Billy Biggs <vektor@dumbterm.net>.
 *
 * This code is ported from DScaler: http://deinterlace.sf.net/
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2, or (at your option)
 * any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

#include <stdio.h>
#include <stdint.h>

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include "attributes.h"
#include "xineutils.h"
#include "deinterlace.h"
#include "speedtools.h"
#include "speedy.h"

static int GreedyTwoFrameThreshold = 4;
static int GreedyTwoFrameThreshold2 = 8;

static void deinterlace_greedytwoframe_packed422_scanline_mmxext( uint8_t *output,
                                                                  deinterlace_scanline_data_t *data,
                                                                  int width )
{
#ifdef ARCH_X86
    const mmx_t Mask = { 0x7f7f7f7f7f7f7f7fULL };
    const mmx_t DwordOne = { 0x0000000100000001ULL };    
    const mmx_t DwordTwo = { 0x0000000200000002ULL };    
    mmx_t qwGreedyTwoFrameThreshold;
    uint8_t *m0 = data->m0;
    uint8_t *t1 = data->t1;
    uint8_t *b1 = data->b1;
    uint8_t *m2 = data->m2;
    uint8_t *t3 = data->t1;
    uint8_t *b3 = data->b1;

    qwGreedyTwoFrameThreshold.b[ 0 ] = GreedyTwoFrameThreshold;
    qwGreedyTwoFrameThreshold.b[ 1 ] = GreedyTwoFrameThreshold2;
    qwGreedyTwoFrameThreshold.b[ 2 ] = GreedyTwoFrameThreshold;
    qwGreedyTwoFrameThreshold.b[ 4 ] = GreedyTwoFrameThreshold;
    qwGreedyTwoFrameThreshold.b[ 6 ] = GreedyTwoFrameThreshold;

    width /= 4;
    while( width-- ) {
        movq_m2r( *m0, mm0 );
        movq_m2r( *t1, mm1 );
        movq_m2r( *b1, mm3 );
        movq_m2r( *m2, mm2 );

        // Average T1 and B1 so we can do interpolated bobbing if we bob onto T1.
        movq_r2r( mm3, mm7 );                 // mm7 = B1
        pavgb_r2r( mm1, mm7 );

        // calculate |M1-M0| put result in mm4 need to keep mm0 intact
        // if we have a good processor then make mm0 the average of M1 and M0
        // which should make weave look better when there is small amounts of
        // movement
        movq_r2r( mm0, mm4 );
        movq_r2r( mm2, mm5 );
        psubusb_r2r( mm2, mm4 );
        psubusb_r2r( mm0, mm5 );
        por_r2r( mm5, mm4 );
        psrlw_i2r( 1, mm4 );
        pavgb_r2r( mm2, mm0 );
        pand_r2r( mm6, mm4 );

        // if |M1-M0| > Threshold we want dword worth of twos
        pcmpgtb_m2r( qwGreedyTwoFrameThreshold, mm4 );
        pand_m2r( Mask, mm4 );          // get rid of any sign bit
        pcmpgtd_m2r( DwordOne, mm4 );   // do we want to bob
        pandn_m2r( DwordTwo, mm4 );

        movq_m2r( *t3, mm2 );           // mm2 = T0

        // calculate |T1-T0| put result in mm5
        movq_r2r( mm2, mm5 );
        psubusb_r2r( mm1, mm5 );
        psubusb_r2r( mm2, mm1 );
        por_r2r( mm1, mm5 );
        psrlw_i2r( 1, mm5 );
        pand_r2r( mm6, mm5 );

        // if |T1-T0| > Threshold we want dword worth of ones
        pcmpgtb_m2r( qwGreedyTwoFrameThreshold, mm5 );
        pand_r2r( mm6, mm5 );                // get rid of any sign bit
        pcmpgtd_m2r( DwordOne, mm5 );
        pandn_m2r( DwordOne, mm5 );
        paddd_r2r( mm5, mm4 );

        movq_m2r( *b3, mm2 ); // B0

        // calculate |B1-B0| put result in mm5
        movq_r2r( mm2, mm5 );
        psubusb_r2r( mm3, mm5 );
        psubusb_r2r( mm2, mm3 );
        por_r2r( mm3, mm5 );
        psrlw_i2r( 1, mm5 );
        pand_r2r( mm6, mm5 );

        // if |B1-B0| > Threshold we want dword worth of ones
        pcmpgtb_m2r( qwGreedyTwoFrameThreshold, mm5 );
        pand_r2r( mm6, mm5 );              // get rid of any sign bit
        pcmpgtd_m2r( DwordOne, mm5 );
        pandn_m2r( DwordOne, mm5 );
        paddd_r2r( mm5, mm4 );

        pcmpgtd_m2r( DwordTwo, mm4 );

        movq_r2r( mm4, mm5 );
         // mm4 now is 1 where we want to weave and 0 where we want to bob
        pand_r2r( mm0, mm4 );
        pandn_r2r( mm7, mm5 );
        por_r2r( mm5, mm4 );

        movq_r2m( mm4, *output );

        // Advance to the next set of pixels.
        output += 8;
        m0 += 8;
        t1 += 8;
        b1 += 8;
        m2 += 8;
        t3 += 8;
        b3 += 8;
    }
    sfence();
    emms();
#endif
}

static void copy_scanline( uint8_t *output,
                           deinterlace_scanline_data_t *data,
                           int width )
{
    blit_packed422_scanline( output, data->m1, width );
}


static deinterlace_setting_t settings[] =
{
    {
        "Greedy 2 Frame Luma Threshold",
        SETTING_SLIDER,
        &GreedyTwoFrameThreshold,
        4, 0, 128, 1,
        0
    },
    {
        "Greedy 2 Frame Chroma Threshold",
        SETTING_SLIDER,
        &GreedyTwoFrameThreshold2,
        8, 0, 128, 1,
        0
    }
};

static deinterlace_method_t greedymethod =
{
    DEINTERLACE_PLUGIN_API_VERSION,
    "Greedy - 2-frame (DScaler)",
    "Greedy2Frame",
    4,
    MM_ACCEL_X86_MMXEXT,
    0,
    2,
    settings,
    1,
    copy_scanline,
    deinterlace_greedytwoframe_packed422_scanline_mmxext,
    0
};

#ifdef BUILD_TVTIME_PLUGINS
void deinterlace_plugin_init( void )
#else
void greedy2frame_plugin_init( void )
#endif
{
    register_deinterlace_method( &greedymethod );
}

