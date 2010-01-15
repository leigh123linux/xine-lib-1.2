/*
 * Copyright (C) 2008 the xine project
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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110, USA
 *
 * DVB Subtitle decoder (ETS 300 743)
 * (c) 2004 Mike Lampard <mlampard@users.sourceforge.net>
 * based on the application dvbsub by Dave Chapman
 *
 * TODO:
 * - Implement support for teletext based subtitles
 */

#include "pthread.h"
#include <errno.h>
#include <xine/xine_internal.h>
#include <xine/spu.h>
#include <xine/osd.h>
#define MAX_REGIONS 7

#define SPU_MAX_WIDTH 720
#define SPU_MAX_HEIGHT 576

/*#define LOG*/
#define LOG_MODULE "spudvb"

typedef struct {
  int			x, y;
  unsigned char	is_visible;
} visible_region_t;

typedef struct {
  int			page_time_out;
  int			page_version_number;
  int			page_state;
  int			page_id;
  visible_region_t	regions[MAX_REGIONS];
} page_t;

typedef struct {
  int                   version_number;
  int			width, height;
  int                   empty;
  int			depth;
  int			CLUT_id;
  int			objects_start;
  int			objects_end;
  unsigned int		object_pos[65536];
  unsigned char	*img;
  osd_object_t          *osd;
} region_t;

typedef struct {
/* dvbsub stuff */
  int			x;
  int			y;
  unsigned int		curr_obj;
  unsigned int		curr_reg[64];
  uint8_t	       *buf;
  int			i;
  int			nibble_flag;
  int			in_scanline;
  page_t		page;
  region_t		regions[MAX_REGIONS];
  clut_t		colours[MAX_REGIONS*256];
  unsigned char		trans[MAX_REGIONS*256];
} dvbsub_func_t;

typedef struct		dvb_spu_class_s {
  spu_decoder_class_t	class;
  xine_t	       *xine;
} dvb_spu_class_t;

typedef struct dvb_spu_decoder_s {
  spu_decoder_t	spu_decoder;

  dvb_spu_class_t      *class;
  xine_stream_t        *stream;

  spu_dvb_descriptor_t *spu_descriptor;

  /* dvbsub_osd_mutex should be locked around all calls to this->osd_renderer->show()
     and this->osd_renderer->hide() */
  pthread_mutex_t	dvbsub_osd_mutex;

  char		       *pes_pkt;
  char                 *pes_pkt_wrptr;
  unsigned int	pes_pkt_size;

  uint64_t		pts;
  uint64_t		vpts;
  uint64_t		end_vpts;

  pthread_t	dvbsub_timer_thread;
  struct timespec       dvbsub_hide_timeout;
  pthread_cond_t        dvbsub_restart_timeout;
  dvbsub_func_t        *dvbsub;
  int			show;
} dvb_spu_decoder_t;


static void update_osd(dvb_spu_decoder_t *this, int region_id)
{
  dvbsub_func_t *dvbsub = this->dvbsub;
  region_t *reg = &dvbsub->regions[region_id];

  if ( !reg->img ) {
    if ( reg->osd ) {
      pthread_mutex_lock( &this->dvbsub_osd_mutex );
      this->stream->osd_renderer->free_object( reg->osd );
      reg->osd = NULL;
      pthread_mutex_unlock( &this->dvbsub_osd_mutex );
    }
    return;
  }

  if ( reg->osd ) {
    if ( reg->width!=reg->osd->width || reg->height!=reg->osd->height ) {
      pthread_mutex_lock( &this->dvbsub_osd_mutex );
      this->stream->osd_renderer->free_object( reg->osd );
      reg->osd = NULL;
      pthread_mutex_unlock( &this->dvbsub_osd_mutex );
    }
  }

  if ( !reg->osd )
    reg->osd = this->stream->osd_renderer->new_object( this->stream->osd_renderer, reg->width, reg->height );
}

static void update_region (dvb_spu_decoder_t * this, int region_id, int region_width, int region_height, int fill, int fill_color)
{

  dvbsub_func_t *dvbsub = this->dvbsub;
  region_t *reg = &dvbsub->regions[region_id];
  page_t *page = &dvbsub->page;

  /* reject invalid sizes and set some limits ! */
  if ( region_width<=0 || region_height<=0 || region_width>SPU_MAX_WIDTH || region_height>SPU_MAX_HEIGHT ) {
    if ( reg->img ) {
      free( reg->img );
      reg->img = NULL;
    }
    lprintf("rejected region %d = %dx%d\n", region_id, region_width, region_height );
    return;
  }

  if ( reg->width*reg->height<region_width*region_height ) {
    lprintf("update size of region %d = %dx%d\n", region_id, region_width, region_height);
    if ( reg->img ) {
      free( reg->img );
      reg->img = NULL;
    }
  }

  if ( !reg->img ) {
    if ( !(reg->img=xine_xmalloc(region_width*region_height)) ) {
      lprintf( "can't allocate mem for region %d\n", region_id );
      return;
    }
    fill = 1;
  }

  if ( fill ) {
    memset( reg->img, fill_color, region_width*region_height );
    reg->empty = 1;
    lprintf("FILL REGION %d\n", region_id);
  }
  reg->width = region_width;
  reg->height = region_height;
}


static void do_plot (dvb_spu_decoder_t * this, int r, int x, int y, unsigned char pixel)
{
  int i;
  dvbsub_func_t *dvbsub = this->dvbsub;

  i = (y * dvbsub->regions[r].width) + x;
  /* do some clipping */
  if ( i<(dvbsub->regions[r].width*dvbsub->regions[r].height) ) {
    dvbsub->regions[r].img[i] = pixel;
    dvbsub->regions[r].empty = 0;
  }
}

static void plot (dvb_spu_decoder_t * this, int r, int run_length, unsigned char pixel)
{

  dvbsub_func_t *dvbsub = this->dvbsub;

  int x2 = dvbsub->x + run_length;

  while (dvbsub->x < x2) {
    do_plot (this, r, dvbsub->x, dvbsub->y, pixel);
    dvbsub->x++;
  }
}

static unsigned char next_nibble (dvb_spu_decoder_t * this)
{
  unsigned char x;
  dvbsub_func_t *dvbsub = this->dvbsub;

  if (dvbsub->nibble_flag == 0) {
    x = (dvbsub->buf[dvbsub->i] & 0xf0) >> 4;
    dvbsub->nibble_flag = 1;
  }
  else {
    x = (dvbsub->buf[dvbsub->i++] & 0x0f);
    dvbsub->nibble_flag = 0;
  }
  return (x);
}

static void decode_4bit_pixel_code_string (dvb_spu_decoder_t * this, int r, int object_id, int ofs, int n)
{
  int next_bits, switch_1, switch_2, switch_3, run_length, pixel_code;

  dvbsub_func_t *dvbsub = this->dvbsub;

  int bits;
  unsigned int data;
  int j;

  if (dvbsub->in_scanline == 0) {
    dvbsub->in_scanline = 1;
  }
  dvbsub->nibble_flag = 0;
  j = dvbsub->i + n;
  while (dvbsub->i < j) {

    bits = 0;
    pixel_code = 0;
    next_bits = next_nibble (this);

    if (next_bits != 0) {
      pixel_code = next_bits;
      plot (this, r, 1, pixel_code);
      bits += 4;
    }
    else {
      bits += 4;
      data = next_nibble (this);
      switch_1 = (data & 0x08) >> 3;
      bits++;
      if (switch_1 == 0) {
	run_length = (data & 0x07);
	bits += 3;
	if (run_length != 0) {
	  plot (this, r, run_length + 2, pixel_code);
	}
	else {
	  break;
	}
      }
      else {
	switch_2 = (data & 0x04) >> 2;
	bits++;
	if (switch_2 == 0) {
	  run_length = (data & 0x03);
	  bits += 2;
	  pixel_code = next_nibble (this);
	  bits += 4;
	  plot (this, r, run_length + 4, pixel_code);
	}
	else {
	  switch_3 = (data & 0x03);
	  bits += 2;
	  switch (switch_3) {
	  case 0:
	    plot (this, r, 1, pixel_code);
	    break;
	  case 1:
	    plot (this, r, 2, pixel_code);
	    break;
	  case 2:
	    run_length = next_nibble (this);
	    bits += 4;
	    pixel_code = next_nibble (this);
	    bits += 4;
	    plot (this, r, run_length + 9, pixel_code);
	    break;
	  case 3:
	    run_length = next_nibble (this);
	    run_length = (run_length << 4) | next_nibble (this);
	    bits += 8;
	    pixel_code = next_nibble (this);
	    bits += 4;
	    plot (this, r, run_length + 25, pixel_code);
	  }
	}
      }
    }

  }
  if (dvbsub->nibble_flag == 1) {
    dvbsub->i++;
    dvbsub->nibble_flag = 0;
  }
}

static void recalculate_trans (dvb_spu_decoder_t *this)
{
  dvbsub_func_t *const dvbsub = this->dvbsub;
  xine_spu_opacity_t opacity;
  int i;

  _x_spu_get_opacity (this->stream->xine, &opacity);
  for (i = 0; i < MAX_REGIONS * 256; ++i) {
    /* ETSI-300-743 says "full transparency if Y == 0". */
    if (dvbsub->colours[i].y == 0)
      dvbsub->trans[i] = 0;
    else {
      int v = _x_spu_calculate_opacity (&dvbsub->colours[i], dvbsub->colours[i].foo, &opacity);
      dvbsub->trans[i] = v * 14 / 255 + 1;
    }
  }

}

static void set_clut(dvb_spu_decoder_t *this,int CLUT_id,int CLUT_entry_id,int Y_value, int Cr_value, int Cb_value, int T_value) {

  dvbsub_func_t *dvbsub = this->dvbsub;

  if ((CLUT_id>=MAX_REGIONS) || (CLUT_entry_id>15)) {
    return;
  }

  dvbsub->colours[(CLUT_id*256)+CLUT_entry_id].y=Y_value;
  dvbsub->colours[(CLUT_id*256)+CLUT_entry_id].cr=Cr_value;
  dvbsub->colours[(CLUT_id*256)+CLUT_entry_id].cb=Cb_value;
  dvbsub->colours[(CLUT_id*256)+CLUT_entry_id].foo = T_value;
}

static void process_CLUT_definition_segment(dvb_spu_decoder_t *this) {
  int page_id,
      segment_length,
      CLUT_id,
      CLUT_version_number;

  int CLUT_entry_id,
      CLUT_flag_8_bit,
      CLUT_flag_4_bit,
      CLUT_flag_2_bit,
      full_range_flag,
      Y_value,
      Cr_value,
      Cb_value,
      T_value;
  dvbsub_func_t *dvbsub = this->dvbsub;

  int j;

  page_id=(dvbsub->buf[dvbsub->i]<<8)|dvbsub->buf[dvbsub->i+1]; dvbsub->i+=2;
  segment_length=(dvbsub->buf[dvbsub->i]<<8)|dvbsub->buf[dvbsub->i+1]; dvbsub->i+=2;
  j=dvbsub->i+segment_length;

  CLUT_id=dvbsub->buf[dvbsub->i++];
  CLUT_version_number=(dvbsub->buf[dvbsub->i]&0xf0)>>4;
  dvbsub->i++;

  while (dvbsub->i < j) {
    CLUT_entry_id=dvbsub->buf[dvbsub->i++];

    CLUT_flag_2_bit=(dvbsub->buf[dvbsub->i]&0x80)>>7;
    CLUT_flag_4_bit=(dvbsub->buf[dvbsub->i]&0x40)>>6;
    CLUT_flag_8_bit=(dvbsub->buf[dvbsub->i]&0x20)>>5;
    full_range_flag=dvbsub->buf[dvbsub->i]&1;
    dvbsub->i++;

    if (full_range_flag==1) {
      Y_value=dvbsub->buf[dvbsub->i++];
      Cr_value=dvbsub->buf[dvbsub->i++];
      Cb_value=dvbsub->buf[dvbsub->i++];
      T_value=dvbsub->buf[dvbsub->i++];
    } else {
      Y_value = dvbsub->buf[dvbsub->i] & 0xfc;
      Cr_value = (dvbsub->buf[dvbsub->i] << 6 | dvbsub->buf[dvbsub->i + 1] >> 2) & 0xf0;
      Cb_value = (dvbsub->buf[dvbsub->i + 1] << 2) & 0xf0;
      T_value = (dvbsub->buf[dvbsub->i + 1] & 3) * 0x55; /* expand only this one to full range! */
      dvbsub->i+=2;
    }
    set_clut(this, CLUT_id,CLUT_entry_id,Y_value,Cr_value,Cb_value,T_value);
  }
}

static void process_pixel_data_sub_block (dvb_spu_decoder_t * this, int r, int o, int ofs, int n)
{
  int data_type;
  int j;

  dvbsub_func_t *dvbsub = this->dvbsub;

  j = dvbsub->i + n;

  dvbsub->x = (dvbsub->regions[r].object_pos[o]) >> 16;
  dvbsub->y = ((dvbsub->regions[r].object_pos[o]) & 0xffff) + ofs;
  while (dvbsub->i < j) {
    data_type = dvbsub->buf[dvbsub->i++];

    switch (data_type) {
    case 0:
      dvbsub->i++;
    case 0x11:
      decode_4bit_pixel_code_string (this, r, o, ofs, n - 1);
      break;
    case 0xf0:
      dvbsub->in_scanline = 0;
      dvbsub->x = (dvbsub->regions[r].object_pos[o]) >> 16;
      dvbsub->y += 2;
      break;
    default:
      lprintf ("unimplemented data_type %02x in pixel_data_sub_block\n", data_type);
    }
  }

  dvbsub->i = j;
}

static void process_page_composition_segment (dvb_spu_decoder_t * this)
{
  int segment_length;
  int region_id, region_x, region_y;
  dvbsub_func_t *dvbsub = this->dvbsub;

  dvbsub->page.page_id = (dvbsub->buf[dvbsub->i] << 8) | dvbsub->buf[dvbsub->i + 1];
  dvbsub->i += 2;
  segment_length = (dvbsub->buf[dvbsub->i] << 8) | dvbsub->buf[dvbsub->i + 1];
  dvbsub->i += 2;

  int j = dvbsub->i + segment_length;

  dvbsub->page.page_time_out = dvbsub->buf[dvbsub->i++];
  if ( dvbsub->page.page_time_out>6 ) /* some timeout are insane, e.g. 65s ! */
    dvbsub->page.page_time_out = 6;

  int version = (dvbsub->buf[dvbsub->i] & 0xf0) >> 4;
  if ( version == dvbsub->page.page_version_number )
    return;
  dvbsub->page.page_version_number = version;
  dvbsub->page.page_state = (dvbsub->buf[dvbsub->i] & 0x0c) >> 2;
  dvbsub->i++;

  int r;
  for (r=0; r<MAX_REGIONS; r++) { /* reset */
    dvbsub->page.regions[r].is_visible = 0;
  }

  while (dvbsub->i < j) {
    region_id = dvbsub->buf[dvbsub->i++];
    dvbsub->i++;		/* reserved */
    region_x = (dvbsub->buf[dvbsub->i] << 8) | dvbsub->buf[dvbsub->i + 1];
    dvbsub->i += 2;
    region_y = (dvbsub->buf[dvbsub->i] << 8) | dvbsub->buf[dvbsub->i + 1];
    dvbsub->i += 2;

    dvbsub->page.regions[region_id].x = region_x;
    dvbsub->page.regions[region_id].y = region_y;
    dvbsub->page.regions[region_id].is_visible = 1;
  }
}


static void process_region_composition_segment (dvb_spu_decoder_t * this)
{
  int segment_length,
    region_id,
    region_version_number,
    region_fill_flag, region_width, region_height, region_level_of_compatibility, region_depth, CLUT_id, region_8_bit_pixel_code, region_4_bit_pixel_code, region_2_bit_pixel_code;
  int object_id, object_type, object_provider_flag, object_x, object_y, foreground_pixel_code, background_pixel_code;
  int j;
  int o;
  dvbsub_func_t *dvbsub = this->dvbsub;

  dvbsub->page.page_id = (dvbsub->buf[dvbsub->i] << 8) | dvbsub->buf[dvbsub->i + 1];
  dvbsub->i += 2;
  segment_length = (dvbsub->buf[dvbsub->i] << 8) | dvbsub->buf[dvbsub->i + 1];
  dvbsub->i += 2;
  j = dvbsub->i + segment_length;

  region_id = dvbsub->buf[dvbsub->i++];
  region_version_number = (dvbsub->buf[dvbsub->i] & 0xf0) >> 4;
  region_fill_flag = (dvbsub->buf[dvbsub->i] & 0x08) >> 3;
  dvbsub->i++;
  region_width = (dvbsub->buf[dvbsub->i] << 8) | dvbsub->buf[dvbsub->i + 1];
  dvbsub->i += 2;
  region_height = (dvbsub->buf[dvbsub->i] << 8) | dvbsub->buf[dvbsub->i + 1];
  dvbsub->i += 2;
  region_level_of_compatibility = (dvbsub->buf[dvbsub->i] & 0xe0) >> 5;
  region_depth = (dvbsub->buf[dvbsub->i] & 0x1c) >> 2;
  dvbsub->i++;
  CLUT_id = dvbsub->buf[dvbsub->i++];
  region_8_bit_pixel_code = dvbsub->buf[dvbsub->i++];
  region_4_bit_pixel_code = (dvbsub->buf[dvbsub->i] & 0xf0) >> 4;
  region_2_bit_pixel_code = (dvbsub->buf[dvbsub->i] & 0x0c) >> 2;
  dvbsub->i++;

  if(region_id>=MAX_REGIONS)
    return;

  if ( dvbsub->regions[region_id].version_number == region_version_number )
    return;

  dvbsub->regions[region_id].version_number = region_version_number;

  /* Check if region size has changed and fill background. */
  update_region (this, region_id, region_width, region_height, region_fill_flag, region_4_bit_pixel_code);
  if ( CLUT_id<MAX_REGIONS )
    dvbsub->regions[region_id].CLUT_id = CLUT_id;

  dvbsub->regions[region_id].objects_start = dvbsub->i;
  dvbsub->regions[region_id].objects_end = j;

  for (o = 0; o < 65536; o++) {
    dvbsub->regions[region_id].object_pos[o] = 0xffffffff;
  }

  while (dvbsub->i < j) {
    object_id = (dvbsub->buf[dvbsub->i] << 8) | dvbsub->buf[dvbsub->i + 1];
    dvbsub->i += 2;
    object_type = (dvbsub->buf[dvbsub->i] & 0xc0) >> 6;
    object_provider_flag = (dvbsub->buf[dvbsub->i] & 0x30) >> 4;
    object_x = ((dvbsub->buf[dvbsub->i] & 0x0f) << 8) | dvbsub->buf[dvbsub->i + 1];
    dvbsub->i += 2;
    object_y = ((dvbsub->buf[dvbsub->i] & 0x0f) << 8) | dvbsub->buf[dvbsub->i + 1];
    dvbsub->i += 2;

    dvbsub->regions[region_id].object_pos[object_id] = (object_x << 16) | object_y;

    if ((object_type == 0x01) || (object_type == 0x02)) {
      foreground_pixel_code = dvbsub->buf[dvbsub->i++];
      background_pixel_code = dvbsub->buf[dvbsub->i++];
    }
  }

}

static void process_object_data_segment (dvb_spu_decoder_t * this)
{
  int segment_length, object_id, object_version_number, object_coding_method, non_modifying_colour_flag;

  int top_field_data_block_length, bottom_field_data_block_length;

  dvbsub_func_t *dvbsub = this->dvbsub;

  int j;
  int old_i;
  int r;

  dvbsub->page.page_id = (dvbsub->buf[dvbsub->i] << 8) | dvbsub->buf[dvbsub->i + 1];
  dvbsub->i += 2;
  segment_length = (dvbsub->buf[dvbsub->i] << 8) | dvbsub->buf[dvbsub->i + 1];
  dvbsub->i += 2;

  object_id = (dvbsub->buf[dvbsub->i] << 8) | dvbsub->buf[dvbsub->i + 1];
  dvbsub->i += 2;
  dvbsub->curr_obj = object_id;
  object_version_number = (dvbsub->buf[dvbsub->i] & 0xf0) >> 4;
  object_coding_method = (dvbsub->buf[dvbsub->i] & 0x0c) >> 2;
  non_modifying_colour_flag = (dvbsub->buf[dvbsub->i] & 0x02) >> 1;
  dvbsub->i++;

  old_i = dvbsub->i;
  for (r = 0; r < MAX_REGIONS; r++) {

    /* If this object is in this region... */
    if (dvbsub->regions[r].img) {
      if (dvbsub->regions[r].object_pos[object_id] != 0xffffffff) {
	dvbsub->i = old_i;
	if (object_coding_method == 0) {
	  top_field_data_block_length = (dvbsub->buf[dvbsub->i] << 8) | dvbsub->buf[dvbsub->i + 1];
	  dvbsub->i += 2;
	  bottom_field_data_block_length = (dvbsub->buf[dvbsub->i] << 8) | dvbsub->buf[dvbsub->i + 1];
	  dvbsub->i += 2;

	  process_pixel_data_sub_block (this, r, object_id, 0, top_field_data_block_length);

	  process_pixel_data_sub_block (this, r, object_id, 1, bottom_field_data_block_length);
	}
      }
    }
  }
}

static void unlock_mutex_cancellation_func(void *mutex_gen)
{
  pthread_mutex_t *mutex = (pthread_mutex_t*) mutex_gen;
  pthread_mutex_unlock(mutex);
}

/* Thread routine that checks for subtitle timeout periodically.
   To avoid unexpected subtitle hiding, calls to this->stream->osd_renderer->show()
   should be in blocks like:

   pthread_mutex_lock(&this->dvbsub_osd_mutex);
   this->stream->osd_renderer->show(...);
   this->dvbsub_hide_timeout.tv_sec = time(NULL) + timeout value;
   pthread_cond_signal(&this->dvbsub_restart_timeout);
   pthread_mutex_unlock(&this->dvbsub_osd_mutex);

   This ensures that the timeout is changed with the lock held, and
   that the thread is signalled to pick up the new timeout.
*/
static void* dvbsub_timer_func(void *this_gen)
{
  dvb_spu_decoder_t *this = (dvb_spu_decoder_t *) this_gen;
  pthread_mutex_lock(&this->dvbsub_osd_mutex);
  int i;

 /* If we're cancelled via pthread_cancel, unlock the mutex */
  pthread_cleanup_push(unlock_mutex_cancellation_func, &this->dvbsub_osd_mutex);

  while(1)
  {
    /* Record the current timeout, and wait - note that pthread_cond_timedwait
       will unlock the mutex on entry, and lock it on exit */
    struct timespec timeout = this->dvbsub_hide_timeout;
    int result = pthread_cond_timedwait(&this->dvbsub_restart_timeout,
                                        &this->dvbsub_osd_mutex,
                                        &this->dvbsub_hide_timeout);
    if(result == ETIMEDOUT &&
       timeout.tv_sec == this->dvbsub_hide_timeout.tv_sec &&
       timeout.tv_nsec == this->dvbsub_hide_timeout.tv_nsec)
    {
      /* We timed out, and no-one changed the timeout underneath us.
         Hide the OSD, then wait until we're signalled. */
      if(this && this->stream && this->stream->osd_renderer)
      {
	for ( i=0; i<MAX_REGIONS; i++ ) {
	  if ( this->dvbsub->regions[i].osd ) {
	    this->stream->osd_renderer->hide( this->dvbsub->regions[i].osd, 0 );
	    lprintf("thread hiding = %d\n",i);
	  }
	}
      }
      pthread_cond_wait(&this->dvbsub_restart_timeout, &this->dvbsub_osd_mutex);
    }
  }

  pthread_cleanup_pop(1);
  return NULL;
}

static void downscale_region_image( region_t *reg, unsigned char *dest, int dest_width )
{
  float i, k, inc=reg->width/(float)dest_width;
  int j;
  for ( j=0; j<reg->height; j++ ) {
    for ( i=0,k=0; i<reg->width && k<dest_width; i+=inc,k++ ) {
      dest[(j*dest_width)+(int)k] = reg->img[(j*reg->width)+(int)i];
    }
  }
}

static void draw_subtitles (dvb_spu_decoder_t * this)
{
  int r;
  int display=0;
  int64_t dum;
  int dest_width=0, dest_height, reg_width;
  this->stream->video_out->status(this->stream->video_out, NULL, &dest_width, &dest_height, &dum);
  unsigned char tmp[dest_width*576];
  unsigned char *reg;

  if ( !dest_width || !dest_height )
    return;

  /* render all regions onto the page */

  for ( r=0; r<MAX_REGIONS; r++ ) {
    if ( this->dvbsub->page.regions[r].is_visible )
      display++;
  }
  if ( !display )
    return;

  for (r = 0; r < MAX_REGIONS; r++) {
    if (this->dvbsub->regions[r].img) {
      if (this->dvbsub->page.regions[r].is_visible && !this->dvbsub->regions[r].empty) {
        update_osd( this, r );
	if ( !this->dvbsub->regions[r].osd )
	  continue;
        /* clear osd */
        this->stream->osd_renderer->clear( this->dvbsub->regions[r].osd );
        if ( this->dvbsub->regions[r].width>dest_width && !(this->stream->video_driver->get_capabilities(this->stream->video_driver) & VO_CAP_CUSTOM_EXTENT_OVERLAY)) {
	  downscale_region_image(&this->dvbsub->regions[r], tmp, dest_width);
	  reg = tmp;
	  reg_width = dest_width;
	}
	else {
	  reg = this->dvbsub->regions[r].img;
	  reg_width = this->dvbsub->regions[r].width;
	}
	this->stream->osd_renderer->set_palette( this->dvbsub->regions[r].osd, (uint32_t*)(&this->dvbsub->colours[this->dvbsub->regions[r].CLUT_id*256]), &this->dvbsub->trans[this->dvbsub->regions[r].CLUT_id*256]);
	this->stream->osd_renderer->draw_bitmap( this->dvbsub->regions[r].osd, reg, 0, 0, reg_width, this->dvbsub->regions[r].height, NULL );
      }
    }
  }

  pthread_mutex_lock(&this->dvbsub_osd_mutex);
  lprintf("this->vpts=%llu\n",this->vpts);
  for ( r=0; r<MAX_REGIONS; r++ ) {
    lprintf("region=%d, visible=%d, osd=%d, empty=%d\n", r, this->dvbsub->page.regions[r].is_visible, this->dvbsub->regions[r].osd?1:0, this->dvbsub->regions[r].empty );
    if ( this->dvbsub->page.regions[r].is_visible && this->dvbsub->regions[r].osd && !this->dvbsub->regions[r].empty ) {
      this->stream->osd_renderer->set_extent(this->dvbsub->regions[r].osd, SPU_MAX_WIDTH, SPU_MAX_HEIGHT);
      this->stream->osd_renderer->set_position( this->dvbsub->regions[r].osd, this->dvbsub->page.regions[r].x, this->dvbsub->page.regions[r].y );
      this->stream->osd_renderer->show( this->dvbsub->regions[r].osd, this->vpts );
      lprintf("show region = %d\n",r);
    }
    else {
      if ( this->dvbsub->regions[r].osd ) {
        this->stream->osd_renderer->hide( this->dvbsub->regions[r].osd, this->vpts );
        lprintf("hide region = %d\n",r);
      }
    }
  }
  this->dvbsub_hide_timeout.tv_nsec = 0;
  this->dvbsub_hide_timeout.tv_sec = time(NULL) + this->dvbsub->page.page_time_out;
  lprintf("page_time_out %d\n",this->dvbsub->page.page_time_out);
  pthread_cond_signal(&this->dvbsub_restart_timeout);
  pthread_mutex_unlock(&this->dvbsub_osd_mutex);
}


static void spudec_decode_data (spu_decoder_t * this_gen, buf_element_t * buf)
{
  dvb_spu_decoder_t *this = (dvb_spu_decoder_t *) this_gen;
      int new_i;
      int data_identifier, subtitle_stream_id;
      int segment_length, segment_type;
      int PES_header_data_length;
      int PES_packet_length;
      int i;

  if((buf->type & 0xffff0000)!=BUF_SPU_DVB)
    return;

  if (buf->decoder_flags & BUF_FLAG_SPECIAL) {
    if (buf->decoder_info[1] == BUF_SPECIAL_SPU_DVB_DESCRIPTOR) {
      if (buf->decoder_info[2] == 0) {
        /* Hide the osd - note that if the timeout thread times out, it'll rehide, which is harmless */
        pthread_mutex_lock(&this->dvbsub_osd_mutex);
	for ( i=0; i<MAX_REGIONS; i++ ) {
	  if ( this->dvbsub->regions[i].osd )
	    this->stream->osd_renderer->hide( this->dvbsub->regions[i].osd, 0 );
	}
        pthread_mutex_unlock(&this->dvbsub_osd_mutex);
      }
      else {
	xine_fast_memcpy (this->spu_descriptor, buf->decoder_info_ptr[2], buf->decoder_info[2]);
      }
    }
    return;
  }
  else {
    if (buf->decoder_info[2]) {
      memset (this->pes_pkt, 0xff, 64*1024);
      this->pes_pkt_wrptr = this->pes_pkt;
      this->pes_pkt_size = buf->decoder_info[2];
      this->pts = buf->pts;

      xine_fast_memcpy (this->pes_pkt, buf->content, buf->size);
      this->pes_pkt_wrptr += buf->size;
    }
    else {
      if (this->pes_pkt && (this->pes_pkt_wrptr != this->pes_pkt)) {
	xine_fast_memcpy (this->pes_pkt_wrptr, buf->content, buf->size);
	this->pes_pkt_wrptr += buf->size;
      }
    }
  }
  /* don't ask metronom for a vpts but rather do the calculation
   * because buf->pts could be too far in future and metronom won't accept
   * further backwards pts (see metronom_got_spu_packet) */
  if (buf->pts) {
    metronom_t *metronom = this->stream->metronom;
    int64_t vpts_offset = metronom->get_option( metronom, METRONOM_VPTS_OFFSET );
    int64_t spu_offset = metronom->get_option( metronom, METRONOM_SPU_OFFSET );
    int64_t vpts = (int64_t)(buf->pts)+vpts_offset+spu_offset;
    metronom_clock_t *clock = this->stream->xine->clock;
    int64_t curvpts = clock->get_current_time( clock );
    /* if buf->pts is unreliable, show page asap (better than nothing) */
    lprintf("spu_vpts=%lld - current_vpts=%lld\n", vpts, curvpts);
    if ( vpts<=curvpts || (vpts-curvpts)>(5*90000) )
      this->vpts = 0;
    else
      this->vpts = vpts;
  }

  /* completely ignore pts since it makes a lot of problems with various providers */
  this->vpts = 0;

  /* process the pes section */

      PES_packet_length = this->pes_pkt_size;

      this->dvbsub->buf = this->pes_pkt;

      this->dvbsub->i = 0;

      data_identifier = this->dvbsub->buf[this->dvbsub->i++];
      subtitle_stream_id = this->dvbsub->buf[this->dvbsub->i++];

      while (this->dvbsub->i <= (PES_packet_length)) {
	/* SUBTITLING SEGMENT */
	this->dvbsub->i++;
	segment_type = this->dvbsub->buf[this->dvbsub->i++];

	this->dvbsub->page.page_id = (this->dvbsub->buf[this->dvbsub->i] << 8) | this->dvbsub->buf[this->dvbsub->i + 1];
	segment_length = (this->dvbsub->buf[this->dvbsub->i + 2] << 8) | this->dvbsub->buf[this->dvbsub->i + 3];
	new_i = this->dvbsub->i + segment_length + 4;

	/* only process complete segments */
	if(new_i > (this->pes_pkt_wrptr - this->pes_pkt))
	  break;
	/* verify we've the right segment */
	if(this->dvbsub->page.page_id==this->spu_descriptor->comp_page_id){
	  /* SEGMENT_DATA_FIELD */
	  switch (segment_type & 0xff) {
	    case 0x10:
	      process_page_composition_segment (this);
              break;
            case 0x11:
              process_region_composition_segment (this);
              break;
            case 0x12:
              process_CLUT_definition_segment(this);
              break;
            case 0x13:
              process_object_data_segment (this);
              break;
            case 0x80:		/* Page is now completely rendered */
              recalculate_trans(this);
	      draw_subtitles( this );
              break;
            default:
              return;
              break;
          }
	}
	this->dvbsub->i = new_i;
      }

      /* verify we've the right segment */
      if(this->dvbsub->page.page_id==this->spu_descriptor->comp_page_id) {
	/* SEGMENT_DATA_FIELD */
	switch (segment_type) {
	case 0x10:
	  process_page_composition_segment(this);
	  break;
	case 0x11:
	  process_region_composition_segment(this);
	  break;
	case 0x12:
	  process_CLUT_definition_segment(this);
	  break;
	case 0x13:
	  process_object_data_segment (this);
	  break;
	case 0x80:
	  draw_subtitles( this ); /* Page is now completely rendered */
	  break;
	default:
	  return;
	  break;
	}
      }
      this->dvbsub->i = new_i;
}

static void spudec_reset (spu_decoder_t * this_gen)
{
  dvb_spu_decoder_t *this = (dvb_spu_decoder_t *) this_gen;
  int i;

  /* Hide the osd - if the timeout thread times out, it'll rehide harmlessly */
  pthread_mutex_lock(&this->dvbsub_osd_mutex);
  for ( i=0; i<MAX_REGIONS; i++ ) {
    if ( this->dvbsub->regions[i].osd )
      this->stream->osd_renderer->hide(this->dvbsub->regions[i].osd, 0);
    this->dvbsub->regions[i].version_number = -1;
  }
  this->dvbsub->page.page_version_number = -1;
  pthread_mutex_unlock(&this->dvbsub_osd_mutex);

}

static void spudec_discontinuity (spu_decoder_t * this_gen)
{
  /* do nothing */
}

static void spudec_dispose (spu_decoder_t * this_gen)
{
  dvb_spu_decoder_t *this = (dvb_spu_decoder_t *) this_gen;
  int i;

  pthread_cancel(this->dvbsub_timer_thread);
  pthread_join(this->dvbsub_timer_thread, NULL);
  pthread_mutex_destroy(&this->dvbsub_osd_mutex);
  pthread_cond_destroy(&this->dvbsub_restart_timeout);

  if(this->spu_descriptor){
    free(this->spu_descriptor);
    this->spu_descriptor=NULL;
  }

  for ( i=0; i<MAX_REGIONS; i++ ) {
    if ( this->dvbsub->regions[i].img )
      free( this->dvbsub->regions[i].img );
    if ( this->dvbsub->regions[i].osd )
      this->stream->osd_renderer->free_object( this->dvbsub->regions[i].osd );
  }

  if (this->pes_pkt)
    free (this->pes_pkt);

  if (this->dvbsub)
    free (this->dvbsub);

  free (this);
}

static spu_decoder_t *dvb_spu_class_open_plugin (spu_decoder_class_t * class_gen, xine_stream_t * stream)
{
  dvb_spu_decoder_t *this = calloc(1, sizeof (dvb_spu_decoder_t));
  dvb_spu_class_t *class = (dvb_spu_class_t *)class_gen;

  this->spu_decoder.decode_data = spudec_decode_data;
  this->spu_decoder.reset = spudec_reset;
  this->spu_decoder.discontinuity = spudec_discontinuity;
  this->spu_decoder.dispose = spudec_dispose;
  this->spu_decoder.get_interact_info = NULL;
  this->spu_decoder.set_button = NULL;

  this->class = class;
  this->stream = stream;

  this->pes_pkt = calloc(65, 1024);
  this->spu_descriptor = calloc(1, sizeof(spu_dvb_descriptor_t));

  this->dvbsub = calloc(1, sizeof (dvbsub_func_t));

  int i;
  for (i = 0; i < MAX_REGIONS; i++) {
    this->dvbsub->page.regions[i].is_visible = 0;
    this->dvbsub->regions[i].img = NULL;
    this->dvbsub->regions[i].osd = NULL;
    this->dvbsub->regions[i].CLUT_id = 0;
  }

  {
    xine_spu_opacity_t opacity;
    static const clut_t black = { 0, 0, 0, 0 };
    int t;

    _x_spu_get_opacity (this->stream->xine, &opacity);
    t = _x_spu_calculate_opacity (&black, 0, &opacity);

    for (i = 0; i < MAX_REGIONS * 256; i++)
      this->dvbsub->colours[i].foo = t;
  }

  pthread_mutex_init(&this->dvbsub_osd_mutex, NULL);
  pthread_cond_init(&this->dvbsub_restart_timeout, NULL);
  this->dvbsub_hide_timeout.tv_nsec = 0;
  this->dvbsub_hide_timeout.tv_sec = time(NULL);
  pthread_create(&this->dvbsub_timer_thread, NULL, dvbsub_timer_func, this);

  return (spu_decoder_t *) this;
}

static void *init_spu_decoder_plugin (xine_t * xine, void *data)
{

  dvb_spu_class_t *this;
  this = calloc(1, sizeof (dvb_spu_class_t));

  this->class.open_plugin = dvb_spu_class_open_plugin;
  this->class.identifier  = "spudvb";
  this->class.description = N_("DVB subtitle decoder plugin");
  this->class.dispose = default_spu_decoder_class_dispose;

  this->xine = xine;

  return &this->class;
}


/* plugin catalog information */
static const uint32_t supported_types[] = { BUF_SPU_DVB, 0 };

static const decoder_info_t spudec_info = {
  supported_types,		/* supported types */
  1				/* priority        */
};

const plugin_info_t xine_plugin_info[] EXPORTED = {
/* type, API, "name", version, special_info, init_function */
  {PLUGIN_SPU_DECODER, 17, "spudvb", XINE_VERSION_CODE, &spudec_info,
   &init_spu_decoder_plugin},
  {PLUGIN_NONE, 0, "", 0, NULL, NULL}
};
