/*
 * Copyright (C) 2000-2006 the xine project
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
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA
 */

#ifndef __BSWAP_H__
#define __BSWAP_H__

#include "config.h"

#define always_inline inline 

#include "ffmpeg_bswap.h"

/* These are the Aligned variants */
#define ABE_16(x) (be2me_16(*(uint16_t*)(x)))
#define ABE_32(x) (be2me_32(*(uint32_t*)(x)))
#define ABE_64(x) (be2me_64(*(uint64_t*)(x)))
#define ALE_16(x) (le2me_16(*(uint16_t*)(x)))
#define ALE_32(x) (le2me_32(*(uint32_t*)(x)))
#define ALE_64(x) (le2me_64(*(uint64_t*)(x)))

#define BE_16(x) (((uint16_t)(((uint8_t*)(x))[0]) << 8) | \
                  ((uint16_t)((uint8_t*)(x))[1]))
#define BE_24(x) (((uint32_t)(((uint8_t*)(x))[0]) << 16) | \
                  ((uint32_t)(((uint8_t*)(x))[1]) << 8) | \
                  ((uint32_t)(((uint8_t*)(x))[2])))
#define BE_32(x) (((uint32_t)(((uint8_t*)(x))[0]) << 24) | \
                  ((uint32_t)(((uint8_t*)(x))[1]) << 16) | \
                  ((uint32_t)(((uint8_t*)(x))[2]) << 8) | \
                  ((uint32_t)((uint8_t*)(x))[3]))
#define BE_64(x) (((uint64_t)(((uint8_t*)(x))[0]) << 56) | \
                  ((uint64_t)(((uint8_t*)(x))[1]) << 48) | \
                  ((uint64_t)(((uint8_t*)(x))[2]) << 40) | \
                  ((uint64_t)(((uint8_t*)(x))[3]) << 32) | \
                  ((uint64_t)(((uint8_t*)(x))[4]) << 24) | \
                  ((uint64_t)(((uint8_t*)(x))[5]) << 16) | \
                  ((uint64_t)(((uint8_t*)(x))[6]) << 8) | \
                  ((uint64_t)((uint8_t*)(x))[7]))

#define LE_16(x) (((uint16_t)(((uint8_t*)(x))[1]) << 8) | \
                  ((uint16_t)((uint8_t*)(x))[0]))
#define LE_24(x) (((uint32_t)(((uint8_t*)(x))[2]) << 16) | \
                  ((uint32_t)(((uint8_t*)(x))[1]) << 8) | \
                  ((uint32_t)(((uint8_t*)(x))[0])))
#define LE_32(x) (((uint32_t)(((uint8_t*)(x))[3]) << 24) | \
                  ((uint32_t)(((uint8_t*)(x))[2]) << 16) | \
                  ((uint32_t)(((uint8_t*)(x))[1]) << 8) | \
                  ((uint32_t)((uint8_t*)(x))[0]))
#define LE_64(x) (((uint64_t)(((uint8_t*)(x))[7]) << 56) | \
                  ((uint64_t)(((uint8_t*)(x))[6]) << 48) | \
                  ((uint64_t)(((uint8_t*)(x))[5]) << 40) | \
                  ((uint64_t)(((uint8_t*)(x))[4]) << 32) | \
                  ((uint64_t)(((uint8_t*)(x))[3]) << 24) | \
                  ((uint64_t)(((uint8_t*)(x))[2]) << 16) | \
                  ((uint64_t)(((uint8_t*)(x))[1]) << 8) | \
                  ((uint64_t)((uint8_t*)(x))[0]))

#ifdef WORDS_BIGENDIAN
#define ME_16(x) BE_16(x)
#define ME_32(x) BE_32(x)
#define ME_64(x) BE_64(x)
#define AME_16(x) ABE_16(x)
#define AME_32(x) ABE_32(x)
#define AME_64(x) ABE_64(x)
#else
#define ME_16(x) LE_16(x)
#define ME_32(x) LE_32(x)
#define ME_64(x) LE_64(x)
#define AME_16(x) ALE_16(x)
#define AME_32(x) ALE_32(x)
#define AME_64(x) ALE_64(x)
#endif

#define BE_FOURCC( ch0, ch1, ch2, ch3 )             \
        ( (uint32_t)(unsigned char)(ch3) |          \
        ( (uint32_t)(unsigned char)(ch2) << 8 ) |   \
        ( (uint32_t)(unsigned char)(ch1) << 16 ) |  \
        ( (uint32_t)(unsigned char)(ch0) << 24 ) )

#define LE_FOURCC( ch0, ch1, ch2, ch3 )             \
        ( (uint32_t)(unsigned char)(ch0) |          \
        ( (uint32_t)(unsigned char)(ch1) << 8 ) |   \
        ( (uint32_t)(unsigned char)(ch2) << 16 ) |  \
        ( (uint32_t)(unsigned char)(ch3) << 24 ) )
        
#ifdef WORDS_BIGENDIAN
#define ME_FOURCC BE_FOURCC
#else
#define ME_FOURCC LE_FOURCC
#endif

#endif
