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
 * Written by Daniel Mack <xine@zonque.org>
 * 
 * Most parts of this code were taken from VLC, http://www.videolan.org
 * Thanks for the good research!
 */

#include <OpenGL/OpenGL.h>
#include <OpenGL/gl.h>
#include <OpenGL/glext.h>

#import "video_window.h"

NSString *XineViewDidResizeNotification = @"XineViewDidResizeNotification";

@implementation XineVideoWindow


- (void) setContentSize: (NSSize) size {
    [xineView setViewSizeInMainThread:size];
   
    [super setContentSize: size];
}

- (id) initWithContentRect: (NSRect)rect 
       styleMask:(unsigned int)styleMask 
       backing:(NSBackingStoreType)bufferingType 
       defer:(BOOL)flag 
       screen:(NSScreen *)aScreen {
    self = [super initWithContentRect: rect
                      styleMask: styleMask
                      backing: bufferingType
                      defer: flag
                      screen: aScreen];

    xineView = [[XineOpenGLView alloc] initWithFrame:rect];

    /* receive notifications about window resizing from the xine view */
    [xineView setDelegate:self];

    [self setContentView: xineView];
    [self setTitle: @"xine video output"];
    keepAspectRatio = NO;

    return self;
}

- (XineOpenGLView *) xineView {
    return xineView;
}

- (void) fitToScreen {
    NSSize size, video_size;
    float screen_width, screen_height;
    
    if ([xineView isFullScreen])
        return;
    
    screen_width = CGDisplayPixelsWide (kCGDirectMainDisplay);
    screen_height = CGDisplayPixelsHigh (kCGDirectMainDisplay) - 40;

    video_size = [xineView videoSize];

    if ((screen_width / screen_height) > (video_size.width / video_size.height)) {
        size.width = video_size.width * (screen_height / video_size.height);
        size.height = screen_height;
    } else {
        size.width = screen_width;
        size.height = video_size.height * (screen_width / video_size.width);
    }

    [super setContentSize: size];
}

- (void) setKeepsAspectRatio: (BOOL) flag {
    if (flag) {
        NSSize size = [self frame].size;
        [self setAspectRatio:size];
    }
    else {
        [self setResizeIncrements:NSMakeSize(1.0, 1.0)];
    }

    keepAspectRatio = flag;
}

- (int) keepsAspectRatio {
    return keepAspectRatio;
}

/* Delegate methods */

- (void) xineViewDidResize:(NSNotification *)note {
  NSRect frame = [self frame];
  frame.size = [[self contentView] frame].size;

  [self setFrame:[self frameRectForContentRect:frame] display:YES];
}

@end


@implementation XineOpenGLView

- (BOOL)mouseDownCanMoveWindow {
    return YES;
}

- (NSSize)videoSize {
    return NSMakeSize(video_width, video_height);
}

- (void) displayTexture {
    if ([self lockFocusIfCanDraw]) {
    [self drawRect: [self bounds]];
    [self reloadTexture];
    [self unlockFocus];
    }
}

- (id) initWithFrame: (NSRect) frame {

    NSOpenGLPixelFormatAttribute attribs[] = {
        NSOpenGLPFAAccelerated,
        NSOpenGLPFANoRecovery,
        NSOpenGLPFADoubleBuffer,
        NSOpenGLPFAColorSize, 24,
        NSOpenGLPFAAlphaSize, 8,
        NSOpenGLPFADepthSize, 24,
        NSOpenGLPFAWindow,
        0
    };

    NSOpenGLPixelFormat * fmt = [[NSOpenGLPixelFormat alloc]
        initWithAttributes: attribs];

    if (!fmt) {
        printf  ("Cannot create NSOpenGLPixelFormat\n");
        return nil;
    }

    self = [super initWithFrame:frame pixelFormat:fmt];

    currentContext = [self openGLContext];
    [currentContext makeCurrentContext];
    [currentContext update];

    /* Black background */
    glClearColor (0.0, 0.0, 0.0, 0.0);
    
    i_texture      = 0;
    initDone       = NO;
    isFullScreen   = NO;
    video_width    = frame.size.width;
    video_height   = frame.size.height;
    texture_buffer = nil;

    [self initTextures];

    return self;
}

- (void) reshape {
    if (!initDone)
        return;
   
    [currentContext makeCurrentContext];

    NSRect bounds = [self bounds];
    glViewport (0, 0, bounds.size.width, bounds.size.height);
}

- (void) setNormalSize {
    NSSize size;
    
    if (isFullScreen)
        return;
    
    size.width = video_width;
    size.height = video_height;

    [self setViewSizeInMainThread:size];
}

- (void) setHalfSize {
    NSSize size;
    
    if (isFullScreen)
        return;
    
    size.width = video_width / 2;
    size.height = video_height / 2;

    [self setViewSizeInMainThread:size];
}

- (void) setDoubleSize {
    NSSize size;
    
    if (isFullScreen)
        return;
    
    size.width = video_width * 2;
    size.height = video_height * 2;

    [self setViewSizeInMainThread:size];
}


- (void) initTextures {
    [currentContext makeCurrentContext];

    /* Free previous texture if any */
    if (i_texture)
        glDeleteTextures (1, &i_texture);

    if (texture_buffer)
        texture_buffer = realloc (texture_buffer, sizeof (char) *
                                  video_width * video_height * 2);
    else
        texture_buffer = malloc (sizeof (char) *
                                 video_width * video_height * 2);

    /* Create textures */
    glGenTextures (1, &i_texture);

    glEnable (GL_TEXTURE_RECTANGLE_EXT);
    glEnable (GL_UNPACK_CLIENT_STORAGE_APPLE);

    glBindTexture (GL_TEXTURE_RECTANGLE_EXT, i_texture);

    /* Use VRAM texturing */
    glTexParameteri (GL_TEXTURE_RECTANGLE_EXT,
            GL_TEXTURE_STORAGE_HINT_APPLE, GL_STORAGE_CACHED_APPLE);

    /* Tell the driver not to make a copy of the texture but to use
       our buffer */
    glPixelStorei (GL_UNPACK_CLIENT_STORAGE_APPLE, GL_TRUE);

    /* Linear interpolation */
    glTexParameteri (GL_TEXTURE_RECTANGLE_EXT,
            GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri (GL_TEXTURE_RECTANGLE_EXT,
            GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    /* I have no idea what this exactly does, but it seems to be
       necessary for scaling */
    glTexParameteri (GL_TEXTURE_RECTANGLE_EXT,
            GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri (GL_TEXTURE_RECTANGLE_EXT,
            GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glPixelStorei (GL_UNPACK_ROW_LENGTH, 0);

    glTexImage2D (GL_TEXTURE_RECTANGLE_EXT, 0, GL_RGBA,
            video_width, video_height, 0,
            GL_YCBCR_422_APPLE, GL_UNSIGNED_SHORT_8_8_APPLE,
            texture_buffer);

    initDone = YES;
}

- (void) reloadTexture {
    if (!initDone)
        return;
    
    [currentContext makeCurrentContext];

    glBindTexture (GL_TEXTURE_RECTANGLE_EXT, i_texture);

    /* glTexSubImage2D is faster than glTexImage2D
     *  http://developer.apple.com/samplecode/Sample_Code/Graphics_3D/TextureRange/MainOpenGLView.m.htm
     */
    glTexSubImage2D (GL_TEXTURE_RECTANGLE_EXT, 0, 0, 0,
            video_width, video_height,
            GL_YCBCR_422_APPLE, GL_UNSIGNED_SHORT_8_8_APPLE,
            texture_buffer);
}

- (void) calcFullScreenAspect {
    int fs_width, fs_height, x, y, w, h;
   
    fs_width = CGDisplayPixelsWide (kCGDirectMainDisplay);
    fs_height = CGDisplayPixelsHigh (kCGDirectMainDisplay);

    switch (fullscreen_mode) {
    case XINE_FULLSCREEN_OVERSCAN:
        if (((float) fs_width / (float) fs_height) > ((float) video_width / (float) video_height)) {
            w = (float) video_width * ((float) fs_height / (float) video_height);
            h = fs_height;
            x = (fs_width - w) / 2;
            y = 0;
        } else {
            w = fs_width;
            h = (float) video_height * ((float) fs_width / (float) video_width);
            x = 0;
            y = (fs_height - h) / 2;
        }
        break;
    
    case XINE_FULLSCREEN_CROP:
        if (((float) fs_width / (float) fs_height) > ((float) video_width / (float) video_height)) {
            w = fs_width;
            h = (float) video_height * ((float) fs_width / (float) video_width);
            x = 0;
            y = (fs_height - h) / 2;
        } else {
            w = (float) video_width * ((float) fs_height / (float) video_height);
            h = fs_height;
            x = (fs_width - w) / 2;
            y = 0;
        }
        break;
    }

    printf ("MacOSX fullscreen mode: %dx%d => %dx%d @ %d,%d\n",
            video_width, video_height, w, h, x, y);

    glViewport (x, y, w, h);
}

- (void) goFullScreen: (XineVideoWindowFullScreenMode) mode {

    /* Create the new pixel format */
    NSOpenGLPixelFormatAttribute attribs[] = {
        NSOpenGLPFAAccelerated,
        NSOpenGLPFANoRecovery,
        NSOpenGLPFADoubleBuffer,
        NSOpenGLPFAColorSize, 24,
        NSOpenGLPFAAlphaSize, 8,
        NSOpenGLPFADepthSize, 24,
        NSOpenGLPFAFullScreen,
        NSOpenGLPFAScreenMask,
        CGDisplayIDToOpenGLDisplayMask (kCGDirectMainDisplay),
        0
    };
    
    NSOpenGLPixelFormat * fmt = [[NSOpenGLPixelFormat alloc]
        initWithAttributes: attribs];    
    if (!fmt) {
        printf ("Cannot create NSOpenGLPixelFormat\n");
        return;
    }

    /* Create the new OpenGL context */
    fullScreenContext = [[NSOpenGLContext alloc]
        initWithFormat: fmt shareContext: nil];
    if (!fullScreenContext) {
        printf ("Failed to create new NSOpenGLContext\n");
        return;
    }
    currentContext = fullScreenContext;

    /* Capture display, switch to fullscreen */
    if (CGCaptureAllDisplays() != CGDisplayNoErr) {
        printf ("CGCaptureAllDisplays() failed\n");
        return;
    }
    [fullScreenContext setFullScreen];
    [fullScreenContext makeCurrentContext];

    fullscreen_mode = mode;

    [self initTextures];
    [self calcFullScreenAspect];

    /* Redraw the last picture */
    [self setNeedsDisplay: YES];

    isFullScreen = YES;
}

- (void) exitFullScreen {
    initDone = NO;
    currentContext = [self openGLContext];
    
    /* Free current OpenGL context */
    [NSOpenGLContext clearCurrentContext];
    [fullScreenContext clearDrawable];
    [fullScreenContext release];
    CGReleaseAllDisplays();

    [self reshape];
    [self initTextures];

    /* Redraw the last picture */
    [self setNeedsDisplay: YES];

    isFullScreen = NO;
    initDone = YES;
}

- (void) drawQuad {
    float f_x = 1.0, f_y = 1.0;
    
    glBegin (GL_QUADS);
        /* Top left */
        glTexCoord2f (0.0, 0.0);
        glVertex2f (-f_x, f_y);
        /* Bottom left */
        glTexCoord2f (0.0, (float) video_height);
        glVertex2f (-f_x, -f_y);
        /* Bottom right */
        glTexCoord2f ((float) video_width, (float) video_height);
        glVertex2f (f_x, -f_y);
        /* Top right */
        glTexCoord2f ((float) video_width, 0.0);
        glVertex2f (f_x, f_y);
    glEnd();
}

- (void) drawRect: (NSRect) rect {
    [currentContext makeCurrentContext];

    /* Swap buffers only during the vertical retrace of the monitor.
       http://developer.apple.com/documentation/GraphicsImaging/Conceptual/OpenGL/chap5/chapter_5_section_44.html */

    long params[] = { 1 };
    CGLSetParameter (CGLGetCurrentContext(), kCGLCPSwapInterval, params);
  
    /* Black background */
    glClear (GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    if (!initDone) {
        [currentContext flushBuffer];
        return;
    }

    /* Draw */
    glBindTexture (GL_TEXTURE_RECTANGLE_EXT, i_texture);
    [self drawQuad];

    /* Wait for the job to be done */
    [currentContext flushBuffer];
}

- (char *) getTextureBuffer {
    return texture_buffer;
}

- (void) setVideoSize:(NSSize)size
{
    video_width = size.width;
    video_height = size.height;
}

- (void) setViewSizeInMainThread:(NSSize)size
{
    /* create an autorelease pool, since we're running in a xine thread that
     * may not have a pool of its own */
    NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];

    NSValue *sizeWrapper = [NSValue valueWithBytes:&size
                                          objCType:@encode(NSSize)];
                                  
    [self performSelectorOnMainThread:@selector(setViewSize:)
                           withObject:sizeWrapper
		        waitUntilDone:NO];

    [pool release];
}

- (void) setViewSize:(NSValue *)sizeWrapper
{
    NSSize proposedSize, newSize, currentSize;

    [sizeWrapper getValue:&proposedSize];
    newSize = proposedSize;

    currentSize = [self frame].size;
    if (proposedSize.width == currentSize.width &&
        proposedSize.height == currentSize.height) {
        return;
    }

    /* If our delegate handles xineViewWillResize:toSize:, send the
     * message to him; otherwise, just resize ourselves */
    if ([delegate respondsToSelector:@selector(xineViewWillResize:toSize:)]) {
        NSSize oldSize = [self frame].size;
        newSize = [delegate xineViewWillResize:oldSize toSize:proposedSize];
    }
    
    [self setFrameSize:newSize];
    [self setBoundsSize:newSize];

    /* Post a notification that we resized and also notify our delegate */
    NSNotification *note =
        [NSNotification notificationWithName:XineViewDidResizeNotification
                                      object:self];
    [[NSNotificationCenter defaultCenter] postNotification:note];
    if ([delegate respondsToSelector:@selector(xineViewDidResize:)]) { 
        [delegate xineViewDidResize:note];
    }

    if (isFullScreen)
        [self calcFullScreenAspect];

    [self initTextures];    
}

- (BOOL) isFullScreen {
    return isFullScreen;
}

- (id) delegate {
    return delegate;
}

- (void) setDelegate:(id)aDelegate {
    delegate = aDelegate;
}

@end
