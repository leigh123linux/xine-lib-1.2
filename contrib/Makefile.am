SUBDIRS = \
	a52dec \
	gsm610 \
	libdca \
	libfaad \
	libmad \
	libmpcdec \
	libxdg-basedir \
	nosefart


if BUILD_DHA_KMOD
SUBDIRS += libdha
endif

# vidix depends on portions of dha
if ENABLE_VIDIX
if !BUILD_DHA_KMOD
SUBDIRS += libdha
endif
SUBDIRS += vidix
endif

# Does anyone know what this is good for?
# It just resolves full path, and triggers an automake warning...
# srcdir = $(shell cd @srcdir@; pwd)

EXTRA_DIST = README.contrib
