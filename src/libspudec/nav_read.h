/**
 * Copyright (C) 2000, 2001 H�kan Hjort <d95hjort@dtek.chalmers.se>.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#ifndef NAV_READ_H_INCLUDED
#define NAV_READ_H_INCLUDED

#include "nav_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Reads the PCI packet which begins at buffer into pci.
 */
void nav_read_pci(pci_t *pci, unsigned char *buffer);

/**
 * Reads the DSI packet which begins at buffer into dsi.
 */
void nav_read_dsi(dsi_t *dsi, unsigned char *buffer);

#ifdef __cplusplus
};
#endif
#endif /* NAV_READ_H_INCLUDED */
