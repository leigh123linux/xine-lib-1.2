/*
 * Copyright (C) 2007 the xine project
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
 */

/**
 * @file
 * @brief Internal functions related to the plugin catalog.
 * 
 * @internal This code should not be used by plugins or frontends, it's only
 * used by the xine-engine.
 */

#ifndef __LOAD_PLUGINS_H__
#define __LOAD_PLUGINS_H__

#include <xine.h>

/*
 * load plugins into catalog
 *
 * all input+demux plugins will be fully loaded+initialized
 * decoder plugins are loaded on demand
 * video/audio output plugins have special load/probe functions
 */
void _x_scan_plugins (xine_t *this);


/*
 * dispose all currently loaded plugins (shutdown)
 */

void _x_dispose_plugins (xine_t *this);

#endif
