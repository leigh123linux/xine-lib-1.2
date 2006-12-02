include $(top_srcdir)/misc/Makefile.common

SUBDIRS = codebook

libdir = $(XINE_PLUGINDIR)

if BUILD_FAAD
faad_module = xineplug_decode_faad.la
endif

lib_LTLIBRARIES = $(faad_module)

VPATH = @srcdir@:@srcdir@/codebook:

xineplug_decode_faad_la_CFLAGS = $(VISIBILITY_FLAG) $(AM_CFLAGS) -fno-strict-aliasing
xineplug_decode_faad_la_SOURCES = \
	bits.c \
	cfft.c \
	common.c \
	decoder.c \
	drc.c \
	drm_dec.c \
	error.c \
	filtbank.c \
	hcr.c \
	huffman.c \
	ic_predict.c \
	is.c \
	lt_predict.c \
	mdct.c \
	mp4.c \
	ms.c \
	output.c \
	pns.c \
	ps_dec.c \
	ps_syntax.c \
	pulse.c \
	rvlc.c \
	sbr_dct.c \
	sbr_dec.c \
	sbr_e_nf.c \
	sbr_fbt.c \
	sbr_hfadj.c \
	sbr_hfgen.c \
	sbr_huff.c \
	sbr_qmf.c \
	sbr_syntax.c \
	sbr_tf_grid.c \
	specrec.c \
	ssr.c \
	ssr_fb.c \
	ssr_ipqf.c \
	syntax.c \
	tns.c \
	xine_decoder.c

xineplug_decode_faad_la_LDFLAGS = -avoid-version -module
xineplug_decode_faad_la_LIBADD = $(XINE_LIB)

noinst_HEADERS = \
	analysis.h \
	bits.h \
	cfft.h \
	cfft_tab.h \
	common.h \
	decoder.h \
	drc.h \
	drm_dec.h \
	error.h \
	filtbank.h \
	fixed.h \
	huffman.h \
	ic_predict.h \
	is.h \
	iq_table.h \
	kbd_win.h \
	lt_predict.h \
	mdct.h \
	mdct_tab.h \
	mp4.h \
	ms.h \
	output.h \
	pns.h \
	ps_dec.h \
	ps_tables.h \
	pulse.h \
	rvlc.h \
	sbr_dct.h \
	sbr_dec.h \
	sbr_e_nf.h \
	sbr_fbt.h \
	sbr_hfadj.h \
	sbr_hfgen.h \
	sbr_huff.h \
	sbr_noise.h \
	sbr_qmf.h \
	sbr_qmf_c.h \
	sbr_syntax.h \
	sbr_tf_grid.h \
	sine_win.h \
	specrec.h \
	ssr.h \
	ssr_fb.h \
	ssr_ipqf.h \
	ssr_win.h \
	structs.h \
	syntax.h \
	tns.h 
