include $(top_srcdir)/misc/Makefile.common

hackersguide_docbook = hackersguide.docbook \
		    intro.docbook \
		    library.docbook \
		    overview.docbook \
		    internals.docbook \
		    stream.docbook \
		    output.docbook
hackersguide_svg = architecture.svg \
		   library.svg \
		   overlays.svg \
		   post_frame.svg

hackersguidedir = $(htmldir)/hackersguide
hackersguide_DATA = hackersguide.html \
            architecture.png library.png overlays.png post_frame.png

EXTRA_DIST = README $(hackersguide_docbook) $(hackersguide_svg) \
	 $(hackersguide_DATA)

DISTCLEANFILES = $(hackersguide_DATA)

SUFFIXES = .png .fig

docs: $(hackersguide_DATA)

clean-docs:
	rm -f $(hackersguide_DATA)

dist-hook:
	@if test x"$(distcheck_lax)" = x ; then \
	  $(MAKE) fail_if_missing=yes docs ; \
	else \
	  $(MAKE) docs ; \
	fi

distclean-local: clean-docs

if HAVE_XMLTO
hackersguide.html: $(hackersguide_docbook)
	xmlto html-nochunks "$(srcdir)/hackersguide.docbook"
else
hackersguide.html: $(hackersguide_docbook)
	if test x"$(fail_if_missing)" = x"yes"; then \
	  echo "Please install xmlto."; \
	  exit 1; \
	fi
	if test x"$(fail_if_missing)" != x"yes"; then \
	  touch $@; \
	  sleep 1; \
	  touch $^; \
	fi
endif

if HAVE_RSVG
.svg.png:
	rsvg -f png $^ $@
else
.svg.png:
	if test x"$(fail_if_missing)" = x"yes"; then \
	  echo "Please install rsvg."; \
	  exit 1; \
	fi
	if test x"$(fail_if_missing)" != x"yes"; then \
	  touch $@; \
	  sleep 1; \
	  touch $^; \
	fi
endif
