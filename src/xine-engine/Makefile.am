include $(top_srcdir)/misc/Makefile.common
include $(top_srcdir)/lib/Makefile.common

AM_CFLAGS = $(X_CFLAGS) $(FT2_CFLAGS) $(FONTCONFIG_CFLAGS) $(VISIBILITY_FLAG)
AM_CPPFLAGS = $(ZLIB_CPPFLAGS) -DXINE_LIBRARY_COMPILE

LIBTOOL = $(SHELL) $(top_builddir)/libtool
lib_LTLIBRARIES = libxine.la

XINEUTILS_LIB  = $(top_builddir)/src/xine-utils/libxineutils.la
DEF_FILE = libxine-$(XINE_MAJOR).def
if WIN32
def_ldflags="-Wl,--output-def,$(DEF_FILE)"
endif

libxine_la_SOURCES = xine.c metronom.c configfile.c buffer.c \
	load_plugins.c video_decoder.c buffer_types.c \
	audio_decoder.c video_out.c audio_out.c resample.c events.c \
	video_overlay.c osd.c scratch.c demux.c vo_scale.c \
	xine_interface.c post.c tvmode.c broadcaster.c io_helper.c \
	input_rip.c input_cache.c info_helper.c refcounter.c \
	alphablend.c

# FIXME: these are currently unused:
EXTRA_DIST = lrb.c lrb.h accel_xvmc.h

libxine_la_DEPENDENCIES = $(XINEUTILS_LIB) \
                          $(pthread_dep) $(LIBXINEPOSIX)
libxine_la_LIBADD = $(PTHREAD_LIBS) $(DYNAMIC_LD_LIBS) $(LTLIBINTL) $(ZLIB_LIBS) \
		    -lm $(XINEUTILS_LIB) $(LTLIBICONV) $(FT2_LIBS) $(FONTCONFIG_LIBS) \
                    $(LIBXINEPOSIX) $(RT_LIBS) $(NET_LIBS)

libxine_la_LDFLAGS =  \
	-version-info $(XINE_LT_CURRENT):$(XINE_LT_REVISION):$(XINE_LT_AGE) \
	$(def_ldflags)


xineinclude_HEADERS  = buffer.h metronom.h configfile.h vo_scale.h \
        audio_out.h resample.h video_out.h xine_internal.h spu_decoder.h \
	video_overlay.h osd.h scratch.h xine_plugin.h xineintl.h  \
	plugin_catalog.h audio_decoder.h video_decoder.h post.h \
	io_helper.h broadcaster.h info_helper.h refcounter.h alphablend.h

noinst_HEADERS = bswap.h ffmpeg_bswap.h

if WIN32
install-exec-local:
	cp -p $(DEF_FILE) $(DESTDIR)$(libdir)

uninstall-local:
	rm -f $(DEF_FILE)
endif

$(XINEUTILS_LIB):
	$(MAKE) -C $(top_builddir)/src/xine-utils libxineutils.la
