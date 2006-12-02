include $(top_srcdir)/misc/Makefile.common

SUBDIRS = musepack

EXTRA_DIST = diff_against_svn.patch

libdir = $(XINE_PLUGINDIR)

lib_LTLIBRARIES = xineplug_decode_mpc.la

xineplug_decode_mpc_la_SOURCES = huffsv46.c huffsv7.c idtag.c mpc_decoder.c \
	mpc_reader.c requant.c streaminfo.c synth_filter.c xine_decoder.c
xineplug_decode_mpc_la_LIBADD = $(XINE_LIB)
xineplug_decode_mpc_la_CFLAGS = $(VISIBILITY_FLAG)
xineplug_decode_mpc_la_LDFLAGS = -avoid-version -module

