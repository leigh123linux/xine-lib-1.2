include $(top_srcdir)/misc/Makefile.common

AM_CFLAGS = $(DEFAULT_OCFLAGS) $(VISIBILITY_FLAG)
AM_LDFLAGS = $(xineplug_ldflags)

if ENABLE_GSM610
noinst_LTLIBRARIES = libgsm610.la
endif

libgsm610_la_SOURCES = \
	add.c \
	decode.c \
	gsm.h \
	gsm_config.h \
	gsm_create.c \
	gsm_decode.c \
	gsm_destroy.c \
	long_term.c \
	lpc.c \
	private.h \
	proto.h \
	rpe.c \
	short_term.c \
	table.c \
	unproto.h


