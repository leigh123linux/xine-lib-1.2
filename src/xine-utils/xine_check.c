/*
* Copyright (C) 2000-2002 the xine project
*
* This file is part of xine, a free video player.
*
* xine is free software; you can redistribute it and/or modify
* it under the terms of the GNU General Public License as published by
* the Free Software Foundation; either version 2 of the License, or
* (at your option) any later version.
*
* xine is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
* GNU General Public License for more details.
*
* You should have received a copy of the GNU General Public License
* along with this program; if not, write to the Free Software
* Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA
*
* Xine Health Check:
*
* Overview: Checking the setup of the user's system is the task of
* xine-check.sh for now. At present this is intended to replace
* xine_check to provide a more robust way of informing users new
* to xine of the setup of their system.
*
* Interface: The function xine_health_check is the starting point
* to check the user's system. It is expected that the values for
* hc->cdrom_dev and hc->dvd_dev will be defined. For example,
* hc->cdrom_dev = /dev/cdrom and hc->/dev/dvd. If at any point a
* step fails the function returns with a failed status,
* XINE_HEALTH_CHECK_FAIL, and an error message contained in hc->msg.
*
* Author: Stephen Torri <storri@users.sourceforge.net>
*/

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdlib.h>
#include <stdarg.h>
#include <errno.h>
#include <fcntl.h>

#include "xine_check.h"
#include "xineutils.h"

#if defined(__linux__)

#include <dlfcn.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <sys/utsname.h>
#include <linux/major.h>
#include <linux/hdreg.h>

#ifdef HAVE_X11
#include <X11/Xlib.h>
#endif

#ifdef HAVE_XV
#include <X11/extensions/Xvlib.h>
#endif

#endif  /* !__linux__ */

static void set_hc_result(xine_health_check_t* hc, int state, char *format, ...) {

  va_list   args;
  char     *buf = NULL;
  int       n, size;

  if (!hc) {
    printf("%s() GASP, hc is NULL\n", __XINE_FUNCTION__);
    abort();
  }

  if (!format) {
    printf("%s() GASP, format is NULL\n", __XINE_FUNCTION__);
    abort();
  }

  size = strlen(format) + 1;

  if((buf = malloc(size)) == NULL) {
    printf("%s() GASP, malloc() failed\n", __XINE_FUNCTION__);
    abort();
  }

  while(1) {
    va_start(args, format);
    n = vsnprintf(buf, size, format, args);
    va_end(args);

    if(n > -1 && n < size) {
      break;
      }

    if(n > -1) {
      size = n + 1;
    }
    else {
      size *= 2;
    }

    if((buf = realloc(buf, size)) == NULL) {
      printf("%s() GASP, realloc() failed\n", __XINE_FUNCTION__);
      abort();
    }
  }

  hc->msg = buf;
  hc->status = state;
}

#if defined(__linux__)

xine_health_check_t* xine_health_check (xine_health_check_t* hc, int check_num) {

  switch(check_num) {
    case CHECK_KERNEL:
      hc = xine_health_check_kernel (hc);
      break;
    case CHECK_MTRR:
      hc = xine_health_check_mtrr (hc);
      break;
    case CHECK_CDROM:
      hc = xine_health_check_cdrom (hc);
      break;
    case CHECK_DVDROM:
      hc = xine_health_check_dvdrom (hc);
      break;
    case CHECK_DMA:
      hc = xine_health_check_dma (hc);
      break;
    case CHECK_X:
      hc = xine_health_check_x (hc);
      break;
    case CHECK_XV:
      hc = xine_health_check_xv (hc);
      break;
    default:
      hc->status = XINE_HEALTH_CHECK_NO_SUCH_CHECK;
  }

  return hc;
}

xine_health_check_t* xine_health_check_kernel (xine_health_check_t* hc) {
  struct utsname kernel;

  if (uname (&kernel) == 0) {
    fprintf (stdout,"  sysname: %s\n", kernel.sysname);
    fprintf (stdout,"  release: %s\n", kernel.release);
    fprintf (stdout,"  machine: %s\n", kernel.machine);
    hc->status = XINE_HEALTH_CHECK_OK;
  }
  else {
    set_hc_result (hc, XINE_HEALTH_CHECK_FAIL, "FAILED - Could not get kernel information.");
  }
  return hc;
}

#ifdef ARCH_X86
xine_health_check_t* xine_health_check_mtrr (xine_health_check_t* hc) {
  char *file = "/proc/mtrr";
  FILE *fd;

  fd = fopen(file, "r");
  if (fd < 0) {
    set_hc_result (hc, XINE_HEALTH_CHECK_FAIL, "FAILED: mtrr is not enabled.");
  }
  else {
    hc->status = XINE_HEALTH_CHECK_OK;
    fclose (fd);
  }
  return hc;
}
#else
xine_health_check_t* xine_health_check_mtrr (xine_health_check_t* hc) {

  set_hc_result (hc, XINE_HEALTH_CHECK_OK, "FAILED: mtrr does not apply on this hw platform.");

  return hc;
}
#endif

xine_health_check_t* xine_health_check_cdrom (xine_health_check_t* hc) {
  struct stat cdrom_st;

  if (stat (hc->cdrom_dev,&cdrom_st) < 0) {
    set_hc_result (hc, XINE_HEALTH_CHECK_FAIL, "FAILED - could not cdrom: %s\n", hc->cdrom_dev);
    return hc;
  }
  else {
    if ((cdrom_st.st_mode & S_IFMT) != S_IFBLK) {
      set_hc_result (hc, XINE_HEALTH_CHECK_FAIL, "FAILED - %s is not a block device.\n", hc->cdrom_dev);
      return hc;
    }

    if ((cdrom_st.st_mode & S_IFMT & S_IRWXU & S_IRWXG & S_IRWXO) !=
        (S_IRUSR & S_IXUSR & S_IRGRP & S_IXGRP & S_IROTH & S_IXOTH)) {
      set_hc_result (hc, XINE_HEALTH_CHECK_FAIL, "FAILED - %s permissions are not 'rwxrwxrx'\n.", hc->cdrom_dev);
      return hc;
    }
    hc->status = XINE_HEALTH_CHECK_OK;
  }
  return hc;
}

xine_health_check_t* xine_health_check_dvdrom(xine_health_check_t* hc) {

  struct stat dvdrom_st;

  if (stat (hc->dvd_dev,&dvdrom_st) < 0) {
    set_hc_result (hc, XINE_HEALTH_CHECK_FAIL, "FAILED - could not dvdrom: %s\n", hc->dvd_dev);
    return hc;
  }

  if ((dvdrom_st.st_mode & S_IFMT) != S_IFBLK) {
    set_hc_result(hc, XINE_HEALTH_CHECK_FAIL, "FAILED - %s is not a block device.\n", hc->dvd_dev);
    return hc;
  }

  if ((dvdrom_st.st_mode & S_IFMT & S_IRWXU & S_IRWXG & S_IRWXO) !=
      (S_IRUSR & S_IXUSR & S_IRGRP & S_IXGRP & S_IROTH & S_IXOTH)) {
    set_hc_result(hc, XINE_HEALTH_CHECK_FAIL, "FAILED - %s permissions are not 'rwxrwxrx'.\n", hc->dvd_dev);
    return hc;
  }

  hc->status = XINE_HEALTH_CHECK_OK;
  return hc;
}

xine_health_check_t* xine_health_check_dma (xine_health_check_t* hc) {

  int is_scsi_dev = 0;
  int fd = 0;
  static long param = 0;
  struct stat st;

  /* If /dev/dvd points to /dev/scd0 but the drive is IDE (e.g. /dev/hdc)
   * and not scsi how do we detect the correct one */
  if (stat (hc->dvd_dev, &st)) {
    set_hc_result(hc, XINE_HEALTH_CHECK_FAIL, "FAILED - Could not read stats for %s.\n", hc->dvd_dev);
    return hc;
  }

  if (major (st.st_rdev) == LVM_BLK_MAJOR) {
    is_scsi_dev = 1;
    set_hc_result(hc, XINE_HEALTH_CHECK_OK, "SKIPPED - Operation not supported on SCSI drives or drives that use the ide-scsi module.");
    return hc;
  }

  fd = open (hc->dvd_dev, O_RDONLY | O_NONBLOCK);
  if (fd < 0) {
    set_hc_result(hc, XINE_HEALTH_CHECK_FAIL, "FAILED - Could not open %s.\n", hc->dvd_dev);
    return hc;
  }

  if (!is_scsi_dev) {

    if(ioctl (fd, HDIO_GET_DMA, &param)) {
      set_hc_result(hc, XINE_HEALTH_CHECK_FAIL,
          "FAILED -  HDIO_GET_DMA failed. Ensure the permissions for %s are 0664.\n",
          hc->dvd_dev);
      return hc;
    }

    if (param != 1) {
      char* instructions = "If you are using the ide-cd module ensure \
        that you have the following entry in /etc/modules.conf:\n \
        options ide-cd dma=1\n Reload ide-cd module.";
      set_hc_result(hc, XINE_HEALTH_CHECK_FAIL,
          "FAILED - DMA not turned on for %s.\n%s\n",
          hc->dvd_dev,
          instructions);
      return hc;
    }
  }
  close (fd);
  hc->status = XINE_HEALTH_CHECK_OK;
  return hc;
}


xine_health_check_t* xine_health_check_x (xine_health_check_t* hc) {
  char* env_display = getenv("DISPLAY");

  if (strlen (env_display) == 0) {
    set_hc_result (hc, XINE_HEALTH_CHECK_FAIL, "FAILED - DISPLAY environment variable not set.");
  }
  else {
    hc->status = XINE_HEALTH_CHECK_OK;
  }
  return hc;
}

xine_health_check_t* xine_health_check_xv (xine_health_check_t* hc) {

#ifdef HAVE_X11
#ifdef HAVE_XV
  Display               *dpy;
  unsigned int          ver, rev, eventB, reqB, errorB;
  char                  *disname = NULL;
  void                  *dl_handle;
  Display               *(*xopendisplay)(char*);
  char                  *(*xdisplayname)(char*);
  char                  *err = NULL;
  int                   formats, adaptors, i;
  XvImageFormatValues   *img_formats;
  XvAdaptorInfo     *adaptor_info;

  /* Majority of thi code was taken from or inspired by the xvinfo.c file of XFree86 */

  /* Get reference to XOpenDisplay */
  dlerror(); /* clear error code */
  dl_handle = dlopen("libX11.so", RTLD_LAZY);
  if(!dl_handle) {
    hc->msg = dlerror();
    hc->status = XINE_HEALTH_CHECK_FAIL;
    return hc;
  }

  xopendisplay = dlsym(dl_handle,"XOpenDisplay");

  if((err = dlerror()) != NULL) {
    hc->msg = err;
    hc->status = XINE_HEALTH_CHECK_FAIL;
    return hc;
  }

  /* Get reference to XDisplayName */
  dlerror(); /* clear error code */
  dl_handle = dlopen("libX11.so", RTLD_LAZY);
  if(!dl_handle) {
    hc->msg = dlerror();
    hc->status = XINE_HEALTH_CHECK_FAIL;
    return hc;
  }

  xdisplayname = dlsym(dl_handle,"XDisplayName");

  if((err = dlerror()) != NULL) {
    hc->msg = err;
    hc->status = XINE_HEALTH_CHECK_FAIL;
    return hc;
  }
  dlclose(dl_handle);

  if(!(dpy = (*xopendisplay)(disname))) {

    if (!disname) {
      disname = (*xdisplayname)(NULL);
    }
    set_hc_result (hc, XINE_HEALTH_CHECK_FAIL, "Unable to open display: %s\n", disname);
    return hc;
  }

  if((Success != XvQueryExtension(dpy, &ver, &rev, &reqB, &eventB, &errorB))) {
    if (!disname) {
      disname = xdisplayname(NULL);
    }
    set_hc_result (hc, XINE_HEALTH_CHECK_FAIL, "Unable to open display: %s\n",disname);
    return hc;
  }
  else {
    printf("X-Video Extension version %d.%d\n", ver, rev);
  }

  /*
   * check adaptors, search for one that supports (at least) yuv12
   */

  if (Success != XvQueryAdaptors(dpy,DefaultRootWindow(dpy),
				 &adaptors,&adaptor_info))  {
    set_hc_result (hc, XINE_HEALTH_CHECK_FAIL, "video_out_xv: XvQueryAdaptors failed.\n");
    return hc;
  }

  img_formats = XvListImageFormats (dpy, adaptor_info->base_id, &formats);

  for(i = 0; i < formats; i++) {

    printf ("video_out_xv: Xv image format: 0x%x (%4.4s) %s\n",
	    img_formats[i].id, (char*)&img_formats[i].id,
	    (img_formats[i].format == XvPacked) ? "packed" : "planar");

    if (img_formats[i].id == XINE_IMGFMT_YV12)  {
      printf("video_out_xv: this adaptor supports the yv12 format.\n");
      set_hc_result (hc, XINE_HEALTH_CHECK_OK, "video_out_xv: this adaptor supports the yv12 format.\n");
    } else if (img_formats[i].id == XINE_IMGFMT_YUY2) {
      printf("video_out_xv: this adaptor supports the yuy2 format.\n");
      set_hc_result (hc, XINE_HEALTH_CHECK_OK, "video_out_xv: this adaptor supports the yuy2 format.\n");
    }
  }

  return hc;
#else
  set_hc_result(hc, XINE_HEALTH_CHECK_FAIL, "No X-Video Extension");
  return hc;
#endif /* ! HAVE_HV */
#else
  set_hc_result(hc, XINE_HEALTH_CHECK_FAIL, "No X11 windowing system");
  return hc;
#endif /* ! HAVE_X11 */
}

#else	/* !__linux__ */
xine_health_check_t* xine_health_check (xine_health_check_t* hc, int check_num) {
  set_hc_result(hc, XINE_HEALTH_CHECK_UNSUPPORTED, "Xine health check not supported on the OS.\n");
  return hc;
}
#endif	/* !__linux__ */
