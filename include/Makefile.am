SUBDIRS = xine
#EXTRA_DIST = xine.h.in

if GENERATED_INTTYPES_H
inttypes_h = inttypes.h
endif

include_HEADERS = xine.h

noinst_HEADERS = config.h configure.h

CONFIG_CLEAN_FILES = $(inttypes_h)

install-data-local: install-includeHEADERS
	sed -e '/^\/\*_x_/d' xine.h > $(DESTDIR)$(includedir)/xine.h
if GENERATED_INTTYPES_H
	$(INSTALL) inttypes.h $(DESTDIR)$(includedir)
endif
