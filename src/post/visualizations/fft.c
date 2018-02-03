/*
 * Copyright (C) 2000-2018 the xine project
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
 * FFT code by Steve Haehnichen, originally licensed under GPL v1
 * modified by Thibaut Mattern (tmattern@noos.fr) to remove global vars
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <math.h>

#include "fft.h"

/**************************************************************************
 * fft specific decode functions
 *************************************************************************/

# define                SINE(x)         (fft->SineTable[(x)])
# define                COSINE(x)       (fft->CosineTable[(x)])
# define                WINDOW(x)       (fft->WinTable[(x)])

#define PERMUTE(x, y)   reverse((x), (y))

/* Number of samples in one "frame" */
#define SAMPLES         (1 << bits)

#define REAL(x)         wave[(x)].re
#define IMAG(x)         wave[(x)].im
#define ALPHA           0.54

/*
 *  Bit reverser for unsigned ints
 *  Reverses 'bits' bits.
 */
static inline unsigned int
reverse (unsigned int val, int bits)
{
  unsigned int retn = 0;

  while (bits--)
    {
      retn <<= 1;
      retn |= (val & 1);
      val >>= 1;
    }
  return (retn);
}

/*
 *  Here is the real work-horse.
 *  It's a generic FFT, so nothing is lost or approximated.
 *  The samples in wave[] should be in order, and they
 *  will be decimated when fft() returns.
 */
void fft_compute (fft_t *fft, complex_t wave[])
{
  int      loop;
  unsigned i1;             /* going to right shift this */
  int      i2;
  int      bits = fft->bits;

  i1 = SAMPLES / 2;
  i2 = 1;

  /* perform the butterflys */

  for (loop = 0; loop < bits; loop++)
    {
      int loop1;
      int i3, i4;

      i3 = 0;
      i4 = i1;

      for (loop1 = 0; loop1 < i2; loop1++)
        {
          int loop2;
          int y;
          double z1, z2;

          y  = fft->PermuteTable[(i3 / (int)i1) & fft->bmask];
          z1 = COSINE(y);
          z2 = -SINE(y);

          for (loop2 = i3; loop2 < i4; loop2++)
            {
              double a1, a2, b1, b2;

              a1 = REAL(loop2);
              a2 = IMAG(loop2);

              b1 = z1 * REAL(loop2+i1) - z2 * IMAG(loop2+i1);
              b2 = z2 * REAL(loop2+i1) + z1 * IMAG(loop2+i1);

              REAL(loop2) = a1 + b1;
              IMAG(loop2) = a2 + b2;

              REAL(loop2+i1) = a1 - b1;
              IMAG(loop2+i1) = a2 - b2;
            }

          i3 += (i1 << 1);
          i4 += (i1 << 1);
        }

      i1 >>= 1;
      i2 <<= 1;
    }
}

/*
 *  Initializer for FFT routines.  Currently only sets up tables.
 *  - Generates scaled lookup tables for sin() and cos()
 *  - Fills a table for the Hamming window function
 */
fft_t *fft_new (int bits)
{
  fft_t *fft;
  int i;
  const double   TWOPIoN   = (atan(1.0) * 8.0) / (double)SAMPLES;
  const double   TWOPIoNm1 = (atan(1.0) * 8.0) / (double)(SAMPLES - 1);

  /* printf("fft_new: bits=%d\n", bits); */

  fft = malloc (sizeof (fft_t));
  if (!fft)
    return NULL;

  /* xine just uses 9 or 11. */
  fft->bits = bits;
  fft->bmask = SAMPLES - 1;
  fft->PermuteTable = malloc (sizeof (int) * SAMPLES);
  if (!fft->PermuteTable) {
    free (fft);
    return NULL;
  }
  for (i = 0; i < SAMPLES; i++) {
    fft->PermuteTable[i] = PERMUTE (i, bits);
  }

  fft->SineTable   = malloc (3 * sizeof(double) * SAMPLES);
  if (!fft->SineTable) {
    free (fft->PermuteTable);
    free (fft);
    return NULL;
  }
  fft->CosineTable = fft->SineTable + SAMPLES;
  fft->WinTable    = fft->SineTable + 2 * SAMPLES;
  for (i = 0; i < SAMPLES; i++) {
    double a = (double) i * TWOPIoN;
    fft->SineTable[i]   = sin (a);
    fft->CosineTable[i] = cos (a);
  }
      /*
       * Generalized Hamming window function.
       * Set ALPHA to 0.54 for a hanning window. (Good idea)
       */
  for (i = 0; i < SAMPLES; i++)
    fft->WinTable[i] = ALPHA + ((1.0 - ALPHA) * cos (TWOPIoNm1 * (i - SAMPLES/2)));

  return fft;
}

void fft_dispose(fft_t *fft)
{
  if (fft)
  {
    free(fft->PermuteTable);
    free(fft->SineTable);
    free(fft);
  }
}

/*
 *  Apply some windowing function to the samples.
 */
void fft_window (fft_t *fft, complex_t wave[])
{
  int i;
  int bits = fft->bits;

  for (i = 0; i < SAMPLES; i++)
    {
      REAL(i) *= WINDOW(i);
      IMAG(i) *= WINDOW(i);
    }
}

/*
 *  Calculate amplitude of component n in the decimated wave[] array.
 */
double fft_amp (int n, complex_t wave[], int bits)
{
  n = PERMUTE (n, bits);
  return (hypot (REAL(n), IMAG(n)));
}

double fft_amp2 (fft_t *fft, int n, complex_t wave[])
{
  n = fft->PermuteTable[n & fft->bmask];
  return (hypot (REAL(n), IMAG(n)));
}

/*
 *  Scale sampled values.
 *  Do this *before* the fft.
 */
void fft_scale (complex_t wave[], int bits)
{
  int i, n = 1 << bits;
  double m = (double)1 / n;

  for (i = 0; i < n; i++)  {
    wave[i].re *= m;
    wave[i].im *= m;
  }
}
