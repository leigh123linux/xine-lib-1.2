include $(top_srcdir)/misc/Makefile.common

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
radeon_vid_la_LDFLAGS = -avoid-version -module -lm

rage128_vid_la_SOURCES = radeon_vid.c
rage128_vid_la_LDFLAGS = -avoid-version -module -lm
rage128_vid_la_CFLAGS = -DRAGE128 $(AM_CFLAGS)

pm2_vid_la_SOURCES = pm2_vid.c
pm2_vid_la_LDFLAGS = -avoid-version -module

pm3_vid_la_SOURCES = pm3_vid.c
pm3_vid_la_LDFLAGS = -avoid-version -module

mach64_vid_la_SOURCES = mach64_vid.c
mach64_vid_la_LDFLAGS = -avoid-version -module

mga_vid_la_SOURCES = mga_vid.c
mga_vid_la_LDFLAGS = -avoid-version -module -lm

mga_crtc2_vid_la_SOURCES = mga_vid.c
mga_crtc2_vid_la_LDFLAGS = -avoid-version -module -lm
mga_crtc2_vid_la_CFLAGS = -DCRTC2 $(AM_CFLAGS)

cyberblade_vid_la_SOURCES = cyberblade_vid.c
cyberblade_vid_la_LDFLAGS = -avoid-version -module

unichrome_vid_la_SOURCES = unichrome_vid.c
unichrome_vid_la_LDFLAGS = -avoid-version -module

nvidia_vid_la_SOURCES = nvidia_vid.c
nvidia_vid_la_LDFLAGS = -avoid-version -module

sis_vid_la_SOURCES = sis_vid.c sis_bridge.c
sis_vid_la_LDFLAGS = -avoid-version -module

savage_vid_la_SOURCES = savage_vid.c
savage_vid_la_LDFLAGS = -avoid-version -module -lm

noinst_HEADERS = mach64.h glint_regs.h pm3_regs.h radeon.h savage_regs.h \
	cyberblade_regs.h unichrome_regs.h sis_defs.h sis_regs.h

AM_CPPFLAGS = -I$(top_srcdir)/src/video_out/vidix \
	-I$(top_srcdir)/src/video_out/libdha \
	-I$(top_builddir)/src/video_out/libdha
