/*
 * Copyright (C) 2008-2018 the xine project
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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110, USA
 *
 * A group of video stream parsers using the VDPAU hardware decoder.
 *
 */

#ifndef GROUP_VDPAU_H
#define GROUP_VDPAU_H 1

#include <xine/xine_internal.h>

/* TJ. My vdpau.h says

   typedef void * VdpPictureInfo;

   then defines arg #3 of VdpDecoderRender () as

   VdpPictureInfo const * picture_info

   This is obviously wrong. If this should have
   been fixed in the meantime, change the following
   define to 0.
*/

#if 1
#  define CAST_VdpPictureInfo_PTR (void *)
#else
#  define CAST_VdpPictureInfo_PTR (VdpPictureInfo *)
#endif

void *h264_alter_init_plugin (xine_t *xine, const void *data);
void *h264_init_plugin       (xine_t *xine, const void *data);
void *vc1_init_plugin        (xine_t *xine, const void *data);
void *mpeg12_init_plugin     (xine_t *xine, const void *data);
void *mpeg4_init_plugin      (xine_t *xine, const void *data);

#endif
