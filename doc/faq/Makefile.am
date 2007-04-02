include $(top_srcdir)/misc/Makefile.common

doc_DATA = faq.txt
html_DATA = faq.html

EXTRA_DIST = README faq.sgml $(doc_DATA) $(html_DATA)

docs: $(doc_DATA) $(html_DATA)

distclean-local: clean-docs

docs-prepare: $(srcdir)/faq.sgml
	if test "$(srcdir)" != "." -a "$(srcdir)" != "`pwd`"; then \
	  test -f ./faq.sgml -a ./faq.sgml -nt $(srcdir)/faq.sgml || cp $^ . ; \
	fi

clean-docs:
	rm -f $(doc_DATA) $(html_DATA)
	test "x$(srcdir)" = x. || rm -f faq.sgml

dist-hook:
	@if test x"$(distcheck_lax)" = x ; then \
	  $(MAKE) fail_if_missing=yes docs ; \
	else \
	  $(MAKE) docs ; \
	fi

if HAVE_SGMLTOOLS
faq.html: faq.sgml
	@$(MAKE) docs-prepare
	$(SGMLTOOLS) -b onehtml faq.sgml;
else
faq.html: faq.sgml
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

if HAVE_SGMLTOOLS
faq.txt: faq.sgml
	@$(MAKE) docs-prepare
	$(SGMLTOOLS) -b txt faq.sgml;
else
faq.txt: faq.sgml
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
