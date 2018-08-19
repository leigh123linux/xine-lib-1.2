include $(top_builddir)/misc/Makefile.plugins
include $(top_srcdir)/misc/Makefile.common

AM_CFLAGS   = $(DEFAULT_OCFLAGS) $(VISIBILITY_FLAG)
AM_CPPFLAGS +=
AM_LDFLAGS  = $(xineplug_ldflags)

if HAVE_X11
AM_CFLAGS  += $(X_CFLAGS)
link_x_libs = $(X_LIBS) -lXext
endif
if HAVE_LIBFAME
AM_CFLAGS += $(LIBFAME_CFLAGS)
link_fame  = $(LIBFAME_LIBS)
endif
if HAVE_LIBRTE
link_rte = -lrte
endif

if ENABLE_DXR3
if ENABLE_FFMPEG
xineplug_LTLIBRARIES = xineplug_dxr3.la
endif
endif

# foo_C[PP]FLAGS supersedes AM_C[PP]FLAGS when it is set somewhere.
# This happens without respect to conditionals.
# So make sure both alternatives use the same set of vars.
# Otherwise, inherited flags may get lost in some cases.
if WITH_EXTERNAL_DVDNAV
link_dvdnav = $(DVDNAV_LIBS) $(DVDREAD_LIBS)
AM_CFLAGS += $(DVDNAV_CFLAGS) $(DVDREAD_CFLAGS)
else
sources_dvdnav = nav_read.c
AM_CPPFLAGS += -I$(top_srcdir)/src/input/libdvdnav
endif

xineplug_dxr3_la_SOURCES = \
	$(sources_dvdnav) \
	compat.c \
	ffmpeg_encoder.c \
	group_dxr3.c \
	group_dxr3.h \
	dxr3.h \
	dxr3_decode_spu.c \
	dxr3_decode_video.c \
	dxr3_mpeg_encoders.c \
	dxr3_scr.c \
	dxr3_scr.h \
	dxr3_spu_encoder.c \
	em8300.h \
	video_out_dxr3.c \
	video_out_dxr3.h

xineplug_dxr3_la_CFLAGS = $(AM_CFLAGS) $(AVUTIL_CFLAGS) $(FFMPEG_CFLAGS)
xineplug_dxr3_la_LIBADD = $(XINE_LIB) $(link_fame) $(link_rte) $(link_x_libs) $(link_dvdnav) $(LTLIBINTL) $(AVUTIL_LIBS) $(FFMPEG_LIBS) -lm
xineplug_dxr3_la_CPPFLAGS = $(AM_CPPFLAGS)
