include $(top_srcdir)/misc/Makefile.common

EXTRA_DIST = README dhahelper.c

KCFLAGS = -O2 -Wall -D__KERNEL__ -DMODULE -include `echo @LINUX_INCLUDE@ | sed -e 's/\-I//g'`/linux/modversions.h

KVERSION = $(shell $(SHELL) -c 'uname -r')
moddir = /lib/modules/$(KVERSION)/misc

KCOMPILE = $(CC) $(CFLAGS) $(KCFLAGS) $(INCLUDES) $(LINUX_INCLUDE)

if HAVE_LINUX
if BUILD_DHA_KMOD
KERNEL_MODULE = dhahelper.o
endif
endif

noinst_HEADERS = dhahelper.h

EXTRA_PROGRAMS = test
test_SOURCES = test.c

dhahelper.o:
	$(KCOMPILE) -c `test -f $*.c || echo '$(srcdir)/'`$*.c

nodes:
	$(MKNOD) -m 666 /dev/dhahelper c 252 0

all: $(KERNEL_MODULE)

install-exec-local: $(KERNEL_MODULE)
@BUILD_DHA_KMOD_TRUE@	@$(NORMAL_INSTALL)
@BUILD_DHA_KMOD_TRUE@	$(mkinstalldirs) $(DESTDIR)$(moddir)
@BUILD_DHA_KMOD_TRUE@	@list='$(KERNEL_MODULE)'; \
@BUILD_DHA_KMOD_TRUE@	for p in $$list; do \
@BUILD_DHA_KMOD_TRUE@	  if test -f $$p; then \
@BUILD_DHA_KMOD_TRUE@	    echo "$(INSTALL) -o root -g root -m 644 $$p $(DESTDIR)$(moddir)/$$p"; \
@BUILD_DHA_KMOD_TRUE@	    $(INSTALL) -o root -g root -m 644 $$p $(DESTDIR)$(moddir)/$$p; \
@BUILD_DHA_KMOD_TRUE@	  else :; fi; \
@BUILD_DHA_KMOD_TRUE@	done; \
@BUILD_DHA_KMOD_TRUE@	$(DEPMOD) -a
@BUILD_DHA_KMOD_TRUE@	if test ! -c /dev/dhahelper; then \
@BUILD_DHA_KMOD_TRUE@	  $(MAKE) nodes; \
@BUILD_DHA_KMOD_TRUE@	fi


uninstall-local:
@BUILD_DHA_KMOD_TRUE@	@$(NORMAL_UNINSTALL)
@BUILD_DHA_KMOD_TRUE@	@list='$(KERNEL_MODULE)'; \
@BUILD_DHA_KMOD_TRUE@	for p in $$list; do \
@BUILD_DHA_KMOD_TRUE@	  echo "rm -f $(DESTDIR)$(moddir)/`echo $$p|sed 's/$(EXEEXT)$$//'|sed '$(transform)'|sed 's/$$/$(EXEEXT)/'`"; \
@BUILD_DHA_KMOD_TRUE@	  rm -f $(DESTDIR)$(moddir)/`echo $$p|sed 's/$(EXEEXT)$$//'|sed '$(transform)'|sed 's/$$/$(EXEEXT)/'`; \
@BUILD_DHA_KMOD_TRUE@	done
