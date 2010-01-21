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
 * tvmode - TV output selection
 *
 * Currently uses nvtvd (Dirk Thierbach <dthierbach@gmx.de>)
 * for setting TV mode
 * xine support hacked in by Matthias Hopf <mat@mshopf.de>
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "xine_internal.h"
#include "xineutils.h"

/* nvtv support is deprecated (and will be removed)
 * these dummy functions are only here to prevent serious breakage
 * until front ends are updated.
 */

int xine_tvmode_switch (xine_t *this, int type, int width, int height, double fps) {
    /* not supported: return regular mode */
    return 0;
}

void xine_tvmode_size (xine_t *this, int *width, int *height,
        double *pixelratio, double *fps) {
}

int xine_tvmode_init (xine_t *this) {
    return 0;
}

void xine_tvmode_exit (xine_t *this) {
}

void xine_tvmode_set_tvsystem(xine_t *self, xine_tvsystem system) {
}

int xine_tvmode_use(xine_t *self, int use_tvmode) {
    return 0;
}
