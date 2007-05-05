EXTRA_DIST = xine.h.in

if GENERATED_INTTYPES_H
inttypes_h = inttypes.h
endif

include_HEADERS = xine.h

CONFIG_CLEAN_FILES = xine.h $(inttypes_h)

mostlyclean-generic:
	-rm -f *~ \#* .*~ .\#* 

maintainer-clean-generic:
	-@echo "This command is intended for maintainers to use;"
	-@echo "it deletes files that may require special tools to rebuild."
	-rm -f Makefile.in

install-data-local: install-includeHEADERS
	sed -e '/^\/\*_x_/d' xine.h > $(DESTDIR)$(includedir)/xine.h
if GENERATED_INTTYPES_H
	$(INSTALL) inttypes.h $(DESTDIR)$(includedir)
endif
