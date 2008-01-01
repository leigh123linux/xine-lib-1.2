include $(top_srcdir)/misc/Makefile.common

AM_CPPFLAGS = -DXINE_COMPILE
AM_LDFLAGS = $(xineplug_ldflags)

SUBDIRS = libdha vidix macosx

EXTRA_DIST = video_out_directfb.c video_out_directfb_fb.c video_out_directfb_x.c video_out_directx.c video_out_macosx.m

VIDIX_CFLAGS = -I$(top_builddir)/src/video_out/vidix \
	-I$(top_srcdir)/src/video_out/vidix

if HAVE_X11
X11OSD = x11osd.c
xshm_module = xineplug_vo_out_xshm.la
if HAVE_XV
xv_module = xineplug_vo_out_xv.la
endif
if HAVE_XVMC
xvmc_module = xineplug_vo_out_xvmc.la
endif
if HAVE_XXMC
xxmc_module = xineplug_vo_out_xxmc.la
endif
if HAVE_OPENGL
opengl_module = xineplug_vo_out_opengl.la
endif
if HAVE_SYNCFB
syncfb_module = xineplug_vo_out_syncfb.la
endif
if HAVE_SUNFB
if HAVE_SUNDGA
pgx64_module = xineplug_vo_out_pgx64.la
pgx32_module = xineplug_vo_out_pgx32.la
endif
endif
endif

if HAVE_XCB
XCBOSD = xcbosd.c
if HAVE_XCBSHM
xcbshm_module = xineplug_vo_out_xcbshm.la
endif
if HAVE_XCBXV
xcbxv_module = xineplug_vo_out_xcbxv.la
endif
endif

if HAVE_VIDIX
vidix_module = xineplug_vo_out_vidix.la
endif

if HAVE_AA
aa_module = xineplug_vo_out_aa.la
endif

if HAVE_CACA
caca_module = xineplug_vo_out_caca.la
endif

if HAVE_FB
fb_module = xineplug_vo_out_fb.la
endif

if HAVE_DIRECTFB
directfb_module = xineplug_vo_out_directfb.la
if HAVE_X11
directfb_module += xineplug_vo_out_xdirectfb.la
endif
endif

if HAVE_SDL
sdl_module = xineplug_vo_out_sdl.la
endif

if HAVE_STK
stk_module = xineplug_vo_out_stk.la
endif

if HAVE_DIRECTX
directx_module = xineplug_vo_out_directx.la
endif

if HAVE_MACOSX_VIDEO
macosx_module = xineplug_vo_out_macosx.la
endif

xineplug_LTLIBRARIES = $(xshm_module) $(xv_module) $(xvmc_module) \
                  $(opengl_module) \
                  $(syncfb_module) \
                  $(pgx64_module) $(pgx32_module)\
	          $(vidix_module) \
	          $(aa_module) \
	          $(fb_module) $(directfb_module) \
	          $(sdl_module) \
                  $(stk_module) \
                  $(directx_module) \
                  $(caca_module) \
		  $(macosx_module) \
		  $(xxmc_module) \
		  $(xcbshm_module) \
		  $(xcbxv_module) \
                  xineplug_vo_out_none.la

xineplug_vo_out_xcbshm_la_SOURCES = yuv2rgb.c yuv2rgb_mmx.c yuv2rgb_mlib.c video_out_xcbshm.c $(XCBOSD)
xineplug_vo_out_xcbshm_la_LIBADD = $(XINE_LIB) $(MLIB_LIBS) $(PTHREAD_LIBS) $(XCB_LIBS) $(XCBSHM_LIBS) $(LTLIBINTL)
xineplug_vo_out_xcbshm_la_CFLAGS = $(VISIBILITY_FLAG) $(MLIB_CFLAGS) $(XCB_CFLAGS) $(XCBSHM_CFLAGS)

xineplug_vo_out_xcbxv_la_SOURCES = deinterlace.c video_out_xcbxv.c $(XCBOSD)
xineplug_vo_out_xcbxv_la_LIBADD = $(XINE_LIB) $(PTHREAD_LIBS) $(LTLIBINTL) $(XCBXV_LIBS) $(XCB_LIBS)
xineplug_vo_out_xcbxv_la_CFLAGS = $(VISIBILITY_FLAG) $(XCB_CFLAGS) $(XCBXV_CFLAGS)

xineplug_vo_out_xshm_la_SOURCES = yuv2rgb.c yuv2rgb_mmx.c yuv2rgb_mlib.c \
	video_out_xshm.c $(X11OSD)
xineplug_vo_out_xshm_la_LIBADD = $(XINE_LIB) $(MLIB_LIBS) $(X_LIBS) $(PTHREAD_LIBS) $(LTLIBINTL)
xineplug_vo_out_xshm_la_CFLAGS = $(VISIBILITY_FLAG) $(X_CFLAGS) $(MLIB_CFLAGS) -fno-strict-aliasing

xineplug_vo_out_xv_la_SOURCES = $(X11OSD) deinterlace.c video_out_xv.c
xineplug_vo_out_xv_la_LIBADD = $(XINE_LIB) $(XV_LIBS) $(X_LIBS) $(PTHREAD_LIBS) $(LTLIBINTL)
xineplug_vo_out_xv_la_CFLAGS = $(VISIBILITY_FLAG) $(X_CFLAGS) $(XV_CFLAGS) -fno-strict-aliasing

xineplug_vo_out_xvmc_la_SOURCES = deinterlace.c video_out_xvmc.c
xineplug_vo_out_xvmc_la_LIBADD = $(XINE_LIB) $(XVMC_LIBS) $(XV_LIBS) $(X_LIBS) $(PTHREAD_LIBS) $(LTLIBINTL)
xineplug_vo_out_xvmc_la_CFLAGS = $(VISIBILITY_FLAG) $(X_CFLAGS) $(XV_CFLAGS)

xineplug_vo_out_xxmc_la_SOURCES = $(X11OSD) deinterlace.c video_out_xxmc.c\
				xvmc_mocomp.c xvmc_vld.c xxmc.h
xineplug_vo_out_xxmc_la_LIBADD = $(XINE_LIB) $(XXMC_LIBS) $(XV_LIBS) $(X_LIBS) $(PTHREAD_LIBS) $(LTLIBINTL)
xineplug_vo_out_xxmc_la_CFLAGS = $(VISIBILITY_FLAG) $(X_CFLAGS) $(XV_CFLAGS) -fno-strict-aliasing

xineplug_vo_out_opengl_la_SOURCES = yuv2rgb.c yuv2rgb_mmx.c yuv2rgb_mlib.c \
	video_out_opengl.c myglext.h $(X11OSD)
xineplug_vo_out_opengl_la_LIBADD = $(MLIB_LIBS) $(OPENGL_LIBS) $(GLUT_LIBS) \
	$(GLU_LIBS) $(X_LIBS) $(XINE_LIB) $(PTHREAD_LIBS) $(DYNAMIC_LD_LIBS) $(LTLIBINTL)
xineplug_vo_out_opengl_la_CFLAGS = $(VISIBILITY_FLAG) $(X_CFLAGS) $(MLIB_CFLAGS) -fno-strict-aliasing

xineplug_vo_out_syncfb_la_SOURCES = video_out_syncfb.c 
xineplug_vo_out_syncfb_la_LIBADD = $(XINE_LIB) $(X_LIBS) $(PTHREAD_LIBS) $(LTLIBINTL)
xineplug_vo_out_syncfb_la_CFLAGS = $(VISIBILITY_FLAG)

xineplug_vo_out_pgx64_la_SOURCES = video_out_pgx64.c
xineplug_vo_out_pgx64_la_LIBADD = $(XINE_LIB) $(X_LIBS) $(SUNDGA_LIBS) $(PTHREAD_LIBS) $(LTLIBINTL)
xineplug_vo_out_pgx64_la_CFLAGS = $(VISIBILITY_FLAG) $(X_CFLAGS) $(SUNDGA_CFLAGS)

xineplug_vo_out_pgx32_la_SOURCES = video_out_pgx32.c
xineplug_vo_out_pgx32_la_LIBADD = $(XINE_LIB) $(X_LIBS) $(SUNDGA_LIBS) $(PTHREAD_LIBS) $(LTLIBINTL)
xineplug_vo_out_pgx32_la_CFLAGS = $(VISIBILITY_FLAG) $(X_CFLAGS) $(SUNDGA_CFLAGS)

xineplug_vo_out_vidix_la_SOURCES = video_out_vidix.c $(X11OSD)
xineplug_vo_out_vidix_la_LIBADD = $(XINE_LIB)  $(X_LIBS) \
	$(top_builddir)/src/video_out/vidix/libvidix.la $(PTHREAD_LIBS) $(LTLIBINTL)
xineplug_vo_out_vidix_la_CFLAGS = $(VISIBILITY_FLAG) $(VIDIX_CFLAGS) $(X_CFLAGS) -fno-strict-aliasing

xineplug_vo_out_aa_la_SOURCES =  video_out_aa.c
xineplug_vo_out_aa_la_LIBADD = $(XINE_LIB) $(AALIB_LIBS) $(PTHREAD_LIBS) $(LTLIBINTL)
xineplug_vo_out_aa_la_CFLAGS = $(VISIBILITY_FLAG) $(AALIB_CFLAGS)

xineplug_vo_out_caca_la_SOURCES = yuv2rgb.c yuv2rgb_mmx.c yuv2rgb_mlib.c \
	video_out_caca.c
xineplug_vo_out_caca_la_LIBADD = $(XINE_LIB) $(CACA_LIBS) $(PTHREAD_LIBS) $(LTLIBINTL)
xineplug_vo_out_caca_la_CFLAGS = $(VISIBILITY_FLAG) $(CACA_CFLAGS)

xineplug_vo_out_fb_la_SOURCES = yuv2rgb.c yuv2rgb_mmx.c yuv2rgb_mlib.c \
	video_out_fb.c
xineplug_vo_out_fb_la_LIBADD = $(XINE_LIB) $(MLIB_LIBS) $(PTHREAD_LIBS) $(LTLIBINTL)
xineplug_vo_out_fb_la_CFLAGS = $(VISIBILITY_FLAG) $(MLIB_CFLAGS)

xineplug_vo_out_directfb_la_SOURCES = video_out_directfb_fb.c
xineplug_vo_out_directfb_la_LIBADD = $(XINE_LIB) $(DIRECTFB_LIBS) $(PTHREAD_LIBS) $(LTLIBINTL)
xineplug_vo_out_directfb_la_CFLAGS = $(VISIBILITY_FLAG) $(DIRECTFB_CFLAGS) -fno-strict-aliasing

xineplug_vo_out_xdirectfb_la_SOURCES = video_out_directfb_x.c $(X11OSD)
xineplug_vo_out_xdirectfb_la_LIBADD = $(XINE_LIB) $(DIRECTFB_LIBS) $(X_LIBS) $(PTHREAD_LIBS) $(LTLIBINTL)
xineplug_vo_out_xdirectfb_la_CFLAGS = $(VISIBILITY_FLAG) $(DIRECTFB_CFLAGS) -fno-strict-aliasing

xineplug_vo_out_sdl_la_SOURCES = video_out_sdl.c
xineplug_vo_out_sdl_la_LIBADD = $(XINE_LIB) $(SDL_LIBS) $(X_LIBS) $(PTHREAD_LIBS) $(LTLIBINTL)
xineplug_vo_out_sdl_la_CFLAGS = $(VISIBILITY_FLAG) $(X_CFLAGS) $(SDL_CFLAGS)

xineplug_vo_out_stk_la_SOURCES = video_out_stk.c 
xineplug_vo_out_stk_la_LIBADD = $(XINE_LIB) $(LIBSTK_LIBS) $(PTHREAD_LIBS)
xineplug_vo_out_stk_la_CFLAGS = $(VISIBILITY_FLAG) $(LIBSTK_CFLAGS)

xineplug_vo_out_directx_la_SOURCES = yuv2rgb.c yuv2rgb_mmx.c video_out_directx.c 
xineplug_vo_out_directx_la_CPPFLAGS = $(AM_CPPFLAGS) $(DIRECTX_CPPFLAGS)
xineplug_vo_out_directx_la_LIBADD = $(XINE_LIB) $(DIRECTX_VIDEO_LIBS) $(PTHREAD_LIBS)
xineplug_vo_out_directx_la_CFLAGS = $(VISIBILITY_FLAG)

xineplug_vo_out_none_la_SOURCES =  video_out_none.c
xineplug_vo_out_none_la_LIBADD = $(XINE_LIB) $(PTHREAD_LIBS) $(LTLIBINTL)
xineplug_vo_out_none_la_CFLAGS = $(VISIBILITY_FLAG)

xineplug_vo_out_macosx_la_SOURCES = video_out_macosx.m
xineplug_vo_out_macosx_la_CPPFLAGS = $(AM_CPPFLAGS) $(X_CFLAGS) $(MLIB_CFLAGS)
xineplug_vo_out_macosx_la_OBJCFLAGS = $(VISIBILITY_FLAG)
xineplug_vo_out_macosx_la_LIBADD = $(MLIB_LIBS) $(OPENGL_LIBS) $(GLUT_LIBS) \
	$(GLU_LIBS) $(X_LIBS) $(XINE_LIB) $(PTHREAD_LIBS)
# The "-Wl,-framework -Wl,Cocoa" is needed for libtool versions before
# 1.5.x (1.257): the default version that ships with Mac OS X is 1.5 (1.1220)
xineplug_vo_out_macosx_la_LDFLAGS = $(AM_LDFLAGS) \
	-Wl,-framework -Wl,Cocoa -framework Cocoa -framework OpenGL

noinst_HEADERS = deinterlace.h video_out_syncfb.h \
	yuv2rgb.h x11osd.h xcbosd.h
