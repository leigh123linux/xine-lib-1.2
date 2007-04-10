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
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA
 *
 * xine interface to libwavpack by Diego Pettenò <flameeyes@gmail.com>
 *
 * $Id: combined_wavpack.h,v 1.3 2007/03/17 07:34:02 dgp85 Exp $
 */

#include "os_types.h"

typedef struct {
  uint32_t idcode;        /* This should always be the string "wvpk" */
  uint32_t block_size;    /* Size of the rest of the frame */
  uint16_t wv_version;    /* Version of the wavpack, 0x0403 should be latest */
  uint8_t track;          /* Unused, has to be 0 */
  uint8_t index;          /* Unused, has to be 0 */
  uint32_t file_samples;  /* (uint32_t)-1 if unknown, else the total number
			     of samples for the file */
  uint32_t samples_index; /* Index of the first sample in block, from the
			     start of the file */
  uint32_t samples_count; /* Count of samples in the current frame */
  uint32_t flags;         /* Misc flags */
  uint32_t decoded_crc32; /* CRC32 of the decoded data */
} __attribute__((packed)) wvheader_t;

static const uint32_t wvpk_signature = ME_FOURCC('w', 'v', 'p', 'k')

void *demux_wv_init_plugin (xine_t *const xine, void *const data);
void *decoder_wavpack_init_plugin (xine_t *xine, void *data);
