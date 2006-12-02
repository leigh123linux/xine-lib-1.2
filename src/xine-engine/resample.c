/* 
 * Copyright (C) 2000-2003 the xine project
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
 *
 * $Id: resample.c,v 1.10 2006/09/26 05:19:49 dgp85 Exp $
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <inttypes.h>
#include "attributes.h"
#include "resample.h"

/* contributed by paul flinders */

void _x_audio_out_resample_mono(int16_t* input_samples, uint32_t in_samples, 
				int16_t* output_samples, uint32_t out_samples)
{
  unsigned int osample;
  /* 16+16 fixed point math */
  uint32_t isample = 0;
  uint32_t istep = ((in_samples-2) << 16)/(out_samples-2);

#ifdef VERBOSE
  printf ("Audio : resample %d samples to %d\n",
          in_samples, out_samples);
#endif

  for (osample = 0; osample < out_samples - 1; osample++) {
    int  s1;
    int  s2;
    int16_t  os;
    uint32_t t = isample&0xffff;
    
    /* don't "optimize" the (isample >> 16)*2 to (isample >> 15) */
    s1 = input_samples[(isample >> 16)];
    s2 = input_samples[(isample >> 16)+1];
    
    os = (s1 * (0x10000-t)+ s2 * t) >> 16;
    output_samples[osample] = os;

    isample += istep;
  }
  output_samples[out_samples-1] = input_samples[in_samples-1];
}

void _x_audio_out_resample_stereo(int16_t* input_samples, uint32_t in_samples, 
				  int16_t* output_samples, uint32_t out_samples)
{
  unsigned int osample;
  /* 16+16 fixed point math */
  uint32_t isample = 0;
  uint32_t istep = ((in_samples-2) << 16)/(out_samples-2);

#ifdef VERBOSE
  printf ("Audio : resample %d samples to %d\n",
          in_samples, out_samples);
#endif

  for (osample = 0; osample < out_samples - 1; osample++) {
    int  s1;
    int  s2;
    int16_t  os;
    uint32_t t = isample&0xffff;
    
    /* don't "optimize" the (isample >> 16)*2 to (isample >> 15) */
    s1 = input_samples[(isample >> 16)*2];
    s2 = input_samples[(isample >> 16)*2+2];
    
    os = (s1 * (0x10000-t)+ s2 * t) >> 16;
    output_samples[osample * 2] = os;

    s1 = input_samples[(isample >> 16)*2+1];
    s2 = input_samples[(isample >> 16)*2+3];
    
    os = (s1 * (0x10000-t)+ s2 * t) >> 16;
    output_samples[(osample * 2 )+1] = os;
    isample += istep;
  }
  output_samples[out_samples*2-2] = input_samples[in_samples*2-2];
  output_samples[out_samples*2-1] = input_samples[in_samples*2-1];
}


void _x_audio_out_resample_4channel(int16_t* input_samples, uint32_t in_samples, 
				    int16_t* output_samples, uint32_t out_samples)
{
  unsigned int osample;
  /* 16+16 fixed point math */
  uint32_t isample = 0;
  uint32_t istep = ((in_samples-2) << 16)/(out_samples-2);

#ifdef VERBOSE
  printf ("Audio : resample %d samples to %d\n",
          in_samples, out_samples);
#endif

  for (osample = 0; osample < out_samples - 1; osample++) {
    int  s1;
    int  s2;
    int16_t  os;
    uint32_t t = isample&0xffff;
    
    /* don't "optimize" the (isample >> 16)*2 to (isample >> 15) */
    s1 = input_samples[(isample >> 16)*4];
    s2 = input_samples[(isample >> 16)*4+4];
    
    os = (s1 * (0x10000-t)+ s2 * t) >> 16;
    output_samples[osample * 4] = os;

    s1 = input_samples[(isample >> 16)*4+1];
    s2 = input_samples[(isample >> 16)*4+5];
    
    os = (s1 * (0x10000-t)+ s2 * t) >> 16;
    output_samples[(osample * 4 )+1] = os;

    s1 = input_samples[(isample >> 16)*4+2];
    s2 = input_samples[(isample >> 16)*4+6];
    
    os = (s1 * (0x10000-t)+ s2 * t) >> 16;
    output_samples[(osample * 4 )+2] = os;

    s1 = input_samples[(isample >> 16)*4+3];
    s2 = input_samples[(isample >> 16)*4+7];
    
    os = (s1 * (0x10000-t)+ s2 * t) >> 16;
    output_samples[(osample * 4 )+3] = os;

    isample += istep;
  }
  output_samples[out_samples*4-4] = input_samples[in_samples*4-4];
  output_samples[out_samples*4-3] = input_samples[in_samples*4-3];
  output_samples[out_samples*4-2] = input_samples[in_samples*4-2];
  output_samples[out_samples*4-1] = input_samples[in_samples*4-1];

}


void _x_audio_out_resample_5channel(int16_t* input_samples, uint32_t in_samples, 
				    int16_t* output_samples, uint32_t out_samples)
{
  unsigned int osample;
  /* 16+16 fixed point math */
  uint32_t isample = 0;
  uint32_t istep = ((in_samples-2) << 16)/(out_samples-2);

#ifdef VERBOSE
  printf ("Audio : resample %d samples to %d\n",
          in_samples, out_samples);
#endif

  for (osample = 0; osample < out_samples - 1; osample++) {
    int  s1;
    int  s2;
    int16_t  os;
    uint32_t t = isample&0xffff;
    
    /* don't "optimize" the (isample >> 16)*2 to (isample >> 15) */
    s1 = input_samples[(isample >> 16)*5];
    s2 = input_samples[(isample >> 16)*5+5];
    
    os = (s1 * (0x10000-t)+ s2 * t) >> 16;
    output_samples[osample * 5] = os;

    s1 = input_samples[(isample >> 16)*5+1];
    s2 = input_samples[(isample >> 16)*5+6];
    
    os = (s1 * (0x10000-t)+ s2 * t) >> 16;
    output_samples[(osample * 5 )+1] = os;

    s1 = input_samples[(isample >> 16)*5+2];
    s2 = input_samples[(isample >> 16)*5+7];
    
    os = (s1 * (0x10000-t)+ s2 * t) >> 16;
    output_samples[(osample * 5 )+2] = os;

    s1 = input_samples[(isample >> 16)*5+3];
    s2 = input_samples[(isample >> 16)*5+8];
    
    os = (s1 * (0x10000-t)+ s2 * t) >> 16;
    output_samples[(osample * 5 )+3] = os;

    s1 = input_samples[(isample >> 16)*5+4];
    s2 = input_samples[(isample >> 16)*5+9];
    
    os = (s1 * (0x10000-t)+ s2 * t) >> 16;
    output_samples[(osample * 5 )+4] = os;

    isample += istep;
  }

  output_samples[out_samples*5-5] = input_samples[in_samples*5-5];
  output_samples[out_samples*5-4] = input_samples[in_samples*5-4];
  output_samples[out_samples*5-3] = input_samples[in_samples*5-3];
  output_samples[out_samples*5-2] = input_samples[in_samples*5-2];
  output_samples[out_samples*5-1] = input_samples[in_samples*5-1];
}


void _x_audio_out_resample_6channel(int16_t* input_samples, uint32_t in_samples, 
				    int16_t* output_samples, uint32_t out_samples)
{
  unsigned int osample;
  /* 16+16 fixed point math */
  uint32_t isample = 0;
  uint32_t istep = ((in_samples-2) << 16)/(out_samples-2);

#ifdef VERBOSE
  printf ("Audio : resample %d samples to %d\n",
          in_samples, out_samples);
#endif

  for (osample = 0; osample < out_samples - 1; osample++) {
    int  s1;
    int  s2;
    int16_t  os;
    uint32_t t = isample&0xffff;
    
    /* don't "optimize" the (isample >> 16)*2 to (isample >> 15) */
    s1 = input_samples[(isample >> 16)*6];
    s2 = input_samples[(isample >> 16)*6+6];
    
    os = (s1 * (0x10000-t)+ s2 * t) >> 16;
    output_samples[osample * 6] = os;

    s1 = input_samples[(isample >> 16)*6+1];
    s2 = input_samples[(isample >> 16)*6+7];
    
    os = (s1 * (0x10000-t)+ s2 * t) >> 16;
    output_samples[(osample * 6 )+1] = os;

    s1 = input_samples[(isample >> 16)*6+2];
    s2 = input_samples[(isample >> 16)*6+8];
    
    os = (s1 * (0x10000-t)+ s2 * t) >> 16;
    output_samples[(osample * 6 )+2] = os;

    s1 = input_samples[(isample >> 16)*6+3];
    s2 = input_samples[(isample >> 16)*6+9];
    
    os = (s1 * (0x10000-t)+ s2 * t) >> 16;
    output_samples[(osample * 6 )+3] = os;

    s1 = input_samples[(isample >> 16)*6+4];
    s2 = input_samples[(isample >> 16)*6+10];
    
    os = (s1 * (0x10000-t)+ s2 * t) >> 16;
    output_samples[(osample * 6 )+4] = os;

    s1 = input_samples[(isample >> 16)*6+5];
    s2 = input_samples[(isample >> 16)*6+11];
    
    os = (s1 * (0x10000-t)+ s2 * t) >> 16;
    output_samples[(osample * 6 )+5] = os;

    isample += istep;
  }

  output_samples[out_samples*6-6] = input_samples[in_samples*6-6];
  output_samples[out_samples*6-5] = input_samples[in_samples*6-5];
  output_samples[out_samples*6-4] = input_samples[in_samples*6-4];
  output_samples[out_samples*6-3] = input_samples[in_samples*6-3];
  output_samples[out_samples*6-2] = input_samples[in_samples*6-2];
  output_samples[out_samples*6-1] = input_samples[in_samples*6-1];
}

void _x_audio_out_resample_8to16(int8_t* input_samples, 
				 int16_t* output_samples, uint32_t samples)
{
  while( samples-- ) {
    int16_t os;
    
    os = *input_samples++;
    os = (os - 0x80) << 8;
    *output_samples++ = os;
  }
}

void _x_audio_out_resample_16to8(int16_t* input_samples, 
				 int8_t* output_samples, uint32_t samples)
{
  while( samples-- ) {
    int16_t os;
    
    os = *input_samples++;
    os = (os >> 8) + 0x80;
    *output_samples++ = os;
  }
}

void _x_audio_out_resample_monotostereo(int16_t* input_samples, 
					int16_t* output_samples, uint32_t frames)
{
  while( frames-- ) {
    int16_t os;
    
    os = *input_samples++;
    *output_samples++ = os;
    *output_samples++ = os;
  }
}

void _x_audio_out_resample_stereotomono(int16_t* input_samples, 
					int16_t* output_samples, uint32_t frames)
{
  while( frames-- ) {
    int16_t os;
    
    os = (*input_samples++)>>1;
    os += (*input_samples++)>>1;
    *output_samples++ = os;
  }
}
