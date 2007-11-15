EXTRA_DIST = xine.h.in

if GENERATED_INTTYPES_H
inttypes_h = inttypes.h
endif

include_HEADERS = xine.h

noinst_HEADERS = config.h configure.h

CONFIG_CLEAN_FILES = xine.h $(inttypes_h)

debug:
	@$(MAKE) CFLAGS="$(DEBUG_CFLAGS)"

install-debug: debug
	@$(MAKE) $(AM_MAKEFLAGS) install-exec-am install-data-am

mostlyclean-generic:
	-rm -f *~ \#* .*~ .\#* 

maintainer-clean-generic:
	-@echo "This command is intended for maintainers to use;"
	-@echo "it deletes files that may require special tools to rebuild."
	-rm -f Makefile.in configure.h.in

install-data-local: install-includeHEADERS
	sed -e '/^\/\*_x_/d' xine.h > $(DESTDIR)$(includedir)/xine.h
if GENERATED_INTTYPES_H
	$(INSTALL) inttypes.h $(DESTDIR)$(includedir)
endif
