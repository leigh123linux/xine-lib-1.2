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
 * $Id: dxr3_decode_spu.c,v 1.8 2002/07/05 15:10:22 mroi Exp $
 */
 
/* dxr3 spu decoder plugin.
 * Accepts the spu data from xine and sends it directly to the
 * corresponding dxr3 device. Also handles dvd menu button highlights.
 * Takes precedence over libspudec due to a higher priority.
 */

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>

#include "xine_internal.h"
#include "xineutils.h"
#include "buffer.h"
#include "xine-engine/bswap.h"
#include "nav_types.h"
#include "nav_read.h"
#include "dxr3.h"

#define LOG_PTS 0
#define LOG_SPU 0
#define LOG_BTN 0

#define MAX_SPU_STREAMS 32


/* plugin initialization function */
spu_decoder_t *init_spu_decoder_plugin(int iface_version, xine_t *xine);

/* functions required by xine api */
static char   *dxr3_spudec_get_id(void);
static int     dxr3_spudec_can_handle(spu_decoder_t *this_gen, int buf_type);
static void    dxr3_spudec_init(spu_decoder_t *this_gen, vo_instance_t *vo_out);
static void    dxr3_spudec_decode_data(spu_decoder_t *this_gen, buf_element_t *buf);
static void    dxr3_spudec_reset(spu_decoder_t *this_gen);
static void    dxr3_spudec_close(spu_decoder_t *this_gen);
static void    dxr3_spudec_dispose(spu_decoder_t *this_gen);

/* plugin structures */
typedef struct dxr3_spu_stream_state_s {
  uint32_t                 stream_filter;
  
  int                      spu_length;
  int                      spu_ctrl;
  int                      spu_end;
  int                      end_found;
  int                      bytes_passed; /* used to parse the spu */
} dxr3_spu_stream_state_t;

typedef struct dxr3_spudec_s {
  spu_decoder_t            spu_decoder;
  vo_instance_t           *vo_out;
  xine_t                  *xine;
  
  char                     devname[128];
  char                     devnum[3];
  int                      fd_spu;       /* to access the dxr3 spu device */
  
  dxr3_spu_stream_state_t  spu_stream_state[MAX_SPU_STREAMS];
  int                      menu;         /* are we in a menu? */
  pci_t                    pci;
  uint32_t                 buttonN;      /* currently highlighted button */
  
  int                      aspect;       /* this is needed for correct highlight placement */
  int                      height;       /* in anamorphic menus */
  int                      pan_scan;
} dxr3_spudec_t;

/* helper functions */
static int     dxr3_present(xine_t *xine);
static void    dxr3_spudec_event_listener(void *this_gen, xine_event_t *event_gen);
static int     dxr3_spudec_copy_nav_to_btn(dxr3_spudec_t *this, int32_t mode, em8300_button_t *btn);
static void    dxr3_swab_clut(int* clut);


spu_decoder_t *init_spu_decoder_plugin(int iface_version, xine_t *xine)
{
  dxr3_spudec_t *this;
  const char *confstr;
  int dashpos;
  
  if (iface_version != 8) {
    printf(_("dxr3_decode_spu: plugin doesn't support plugin API version %d.\n"
      "dxr3_decode_spu: this means there's a version mismatch between xine and this "
      "dxr3_decode_spu: decoder plugin. Installing current plugins should help.\n"),
      iface_version);
    return NULL;
  }
  
  if (!dxr3_present(xine)) return NULL;
  
  this = (dxr3_spudec_t *)malloc(sizeof(dxr3_spudec_t));
  if (!this) return NULL;
  
  confstr = xine->config->register_string(xine->config,
    CONF_LOOKUP, CONF_DEFAULT, CONF_NAME, CONF_HELP, NULL, NULL);
  strncpy(this->devname, confstr, 128);
  this->devname[127] = '\0';
  dashpos = strlen(this->devname) - 2; /* the dash in the new device naming scheme would be here */
  if (this->devname[dashpos] == '-') {
    /* use new device naming scheme with trailing number */
    strncpy(this->devnum, &this->devname[dashpos], 3);
    this->devname[dashpos] = '\0';
  } else {
    /* use old device naming scheme without trailing number */
    /* FIXME: remove this when everyone uses em8300 >=0.12.0 */
    this->devnum[0] = '\0';
  }
  
  this->spu_decoder.interface_version = iface_version;
  this->spu_decoder.get_identifier    = dxr3_spudec_get_id;
  this->spu_decoder.can_handle        = dxr3_spudec_can_handle;
  this->spu_decoder.init              = dxr3_spudec_init;
  this->spu_decoder.decode_data       = dxr3_spudec_decode_data;
  this->spu_decoder.reset             = dxr3_spudec_reset;
  this->spu_decoder.close             = dxr3_spudec_close;
  this->spu_decoder.dispose           = dxr3_spudec_dispose;
  this->spu_decoder.priority          = 10;
  
  this->xine                          = xine;
  this->fd_spu                        = 0;
  this->menu                          = 0;
  this->pci.hli.hl_gi.hli_ss          = 0;
  this->buttonN                       = 1;
  
  this->aspect                        = XINE_ASPECT_RATIO_4_3;
  this->pan_scan                      = 0;
  
  xine_register_event_listener(xine, dxr3_spudec_event_listener, this);
    
  return &this->spu_decoder;
}

static char *dxr3_spudec_get_id(void)
{
  return "dxr3-spudec";
}

static int dxr3_spudec_can_handle(spu_decoder_t *this_gen, int buf_type)
{
  int type = buf_type & 0xFFFF0000;
  return (type == BUF_SPU_PACKAGE || type == BUF_SPU_CLUT || 
    type == BUF_SPU_NAV || type == BUF_SPU_SUBP_CONTROL);
}

static void dxr3_spudec_init(spu_decoder_t *this_gen, vo_instance_t *vo_out)
{
  dxr3_spudec_t *this = (dxr3_spudec_t *)this_gen;
  char tmpstr[128];
  int i;
  
  this->vo_out = vo_out;
  
  /* open dxr3 spu device */
  snprintf(tmpstr, sizeof(tmpstr), "%s_sp%s", this->devname, this->devnum);
  if ((this->fd_spu = open(tmpstr, O_WRONLY)) < 0) {
    printf("dxr3_decode_spu: Failed to open spu device %s (%s)\n",
      tmpstr, strerror(errno));
    return;
  }
#if LOG_SPU
  printf ("dxr3_decode_spu: init: SPU_FD = %i\n",this->fd_spu);
#endif
  for (i=0; i < MAX_SPU_STREAMS; i++) {
    this->spu_stream_state[i].stream_filter = 1;
    this->spu_stream_state[i].spu_length = 0;
  }
}

static void dxr3_spudec_decode_data(spu_decoder_t *this_gen, buf_element_t *buf)
{
  dxr3_spudec_t *this = (dxr3_spudec_t *)this_gen;
  ssize_t written;
  uint32_t stream_id = buf->type & 0x1f;
  dxr3_spu_stream_state_t *state = &this->spu_stream_state[stream_id];
  uint32_t spu_channel = this->xine->spu_channel;
  
  if (buf->type == BUF_SPU_CLUT) {
#if LOG_SPU
    printf("dxr3_decode_spu: BUF_SPU_CLUT\n");
#endif
    if (buf->content[0] == 0)  /* cheap endianess detection */
      dxr3_swab_clut((int *)buf->content);
    if (ioctl(this->fd_spu, EM8300_IOCTL_SPU_SETPALETTE, buf->content))
      printf("dxr3_decode_spu: failed to set CLUT (%s)\n", strerror(errno));
    return;
  }
  if(buf->type == BUF_SPU_SUBP_CONTROL) {
    /* FIXME: is BUF_SPU_SUBP_CONTROL used anymore? */
    int i;
    uint32_t *subp_control = (uint32_t *)buf->content;
    
    for (i = 0; i < MAX_SPU_STREAMS; i++)
      this->spu_stream_state[i].stream_filter = subp_control[i];
    return;
  }
  if(buf->type == BUF_SPU_NAV) {
#if LOG_BTN
    printf("dxr3_decode_spu: got NAV packet\n");
#endif
    uint8_t *p = buf->content;
    
    /* just watch out for menus */
    if (p[3] == 0xbf && p[6] == 0x00) { /* Private stream 2 */
      pci_t pci;
      
      nav_read_pci(&pci, p + 7);
#if LOG_BTN
      printf("dxr3_decode_spu: PCI packet hli_ss is %d\n", pci.hli.hl_gi.hli_ss);
#endif
      if (pci.hli.hl_gi.hli_ss == 1) {
        em8300_button_t btn;
        
        /* menu ahead, remember pci for later evaluation */
        xine_fast_memcpy(&this->pci, &pci, sizeof(pci_t));
        if ((dxr3_spudec_copy_nav_to_btn(this, 0, &btn ) > 0))
          if (ioctl(this->fd_spu, EM8300_IOCTL_SPU_BUTTON, &btn))
            printf("dxr3_decode_spu: failed to set spu button (%s)\n",
              strerror(errno));
        this->menu = 1;
      }
      
      if ((pci.hli.hl_gi.hli_ss == 0) && (this->pci.hli.hl_gi.hli_ss == 1)) {
        /* this is (or: should be, I hope I got this right) a
           subpicture plane, that hides all menu buttons */
        uint8_t empty_spu[] = {
          0x00, 0x26, 0x00, 0x08, 0x80, 0x00, 0x00, 0x80,
          0x00, 0x00, 0x00, 0x20, 0x01, 0x03, 0x00, 0x00,
          0x04, 0x00, 0x00, 0x05, 0x00, 0x00, 0x01, 0x00,
          0x00, 0x01, 0x06, 0x00, 0x04, 0x00, 0x07, 0xFF,
          0x00, 0x01, 0x00, 0x20, 0x02, 0xFF };
        /* leaving menu */
        this->pci.hli.hl_gi.hli_ss = 0;
	this->menu = 0;
        ioctl(this->fd_spu, EM8300_IOCTL_SPU_BUTTON, NULL);
        write(this->fd_spu, empty_spu, sizeof(empty_spu));
      }
    }
    return;
  }
  
  /* Look for the display duration entry in the spu packets.
   * If the spu is a menu button highlight pane, this entry must not exist,
   * because the spu is hidden, when the menu is left, not by timeout.
   * Some broken dvds do not respect this and therefore confuse the spu
   * decoding pipeline of the card. We fix this here.
   */
  if (!state->spu_length) {
    state->spu_length = buf->content[0] << 8 | buf->content[1];
    state->spu_ctrl = (buf->content[2] << 8 | buf->content[3]) + 2;
    state->spu_end = 0;
    state->end_found = 0;
    state->bytes_passed = 0;
  }
  if (!state->end_found) {
    int offset_in_buffer = state->spu_ctrl - state->bytes_passed;
    if (offset_in_buffer >= 0 && offset_in_buffer < buf->size)
      state->spu_end = buf->content[offset_in_buffer] << 8;
    offset_in_buffer++;
    if (offset_in_buffer >= 0 && offset_in_buffer < buf->size) {
      state->spu_end |= buf->content[offset_in_buffer];
      state->end_found = 1;
    }
  }
  if (state->end_found && this->menu) {
    int offset_in_buffer = state->spu_end - state->bytes_passed;
    if (offset_in_buffer >= 0 && offset_in_buffer < buf->size)
      buf->content[offset_in_buffer] = 0x00;
    offset_in_buffer++;
    if (offset_in_buffer >= 0 && offset_in_buffer < buf->size)
      buf->content[offset_in_buffer] = 0x00;
    offset_in_buffer += 3;
    if (offset_in_buffer >= 0 && offset_in_buffer < buf->size &&
        buf->content[offset_in_buffer] == 0x02)
      buf->content[offset_in_buffer] = 0x00;
  }
  state->spu_length -= buf->size;
  if (state->spu_length < 0) state->spu_length = 0;
  state->bytes_passed += buf->size;
  
  /* filter unwanted streams */
  if (buf->decoder_flags & BUF_FLAG_PREVIEW) {
#if LOG_SPU
    printf("dxr3_decode_spu: Dropping SPU channel %d. Preview data\n", stream_id);
#endif
    return;
  }
  if (state->stream_filter == 0) {
#if LOG_SPU
    printf("dxr3_decode_spu: Dropping SPU channel %d. Stream filtered\n", stream_id);
#endif
    return;
  }
  if (this->aspect == XINE_ASPECT_RATIO_ANAMORPHIC && this->xine->spu_channel_user == -1 &&
      !this->pan_scan && this->xine->spu_channel_letterbox >= 0 &&
      this->xine->video_driver->get_property(this->xine->video_driver, VO_PROP_VO_TYPE) ==
      VO_TYPE_DXR3_TVOUT) {
    /* Use the letterbox version of the subpicture for tv out. */
    spu_channel = this->xine->spu_channel_letterbox;
  }
  if ((spu_channel & 0x1f) != stream_id) {
#if LOG_SPU
    printf("dxr3_decode_spu: Dropping SPU channel %d. Not selected stream_id\n", stream_id);
#endif
    return;
  }
  if ((this->menu == 0) && (spu_channel & 0x80)) {
#if LOG_SPU
    printf("dxr3_decode_spu: Dropping SPU channel %d. Only allow forced display SPUs\n", stream_id);
#endif
    return;
  }
  
  /* write sync timestamp to the card */
  if (buf->pts) {
    int64_t vpts;
    uint32_t vpts32;
    
    vpts = this->xine->metronom->got_spu_packet(
      this->xine->metronom, buf->pts);
    /* estimate with current time, when metronom doesn't know */
    if (!vpts) vpts = this->xine->metronom->get_current_time(this->xine->metronom);
#if LOG_PTS
    printf("dxr3_decode_spu: pts = %lld vpts = %lld\n", buf->pts, vpts);
#endif
    vpts32 = vpts;
    if (ioctl(this->fd_spu, EM8300_IOCTL_SPU_SETPTS, &vpts32))
      printf("dxr3_decode_spu: spu setpts failed (%s)\n", strerror(errno));
  }
  
  /* write spu data to the card */
#if LOG_SPU
  printf ("dxr3_decode_spu: write: SPU_FD = %i\n",this->fd_spu);
#endif
  written = write(this->fd_spu, buf->content, buf->size);
  if (written < 0) {
    printf("dxr3_decode_spu: spu device write failed (%s)\n",
      strerror(errno));
    return;
  }
  if (written != buf->size)
    printf("dxr3_decode_spu: Could only write %d of %d spu bytes.\n",
      written, buf->size);
}

static void dxr3_spudec_reset(spu_decoder_t *this_gen)
{
  dxr3_spudec_t *this = (dxr3_spudec_t *)this_gen;
  int i;
 
  for (i = 0; i < MAX_SPU_STREAMS; i++)
    this->spu_stream_state[i].spu_length = 0;
}

static void dxr3_spudec_close(spu_decoder_t *this_gen)
{
  dxr3_spudec_t *this = (dxr3_spudec_t *)this_gen;
#if LOG_SPU
  printf("dxr3_decode_spu: close: SPU_FD = %i\n",this->fd_spu);
#endif
  close(this->fd_spu);
  this->fd_spu = 0;
}

static void dxr3_spudec_dispose(spu_decoder_t *this_gen)
{
  dxr3_spudec_t *this = (dxr3_spudec_t *)this_gen;
  
  xine_remove_event_listener(this->xine, dxr3_spudec_event_listener);
  free (this);
}


static int dxr3_present(xine_t *xine)
{
  int info;
  
  if (xine && xine->video_driver) {
    info = xine->video_driver->get_property(xine->video_driver, VO_PROP_VO_TYPE);
#ifdef LOG_SPU
    printf("dxr3_decode_spu: dxr3 presence test: info = %d\n", info);
#endif
    if ((info != VO_TYPE_DXR3_TVOUT) && (info != VO_TYPE_DXR3_OVERLAY))
      return 0;
  }
  return 1;
}

static void dxr3_spudec_event_listener(void *this_gen, xine_event_t *event_gen)
{
  dxr3_spudec_t *this = (dxr3_spudec_t *)this_gen;
  xine_spu_event_t *event = (xine_spu_event_t *)event_gen;
  
#if LOG_SPU
  printf("dxr3_decode_spu: event caught: SPU_FD = %i\n",this->fd_spu);
#endif
  
  switch (event->event.type) {
  case XINE_EVENT_SPU_BUTTON:
    {
      spu_button_t *but = event->data;
      em8300_button_t btn;
#if LOG_BTN
      printf("dxr3_decode_spu: got SPU_BUTTON\n");
#endif
      this->buttonN = but->buttonN;
      if ((but->show > 0) && (dxr3_spudec_copy_nav_to_btn(
        this, but->show - 1, &btn) > 0))
        if (ioctl(this->fd_spu, EM8300_IOCTL_SPU_BUTTON, &btn))
          printf("dxr3_decode_spu: failed to set spu button (%s)\n",
            strerror(errno));
#if LOG_BTN
      printf("dxr3_decode_spu: buttonN = %u\n",but->buttonN);
#endif
    }
    break;
    
  case XINE_EVENT_SPU_CLUT:
    {
      spudec_clut_table_t *clut = event->data;
#if LOG_SPU
      printf("dxr3_decode_spu: got SPU_CLUT\n");
#endif
#ifdef WORDS_BIGENDIAN
      dxr3_swab_clut(clut->clut);
#endif
      if (ioctl(this->fd_spu, EM8300_IOCTL_SPU_SETPALETTE, clut->clut))
        printf("dxr3_decode_spu: failed to set CLUT (%s)\n",
          strerror(errno));
    }
    break;
  case XINE_EVENT_FRAME_CHANGE:
    this->height = ((xine_frame_change_event_t *)event)->height;
    this->aspect = ((xine_frame_change_event_t *)event)->aspect;
#if LOG_BTN
    printf("dxr3_decode_spu: aspect changed to %d\n", this->aspect);
#endif
    break;
  case XINE_EVENT_ASPECT_CHANGE:
    switch (((xine_aspect_ratio_event_t *)event)->ratio_code) {
    case ASPECT_FULL:
      this->aspect = XINE_ASPECT_RATIO_4_3;
#if LOG_BTN
      printf("dxr3_decode_spu: aspect changed to %d\n", this->aspect);
#endif
      break;
    case ASPECT_ANAMORPHIC:
      this->aspect = XINE_ASPECT_RATIO_ANAMORPHIC;
#if LOG_BTN
      printf("dxr3_decode_spu: aspect changed to %d\n", this->aspect);
#endif
      break;
    }
    if ((((xine_aspect_ratio_event_t *)event)->scale_permission == 1) != this->pan_scan) {
      this->pan_scan = !this->pan_scan;
      this->xine->video_driver->set_property(this->xine->video_driver,
        VO_PROP_ZOOM_FACTOR, this->pan_scan ? 1 : -1);
    }
    break;
#if 0
  /* FIXME: I think this event is not necessary any more
   * We know from nav packet decoding, if we are in a menu. */
  case XINE_EVENT_SPU_FORCEDISPLAY:
    {
      this->menu = (int)event->data;
#if LOG_SPU
      printf("dxr3_decode_spu: got SPU_FORCEDISPLAY, force display is %s\n",
        this->menu ? "on" : "off");
#endif
    }
    break;
#endif
  }  
}

static int dxr3_spudec_copy_nav_to_btn(dxr3_spudec_t *this, int32_t mode, em8300_button_t *btn)
{
  int32_t button = this->buttonN;
  btni_t *button_ptr;
  
  /* if ( this->pci.hli.hl_gi.fosl_btnn > 0) {
   *   button = this->pci.hli.hl_gi.fosl_btnn ;
   * }
   */
   
  if ((button <= 0) || (button > this->pci.hli.hl_gi.btn_ns)) {
    printf("dxr3_decode_spu: Unable to select button number %i as it doesn't exist. Forcing button 1\n",
      button);
    button = 1;
  }
  
  button_ptr = &this->pci.hli.btnit[button-1];
  if(button_ptr->btn_coln != 0) {
#if LOG_BTN
    fprintf(stderr, "dxr3_decode_spu: normal button clut, mode %d\n", mode);
#endif
    btn->color = (this->pci.hli.btn_colit.btn_coli[button_ptr->btn_coln-1][mode] >> 16);
    btn->contrast = (this->pci.hli.btn_colit.btn_coli[button_ptr->btn_coln-1][mode]);
    /* FIXME: Only the first grouping of buttons are used at the moment */
    btn->left = button_ptr->x_start;
    btn->top  = button_ptr->y_start;
    btn->right = button_ptr->x_end;
    btn->bottom = button_ptr->y_end;
    if (this->aspect == XINE_ASPECT_RATIO_ANAMORPHIC && !this->pan_scan &&
        this->xine->video_driver->get_property(this->xine->video_driver, VO_PROP_VO_TYPE) ==
        VO_TYPE_DXR3_TVOUT) {
      /* modify button areas for anamorphic menus on tv out */
      int top_black_bar = this->height / 8;
      btn->top = btn->top * 3 / 4 + top_black_bar;
      btn->bottom = btn->bottom * 3 / 4 + top_black_bar;
    }
    return 1;
  } 
  return -1;
}

static void dxr3_swab_clut(int *clut)
{
  int i;
  for (i=0; i<16; i++)
    clut[i] = bswap_32(clut[i]);
}
