#ifndef _GFONTLIB_H
#define _GFONTLIB_H

#include "goom_graphic.h"

typedef struct goomfont_s goomfont_t;

goomfont_t *gfont_load (void);
void gfont_unload (goomfont_t **);

void goom_draw_text (goomfont_t *, Pixel * buf, int resolx, int resoly, int x, int y,
                     const char *str, float chspace, int center);

#endif
