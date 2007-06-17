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
 * This file contains plugin entries for several visualization post plugins.
 *
 * $Id: visualizations.h,v 1.1 2003/10/30 22:40:53 mroi Exp $
 */

#include "xine_internal.h"

void *oscope_init_plugin(xine_t *xine, void *data);
void *fftscope_init_plugin(xine_t *xine, void *data);
void *fftgraph_init_plugin(xine_t *xine, void *data);
