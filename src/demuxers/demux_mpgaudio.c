/*
 * Copyright (C) 2000-2003 the xine project
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
 * $Id: demux_mpgaudio.c,v 1.108 2003/09/22 23:16:14 tmattern Exp $
 *
 * demultiplexer for mpeg audio (i.e. mp3) streams
 *
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>


/********** logging **********/
#define LOG_MODULE "demux_mpeg_audio"
#define LOG_VERBOSE
/*
#define LOG
*/
#include "xine_internal.h"
#include "xineutils.h"
#include "demux.h"
#include "compat.h"
#include "bswap.h"
#include "group_audio.h"

#define NUM_PREVIEW_BUFFERS  10
#define SNIFF_BUFFER_LENGTH 1024

#define WRAP_THRESHOLD       120000

#define FOURCC_TAG( ch0, ch1, ch2, ch3 )                                \
        ( (long)(unsigned char)(ch3) | ( (long)(unsigned char)(ch2) << 8 ) | \
        ( (long)(unsigned char)(ch1) << 16 ) | ( (long)(unsigned char)(ch0) << 24 ) )

#define RIFF_CHECK_BYTES 1024
#define RIFF_TAG FOURCC_TAG('R', 'I', 'F', 'F')
#define AVI_TAG FOURCC_TAG('A', 'V', 'I', ' ')
#define CDXA_TAG FOURCC_TAG('C', 'D', 'X', 'A')
#define ID3V2_TAG FOURCC_TAG('I', 'D', '3', 0)  /* ID3 v2.x.y tags */

typedef struct {

  demux_plugin_t       demux_plugin;

  xine_stream_t       *stream;

  fifo_buffer_t       *audio_fifo;

  input_plugin_t      *input;

  int                  status;

  int                  stream_length;
  long                 bitrate;
  int64_t              last_pts;
  int                  send_newpts;
  int                  buf_flag_seek;
  uint32_t             blocksize;

} demux_mpgaudio_t ;

typedef struct {

  demux_class_t     demux_class;

  /* class-wide, global variables here */

  xine_t           *xine;

} demux_mpgaudio_class_t;

/* bitrate table tabsel_123[mpeg version][layer][bitrate index]
 * values stored in kbps
 */
const int tabsel_123[2][3][16] = {
   { {0,32,64,96,128,160,192,224,256,288,320,352,384,416,448,},
     {0,32,48,56, 64, 80, 96,112,128,160,192,224,256,320,384,},
     {0,32,40,48, 56, 64, 80, 96,112,128,160,192,224,256,320,} },

   { {0,32,48,56,64,80,96,112,128,144,160,176,192,224,256,},
     {0, 8,16,24,32,40,48, 56, 64, 80, 96,112,128,144,160,},
     {0, 8,16,24,32,40,48, 56, 64, 80, 96,112,128,144,160,} }
};

static int frequencies[2][3] = {
	{ 44100, 48000, 32000 },
	{ 22050, 24000, 16000 }
};


static int mpg123_head_check(unsigned long head) {
  if ((head & 0xffe00000) != 0xffe00000)
    return 0;
  if (!((head >> 17) & 3))
    return 0;
  if (((head >> 12) & 0xf) == 0xf)
    return 0;
  if (!((head >> 12) & 0xf))
    return 0;
  if (((head >> 10) & 0x3) == 0x3)
    return 0;
  if (((head >> 19) & 1) == 1 
      && ((head >> 17) & 3) == 3 
      && ((head >> 16) & 1) == 1)
    return 0;
  if ((head & 0xffff0000) == 0xfffe0000)
    return 0;
  
  return 1;
}

static int mpg123_xhead_check(unsigned char *buf)
{
  if (buf[3] != 'X')
    return 0;
  if (buf[2] != 'i')
    return 0;
  if (buf[1] != 'n')
    return 0;
  if (buf[0] != 'g')
    return 0;

  return 1;
}


/* Return length of an MP3 frame using potential 32-bit header value.  See
 * "http://www.dv.co.yu/mpgscript/mpeghdr.htm" for details on the header
 * format.
 *
 * NOTE: As an optimization and because they are rare, this returns 0 for
 * version 2.5 or free format MP3s.
 */
static size_t get_mp3_frame_length (unsigned long mp3_header)
{
  int ver = 4 - ((mp3_header >> 19) & 3u);
  int br = (mp3_header >> 12) & 0xfu;
  int srf = (mp3_header >> 10) & 3u;

  /* are frame sync and layer 3 bits set? */
  if (((mp3_header & 0xffe20000ul) == 0xffe20000ul)
    /* good version? */
      && ((ver == 1) || (ver == 2))
      /* good bitrate index (not free or invalid)? */
      && (br > 0) && (br < 15)
      /* good sampling rate frequency index? */
      && (srf != 3)
      /* not using reserved emphasis value? */
      && ((mp3_header & 3u) != 2)) {

    /* then this is most likely the beginning of a valid frame */
    size_t length = (size_t) tabsel_123[ver - 1][2][br] * 144000;
    length /= frequencies[ver - 1][srf];
    return length += ((mp3_header >> 9) & 1u) - 4;
  }
  return 0;
}

static unsigned char * demux_mpgaudio_read_buffer_header (input_plugin_t *input)
{
  int count;
  uint8_t buf[MAX_PREVIEW_SIZE];
  unsigned char *retval;

  if(!input)
    return 0;

  if((input->get_capabilities(input) & INPUT_CAP_SEEKABLE) != 0) {
    input->seek(input, 0, SEEK_SET);

    count = input->read(input, buf, SNIFF_BUFFER_LENGTH);
    if (count < SNIFF_BUFFER_LENGTH)
    {
      return NULL;
    }
  } else if ((input->get_capabilities(input) & INPUT_CAP_PREVIEW) != 0) {
    input->get_optional_data (input, buf, INPUT_OPTIONAL_DATA_PREVIEW);
  } else {
    return NULL;
  }

  retval = xine_xmalloc (SNIFF_BUFFER_LENGTH);
  memcpy (retval, buf, SNIFF_BUFFER_LENGTH);

  return retval;
}

/* Scan through the first SNIFF_BUFFER_LENGTH bytes of the
 * buffer to find a potential 32-bit MP3 frame header. */
static int _sniff_buffer_looks_like_mp3 (input_plugin_t *input)
{
  unsigned long mp3_header;
  int offset;
  unsigned char *buf;

  buf = demux_mpgaudio_read_buffer_header (input);
  if (buf == NULL)
    return 0;

  mp3_header = 0;
  for (offset = 0; offset < SNIFF_BUFFER_LENGTH; offset++) {
    size_t length;

    mp3_header <<= 8;
    mp3_header |= buf[offset];
    mp3_header &= 0xfffffffful;

    length = get_mp3_frame_length (mp3_header);

    if (length != 0) {
      /* Since one frame is available, is there another frame
       * just to be sure this is more likely to be a real MP3
       * buffer? */
      offset += 1 + length;

      if(((mp3_header >> 16) & 1) == 1)
        offset -= 2;

      if (offset + 4 > SNIFF_BUFFER_LENGTH)
      {
        free (buf);
	return 0;
      }

      mp3_header = BE_32(&buf[offset]);
      length = get_mp3_frame_length (mp3_header);

      if (length != 0) {
        free (buf);
	return 1;
      }
      break;
    }
  }

  free (buf);

  return 0;
}

struct id3v1_tag_s {
  char tag[3];
  char title[30];
  char artist[30];
  char album[30];
  char year[4];
  char comment[30];
  char genre;
};

static void chomp (char *str) {

  int i,len;

  len = strlen(str);
  i = len - 1;
  
  while (((unsigned char)str[i] <= 32) && (i >= 0)) {
    str[i] = 0;
    i--;
  }
}

static void read_id3_tags (demux_mpgaudio_t *this) {

  off_t len;
  struct id3v1_tag_s tag;

  /* id3v1 */
  len = this->input->read (this->input, (char *)&tag, 128);

  if (len > 0) {

    if ( (tag.tag[0]=='T') && (tag.tag[1]=='A') && (tag.tag[2]=='G') ) {

      lprintf("id3v1 tag found\n");

      tag.title[29]   = 0;
      tag.artist[29]  = 0;
      tag.album[29]   = 0;
      tag.comment[29] = 0;

      chomp (tag.title);
      chomp (tag.artist);
      chomp (tag.album);
      chomp (tag.comment);

      this->stream->meta_info [XINE_META_INFO_TITLE]
	= strdup (tag.title);
      this->stream->meta_info [XINE_META_INFO_ARTIST]
	= strdup (tag.artist);
      this->stream->meta_info [XINE_META_INFO_ALBUM]
	= strdup (tag.album);
      this->stream->meta_info [XINE_META_INFO_COMMENT]
	= strdup (tag.comment);
    }
  }
}

static void mpg123_decode_header(demux_mpgaudio_t *this, unsigned long newhead) {

  int lsf, mpeg25;
  int lay, bitrate_index;
  char *ver;

  /*
   * lsf==0 && mpeg25==0 : MPEG Version 1 (ISO/IEC 11172-3)
   * lsf==1 && mpeg25==0 : MPEG Version 2 (ISO/IEC 13818-3)
   * lsf==1 && mpeg25==1 : MPEG Version 2.5 (later extension of MPEG�2)
   */

  mpeg25 = !((newhead >> 20) & 1);              /* mpeg 2.5 ext */
  lsf    = !((newhead >> 19) & 1);              /* lsf ext */

  if (mpeg25)
    ver = "2.5";
  else
    ver = (lsf) ? "2" : "1";

  /* Layer I, II, III */
  lay = 4 - ((newhead >> 17) & 3);
  bitrate_index = ((newhead >> 12) & 0xf);

  this->bitrate = tabsel_123[lsf][lay - 1][bitrate_index] * 1000;
  
  if( !this->bitrate ) /* bitrate can't be zero, default to 128 */
    this->bitrate = 128000;

  this->stream->stream_info[XINE_STREAM_INFO_BITRATE] = this->bitrate;
  lprintf("mpeg %s audio layer %d\n", ver, lay);
  lprintf("bitrate: %ld\n", this->bitrate);
  this->stream_length = (int)(this->input->get_length(this->input) / (this->bitrate / 8));
  lprintf("stream_length: %d s\n", this->stream_length);
}

static void check_newpts( demux_mpgaudio_t *this, int64_t pts ) {

  int64_t diff;

  diff = pts - this->last_pts;

  if( pts &&
      (this->send_newpts || (this->last_pts && abs(diff)>WRAP_THRESHOLD) ) ) {
    if (this->buf_flag_seek) {
      xine_demux_control_newpts(this->stream, pts, BUF_FLAG_SEEK);
      this->buf_flag_seek = 0;
    } else {
      xine_demux_control_newpts(this->stream, pts, 0);
    }
    this->send_newpts = 0;
  }

  if( pts )
    this->last_pts = pts;
}

static int demux_mpgaudio_next (demux_mpgaudio_t *this, int decoder_flags) {

  buf_element_t *buf;
  uint32_t       blocksize;
  uint32_t       head;
  off_t          buffer_pos;
  off_t          done;
  uint64_t       pts = 0;

  buffer_pos = this->input->get_current_pos(this->input);

  if (!this->audio_fifo)
    return 0;

  buf = this->audio_fifo->buffer_pool_alloc(this->audio_fifo);

  blocksize = (this->blocksize ? this->blocksize : buf->max_size);
  done = this->input->read(this->input, buf->mem, blocksize);

  if (done <= 0) {
    buf->free_buffer(buf);
    return 0;
  }

  if (this->bitrate == 0) {
    int i, ver, srindex, brindex, xbytes, xframes;

    for( i = 0; i < done-4; i++ ) {
      head = (buf->mem[i+0] << 24) + (buf->mem[i+1] << 16) +
             (buf->mem[i+2] << 8) + buf->mem[i+3];

      if (mpg123_head_check(head)) {
         mpg123_decode_header(this,head);
         break;
      }
    }
    /* Now check for the Xing header to get the correct bitrate */
    ver = (buf->mem[i+1] & 0x08) >> 3;
    brindex = (buf->mem[i+2] & 0xf0) >> 4;
    srindex = (buf->mem[i+2] & 0x0c) >> 2;

    for( i = 0; i < buf->size-16; i++ ) {
      head = (buf->mem[i+0] << 24) + (buf->mem[i+1] << 16) +
             (buf->mem[i+2] << 8) + buf->mem[i+3];

      if (mpg123_xhead_check((unsigned char *)&head)) {
        long long total_bytes, magic1, magic2;

        xframes = BE_32(buf->mem+i+8);
	xbytes = BE_32(buf->mem+i+12);

	if (xframes <= 0) {
          break;
	}

	total_bytes = (long long) frequencies[!ver][srindex] * (long long) xbytes;
	magic1 = total_bytes / (long long) (576 + ver * 576);
	magic2 = magic1 / (long long) xframes;

	/* 1 kbit = 1000 bits ! (and not 1024 bits) */
	this->bitrate = (int) ((long long) magic2 / (long long) 125) * 1000;

	this->stream->stream_info[XINE_STREAM_INFO_BITRATE] = this->bitrate;

	this->stream_length = (int)(this->input->get_length(this->input) / (this->bitrate / 8));
	break;
      }
    }
  }

  buf->pts = 0;
  if (this->bitrate) {
    pts = (90000 * buffer_pos) / (this->bitrate / 8);
    check_newpts(this, pts);
  }
#if 0
  buf->pts = pts;
#endif

  buf->extra_info->input_pos       = this->input->get_current_pos(this->input);
  {
    int len = this->input->get_length(this->input);
    if (len>0)
      buf->extra_info->input_time = (int)((int64_t)buf->extra_info->input_pos 
                                          * this->stream_length * 1000 / len);
    else 
      buf->extra_info->input_time = pts / 90;
  }
#if 0
  buf->pts             = pts;
#endif
  buf->size            = done;
  buf->content         = buf->mem;
  buf->type            = BUF_AUDIO_MPEG;
  buf->decoder_info[0] = 1;
  buf->decoder_flags   = decoder_flags;

  this->audio_fifo->put(this->audio_fifo, buf);

  return 1;
}

static int demux_mpgaudio_send_chunk (demux_plugin_t *this_gen) {

  demux_mpgaudio_t *this = (demux_mpgaudio_t *) this_gen;

  if (!demux_mpgaudio_next (this, 0))
    this->status = DEMUX_FINISHED;

  return this->status;
}

static int demux_mpgaudio_get_status (demux_plugin_t *this_gen) {
  demux_mpgaudio_t *this = (demux_mpgaudio_t *) this_gen;

  return this->status;
}

static uint32_t demux_mpgaudio_read_head(input_plugin_t *input, uint8_t *buf) {

  uint32_t  head=0;
  int       bs = 0;
  int       i, optional;

  if(!input)
    return 0;

  if((input->get_capabilities(input) & INPUT_CAP_SEEKABLE) != 0) {
    input->seek(input, 0, SEEK_SET);

    if (input->get_capabilities (input) & INPUT_CAP_BLOCK)
      bs = input->get_blocksize(input);

    if(!bs)
      bs = MAX_PREVIEW_SIZE;

    if(input->read(input, buf, bs))
      head = (buf[0] << 24) + (buf[1] << 16) + (buf[2] << 8) + buf[3];

    lprintf("stream is seekable\n");

  } else if ((input->get_capabilities(input) & INPUT_CAP_PREVIEW) != 0) {

    lprintf("input plugin provides preview\n");

    optional = input->get_optional_data (input, buf, INPUT_OPTIONAL_DATA_PREVIEW);
    optional = optional > 256 ? 256 : optional;

    lprintf("got preview %02x %02x %02x %02x\n",
	    buf[0], buf[1], buf[2], buf[3]);
    
    for(i = 0; i < (optional - 4); i++) {
      head = (buf[i] << 24) + (buf[i + 1] << 16) + (buf[i + 2] << 8) + buf[i + 3];
      if(head == RIFF_TAG)
	return head;
    }

    head = (buf[0] << 24) + (buf[1] << 16) + (buf[2] << 8) + buf[3];
    
  } else {
    lprintf("not seekable, no preview\n");
    return 0;
  }

  return head;
}

static void demux_mpgaudio_send_headers (demux_plugin_t *this_gen) {

  demux_mpgaudio_t *this = (demux_mpgaudio_t *) this_gen;
  uint8_t           buf[MAX_PREVIEW_SIZE];
  int i;

  this->stream_length = 0;
  this->bitrate       = 0;
  this->last_pts      = 0;
  this->status        = DEMUX_OK;

  this->stream->stream_info[XINE_STREAM_INFO_HAS_VIDEO] = 0;
  this->stream->stream_info[XINE_STREAM_INFO_HAS_AUDIO] = 1;


  if ((this->input->get_capabilities(this->input) & INPUT_CAP_SEEKABLE) != 0) {
    uint32_t head;
    off_t pos;

    head = demux_mpgaudio_read_head(this->input, buf);
    if (mpg123_head_check(head))
      mpg123_decode_header(this, head);

    /* check ID3 v1 at the end of the stream */
    pos = this->input->get_length(this->input) - 128;
    this->input->seek (this->input, pos, SEEK_SET);
    read_id3_tags (this);
  }

  this->blocksize = this->input->get_blocksize(this->input);

  /*
   * send preview buffers
   */
  xine_demux_control_start (this->stream);

  if ((this->input->get_capabilities(this->input) & INPUT_CAP_SEEKABLE) != 0)
    this->input->seek (this->input, 0, SEEK_SET);

  for (i = 0; i < NUM_PREVIEW_BUFFERS; i++) {
    if (!demux_mpgaudio_next (this, BUF_FLAG_PREVIEW)) {
      break;
    }
  }
  this->status = DEMUX_OK;
}

static int demux_mpgaudio_seek (demux_plugin_t *this_gen,
				 off_t start_pos, int start_time) {

  demux_mpgaudio_t *this = (demux_mpgaudio_t *) this_gen;
  start_time /= 1000;

  if ((this->input->get_capabilities(this->input) & INPUT_CAP_SEEKABLE) != 0) {

    if (!start_pos && start_time && this->stream_length > 0)
         start_pos = start_time * this->input->get_length(this->input) /
                     this->stream_length;

    this->input->seek (this->input, start_pos, SEEK_SET);
  }

  this->status = DEMUX_OK;
  this->send_newpts = 1;

  if( !this->stream->demux_thread_running ) {
    this->buf_flag_seek = 0;
  }
  else {
    this->buf_flag_seek = 1;
    xine_demux_flush_engine(this->stream);
  }

  return this->status;
}

static void demux_mpgaudio_dispose (demux_plugin_t *this) {

  free (this);
}

static int demux_mpgaudio_get_stream_length (demux_plugin_t *this_gen) {
  demux_mpgaudio_t *this = (demux_mpgaudio_t *) this_gen;

  if (this->stream_length > 0) {
    return this->stream_length * 1000;
  } else
    return 0;
}

static uint32_t demux_mpgaudio_get_capabilities(demux_plugin_t *this_gen) {
  return DEMUX_CAP_NOCAP;
}

static int demux_mpgaudio_get_optional_data(demux_plugin_t *this_gen,
					void *data, int data_type) {
  return DEMUX_OPTIONAL_UNSUPPORTED;
} 

static demux_plugin_t *open_plugin (demux_class_t *class_gen, xine_stream_t *stream,
				    input_plugin_t *input_gen) {

  demux_mpgaudio_t *this;
  input_plugin_t   *input = (input_plugin_t *) input_gen;
  uint8_t           buf[MAX_PREVIEW_SIZE];
  uint8_t          *riff_check;
  int               i;
  uint8_t          *ptr;

  lprintf("trying to open %s...\n", input->get_mrl(input));

  switch (stream->content_detection_method) {

  case METHOD_BY_CONTENT: {
    uint32_t head;

    head = demux_mpgaudio_read_head (input, buf);

    lprintf("head is %x\n", head);
    
    if (head == RIFF_TAG) {
      int ok;

      lprintf("**** found RIFF tag\n");
      /* skip the length */
      ptr = buf + 8;

      riff_check = ptr; ptr += 4;
      if ((buf + MAX_PREVIEW_SIZE) < ptr)
        return NULL;

      /* disqualify the file if it is, in fact, an AVI file or has a CDXA
       * marker */
      if ((BE_32(riff_check) == AVI_TAG) ||
          (BE_32(riff_check) == CDXA_TAG)) {
        lprintf("**** found AVI or CDXA tag\n");
        return NULL;
      }

      /* skip 4 more bytes */
      ptr += 4;

      /* get the length of the next chunk */
      riff_check = ptr; ptr += 4;
      if ((buf + MAX_PREVIEW_SIZE) < ptr)
        return NULL;
      /* head gets to be a generic variable in this case */
      head = LE_32(riff_check);
      /* skip over the chunk and the 'data' tag and length */
      ptr += 8;

      /* load the next, I don't know...n bytes, and check for a valid
       * MPEG audio header */
      riff_check = ptr; ptr += RIFF_CHECK_BYTES;
      if ((buf + MAX_PREVIEW_SIZE) < ptr)
        return NULL;

      ok = 0;
      for (i = 0; i < RIFF_CHECK_BYTES - 4; i++) {
        head = BE_32(riff_check + i);

        lprintf("**** mpg123: checking %08X\n", head);

        if (mpg123_head_check(head)
            || _sniff_buffer_looks_like_mp3(input))
	  ok = 1;
      }
      if (!ok)
	return NULL;

    } else if ((head & 0xFFFFFF00) == ID3V2_TAG) {
      lprintf("id3v2 tag detected (but not parsed)\n");
    } else if (!mpg123_head_check(head) &&
		    !_sniff_buffer_looks_like_mp3 (input)) {

      lprintf ("head_check failed\n");
      return NULL;
    }
  }
  break;

  case METHOD_BY_EXTENSION: {
    char *suffix;
    char *MRL;

    MRL = input->get_mrl (input);

    lprintf("demux_mpgaudio: stage by extension %s\n", MRL);

    if (strncmp (MRL, "ice :/", 6)) {
    
      suffix = strrchr(MRL, '.');
    
      if (!suffix)
	return NULL;
    
      if ( strncasecmp ((suffix+1), "mp3", 3)
	   && strncasecmp ((suffix+1), "mp2", 3)
	   && strncasecmp ((suffix+1), "mpa", 3)
	   && strncasecmp ((suffix+1), "mpega", 5))
	return NULL;
    }
  }
  break;

  case METHOD_EXPLICIT:
  break;
  
  default:
    return NULL;
  }
  
  this = xine_xmalloc (sizeof (demux_mpgaudio_t));

  this->demux_plugin.send_headers      = demux_mpgaudio_send_headers;
  this->demux_plugin.send_chunk        = demux_mpgaudio_send_chunk;
  this->demux_plugin.seek              = demux_mpgaudio_seek;
  this->demux_plugin.dispose           = demux_mpgaudio_dispose;
  this->demux_plugin.get_status        = demux_mpgaudio_get_status;
  this->demux_plugin.get_stream_length = demux_mpgaudio_get_stream_length;
  this->demux_plugin.get_video_frame   = NULL;
  this->demux_plugin.get_capabilities  = demux_mpgaudio_get_capabilities;
  this->demux_plugin.get_optional_data = demux_mpgaudio_get_optional_data;
  this->demux_plugin.demux_class       = class_gen;
  
  this->input      = input;
  this->audio_fifo = stream->audio_fifo;
  this->status     = DEMUX_FINISHED;
  this->stream     = stream;
  
  return &this->demux_plugin;
}

/*
 * demux mpegaudio class
 */

static char *get_description (demux_class_t *this_gen) {
  return "MPEG audio demux plugin";
}

static char *get_identifier (demux_class_t *this_gen) {
  return "MPEGAUDIO";
}

static char *get_extensions (demux_class_t *this_gen) {
  return "mp3 mp2 mpa mpega";
}

static char *get_mimetypes (demux_class_t *this_gen) {
  return "audio/mpeg2: mp2: MPEG audio;"
         "audio/x-mpeg2: mp2: MPEG audio;"
         "audio/mpeg3: mp3: MPEG audio;"
         "audio/x-mpeg3: mp3: MPEG audio;"
         "audio/mpeg: mpa,abs,mpega: MPEG audio;"
         "audio/x-mpeg: mpa,abs,mpega: MPEG audio;"
         "x-mpegurl: mp3: MPEG audio;"
         "audio/mpegurl: mp3: MPEG audio;"
         "audio/mp3: mp3: MPEG audio;"
         "audio/x-mp3: mp3: MPEG audio;";
}

static void class_dispose (demux_class_t *this_gen) {

  demux_mpgaudio_class_t *this = (demux_mpgaudio_class_t *) this_gen;

  free (this);
}

void *demux_mpgaudio_init_class (xine_t *xine, void *data) {
  
  demux_mpgaudio_class_t     *this;
  
  this         = xine_xmalloc (sizeof (demux_mpgaudio_class_t));
  this->xine   = xine;

  this->demux_class.open_plugin     = open_plugin;
  this->demux_class.get_description = get_description;
  this->demux_class.get_identifier  = get_identifier;
  this->demux_class.get_mimetypes   = get_mimetypes;
  this->demux_class.get_extensions  = get_extensions;
  this->demux_class.dispose         = class_dispose;

  return this;
}
