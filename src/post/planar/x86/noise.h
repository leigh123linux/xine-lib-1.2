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
 * mplayer's noise filter, ported by Jason Tackaberry.  Original filter
 * is copyright 2002 Michael Niedermayer <michaelni@gmx.at>
 */

#include <stdint.h>

void lineNoise_MMX(uint8_t *dst, const uint8_t *src, const int8_t *noise, int len, int shift);
void lineNoise_MMX2(uint8_t *dst, const uint8_t *src, const int8_t *noise, int len, int shift);
void lineNoiseAvg_MMX(uint8_t *dst, const uint8_t *src, int len, int8_t **shift);