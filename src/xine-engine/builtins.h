/*
 * Copyright (C) 2000-2017 the xine project
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
 * Collect some very basic plugins when building them into libxine.
 */

#ifndef XINE_ENGINE_BUILTINS_H
#define XINE_ENGINE_BUILTINS_H

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#ifdef XINE_MAKE_BUILTINS

#include <xine/xine_internal.h>

extern const plugin_info_t xine_builtin_plugin_info[];

#endif

#endif
