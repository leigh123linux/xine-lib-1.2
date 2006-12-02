/* 
 * Copyright (C) 2000-2005 the xine project
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
 * $Id: group_audio.h,v 1.8 2005/05/29 19:20:48 jstembridge Exp $
 */

#ifndef HAVE_GROUP_AUDIO_H
#define HAVE_GROUP_AUDIO_H

#include "xine_internal.h"

void *demux_aac_init_plugin (xine_t *xine, void *data);
void *demux_ac3_init_plugin (xine_t *xine, void *data);
void *demux_aud_init_plugin (xine_t *xine, void *data);
void *demux_aiff_init_plugin (xine_t *xine, void *data);
void *demux_cdda_init_plugin (xine_t *xine, void *data);
void *demux_dts_init_plugin (xine_t *xine, void *data);
void *demux_flac_init_plugin (xine_t *xine, void *data);
void *demux_mpgaudio_init_class (xine_t *xine, void *data);
void *demux_mpc_init_plugin (xine_t *xine, void *data);
void *demux_nsf_init_plugin (xine_t *xine, void *data);
void *demux_realaudio_init_plugin (xine_t *xine, void *data);
void *demux_shn_init_plugin (xine_t *xine, void *data);
void *demux_snd_init_plugin (xine_t *xine, void *data);
void *demux_voc_init_plugin (xine_t *xine, void *data);
void *demux_vox_init_plugin (xine_t *xine, void *data);
void *demux_wav_init_plugin (xine_t *xine, void *data);

#ifdef HAVE_MODPLUG
void *demux_mod_init_plugin (xine_t *xine, void *data);
#endif

#endif
