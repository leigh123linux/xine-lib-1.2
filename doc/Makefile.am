include $(top_srcdir)/misc/Makefile.common

SUBDIRS = man hackersguide faq

docs_DATA = README README.dvb README.dxr3 \
	    README.freebsd README.irix README.network_dvd README.opengl \
	    README.solaris README.syncfb README_xxmc.html README.MINGWCROSS \
	    README.WIN32

EXTRA_DIST = $(docs_DATA)

docsdir = $(prefix)/share/doc/xine

docs clean-docs:
	@cd faq && $(MAKE) $@
	@cd hackersguide && $(MAKE) $@
