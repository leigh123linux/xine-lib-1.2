include $(top_srcdir)/misc/Makefile.common

libdir = $(XINE_PLUGINDIR)/post

lib_LTLIBRARIES = xineplug_post_mosaico.la xineplug_post_switch.la

xineplug_post_mosaico_la_SOURCES = mosaico.c
xineplug_post_mosaico_la_LIBADD = $(XINE_LIB) $(PTHREAD_LIBS)
xineplug_post_mosaico_la_CFLAGS = $(VISIBILITY_FLAG)
xineplug_post_mosaico_la_LDFLAGS = -avoid-version -module

xineplug_post_switch_la_SOURCES = switch.c
xineplug_post_switch_la_LIBADD = $(XINE_LIB) $(PTHREAD_LIBS)
xineplug_post_switch_la_CFLAGS = $(VISIBILITY_FLAG)
xineplug_post_switch_la_LDFLAGS = -avoid-version -module
