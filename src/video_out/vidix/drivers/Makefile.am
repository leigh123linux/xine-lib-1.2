include $(top_srcdir)/misc/Makefile.common

AM_CFLAGS = $(DEFAULT_OCFLAGS)
AM_CPPFLAGS = -I$(top_srcdir)/src/video_out/vidix \
	-I$(top_srcdir)/src/video_out/libdha \
	-I$(top_builddir)/src/video_out/libdha
AM_LDFLAGS = $(xineplug_ldflags)

EXTRA_DIST = genfb_vid.c

if HAVE_VIDIX
vidix_drivers = \
	mach64_vid.la \
	mga_crtc2_vid.la \
	mga_vid.la \
	pm2_vid.la \
	pm3_vid.la \
	radeon_vid.la \
	rage128_vid.la \
	cyberblade_vid.la \
	unichrome_vid.la \
	nvidia_vid.la \
	sis_vid.la \
	savage_vid.la 
endif

vidix_LTLIBRARIES = $(vidix_drivers)

radeon_vid_la_SOURCES = radeon_vid.c
radeon_vid_la_LIBADD = -lm

rage128_vid_la_SOURCES = radeon_vid.c
rage128_vid_la_LIBADD = -lm
rage128_vid_la_CPPFLAGS = $(AM_CPPFLAGS) -DRAGE128

pm2_vid_la_SOURCES = pm2_vid.c

pm3_vid_la_SOURCES = pm3_vid.c

mach64_vid_la_SOURCES = mach64_vid.c

mga_vid_la_SOURCES = mga_vid.c
mga_vid_la_LIBADD = -lm

mga_crtc2_vid_la_SOURCES = mga_vid.c
mga_crtc2_vid_la_LIBADD = -lm
mga_crtc2_vid_la_CPPFLAGS = $(AM_CPPFLAGS) -DCRTC2

cyberblade_vid_la_SOURCES = cyberblade_vid.c

unichrome_vid_la_SOURCES = unichrome_vid.c

nvidia_vid_la_SOURCES = nvidia_vid.c

sis_vid_la_SOURCES = sis_vid.c sis_bridge.c

savage_vid_la_SOURCES = savage_vid.c
savage_vid_la_LIBADD = -lm

noinst_HEADERS = mach64.h glint_regs.h pm3_regs.h radeon.h savage_regs.h \
	cyberblade_regs.h unichrome_regs.h sis_defs.h sis_regs.h

