include $(top_srcdir)/misc/Makefile.common

SUBDIRS = plugins

EXTRA_DIST = 

libdir = $(XINE_PLUGINDIR)/post

lib_LTLIBRARIES = xineplug_post_tvtime.la

xineplug_post_tvtime_la_SOURCES = xine_plugin.c \
	deinterlace.c pulldown.c speedy.c tvtime.c 
xineplug_post_tvtime_la_LIBADD = $(XINE_LIB) \
	$(top_builddir)/src/post/deinterlace/plugins/libdeinterlaceplugins.la

xineplug_post_tvtime_la_CFLAGS = $(VISIBILITY_FLAG)
xineplug_post_tvtime_la_LDFLAGS = -avoid-version -module \
	@IMPURE_TEXT_LDFLAGS@

noinst_HEADERS = deinterlace.h pulldown.h speedtools.h speedy.h tvtime.h
