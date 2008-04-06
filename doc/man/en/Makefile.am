include $(top_srcdir)/misc/Makefile.common

STATICMANS = xine-config.1 xine.5
DYNAMICMANS = xine-list-@XINE_SERIES@.1

man_MANS = $(STATICMANS) $(DYNAMICMANS)

BUILT_SOURCES = $(DYNAMICMANS)
DISTCLEANFILES = $(DYNAMICMANS)
EXTRA_DIST = $(STATICMANS) $(suffix .in,$(DYNAMICMANS))

xine-list-@XINE_SERIES@.1: xine-list.1.in
	(echo '.ds xl xine\-list\-@XINE_MAJOR@.@XINE_MINOR@'; cat $<) >$@
