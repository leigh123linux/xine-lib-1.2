include $(top_srcdir)/misc/Makefile.common

AM_CPPFLAGS = -DXINE_COMPILE

EXTRA_DIST = audio_irixal_out.c

if HAVE_OSS
oss_module = xineplug_ao_out_oss.la
endif

if HAVE_ALSA
alsa_module = xineplug_ao_out_alsa.la
endif

if HAVE_ESD
esd_module = xineplug_ao_out_esd.la
endif

if HAVE_SUNAUDIO
sun_module = xineplug_ao_out_sun.la
endif

#if HAVE_IRIXAL
#irixal_module = xineplug_ao_out_irixal.la
#endif

if HAVE_ARTS
arts_module = xineplug_ao_out_arts.la
endif

if HAVE_DIRECTX
directx_module = xineplug_ao_out_directx.la
directx2_module = xineplug_ao_out_directx2.la
endif

if HAVE_COREAUDIO
coreaudio_module = xineplug_ao_out_coreaudio.la
endif

if HAVE_PULSEAUDIO
pulseaudio_module = xineplug_ao_out_pulseaudio.la
endif

if HAVE_FUSIONSOUND
fusionsound_module = xineplug_ao_out_fusionsound.la
endif

if HAVE_JACK
jack_module = xineplug_ao_out_jack.la
endif

##
# IMPORTANT:
# ---------
# all xine audio out plugins should be named like the 
# scheme "xineplug_ao_out_"
#
xineplug_LTLIBRARIES = xineplug_ao_out_none.la xineplug_ao_out_file.la \
	$(oss_module) \
	$(alsa_module) \
	$(sun_module) \
	$(arts_module) \
	$(esd_module) \
	$(directx_module) \
	$(coreaudio_module) \
	$(pulseaudio_module) \
	$(directx2_module)	\
	$(fusionsound_module) \
	$(jack_module)

xineplug_ao_out_none_la_SOURCES = audio_none_out.c
xineplug_ao_out_none_la_LIBADD = $(XINE_LIB) $(LTLIBINTL)
xineplug_ao_out_none_la_CFLAGS = $(VISIBILITY_FLAG)
xineplug_ao_out_none_la_LDFLAGS = -avoid-version -module

xineplug_ao_out_file_la_SOURCES = audio_file_out.c
xineplug_ao_out_file_la_LIBADD = $(XINE_LIB) $(LTLIBINTL)
xineplug_ao_out_file_la_CFLAGS = $(VISIBILITY_FLAG)
xineplug_ao_out_file_la_LDFLAGS = -avoid-version -module

xineplug_ao_out_oss_la_SOURCES = audio_oss_out.c
xineplug_ao_out_oss_la_LIBADD = $(XINE_LIB) $(LTLIBINTL)
xineplug_ao_out_oss_la_CFLAGS = $(VISIBILITY_FLAG)
xineplug_ao_out_oss_la_LDFLAGS = -avoid-version -module

xineplug_ao_out_alsa_la_SOURCES = audio_alsa_out.c
xineplug_ao_out_alsa_la_LIBADD = $(XINE_LIB) $(ALSA_LIBS) $(PTHREAD_LIBS) $(LTLIBINTL)
xineplug_ao_out_alsa_la_CFLAGS = $(VISIBILITY_FLAG) $(ALSA_CFLAGS)
xineplug_ao_out_alsa_la_LDFLAGS = -avoid-version -module

xineplug_ao_out_esd_la_SOURCES = audio_esd_out.c 
xineplug_ao_out_esd_la_LIBADD = $(XINE_LIB) $(ESD_LIBS) $(LTLIBINTL)
xineplug_ao_out_esd_la_CFLAGS = $(VISIBILITY_FLAG) $(ESD_CFLAGS)
xineplug_ao_out_esd_la_LDFLAGS = -avoid-version -module

xineplug_ao_out_sun_la_SOURCES = audio_sun_out.c
xineplug_ao_out_sun_la_LIBADD = $(XINE_LIB)
xineplug_ao_out_sun_la_CFLAGS = $(VISIBILITY_FLAG)
xineplug_ao_out_sun_la_LDFLAGS = -avoid-version -module

#xineplug_ao_out_irixal_la_SOURCES = audio_irixal_out.c 
#xineplug_ao_out_irixal_la_LIBADD = $(IRIXAL_LIBS)
#xineplug_ao_out_irixal_la_CFLAGS = $(VISIBILITY_FLAG) $(IRIXAL_CFLAGS)
#xineplug_ao_out_irixal_la_LDFLAGS = -avoid-version -module

xineplug_ao_out_arts_la_SOURCES = audio_arts_out.c
xineplug_ao_out_arts_la_LIBADD = $(XINE_LIB) $(ARTS_LIBS)
xineplug_ao_out_arts_la_CFLAGS = $(VISIBILITY_FLAG) $(ARTS_CFLAGS)
xineplug_ao_out_arts_la_LDFLAGS = -avoid-version -module

xineplug_ao_out_directx_la_SOURCES = audio_directx_out.c
xineplug_ao_out_directx_la_CPPFLAGS = $(DIRECTX_CPPFLAGS)
xineplug_ao_out_directx_la_LIBADD = $(XINE_LIB) $(DIRECTX_AUDIO_LIBS) $(LTLIBINTL)
xineplug_ao_out_directx_la_CFLAGS = $(VISIBILITY_FLAG)
xineplug_ao_out_directx_la_LDFLAGS = -avoid-version -module

xineplug_ao_out_coreaudio_la_SOURCES = audio_coreaudio_out.c
xineplug_ao_out_coreaudio_la_LIBADD = $(XINE_LIB)
# The "-Wl,-framework -Wl,..." is needed for libtool versions before
# 1.5.x (1.257): the default version that ships with Mac OS X is 1.5 (1.1220)
xineplug_ao_out_coreaudio_la_LDFLAGS = \
	-Wl,-framework -Wl,Cocoa -framework CoreAudio \
	-Wl,-framework -Wl,AudioUnit -framework AudioUnit \
	-avoid-version -module
xineplug_ao_out_coreaudio_la_CFLAGS = -framework CoreAudio -framework AudioUnit $(VISIBILITY_FLAG)

xineplug_ao_out_pulseaudio_la_SOURCES = audio_pulse_out.c
xineplug_ao_out_pulseaudio_la_LIBADD = $(XINE_LIB) $(PULSEAUDIO_LIBS) $(LTLIBINTL)
xineplug_ao_out_pulseaudio_la_CFLAGS = $(VISIBILITY_FLAG) $(PULSEAUDIO_CFLAGS)
xineplug_ao_out_pulseaudio_la_LDFLAGS = -avoid-version -module

xineplug_ao_out_directx2_la_SOURCES = audio_directx2_out.c
xineplug_ao_out_directx2_la_CPPFLAGS = $(DIRECTX_CPPFLAGS)
xineplug_ao_out_directx2_la_LIBADD = $(XINE_LIB) $(DIRECTX_AUDIO_LIBS) $(PTHREAD_LIBS) $(LTLIBINTL)
xineplug_ao_out_directx2_la_CFLAGS = $(VISIBILITY_FLAG)
xineplug_ao_out_directx2_la_LDFLAGS = -avoid-version -module

xineplug_ao_out_fusionsound_la_SOURCES = audio_fusionsound_out.c
xineplug_ao_out_fusionsound_la_LIBADD = $(XINE_LIB) $(FUSIONSOUND_LIBS)
xineplug_ao_out_fusionsound_la_CFLAGS = $(VISIBILITY_FLAG) $(FUSIONSOUND_CFLAGS)
xineplug_ao_out_fusionsound_la_LDFLAGS = -avoid-version -module

xineplug_ao_out_jack_la_SOURCES = audio_jack_out.c
xineplug_ao_out_jack_la_LIBADD = $(XINE_LIB) $(JACK_LIBS) $(LTLIBINTL)
xineplug_ao_out_jack_la_CFLAGS = $(VISIBILITY_FLAG) $(JACK_CFLAGS)
xineplug_ao_out_jack_la_LDFLAGS = -avoid-version -module
