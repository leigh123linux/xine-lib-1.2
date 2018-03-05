include $(top_srcdir)/misc/Makefile.common

AM_CFLAGS = $(DEFAULT_OCFLAGS) $(VISIBILITY_FLAG)

noinst_LTLIBRARIES = libnosefart.la

libnosefart_la_SOURCES = \
	dis6502.c \
	dis6502.h \
	fds_snd.c \
	fds_snd.h \
	fmopl.c \
	fmopl.h \
	log.c \
	log.h \
	memguard.c \
	memguard.h \
	mmc5_snd.c \
	mmc5_snd.h \
	nsf.c \
	nsf.h \
	nes_apu.c \
	nes_apu.h \
	nes6502.c \
	nes6502.h \
	osd.h \
	types.h \
	version.h \
	vrc7_snd.c \
	vrc7_snd.h \
	vrcvisnd.c \
	vrcvisnd.h
libnosefart_la_CFLAGS = $(AM_CFLAGS) -fno-strict-aliasing
libnosefart_la_CPPFLAGS = $(AM_CPPFLAGS) -DNSF_PLAYER
libnosefart_la_LIBADD = -lm
