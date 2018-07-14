/*
 * Copyright (C) 2012-2018 the xine project
 * Copyright (C) 2012 Christophe Thommeret <hftom@free.fr>
 * Copyright (C) 2012-2018 Petri Hintukainen <phintuka@users.sourceforge.net>
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
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 *
 * video_out_opengl2.c, a video output plugin using opengl 2.0
 *
 *
 */

#include "xine_gl.h"

#include <stdlib.h>

#include <xine/xine_internal.h>

#include <X11/Xlib.h>
#include <GL/glx.h>

typedef struct {
  xine_gl_t   gl;

  xine_t     *xine;

  Display    *display;
  Drawable    drawable;
  GLXContext  context;

  int         lock1, lock2;

  /* DEBUG */
  int         is_current;
} xine_glx_t;


static int _glx_make_current(xine_gl_t *gl)
{
  xine_glx_t *glx = (xine_glx_t *)gl;
  int result;

  _x_assert(!glx->is_current);

  /* User may change lock1 anytime, make sure we pair lock calls properly. */
  glx->lock2 = glx->lock1;

  XLockDisplay(glx->display);
  result = glXMakeCurrent(glx->display, glx->drawable, glx->context);
  if (!glx->lock2)
    XUnlockDisplay (glx->display);

  if (!result) {
    if (glx->lock2)
      XUnlockDisplay (glx->display);
    xprintf(glx->xine, XINE_VERBOSITY_LOG, "glx: display unavailable for rendering\n");
    return 0;
  }

  glx->is_current = 1;
  return result;
}

static void _glx_release_current(xine_gl_t *gl)
{
  xine_glx_t *glx = (xine_glx_t *)gl;

  _x_assert(glx->is_current);

  if (!glx->lock2)
    XLockDisplay (glx->display);
  glXMakeCurrent(glx->display, None, NULL);
  XUnlockDisplay(glx->display);

  glx->is_current = 0;
}

static void _glx_swap_buffers(xine_gl_t *gl)
{
  xine_glx_t *glx = (xine_glx_t *)gl;

  XLockDisplay(glx->display);
  glXSwapBuffers(glx->display, glx->drawable);
  XUnlockDisplay(glx->display);
}

static void _glx_set_native_window(xine_gl_t *gl, void *drawable)
{
  xine_glx_t *glx = (xine_glx_t *)gl;

  _x_assert(!glx->is_current);

  XLockDisplay(glx->display);
  glx->drawable = (intptr_t)drawable;
  XUnlockDisplay(glx->display);
}

static void _glx_resize(xine_gl_t *gl, int w, int h)
{
  (void)gl;
  (void)w;
  (void)h;
}

static void _glx_dispose(xine_gl_t **pgl)
{
  xine_glx_t *glx = (xine_glx_t *)*pgl;

  if (!glx) {
    return;
  }

  lprintf("Destroying glx context %p\n", (void*)glx->context);

  glx->xine->config->unregister_callback (glx->xine->config, "video.output.lockdisplay");

  _x_assert(!glx->is_current);

  XLockDisplay(glx->display);
  if (glx->is_current) {
    glXMakeCurrent(glx->display, None, NULL);
  }
  glXDestroyContext(glx->display, glx->context);
  XUnlockDisplay(glx->display);

  _x_freep(pgl);
}

static void _glx_set_lockdisplay (void *this_gen, xine_cfg_entry_t *entry) {
  xine_glx_t *glx = (xine_glx_t *)this_gen;
  glx->lock1 = entry->num_value;
  xprintf (glx->xine, XINE_VERBOSITY_DEBUG, "glx: lockdisplay=%d\n", glx->lock1);
}

static xine_gl_t *_glx_init(xine_t *xine, const void *visual)
{
  const x11_visual_t *vis = visual;
  Window              root;
  XVisualInfo        *visinfo;
  GLXContext          ctx;
  int                 is_direct = 0;
  xine_glx_t         *glx;

  int attribs[] = {
    GLX_RGBA,
    GLX_DOUBLEBUFFER,
    GLX_RED_SIZE, 8,
    GLX_GREEN_SIZE, 8,
    GLX_BLUE_SIZE, 8,
    GLX_DEPTH_SIZE, 16,
    None,
  };

  _x_assert(vis);
  _x_assert(vis->display);

  XLockDisplay(vis->display);

  root = RootWindow(vis->display, vis->screen);
  if (!root) {
    goto fail_locked;
  }

  visinfo = glXChooseVisual(vis->display, vis->screen, attribs);
  if (!visinfo) {
    goto fail_locked;
  }

  ctx = glXCreateContext(vis->display, visinfo, NULL, GL_TRUE);
  XFree(visinfo);
  if (!ctx) {
    goto fail_locked;
  }

  if (!glXMakeCurrent(vis->display, vis->d, ctx)) {
    goto fail_created;
  }

  is_direct = glXIsDirect(vis->display, ctx);
  glXMakeCurrent(vis->display, None, NULL);
  if (!is_direct) {
    goto fail_created;
  }

  glx = calloc(1, sizeof(*glx));
  if (!glx) {
    goto fail_created;
  }

  XUnlockDisplay(vis->display);

  glx->gl.make_current      = _glx_make_current;
  glx->gl.release_current   = _glx_release_current;
  glx->gl.swap_buffers      = _glx_swap_buffers;
  glx->gl.resize            = _glx_resize;
  glx->gl.set_native_window = _glx_set_native_window;
  glx->gl.dispose           = _glx_dispose;

  glx->xine     = xine;
  glx->context  = ctx;
  glx->display  = vis->display;
  glx->drawable = vis->d;

  glx->lock1 = glx->lock2 = xine->config->register_bool (xine->config,
    "video.output.lockdisplay", 0,
    _("Lock X display during whole frame output."),
    _("This sometimes reduces system load and jitter.\n"),
      10, _glx_set_lockdisplay, glx);

  return &glx->gl;

 fail_created:
  glXDestroyContext( vis->display, ctx );
 fail_locked:
  XUnlockDisplay (vis->display);
  return NULL;
}
