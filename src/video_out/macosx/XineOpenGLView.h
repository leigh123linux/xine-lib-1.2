/*
 * Copyright (C) 2004 the xine project
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
 */

#ifndef __HAVE_XINE_OPENGL_VIEW_H__
#define __HAVE_XINE_OPENGL_VIEW_H__

#import <Cocoa/Cocoa.h>

#import "XineVideoWindow.h"

@protocol XineOpenGLViewDelegate;

extern NSString *XineViewDidResizeNotification;

@interface XineOpenGLView : NSOpenGLView
{
    IBOutlet id <NSObject, XineOpenGLViewDelegate>  delegate;
    int                            video_width, video_height;
    char                          *texture_buffer;
    unsigned long                  i_texture;
    BOOL                           initDone;
    BOOL                           isFullScreen;
    XineVideoWindowFullScreenMode  fullscreen_mode;
    NSOpenGLContext               *fullScreenContext;
    NSOpenGLContext               *currentContext;
    NSLock                        *mutex;
    BOOL                           keepsVideoAspectRatio;
    BOOL                           resizeViewOnVideoSizeChange;
    NSCursor                      *currentCursor;
    id <NSObject, XineOpenGLViewDelegate>  _xineController;
    NSColor                       *initialColor;
    unsigned int                   initialColorYUV;
    BOOL                           initialColorYUVIsSet;
}

- (void) displayTexture;
- (void) drawQuad;
- (void) drawRect: (NSRect) rect;
- (void) goFullScreen: (XineVideoWindowFullScreenMode) mode;
- (void) exitFullScreen;
- (BOOL) isFullScreen;
- (void) reshape;
- (void) initTextures;
- (void) reloadTexture;
- (char *) getTextureBuffer;
- (void) setViewSizeInMainThread:(NSSize)size;
// TODO: replace set...Size below with setSize:(double)videoSizeMultiplier
- (void) setNormalSize;
- (void) setHalfSize;
- (void) setDoubleSize;
- (void) setResizeViewOnVideoSizeChange:(BOOL)flag;
- (BOOL) resizeViewOnVideoSizeChange;
- (void) resetCursorRectsInMainThread;

// Accessors
- (void) setVideoSize:(NSSize)size;
- (NSSize) videoSize;
- (void) setKeepsVideoAspectRatio:(BOOL)flag;
- (BOOL) keepsVideoAspectRatio;
- (void) setCurrentCursor:(NSCursor *)cursor;
- (NSCursor *) currentCursor;
- (void) setXineController:(id)controller;
- (id) xineController;
- (void) setInitialColor:(NSColor *)color;
- (NSColor *) initialColor;

// Delegate Methods
- (id) delegate;
- (void) setDelegate:(id)aDelegate;

@end

/* XineOpenGLView delegate methods */

@interface NSObject (XineOpenGLViewDelegate)

- (NSSize) xineViewWillResize:(NSSize)oldSize toSize:(NSSize)proposedSize;
- (void) xineViewDidResize:(NSNotification *)aNotification;
- (void) mouseDown:(NSEvent *)theEvent inXineView:(XineOpenGLView *)theView;
- (void) mouseMoved:(NSEvent *)theEvent inXineView:(XineOpenGLView *)theView;
- (void) otherMouseDown:(NSEvent *)theEvent
             inXineView:(XineOpenGLView *)theView;
- (void) rightMouseDown:(NSEvent *)theEvent
             inXineView:(XineOpenGLView *)theView;

@end

#endif /* __HAVE_XINE_OPENGL_VIEW_H__ */

