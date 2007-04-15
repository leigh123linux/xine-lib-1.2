/*
 * Copyright (C) 2000-2004 the xine project
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
 * $Id: vdr.c,v 1.20 2004/04/17 19:54:32 mroi Exp $
 */
 
/*
 * plugins for VDR
 */

#include "xine_internal.h"
#include "post.h"
#include "post_vdr.h"



static post_info_t vdr_video_special_info = { XINE_POST_TYPE_VIDEO_FILTER };
static post_info_t vdr_audio_special_info = { XINE_POST_TYPE_AUDIO_FILTER };

plugin_info_t xine_plugin_info[] =
{
  /* type, API, "name", version, special_info, init_function */  
  { PLUGIN_POST, 9, "vdr"      , XINE_VERSION_CODE, &vdr_video_special_info, &vdr_video_init_plugin },
  { PLUGIN_POST, 9, "vdr_video", XINE_VERSION_CODE, &vdr_video_special_info, &vdr_video_init_plugin },
  { PLUGIN_POST, 9, "vdr_audio", XINE_VERSION_CODE, &vdr_audio_special_info, &vdr_audio_init_plugin },
  { PLUGIN_NONE, 0, "", 0, NULL, NULL }
};

