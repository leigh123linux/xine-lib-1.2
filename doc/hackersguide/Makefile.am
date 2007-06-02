include $(top_srcdir)/misc/Makefile.common

hackersguide_docbook = hackersguide.docbook \
		    intro.docbook \
		    library.docbook \
		    overview.docbook \
		    internals.docbook \
		    stream.docbook \
		    output.docbook
hackersguide_fig = architecture.fig \
		   library.fig \
		   overlays.fig \
		   post_frame.fig

hackersguide_DATA = hackersguide.html \
            architecture.png library.png overlays.png post_frame.png

EXTRA_DIST = README $(hackersguide_docbook) $(hackersguide_fig) \
	 $(hackersguide_DATA)

hackersguidedir = $(htmldir)/hackersguide

docs: $(hackersguide_DATA)

distclean-local: clean-docs

docs-prepare: $(addprefix $(srcdir)/, $(hackersguide_sgml))
	if test "$(srcdir)" != "."  -a "$(srcdir)" != "`pwd`"; then \
	  for file in $(hackersguide_sgml) ; do \
	    test -f ./$$file -a ./$$file -nt $(srcdir)/$$file || cp $(srcdir)/$$file .; \
	  done ; \
	fi

clean-docs:
	rm -f $(hackersguide_DATA)
	test "x$(srcdir)" = x. || rm -f $(hackersguide_sgml) *.fig

dist-hook:
	@if test x"$(distcheck_lax)" = x ; then \
	  $(MAKE) fail_if_missing=yes docs ; \
	else \
	  $(MAKE) docs ; \
	fi

if HAVE_SGMLTOOLS
hackersguide.html: $(hackersguide_sgml)
	@$(MAKE) docs-prepare
	$(SGMLTOOLS) -b onehtml hackersguide.sgml;
else
hackersguide.html: $(hackersguide_sgml)
	@$(MAKE) docs-prepare
	if test x"$(fail_if_missing)" = x"yes"; then \
	  echo "Please install sgmltools-lite."; \
	  exit 1; \
	fi
	if test x"$(fail_if_missing)" != x"yes"; then \
	  touch $@; \
	  sleep 1; \
	  touch $(notdir $^); \
	fi
endif

if HAVE_FIG2DEV
fig.png:
	$(FIG2DEV) -L png -S 4 $< $@;
else
.fig.png:
	if test x"$(fail_if_missing)" = x"yes"; then \
	  echo "Please install fig2dev."; \
	  exit 1; \
	fi
	if test x"$(fail_if_missing)" != x"yes"; then \
	  touch $@; \
	  sleep 1; \
	  touch $(notdir $^); \
	fi
endif

SUFFIXES = .png .fig
