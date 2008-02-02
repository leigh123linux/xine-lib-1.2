/*
 * Copyright (C) 2008 the xine project
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
 * xv_common.h: X11 Xv common bits
 */

#define VIDEO_DEVICE_XV_COLORKEY_HELP \
	_("video overlay colour key"), \
	_("The colour key is used to tell the graphics card where to " \
	  "overlay the video image. Try different values, if you "\
	  "experience windows becoming transparent.")

#define VIDEO_DEVICE_XV_AUTOPAINT_COLORKEY_HELP \
	_("autopaint colour key"), \
	_("Make Xv autopaint its colour key.")

#define VIDEO_DEVICE_XV_FILTER_HELP \
	_("bilinear scaling mode"), \
	_("Selects the bilinear scaling mode for Permedia cards. " \
	  "The individual values are:\n\n" \
	  "Permedia 2\n" \
	  "0 - disable bilinear filtering\n" \
	  "1 - enable bilinear filtering\n\n" \
	  "Permedia 3\n" \
	  "0 - disable bilinear filtering\n" \
	  "1 - horizontal linear filtering\n" \
	  "2 - enable full bilinear filtering")

#define VIDEO_DEVICE_XV_DOUBLE_BUFFER_HELP \
	_("enable double buffering"), \
	_("Double buffering will synchronize the update of the video " \
	  "image to the repainting of the entire screen (\"vertical " \
	  "retrace\"). This eliminates flickering and tearing artifacts, " \
	  "but will use more graphics memory.")
