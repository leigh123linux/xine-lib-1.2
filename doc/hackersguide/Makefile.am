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

hackersguidedir = $(htmldir)/hackersguide
hackersguide_DATA = hackersguide.html \
            architecture.png library.png overlays.png post_frame.png

EXTRA_DIST = README $(hackersguide_docbook) $(hackersguide_fig) \
	 $(hackersguide_DATA)

DISTCLEANFILES = $(hackersguide_DATA)

hackersguide.html: $(hackersguide_docbook)
	xmlto html-nochunks $(srcdir)/hackersguide.docbook

.svg.png:
	rsvg -f png $< $@

SUFFIXES = .png .fig
