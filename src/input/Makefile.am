AUTOMAKE_OPTIONS = subdir-objects
include $(top_builddir)/misc/Makefile.plugins
include $(top_srcdir)/misc/Makefile.common

# REVISIT: This second line here bothers me more than just a little bit
AM_CFLAGS  = $(DEFAULT_OCFLAGS) $(VISIBILITY_FLAG)
AM_LDFLAGS = $(xineplug_ldflags)

##
# IMPORTANT:
# ---------
# All of xine input plugins should be named like the scheme "xineplug_inp_"
#

SUBDIRS =
if !WITH_EXTERNAL_DVDNAV
SUBDIRS += libdvdnav
endif

#
# helper libs
#

noinst_LTLIBRARIES = \
	http_helper.la \
	input_helper.la \
	media_helper.la \
	net_buf_ctrl.la \
	xine_tls.la

http_helper_la_SOURCES = http_helper.c http_helper.h
http_helper_la_LIBADD = $(XINE_LIB) $(LTLIBINTL)

input_helper_la_SOURCES = input_helper.c input_helper.h
input_helper_la_LIBADD = $(XINE_LIB) $(LTLIBINTL)

media_helper_la_SOURCES = media_helper.c media_helper.h
media_helper_la_LIBADD = $(XINE_LIB) $(LTLIBINTL)

net_buf_ctrl_la_SOURCES = net_buf_ctrl.c net_buf_ctrl.h
net_buf_ctrl_la_LIBADD = $(XINE_LIB) $(LTLIBINTL) $(PTHREAD_LIBS)

xine_tls_la_SOURCES = xine_tls.c xine_tls.h
xine_tls_la_CFLAGS = $(AM_CFLAGS) $(GNUTLS_CFLAGS)
xine_tls_la_LIBADD = $(XINE_LIB) $(LTLIBINTL) $(GNUTLS_LIBS)

#
#
#

noinst_LTLIBRARIES += \
        librtsp.la \
        libreal.la

librtsp_la_SOURCES = \
        librtsp/rtsp.c \
        librtsp/rtsp.h \
        librtsp/rtsp_session.c \
        librtsp/rtsp_session.h
librtsp_la_CPPFLAGS = $(AM_CPPFLAGS) -I$(top_srcdir)/src/input/libreal -I$(top_srcdir)/src/input/librtsp

libreal_la_SOURCES = \
        libreal/real.h \
        libreal/real.c \
        libreal/asmrp.h \
        libreal/asmrp.c \
        libreal/rmff.h \
        libreal/rmff.c \
        libreal/sdpplin.h \
        libreal/sdpplin.c
libreal_la_CPPFLAGS = $(AM_CPPFLAGS) -I$(top_srcdir)/src/input/librtsp -I$(top_srcdir)/src/input/libreal

#
# external libs
#

ASFHEADER_LIB = $(top_builddir)/src/demuxers/libasfheader.la
$(ASFHEADER_LIB):
	$(MAKE) -C $(top_builddir)/src/demuxers libasfheader.la

#
# plugins
#

if !ENABLE_LIBXINE_BUILTINS
in_builtins = xineplug_inp_file.la xineplug_inp_stdin_fifo.la xineplug_inp_test.la
endif

if ENABLE_DVB
in_dvb = xineplug_inp_dvb.la
endif

if ENABLE_VCD
in_vcd = xineplug_inp_vcd.la
endif

if ENABLE_VCDO
in_vcdo = xineplug_inp_vcdo.la
endif

if ENABLE_V4L
in_v4l = xineplug_inp_v4l.la
endif

if ENABLE_V4L2
in_v4l2 = xineplug_inp_v4l2.la
in_pvr = xineplug_inp_pvr.la
endif

if ENABLE_GNOME_VFS
in_gnome_vfs = xineplug_inp_gnome_vfs.la
endif

if ENABLE_LIBSMBCLIENT
in_smb = xineplug_inp_smb.la
endif

if ENABLE_SSH
in_ssh = xineplug_inp_ssh.la
endif

if ENABLE_NFS
in_nfs = xineplug_inp_nfs.la
endif

if ENABLE_DVD
in_dvd = xineplug_inp_dvd.la
endif

if ENABLE_MMS
in_mms = xineplug_inp_mms.la
endif

if WITH_EXTERNAL_DVDNAV
DVD_CFLAGS  = $(DVDNAV_CFLAGS) $(DVDREAD_CFLAGS)
link_dvdnav = $(DVDNAV_LIBS) $(DVDREAD_LIBS)
else
DVD_CFLAGS  = -I$(top_srcdir)/src/input/libdvdnav
link_dvdnav = libdvdnav/libdvdnav.la

$(link_dvdnav):
	$(MAKE) -C libdvdnav libdvdnav.la
endif

# not ported to native Windows
if !WIN32
in_rtp = xineplug_inp_rtp.la
endif

if ENABLE_BLURAY
in_bluray = xineplug_inp_bluray.la
endif

xineplug_LTLIBRARIES = \
	$(in_builtins) \
	$(in_dvd) \
	$(in_vcd) \
	$(in_vcdo) \
	$(in_v4l) \
	$(in_v4l2) \
	$(in_gnome_vfs) \
	$(in_nfs) \
	$(in_smb) \
	$(in_ssh) \
	$(in_mms) \
	$(in_rtp) \
	xineplug_inp_network.la \
	$(in_pvr) \
	$(in_dvb) \
	$(in_bluray) \
	xineplug_inp_cdda.la

xineplug_inp_file_la_SOURCES = input_file.c
xineplug_inp_file_la_LIBADD = $(XINE_LIB) $(LTLIBINTL) input_helper.la

xineplug_inp_test_la_SOURCES = input_test.c
xineplug_inp_test_la_LIBADD = $(XINE_LIB) $(LTLIBINTL) input_helper.la

xineplug_inp_dvd_la_SOURCES = input_dvd.c
xineplug_inp_dvd_la_LIBADD = $(XINE_LIB) $(link_dvdnav) $(PTHREAD_LIBS) $(DYNAMIC_LD_LIBS) $(LTLIBINTL) media_helper.la
xineplug_inp_dvd_la_CFLAGS = $(AM_CFLAGS) $(DVD_CFLAGS)

xineplug_inp_network_la_SOURCES = \
	group_network.c \
	group_network.h \
	input_ftp.c \
	input_http.c \
	input_net.c \
	input_pnm.c \
	input_rtsp.c \
	pnm.c \
	pnm.h
xineplug_inp_network_la_LIBADD = $(XINE_LIB) $(NET_LIBS) $(LTLIBINTL) \
	libreal.la librtsp.la http_helper.la input_helper.la net_buf_ctrl.la xine_tls.la

xineplug_inp_rtp_la_SOURCES = input_rtp.c
xineplug_inp_rtp_la_LIBADD = $(XINE_LIB) $(NET_LIBS) $(PTHREAD_LIBS) $(LTLIBINTL) net_buf_ctrl.la input_helper.la

xineplug_inp_mms_la_SOURCES = input_mms.c mms.c mms.h mmsh.c mmsh.h
xineplug_inp_mms_la_LIBADD = $(XINE_LIB) $(LTLIBICONV) $(LTLIBINTL) $(ASFHEADER_LIB) http_helper.la net_buf_ctrl.la input_helper.la

xineplug_inp_vcd_la_SOURCES = vcd/xineplug_inp_vcd.c vcd/vcdplayer.c vcd/vcdplayer.h vcd/vcdio.c vcd/vcdio.h
xineplug_inp_vcd_la_LIBADD = $(XINE_LIB) $(LTLIBINTL) $(LIBCDIO_LIBS) $(LIBVCD_LIBS) $(LIBVCDINFO_LIBS) $(LIBISO9660_LIBS) -lm
xineplug_inp_vcd_la_DEPENDENCIES = $(LIBCDIO_DEPS) $(LIBVCD_DEPS) $(LIBVCDINFO_DEPS) $(LIBISO9660_DEPS)
xineplug_inp_vcd_la_CFLAGS  = $(AM_CFLAGS) $(LIBCDIO_CFLAGS) $(LIBVCD_CFLAGS)

xineplug_inp_vcdo_la_SOURCES = input_vcd.c
xineplug_inp_vcdo_la_LIBADD = $(XINE_LIB) $(LTLIBINTL) media_helper.la

xineplug_inp_stdin_fifo_la_SOURCES = input_stdin_fifo.c
xineplug_inp_stdin_fifo_la_LIBADD = $(XINE_LIB) $(LTLIBINTL) net_buf_ctrl.la input_helper.la

xineplug_inp_dvb_la_SOURCES = input_dvb.c
xineplug_inp_dvb_la_DEPS = $(XDG_BASEDIR_DEPS)
xineplug_inp_dvb_la_LIBADD = $(XINE_LIB) $(PTHREAD_LIBS) $(LTLIBINTL) $(XDG_BASEDIR_LIBS) net_buf_ctrl.la input_helper.la
xineplug_inp_dvb_la_CPPFLAGS = $(AM_CPPFLAGS) $(XDG_BASEDIR_CPPFLAGS)

xineplug_inp_cdda_la_SOURCES = input_cdda.c
xineplug_inp_cdda_la_DEPS = $(XDG_BASEDIR_DEPS)
xineplug_inp_cdda_la_LIBADD = $(XINE_LIB) $(LTLIBINTL) $(XDG_BASEDIR_LIBS) media_helper.la
xineplug_inp_cdda_la_CPPFLAGS = $(AM_CPPFLAGS) $(XDG_BASEDIR_CPPFLAGS)

xineplug_inp_v4l_la_SOURCES = input_v4l.c
xineplug_inp_v4l_la_LIBADD = $(XINE_LIB) $(ALSA_LIBS) $(LTLIBINTL)
xineplug_inp_v4l_la_CFLAGS = $(AM_CFLAGS) $(ALSA_CFLAGS)

xineplug_inp_v4l2_la_SOURCES = input_v4l2.c
xineplug_inp_v4l2_la_LIBADD = $(XINE_LIB) $(LTLIBINTL) $(V4L2_LIBS)
xineplug_inp_v4l2_la_CFLAGS = $(AM_CFLAGS) $(V4L2_CFLAGS)

xineplug_inp_gnome_vfs_la_SOURCES = input_gnome_vfs.c
xineplug_inp_gnome_vfs_la_LIBADD = $(XINE_LIB) $(GNOME_VFS_LIBS) $(LTLIBINTL) net_buf_ctrl.la
xineplug_inp_gnome_vfs_la_CFLAGS = $(AM_CFLAGS) $(GNOME_VFS_CFLAGS)

xineplug_inp_smb_la_SOURCES = input_smb.c
xineplug_inp_smb_la_LIBADD = $(XINE_LIB) $(LIBSMBCLIENT_LIBS) $(LTLIBINTL)
xineplug_inp_smb_la_CFLAGS = $(AM_CFLAGS) $(LIBSMBCLIENT_CFLAGS)

xineplug_inp_pvr_la_SOURCES = input_pvr.c
xineplug_inp_pvr_la_LIBADD = $(XINE_LIB) $(PTHREAD_LIBS) $(LTLIBINTL)

xineplug_inp_bluray_la_SOURCES = input_bluray.c
xineplug_inp_bluray_la_LIBADD = $(XINE_LIB) $(LIBBLURAY_LIBS) $(PTHREAD_LIBS) $(LTLIBINTL) media_helper.la input_helper.la
xineplug_inp_bluray_la_CFLAGS = $(AM_CFLAGS) $(LIBBLURAY_CFLAGS)

xineplug_inp_nfs_la_SOURCES = input_nfs.c
xineplug_inp_nfs_la_LIBADD = $(XINE_LIB) $(LIBNFS_LIBS) $(LTLIBINTL) input_helper.la
xineplug_inp_nfs_la_CFLAGS = $(AM_CFLAGS) $(LIBNFS_CFLAGS)

xineplug_inp_ssh_la_SOURCES = input_ssh.c
xineplug_inp_ssh_la_LIBADD = $(XINE_LIB) $(LIBSSH2_LIBS) $(LTLIBINTL) http_helper.la input_helper.la net_buf_ctrl.la
xineplug_inp_ssh_la_CFLAGS = $(AM_CFLAGS) $(LIBSSH2_CFLAGS)
