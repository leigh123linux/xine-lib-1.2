include $(top_srcdir)/misc/Makefile.common

SUBDIRS = man hackersguide faq

dist_doc_DATA = README README.dvb README.dxr3 \
	    README.freebsd README.irix README.network_dvd README.opengl \
	    README.solaris README.syncfb README_xxmc.html README.MINGWCROSS \
	    README.WIN32

docs clean-docs:
	@cd faq && $(MAKE) $@
	@cd hackersguide && $(MAKE) $@
