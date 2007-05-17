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
 * $Id: audio_filters.h,v 1.3 2006/02/05 20:38:37 miguelfreitas Exp $
 *
 * catalog for audio filter plugins
 */

#include "xine_internal.h"


void *upmix_init_plugin(xine_t *xine, void *data);
void *upmix_mono_init_plugin(xine_t *xine, void *data);
void *stretch_init_plugin(xine_t *xine, void *data);
void *volnorm_init_plugin(xine_t *xine, void *data);
