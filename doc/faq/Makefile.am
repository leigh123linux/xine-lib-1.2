include $(top_srcdir)/misc/Makefile.common

doc_DATA = faq.txt
html_DATA = faq.html

EXTRA_DIST = README faq.docbook $(doc_DATA) $(html_DATA)

DISTCLEANFILES = $(doc_DATA) $(html_DATA)

docs: $(doc_DATA) $(html_DATA)

faq.html: faq.docbook
	xmlto html-nochunks $(srcdir)/faq.docbook

faq.txt: faq.docbook
	xmlto txt $(srcdir)/faq.docbook
