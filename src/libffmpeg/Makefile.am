include $(top_srcdir)/misc/Makefile.common

DEFAULT_INCLUDES = -I.

if HAVE_FFMPEG
else
SUBDIRS = libavcodec libavutil
endif

EXTRA_DIST = diff_to_ffmpeg_cvs.txt

INTERNAL_DOCS = diff_to_ffmpeg_cvs.txt
