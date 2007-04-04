include $(top_srcdir)/misc/Makefile.common

SUBDIRS = musepack

EXTRA_DIST = diff_against_svn.patch

libdir = $(XINE_PLUGINDIR)

if MUSEPACK
lib_LTLIBRARIES = xineplug_decode_mpc.la
endif

if EXTERNAL_MPCDEC
internal_sources = 
else
internal_sources = huffsv46.c huffsv7.c idtag.c mpc_decoder.c \
	mpc_reader.c requant.c streaminfo.c synth_filter.c
endif

if EXTERNAL_MPCDEC
xineplug_decode_mpc_la_LIBADD = $(XINE_LIB) -lmpcdec
else
xineplug_decode_mpc_la_LIBADD = $(XINE_LIB)
endif

xineplug_decode_mpc_la_SOURCES = $(internal_sources) xine_decoder.c
xineplug_decode_mpc_la_CFLAGS = $(VISIBILITY_FLAG)
xineplug_decode_mpc_la_LDFLAGS = -avoid-version -module

