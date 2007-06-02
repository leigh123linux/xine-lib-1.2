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

DISTCLEANFILES = $(hackersguide_DATA)

dist-hook:
	@if test x"$(distcheck_lax)" = x ; then \
	  $(MAKE) fail_if_missing=yes docs ; \
	else \
	  $(MAKE) docs ; \
	fi

hackersguide.html: $(hackersguide_docbook)
	xmlto html-nochunks $(srcdir)/hackersguide.docbook

.fig.png:
	fig2dev -L png -S 4 $< $@;

SUFFIXES = .png .fig
