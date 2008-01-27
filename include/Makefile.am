EXTRA_DIST = xine/version.h.in

if GENERATED_INTTYPES_H
inttypes_h = inttypes.h
endif

nobase_include_HEADERS = xine.h xine/version.h xine/buffer.h		\
	xine/metronom.h xine/configfile.h xine/vo_scale.h		\
	xine/audio_out.h xine/resample.h xine/video_out.h		\
	xine/xine_internal.h xine/spu_decoder.h xine/video_overlay.h	\
	xine/osd.h xine/spu.h xine/scratch.h xine/xine_plugin.h		\
	xine/xineintl.h xine/plugin_catalog.h xine/audio_decoder.h	\
	xine/video_decoder.h xine/post.h xine/io_helper.h		\
	xine/broadcaster.h xine/info_helper.h xine/refcounter.h		\
	xine/alphablend.h xine/demux.h xine/input_plugin.h		\
	xine/attributes.h xine/compat.h xine/xine_buffer.h		\
	xine/xineutils.h xine/xmllexer.h xine/xmlparser.h xine/list.h	\
	xine/array.h xine/sorted_array.h xine/pool.h			\
	xine/ring_buffer.h xine/os_types.h xine/vdr.h $(inttypes_h)

noinst_HEADERS = config.h configure.h

CONFIG_CLEAN_FILES = $(inttypes_h)
