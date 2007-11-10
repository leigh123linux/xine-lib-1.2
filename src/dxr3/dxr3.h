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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110, USA
 *
 * $Id: dxr3.h,v 1.8 2004/04/10 15:29:57 mroi Exp $
 */

#ifndef HAVE_DXR3_H
#define HAVE_DXR3_H

#include "em8300.h"

#include "xine_internal.h"

/* data for the device name config entry */
#define CONF_KEY  "dxr3.device_number"
#define CONF_NAME _("DXR3 device number")
#define CONF_HELP _("If you have more than one DXR3 in your computer, you can specify which one to use here.")

/* image format used by dxr3_decoder to tag undecoded mpeg data */
#define XINE_IMGFMT_DXR3 (('3'<<24)|('R'<<16)|('X'<<8)|'D')

/* name of the dxr3 video out plugin
 * (used by decoders to check for dxr3 presence) */
#define DXR3_VO_ID "dxr3"

#endif

