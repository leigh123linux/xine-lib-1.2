/*
 * Copyright (C) 2016-2017 the xine project
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
 * "Time Domain Audio Analyzer" Visualization Post Plugin For xine
 *   by Torsten Jager (t.jager@gmx.de)
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h>

#include <xine/xine_internal.h>
#include <xine/xineutils.h>
#include <xine/post.h>
#include "visualizations.h"

typedef struct {
  post_class_t  post_class;
  xine_t       *xine;
} post_class_tdaan_t;

typedef struct {
  int x, y, width, height;
  /* these are in dB (0..-64) */
  int rms;
  int peak;
  int hold;
  /* in frames */
  int age;
  /* in sample values */
  uint64_t squaresum;
  int smax;
} tdaan_leveller_t;

#define RING_LOG2 13
#define RING_SIZE (1 << RING_LOG2)
#define RING_MASK (RING_SIZE - 1)

typedef struct {
  post_plugin_t      post;

  xine_video_port_t *vo_port;
  post_out_t         video_output;
  metronom_t        *metronom;
  /* video out props */
  int                video_width;
  int                video_height;
  double             video_ratio;
  int                video_fps;
  /* phaser */
  int                phaser_x;
  int                phaser_y;
  int                phaser_width;
  int                phaser_height;
  int                phaser_last_x;
  int                phaser_last_y;
  int                amax;
  /* level bar */
  tdaan_leveller_t   lbar;
  tdaan_leveller_t   rbar;

  int                channels;
  int                samples_per_frame;

  int                ring_put, ring_get;
  int16_t            ringbuf[RING_SIZE * 2];
} post_plugin_tdaan_t;

typedef union {
  uint8_t  bytes[4];
  uint32_t word;
} yuy2_color_t;

static const yuy2_color_t /* SD, mpeg range */
  tdaan_BLACK       = {{ 16, 128,  16, 128}},
//tdaan_DARK_BLUE   = {{ 61, 190,  61, 118}},
  tdaan_DARK_RED    = {{ 83, 107,  83, 190}},
//tdaan_DARK_MAGENTA= {{ 97, 169,  97, 180}},
  tdaan_DARK_GREEN  = {{124,  87, 124,  76}},
//tdaan_DARK_CYAN   = {{132, 149, 132,  66}},
  tdaan_DARK_YELLOW = {{155,  66, 155, 138}},
  tdaan_GREY        = {{128, 128, 128, 128}},
  tdaan_LIGHT_GRAY  = {{170, 128, 170, 128}},
//tdaan_BLUE        = {{ 74, 209,  74, 115}},
  tdaan_RED         = {{103, 101, 103, 209}},
//tdaan_MAGENTA     = {{121, 181, 121, 196}},
  tdaan_GREEN       = {{148,  74, 148,  60}},
//tdaan_CYAN        = {{166, 155, 166,  47}},
  tdaan_YELLOW      = {{195,  47, 195, 141}},
  tdaan_WHITE       = {{220, 128, 220, 128}};

/**************************************************************************
 * video frame layout                                                     *
 *************************************************************************/

static void tdaan_video_resize (post_plugin_tdaan_t *this) {
  int w, h;

  w = this->video_width / (11 * 5);
  w += 1;
  w &= ~1;
  h = this->video_height / 10;

  this->phaser_x      = w * 5;
  this->phaser_y      = h;
  this->phaser_width  = w * 7 * 5;
  this->phaser_height = h * 8;

  this->lbar.x      =
  this->rbar.x      = w * 9 * 5;
  this->lbar.y      =
  this->rbar.y      = h;
  this->lbar.width  =
  this->rbar.width  = w * 4;
  this->lbar.height =
  this->rbar.height = h * 8;
  this->lbar.hold   =
  this->rbar.hold   = -64;
}

/**************************************************************************
 * yuy2 rendering primitives - (0, 0) is top left                         *
 *************************************************************************/

static void tdaan_draw_text (vo_frame_t *frame, int x, int y, const char *s) {
#define __ 255
  static const uint8_t map[256] = {
    __,__,__,__,__,__,__,__,__,__,__,__,__,__,__,__,
    __,__,__,__,__,__,__,__,__,__,__,__,__,__,__,__,
    __,__,__,__,__,__,__,__,__,__,__,__,__,10,__,__,
     0, 1, 2, 3, 4, 5, 6, 7, 8, 9,__,__,__,__,__,__,
    __,__,11,__,__,__,__,__,__,__,__,__,12,__,__,__,
    __,__,13,__,__,__,__,__,__,__,__,__,__,__,__,__,
    __,__,__,__,14,__,__,__,__,__,__,__,__,__,__,__,
    __,__,__,__,__,__,__,__,__,__,__,__,__,__,__,__,
    __,__,__,__,__,__,__,__,__,__,__,__,__,__,__,__,
    __,__,__,__,__,__,__,__,__,__,__,__,__,__,__,__,
    __,__,__,__,__,__,__,__,__,__,__,__,__,__,__,__,
    __,__,__,__,__,__,__,__,__,__,__,__,__,__,__,__,
    __,__,__,__,__,__,__,__,__,__,__,__,__,__,__,__,
    __,__,__,__,__,__,__,__,__,__,__,__,__,__,__,__,
    __,__,__,__,__,__,__,__,__,__,__,__,__,__,__,__,
    __,__,__,__,__,__,__,__,__,__,__,__,__,__,__,__
  };
#undef __
#define __ {{128, 128, 128, 128}}
#define _o {{128, 128,  16, 128}}
#define o_ {{ 16, 128, 128, 128}}
#define oo {{ 16, 128,  16, 128}}
  static const yuy2_color_t font[] = {
    __,oo,oo,oo,__,
    _o,oo,oo,oo,o_,
    _o,o_,__,_o,o_,
    _o,o_,__,_o,o_,
    _o,o_,__,_o,o_,
    _o,o_,__,_o,o_,
    _o,o_,__,_o,o_,
    _o,o_,__,_o,o_,
    _o,oo,oo,oo,o_,
    __,oo,oo,oo,__,

    __,__,_o,o_,__,
    __,__,oo,o_,__,
    __,_o,oo,o_,__,
    __,oo,_o,o_,__,
    _o,o_,_o,o_,__,
    __,__,_o,o_,__,
    __,__,_o,o_,__,
    __,__,_o,o_,__,
    __,__,_o,o_,__,
    __,__,_o,o_,__,

    __,oo,oo,oo,__,
    _o,oo,oo,oo,o_,
    _o,o_,__,_o,o_,
    _o,o_,__,_o,o_,
    __,__,_o,oo,o_,
    __,_o,oo,oo,__,
    __,oo,__,__,__,
    _o,o_,__,__,__,
    _o,oo,oo,oo,o_,
    _o,oo,oo,oo,o_,

    __,oo,oo,oo,__,
    _o,oo,oo,oo,o_,
    _o,o_,__,_o,o_,
    __,__,__,_o,o_,
    __,_o,oo,oo,__,
    __,_o,oo,oo,o_,
    __,__,__,_o,o_,
    _o,o_,__,_o,o_,
    _o,oo,oo,oo,o_,
    __,oo,oo,oo,__,

    __,__,oo,__,__,
    __,_o,o_,__,__,
    __,oo,__,__,__,
    __,oo,__,o_,__,
    _o,o_,_o,o_,__,
    _o,oo,oo,oo,o_,
    _o,oo,oo,oo,o_,
    __,__,_o,o_,__,
    __,__,_o,o_,__,
    __,__,_o,o_,__,

    _o,oo,oo,oo,o_,
    _o,oo,oo,oo,o_,
    _o,o_,__,__,__,
    _o,o_,__,__,__,
    _o,oo,oo,oo,__,
    _o,oo,oo,oo,o_,
    __,__,__,_o,o_,
    __,__,__,_o,o_,
    _o,oo,oo,oo,o_,
    _o,oo,oo,oo,__,

    __,__,oo,oo,o_,
    __,_o,oo,oo,o_,
    __,oo,o_,__,__,
    _o,o_,__,__,__,
    _o,oo,oo,oo,__,
    _o,oo,oo,oo,o_,
    _o,o_,__,_o,o_,
    _o,o_,__,_o,o_,
    _o,oo,oo,oo,o_,
    __,oo,oo,oo,__,

    _o,oo,oo,oo,o_,
    _o,oo,oo,oo,o_,
    __,__,__,_o,o_,
    __,__,__,oo,__,
    __,__,__,oo,__,
    __,__,_o,o_,__,
    __,__,_o,o_,__,
    __,__,oo,__,__,
    __,__,oo,__,__,
    __,_o,o_,__,__,

    __,oo,oo,oo,__,
    _o,oo,oo,oo,o_,
    _o,o_,__,_o,o_,
    _o,o_,__,_o,o_,
    __,oo,oo,oo,__,
    _o,oo,oo,oo,o_,
    _o,o_,__,_o,o_,
    _o,o_,__,_o,o_,
    _o,oo,oo,oo,o_,
    __,oo,oo,oo,__,

    __,oo,oo,oo,__,
    _o,oo,oo,oo,o_,
    _o,o_,__,_o,o_,
    _o,o_,__,_o,o_,
    _o,oo,oo,oo,o_,
    __,oo,oo,oo,o_,
    __,__,__,_o,o_,
    __,__,__,oo,o_,
    _o,oo,oo,oo,__,
    _o,oo,oo,__,__,

    __,__,__,__,__,
    __,__,__,__,__,
    __,__,__,__,__,
    __,__,__,__,__,
    __,__,__,__,__,
    _o,oo,oo,oo,__,
    _o,oo,oo,oo,__,
    __,__,__,__,__,
    __,__,__,__,__,
    __,__,__,__,__,

    oo,oo,oo,oo,__,
    oo,oo,oo,oo,o_,
    _o,o_,__,_o,o_,
    _o,o_,__,_o,o_,
    _o,oo,oo,oo,__,
    _o,oo,oo,oo,o_,
    _o,o_,__,_o,o_,
    _o,o_,__,_o,o_,
    oo,oo,oo,oo,o_,
    oo,oo,oo,oo,__,

    _o,o_,__,__,__,
    _o,o_,__,__,__,
    _o,o_,__,__,__,
    _o,o_,__,__,__,
    _o,o_,__,__,__,
    _o,o_,__,__,__,
    _o,o_,__,__,__,
    _o,o_,__,__,__,
    _o,oo,oo,oo,o_,
    _o,oo,oo,oo,o_,

    oo,oo,oo,o_,__,
    _o,oo,oo,oo,__,
    _o,o_,__,_o,o_,
    _o,o_,__,_o,o_,
    _o,oo,oo,oo,__,
    _o,oo,oo,o_,__,
    _o,o_,__,oo,__,
    _o,o_,__,oo,__,
    _o,o_,__,_o,o_,
    _o,o_,__,__,oo,

    __,__,__,_o,o_,
    __,__,__,_o,o_,
    __,__,__,_o,o_,
    __,__,__,_o,o_,
    __,_o,oo,oo,o_,
    _o,oo,oo,oo,o_,
    _o,o_,__,_o,o_,
    _o,o_,__,_o,o_,
    _o,oo,oo,oo,o_,
    __,oo,oo,oo,o_
  };
  const uint8_t *z = (const uint8_t *)s;
  uint32_t down = frame->pitches[0];
  uint32_t *p = (uint32_t *)(frame->base[0] + down * y + ((x >> 1) << 2));
  down /= 4;
  while (*z) {
    int n = map[*z++];
    if (n != 255) {
      uint32_t *q = p;
      const yuy2_color_t *g = font + 5 * 10 * n;
      int i;
      for (i = 10; i; i--) {
        q[0] = g[0].word;
        q[1] = g[1].word;
        q[2] = g[2].word;
        q[3] = g[3].word;
        q[4] = g[4].word;
        g += 5;
        q += down;
      }
    }
    p += 5;
  }
}

static void tdaan_draw_line (vo_frame_t *frame, int x1, int y1, int x2, int y2, uint32_t gray) {
  int w = x2 - x1;
  int h = y2 - y1;
  /* horizontal line */
  if (h == 0) {
    uint8_t *q = frame->base[0] + frame->pitches[0] * y1;
    if (w < 0) {
      w = -w;
      q += x2 * 2;
    } else {
      q += x1 * 2;
    }
    while (w) {
      *q = gray;
      q += 2;
      w--;
    }
    return;
  }
  /* vertical line */
  if (w == 0) {
    uint8_t *q = frame->base[0] + x1 * 2;
    size_t step = frame->pitches[0];
    if (h < 0) {
      h = -h;
      q += step * y2;
    } else {
      q += step * y1;
    }
    while (h) {
      *q = gray;
      q += step;
      h--;
    }
    return;
  }
  /* tilted */
  {
    uint8_t *q = frame->base[0];
    long int stepy = frame->pitches[0];
    /* always render downward */
    if (h < 0) {
      q += stepy * y2 + x2 * 2;
      w = -w;
      h = -h;
    } else {
      q += stepy * y1 + x1 * 2;
    }
    /* right to left */
    if (w < 0) {
      w = -w;
      if (w >= h) {
        /* flat */
        int d = w, n = w;
        while (n) {
          *q = gray;
          d -= h;
          if (d <= 0) {
            d += w;
            q += stepy;
          }
          q -= 2;
          n--;
        }
      } else {
        /* steep */
        int d = h, n = h;
        while (n) {
          *q = gray;
          d -= w;
          if (d <= 0) {
            d += h;
            q -= 2;
          }
          q += stepy;
          n--;
        }
      }
      return;
    }
    /* left to right */
    if (w >= h) {
      /* flat */
      int d = w, n = w;
      while (n) {
        *q = gray;
        d -= h;
        if (d <= 0) {
          d += w;
          q += stepy;
        }
        q += 2;
        n--;
      }
    } else {
      /* steep */
      int d = h, n = h;
      while (n) {
        *q = gray;
        d -= w;
        if (d <= 0) {
          d += h;
          q += 2;
        }
        q += stepy;
        n--;
      }
    }
  }
}

static void tdaan_draw_rect (vo_frame_t *frame, int x, int y, int width, int height, uint32_t color) {
  uint32_t *q;

  if ((width <= 0) || (height <= 0)) return;
  if (x + width > frame->width) return;
  if (y + height > frame->height) return;

  x += 1;
  x &= ~1;
  width += 1;
  width &= ~1;
  q = (uint32_t *)(frame->base[0] + y * frame->pitches[0] + x * 2);

  {
    size_t rest = (frame->pitches[0] - width * 2) / 4;
    int ny = height;
    while (ny) {
      int nx = width;
      while (nx) {
        *q++ = color;
        nx -= 2;
      }
      q += rest;
      ny--;
    }
  }
}

/**************************************************************************
 * logarithmic level gauge with rms, peak and peak hold (0..-64dB)        *
 *************************************************************************/

static void tdaan_levels_draw (post_plugin_tdaan_t *this, vo_frame_t *frame) {
  int top, bott;
  /* 6dB markers */
  {
    const char *s = "  0\0 -6\0-12\0-18\0-24\0-30\0-36\0-42\0-48\0-54\0-60\0";
    int x = this->lbar.x - this->lbar.width / 4, w = this->lbar.width * 6 / 4, i;
    for (i = 0; i < 64; i += 6) {
      top = this->lbar.y + ((i * this->lbar.height) >> 6);
      tdaan_draw_text (frame, x - 34, top - 5, s + (i * 4 / 6));
      tdaan_draw_rect (frame, x, top, w, 1, tdaan_BLACK.word);
    }
    tdaan_draw_text (frame, x - 34, this->lbar.y + this->lbar.height - 5, " dB");
  }
  /* bars */
  bott = this->lbar.y + this->lbar.height;
  /* rms */
  top = (this->lbar.rms + this->rbar.rms) >> 1;
  top = (-top * this->lbar.height) >> 6;
  top += this->lbar.y;
  if (top < bott) {
    tdaan_draw_rect (frame, this->lbar.x, top, this->lbar.width, bott - top, tdaan_YELLOW.word);
    bott = top;
  }
  /* peak mid */
  top = this->lbar.peak < this->rbar.peak ? this->lbar.peak : this->rbar.peak;
  top = (-top * this->lbar.height) >> 6;
  top += this->lbar.y;
  if (top < bott) {
    tdaan_draw_rect (frame, this->lbar.x, top, this->lbar.width, bott - top, tdaan_DARK_YELLOW.word);
    bott = top;
  }
  /* peak side */
  if (this->lbar.peak < this->rbar.peak) {
    top = (-this->rbar.peak * this->lbar.height) >> 6;
    top += this->lbar.y;
    tdaan_draw_rect (frame, this->lbar.x, top, this->lbar.width, bott - top, tdaan_DARK_RED.word);
  } else if (this->lbar.peak > this->rbar.peak) {
    top = (-this->lbar.peak * this->lbar.height) >> 6;
    top += this->lbar.y;
    tdaan_draw_rect (frame, this->lbar.x, top, this->lbar.width, bott - top, tdaan_DARK_GREEN.word);
  }
  /* hold */
  if (this->lbar.peak >= this->lbar.hold) {
    this->lbar.hold = this->lbar.peak;
    this->lbar.age  = this->video_fps; /* hold 1s */
  }
  if (this->rbar.peak >= this->rbar.hold) {
    this->rbar.hold = this->rbar.peak;
    this->rbar.age  = this->video_fps;
  }
  if (this->lbar.age && this->rbar.age && (this->lbar.hold == this->rbar.hold)) {
    top = (-this->rbar.hold * this->lbar.height) >> 6;
    top += this->lbar.y;
    tdaan_draw_rect (frame, this->lbar.x, top, this->lbar.width, 2, tdaan_YELLOW.word);
    this->lbar.age--;
    if (!this->lbar.age)
      this->lbar.hold = -64;
    this->rbar.age--;
    if (!this->rbar.age)
      this->rbar.hold = -64;
  } else {
    if (this->lbar.age) {
      top = (-this->lbar.hold * this->lbar.height) >> 6;
      top += this->lbar.y;
      tdaan_draw_rect (frame, this->lbar.x, top, this->lbar.width, 2, tdaan_GREEN.word);
      this->lbar.age--;
      if (!this->lbar.age)
        this->lbar.hold = -64;
    }
    if (this->rbar.age) {
      top = (-this->rbar.hold * this->lbar.height) >> 6;
      top += this->lbar.y;
      tdaan_draw_rect (frame, this->lbar.x, top, this->lbar.width, 2, tdaan_RED.word);
      this->rbar.age--;
      if (!this->rbar.age)
        this->rbar.hold = -64;
    }
  }
}

static int tdaan_int16todb (int s) {
  /* 20 * log10 (s / 32678) */
  static const int t[64] = {
    32767, 29204, 26028, 23197, 20675, 18426, 16422, 14636, 13045, 11626,
    10362,  9235,  8231,  7335,  6538,  5827,  5193,  4628,  4125,  3676,
     3277,  2920,  2603,  2320,  2062,  1842,  1682,  1463,  1304,  1162,
     1031,   923,   823,   733,   654,   582,   518,   462,   412,   362,
      327,   292,   260,   232,   206,   184,   164,   146,   130,   116,
      103,    92,    82,    73,    65,    58,    52,    46,    41,    37,
       33,    29,    26,    23
  };
  int a = 0, l, m = 0, e = 64;
  do {
    l = m;
    m = (a + e) >> 1;
    if (s < t[m])
      a = m;
    else
      e = m;
  } while (l != m);
  return -m;
}

static int tdaan_int32todb (uint32_t s) {
  /* 10 * log10 (s / (32678 * 32678)) */
  static const uint32_t t[64] = {
    1073676289, 852873616, 677456784, 538100809, 427455625,
     339517476, 269682084, 214212496, 170172025, 135163876,
     107371044,  85285225,  67749361,  53802225,  42745444,
      33953929,  26967249,  21418384,  17015625,  13512976,
      10738729,   8526400,   6775609,   5382400,   4251844,
       3392964,   2829124,   2140369,   1700416,   1350244,
       1062961,    851929,    677329,    537289,    427716,
        338724,    268324,    213444,    169744,    131044,
        106929,     85264,     67600,     53824,     42436,
         33856,     26896,     21316,     16900,     13456,
         10609,      8464,      6724,      5329,      4225,
          3364,      2704,      2116,      1681,      1369,
          1089,       841,       676,       529
  };
  int a = 0, l, m = 0, e = 64;
  do {
    l = m;
    m = (a + e) >> 1;
    if (s < t[m])
      a = m;
    else
      e = m;
  } while (l != m);
  return -m;
}

static void tdaan_levels_reset (post_plugin_tdaan_t *this) {
  this->lbar.squaresum = 0;
  this->rbar.squaresum = 0;
  this->lbar.peak = 0;
  this->rbar.peak = 0;
}

static void tdaan_levels_get (tdaan_leveller_t *v, const int16_t *data, int len) {
  uint64_t s = v->squaresum;
  int p = v->peak;
  const int16_t *q = data;
  if (len) {
    int n = len;
    do {
      uint32_t u;
      int a = *q;
      q += 2;
      if (a < 0)
        a = -a;
      if (a > p)
        p = a;
      u = a * a;
      s += u;
      n--;
    } while (n);
    v->squaresum = s;
    v->peak = p;
  }
}

static uint32_t tdaan_divu_quad_by_short (uint64_t num, uint32_t den) {
  uint32_t hi, lo, r;
  hi  = num >> 16;
  r   = hi / den;
  hi  = hi % den;
  r <<= 16;
  lo  = num & 0xffff;
  lo |= hi << 16;
  r  |= lo / den;
  return r;
}

static void tdaan_levels_done (post_plugin_tdaan_t *this) {
  int amax;
  amax = this->lbar.peak;
  if (this->rbar.peak > amax)
    amax = this->rbar.peak;
  this->amax = amax;
  this->lbar.peak = tdaan_int16todb (this->lbar.peak);
  this->rbar.peak = tdaan_int16todb (this->rbar.peak);
  this->lbar.rms = tdaan_int32todb (tdaan_divu_quad_by_short (this->lbar.squaresum, this->samples_per_frame));
  this->rbar.rms = tdaan_int32todb (tdaan_divu_quad_by_short (this->rbar.squaresum, this->samples_per_frame));
}

/**************************************************************************
 * Normalized Lissajous stereo phase diagram                              *
 *************************************************************************/

static void tdaan_phaser_start (post_plugin_tdaan_t *this, vo_frame_t *frame) {
  /* background X */
  tdaan_draw_line (frame, this->phaser_x, this->phaser_y,
    this->phaser_x + this->phaser_width, this->phaser_y + this->phaser_height, tdaan_BLACK.bytes[0]);
  tdaan_draw_line (frame, this->phaser_x + this->phaser_width, this->phaser_y,
    this->phaser_x, this->phaser_y + this->phaser_height, tdaan_BLACK.bytes[0]);
  tdaan_draw_text (frame, this->phaser_x, this->phaser_y + 12, "L");
  tdaan_draw_text (frame, this->phaser_x + this->phaser_width - 10, this->phaser_y + 12, "R");
}

static void tdaan_phaser_draw (post_plugin_tdaan_t *this, vo_frame_t *frame,
  const uint16_t *data, int len, uint32_t gray) {
  int lx = this->phaser_last_x;
  int ly = this->phaser_last_y;
  int mx = this->phaser_x + (this->phaser_width  >> 1);
  int my = this->phaser_y + (this->phaser_height >> 1);
  int sx, sy;
  const int16_t *p = data;
  /* size */
  {
    int amax = this->amax;
    if (amax < 200)
      amax = 200;
    sx = (this->phaser_width  << 19) / amax;
    sy = (this->phaser_height << 19) / amax;
  }
  /* resume */
  if (!lx || !ly) {
    int al = *p++;
    int ar = *p++;
    lx = mx - (((al - ar) * sx) >> 21);
    ly = my - (((al + ar) * sy) >> 21);
    len--;
  }
  /* main */
  while (len > 0) {
    int x, y;
    {
      int al = *p++;
      int ar = *p++;
      x = mx - (((al - ar) * sx) >> 21);
      y = my - (((al + ar) * sy) >> 21);
    }
    tdaan_draw_line (frame, lx, ly, x, y, gray);
    lx = x;
    ly = y;
    len--;
  }
  this->phaser_last_x = lx;
  this->phaser_last_y = ly;
}

/**************************************************************************
 * downmixing (display use only, no high end quality needed)              *
 * 0.75 * (center + lfe) to both sides                                    *
 * 0.5 * rear to same side                                                *
 * finally, 0.75 * result as saturation headroom                          *
 *************************************************************************/

#define sat16(v) (((v) + 0x8000) & ~0xffff ? ((v) >> 31) ^ 0x7fff : v)

static void tdaan_downmix16_4 (const int16_t *p, int16_t *q, int n) {
  /* L R RL RR */
  while (n--) {
    int32_t v;
    v = (int32_t)p[0] * 6 + (int32_t)p[2] * 3;
    v >>= 3;
    *q++ = sat16 (v);
    v = (int32_t)p[1] * 6 + (int32_t)p[3] * 3;
    v >>= 3;
    *q++ = sat16 (v);
    p += 4;
  }
}

static void tdaan_downmix16_6 (const int16_t *p, int16_t *q, int n) {
  /* L R RL RR C LFE */
  while (n--) {
    int32_t l = ((int32_t)p[4] + (int32_t)p[5]) * 9, r = l;
    l += (int32_t)p[0] * 12 + (int32_t)p[2] * 6;
    l >>= 4;
    *q++ = sat16 (l);
    r += (int32_t)p[1] * 12 + (int32_t)p[3] * 6;
    r >>= 4;
    *q++ = sat16 (r);
    p += 6;
  }
}

/**************************************************************************
 * xine video post plugin functions                                       *
 *************************************************************************/

static int tdaan_rewire_video (xine_post_out_t *output_gen, void *data) {
  post_out_t *output = (post_out_t *)output_gen;
  xine_video_port_t *old_port = *(xine_video_port_t **)output_gen->data;
  xine_video_port_t *new_port = (xine_video_port_t *)data;
  post_plugin_tdaan_t *this = (post_plugin_tdaan_t *)output->post;

  if (!data)
    return 0;
  /* register our stream at the new output port */
  old_port->close (old_port, XINE_ANON_STREAM);
  new_port->open (new_port, XINE_ANON_STREAM);
  /* reconnect ourselves */
  this->vo_port = new_port;
  return 1;
}

static int tdaan_port_open (
  xine_audio_port_t *port_gen, xine_stream_t *stream, uint32_t bits, uint32_t rate, int mode) {

  post_audio_port_t *port = (post_audio_port_t *)port_gen;
  post_plugin_tdaan_t *this = (post_plugin_tdaan_t *)port->post;

  _x_post_rewire (&this->post);
  _x_post_inc_usage (port);

  port->stream = stream;
  port->bits   = bits;
  port->rate   = rate;
  port->mode   = mode;

  this->video_width  = 640;
  this->video_height = 480;
  this->video_ratio  = (double)this->video_width / (double)this->video_height;
  this->video_fps    = 20;
  tdaan_video_resize (this);

  this->channels          = _x_ao_mode2channels (mode);
  this->samples_per_frame = rate / this->video_fps;

  this->vo_port->open (this->vo_port, XINE_ANON_STREAM);
  this->metronom->set_master (this->metronom, stream->metronom);

  return port->original_port->open (port->original_port, stream, bits, rate, mode);
}

static void tdaan_port_close (xine_audio_port_t *port_gen, xine_stream_t *stream) {
  post_audio_port_t *port = (post_audio_port_t *)port_gen;
  post_plugin_tdaan_t *this = (post_plugin_tdaan_t *)port->post;

  port->stream = NULL;
  this->vo_port->close (this->vo_port, XINE_ANON_STREAM);
  this->metronom->set_master (this->metronom, NULL);
  port->original_port->close (port->original_port, stream);
  _x_post_dec_usage (port);
}

static void tdaan_port_put_buffer (
  xine_audio_port_t *port_gen, audio_buffer_t *buf, xine_stream_t *stream) {

  post_audio_port_t *port = (post_audio_port_t *)port_gen;
  post_plugin_tdaan_t *this = (post_plugin_tdaan_t *)port->post;

  int64_t pts = buf->vpts;
  /* let video pts refer to the midle of the audio it is showing */
  if (pts) {
    int offs = (this->ring_put - this->ring_get) & RING_MASK;
    offs    -= this->samples_per_frame >> 1;
    pts     -= 90000 * offs / (int)port->rate;
  }
  /* buffer incoming audio */
  do {
    int16_t *put = this->ringbuf + this->ring_put * 2;
    int n1 = buf->num_frames;
    int n2 = this->ring_put + n1;
    if (n1 <= 0)
      break;
    if (n2 & ~RING_MASK) {
      n1 -= n2 - RING_SIZE;
      n2 &= RING_MASK;
      this->ring_put = n2;
    } else {
      this->ring_put = n2;
      n2 = 0;
    }

    if (port->bits == 8) {
      const uint8_t *in = (uint8_t *)buf->mem;
      if (this->channels == 1) {
        do {
          put[0] = put[1] = ((int)(*in++) << 8) ^ 0x8000;
          put += 2;
          n1--;
        } while (n1);
        put = this->ringbuf;
        while (n2) {
          put[0] = put[1] = ((int)(*in++) << 8) ^ 0x8000;
          put += 2;
          n2--;
        }
      } else if (this->channels == 2) {
        do {
          *put++ = ((int)(*in++) << 8) ^ 0x8000;
          *put++ = ((int)(*in++) << 8) ^ 0x8000;
          n1--;
        } while (n1);
        put = this->ringbuf;
        while (n2) {
          *put++ = ((int)(*in++) << 8) ^ 0x8000;
          *put++ = ((int)(*in++) << 8) ^ 0x8000;
          n2--;
        }
      }
    } else if (port->bits == 16) {
      const int16_t *in = (int16_t *)buf->mem;
      if (this->channels == 1) {
        do {
          put[0] = put[1] = *in++;
          put += 2;
          n1--;
        } while (n1);
        put = this->ringbuf;
        while (n2) {
          put[0] = put[1] = *in++;
          put += 2;
          n2--;
        }
      } else if (this->channels == 2) {
        memcpy (put, in, n1 * 4);
        if (n2) {
          in += n1 * 2;
          put = this->ringbuf;
          memcpy (put, in, n2 * 4);
        }
      } else if (this->channels <= 4) {
        tdaan_downmix16_4 (in, put, n1);
        if (n2) {
          in += n1 * 4;
          put = this->ringbuf;
          tdaan_downmix16_4 (in, put, n2);
        }
      } else if (this->channels <= 6) {
        tdaan_downmix16_6 (in, put, n1);
        if (n2) {
          in += n1 * 6;
          put = this->ringbuf;
          tdaan_downmix16_6 (in, put, n2);
        }
      }
    }
  } while (0);
  /* pass data to original port */
  port->original_port->put_buffer (port->original_port, buf, stream);
  /* output some gfx */
  while (((this->ring_put - this->ring_get) & RING_MASK) >= this->samples_per_frame) {
    vo_frame_t *frame;
    const int16_t *get = this->ringbuf + this->ring_get * 2;
    int oldamax = this->amax;
    int n1 = this->samples_per_frame;
    int p2 = this->ring_get;
    int n2 = p2 + n1;
    if (n2 & ~RING_MASK) {
      n2 &= RING_MASK;
      this->ring_get = n2;
      n1 -= n2;
    } else {
      this->ring_get = n2;
      n2 = 0;
    }
    /* gather level stats */
    tdaan_levels_reset (this);
    tdaan_levels_get (&this->lbar, get, n1);
    tdaan_levels_get (&this->rbar, get + 1, n1);
    if (n2) {
      tdaan_levels_get (&this->lbar, this->ringbuf, n2);
      tdaan_levels_get (&this->rbar, this->ringbuf + 1, n2);
    }
    tdaan_levels_done (this);
    /* get an output frame */
    frame = this->vo_port->get_frame (this->vo_port, this->video_width, this->video_height,
      this->video_ratio, XINE_IMGFMT_YUY2, VO_BOTH_FIELDS);
    frame->extra_info->invalid = 1;
    frame->duration = 90000 * this->samples_per_frame / port->rate;
    frame->pts = pts;
    if (pts)
      pts += frame->duration;
    this->metronom->got_video_frame (this->metronom, frame);
    tdaan_draw_rect (frame, 0, 0, frame->width, frame->height, tdaan_GREY.word);
    /* draw levels */
    tdaan_levels_draw (this, frame);
    /* draw phaser */
    tdaan_phaser_start (this, frame);
    /* repeat last half of previous frame in half bright. */
    /* this old CRT style fadeout makes more pleasant viewing. */
    {
      int newamax = this->amax, p1;
      this->phaser_last_x = 0;
      this->phaser_last_y = 0;
      this->amax = oldamax;
      p1 = this->samples_per_frame >> 1;
      p2 -= p1;
      if (p2 < 0) {
        p1 += p2;
        tdaan_phaser_draw (this, frame, this->ringbuf + (RING_SIZE + p2) * 2, -p2, tdaan_LIGHT_GRAY.bytes[0]);
      }
      if (p1)
        tdaan_phaser_draw (this, frame, get - p1 * 2, p1, tdaan_LIGHT_GRAY.bytes[0]);
      this->amax = newamax;
    }
    /* now, this frame */
    tdaan_phaser_draw (this, frame, get, n1, tdaan_WHITE.bytes[0]);
    if (n2)
      tdaan_phaser_draw (this, frame, this->ringbuf, n2, tdaan_WHITE.bytes[0]);
    /* pass to video out */
    frame->draw (frame, XINE_ANON_STREAM);
    frame->free (frame);
  }
}

static void tdaan_dispose (post_plugin_t *this_gen) {
  post_plugin_tdaan_t *this = (post_plugin_tdaan_t *)this_gen;

  if (_x_post_dispose (this_gen)) {
    this->metronom->exit (this->metronom);
    free (this);
  }
}

/* plugin class functions */
static post_plugin_t *tdaan_open_plugin (
  post_class_t *class_gen, int inputs, xine_audio_port_t **audio_target, xine_video_port_t **video_target) {

  post_class_tdaan_t   *class = (post_class_tdaan_t *)class_gen;
  post_plugin_tdaan_t  *this  = calloc (1, sizeof (post_plugin_tdaan_t));
  post_in_t          *input;
  post_out_t         *output;
  post_out_t         *outputv;
  post_audio_port_t  *port;

  if (!this || !video_target || !video_target[0] || !audio_target || !audio_target[0] ) {
    free(this);
    return NULL;
  }

  _x_post_init (&this->post, 1, 0);

  this->metronom = _x_metronom_init (1, 0, class->xine);

  this->vo_port = video_target[0];

  port = _x_post_intercept_audio_port (&this->post, audio_target[0], &input, &output);
  port->new_port.open       = tdaan_port_open;
  port->new_port.close      = tdaan_port_close;
  port->new_port.put_buffer = tdaan_port_put_buffer;

  outputv                  = &this->video_output;
  outputv->xine_out.name   = "tdaan generated video";
  outputv->xine_out.type   = XINE_POST_DATA_VIDEO;
  outputv->xine_out.data   = (xine_video_port_t **)&this->vo_port;
  outputv->xine_out.rewire = tdaan_rewire_video;
  outputv->post            = &this->post;
  xine_list_push_back (this->post.output, outputv);

  this->post.xine_post.audio_input[0] = &port->new_port;

  this->post.dispose = tdaan_dispose;

  return &this->post;
}

/* plugin class initialization function */
void *tdaan_init_plugin (xine_t *xine, void *data) {

  post_class_tdaan_t *class = (post_class_tdaan_t *)xine_xmalloc (sizeof (post_class_tdaan_t));

  if (!class)
    return NULL;

  class->post_class.open_plugin     = tdaan_open_plugin;
  class->post_class.identifier      = "tdaudioanalyzer";
  class->post_class.description     = N_("Time Domain Audio Analyzer Visualisation");
  class->post_class.dispose         = default_post_class_dispose;

  class->xine                       = xine;

  return class;
}



