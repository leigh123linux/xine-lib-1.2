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
** $Id: mp4.h,v 1.2 2002/12/16 19:00:45 miguelfreitas Exp $
**/

#ifndef __MP4_H__
#define __MP4_H__

#ifdef __cplusplus
extern "C" {
#endif

#include "decoder.h"

int8_t FAADAPI AudioSpecificConfig(uint8_t *pBuffer,
                                   uint32_t buffer_size,
                                   uint32_t *samplerate,
                                   uint8_t *channels,
                                   uint8_t *sf_index,
                                   uint8_t *object_type,
                                   uint8_t *aacSectionDataResilienceFlag,
                                   uint8_t *aacScalefactorDataResilienceFlag,
                                   uint8_t *aacSpectralDataResilienceFlag,
                                   uint8_t *frameLengthFlag);

#ifdef __cplusplus
}
#endif
#endif
