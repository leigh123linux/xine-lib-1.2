/*
 * Copyright (C) 2000-2018 the xine project
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
 * demultiplexer for asf streams
 *
 * based on ffmpeg's
 * ASF compatible encoder and decoder.
 * Copyright (c) 2000, 2001 Gerard Lantau.
 *
 * GUID list from avifile
 * some other ideas from MPlayer
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#ifdef HAVE_ALLOCA_H
#include <alloca.h>
#endif

#define LOG_MODULE "demux_asf"
#define LOG_VERBOSE
/*
#define LOG
*/
#include <xine/xine_internal.h>
#include <xine/demux.h>
#include <xine/xineutils.h>
#include "bswap.h"
#include "asfheader.h"
#include <xine/xmlparser.h>

#define CODEC_TYPE_AUDIO          0
#define CODEC_TYPE_VIDEO          1
#define CODEC_TYPE_CONTROL        2
#define MAX_NUM_STREAMS          23

#define DEFRAG_BUFSIZE        65536

#define WRAP_THRESHOLD     20*90000
#define MAX_FRAME_DUR         90000

#define PTS_AUDIO 0
#define PTS_VIDEO 1

#define ASF_MODE_NORMAL            0
#define ASF_MODE_ASX_REF           1
#define ASF_MODE_HTTP_REF          2
#define ASF_MODE_ASF_REF           3
#define ASF_MODE_ENCRYPTED_CONTENT 4
#define ASF_MODE_NO_CONTENT        5

typedef struct {
  int                 seq;

  int                 frag_offset;
  int64_t             timestamp;
  int                 ts_per_kbyte;
  int                 defrag;

  uint32_t            buf_type;
  int                 stream_id;
  fifo_buffer_t      *fifo;

  uint8_t            *buffer;
  int                 skip;
  int                 resync;
  int                 first_seq;

  int                 payload_size;

  /* palette handling */
  int                  palette_count;
  palette_entry_t      palette[256];

} asf_demux_stream_t;

typedef struct demux_asf_s {
  demux_plugin_t     demux_plugin;

  xine_stream_t     *stream;

  fifo_buffer_t     *audio_fifo;
  fifo_buffer_t     *video_fifo;

  input_plugin_t    *input;

  int64_t            keyframe_ts;
  int                keyframe_found;

  int                seqno;
  uint32_t           packet_size;
  uint8_t            packet_len_flags;
  uint32_t           data_size;
  uint64_t           packet_count;

  asf_demux_stream_t streams[MAX_NUM_STREAMS];
  int                video_stream;
  int                audio_stream;
  int                video_id;
  int                audio_id;

  int64_t            length;
  uint32_t           rate;

  /* packet filling */
  int                packet_size_left;

  /* discontinuity detection */
  int64_t            last_pts[2];
  int                send_newpts;

  /* only for reading */
  uint32_t           packet_padsize;
  int                nb_frames;
  uint8_t            frame_flag;
  uint8_t            packet_prop_flags;
  int                frame;

  int                status;

  /* byte reordering from audio streams */
  uint8_t           *reorder_temp;
  int                reorder_h;
  int                reorder_w;
  int                reorder_b;

  int                buf_flag_seek;

  /* first packet position */
  int64_t            first_packet_pos;

  int                mode;

  /* for fewer error messages */
  uint8_t            last_unknown_guid[16];

  uint8_t            seen_streams[24];

  asf_header_t      *asf_header;

} demux_asf_t ;

typedef enum {
  ASF_OK = 0,
  ASF_EOF,
  ASF_SEEK_ERROR,
  ASF_EOS,
  ASF_NEW_STREAM,
  ASF_INVALID_DATA_LENGTH,
  ASF_INVALID_FRAGMENT_LENGTH,
  ASF_INVALID_RLEN,
  ASF_INVALID_PAD_SIZE,
  ASF_UNFINISHED_PACKET
} asf_error_t;

static const char *error_strings[] = {
  "success",
  "unexpected end of input",
  "seek error",
  "end of stream",
  "unexpected new stream",
  "invalid data length",
  "invalid fragment length",
  "invalid rlen",
  "invalid pad size",
  "unfinished packet"
};


static asf_guid_t get_guid_id (demux_asf_t *this, const uint8_t *guid) {
  uint8_t b[40];
  asf_guid_t i = asf_guid_2_num (guid);

  if (i != GUID_ERROR)
    return i;

  if (!memcmp (guid, &this->last_unknown_guid, 16))
    return GUID_ERROR;

  memcpy (&this->last_unknown_guid, guid, 16);
  asf_guid_2_str (b, guid);
  xprintf (this->stream->xine, XINE_VERBOSITY_DEBUG,
    "demux_asf: unknown GUID: %s\n", b);
  return GUID_ERROR;
}


static asf_guid_t get_guid (demux_asf_t *this) {
  uint8_t buf[16];
  if (this->input->read (this->input, buf, 16) != 16) {
    xprintf (this->stream->xine, XINE_VERBOSITY_DEBUG, "demux_asf: end of data\n");
    this->status = DEMUX_FINISHED;
    return GUID_ERROR;
  }
  return get_guid_id (this, buf);
}

#if 0
static void get_str16_nolen(demux_asf_t *this, int len,
			    char *buf, int buf_size) {

  int c;
  char *q;

  q = buf;
  while (len > 0) {
    c = get_le16(this);
    if ((q - buf) < buf_size - 1)
      *q++ = c;
    len-=2;
  }
  *q = '\0';
}
#endif

static void asf_send_audio_header (demux_asf_t *this, int stream) {
  buf_element_t *buf;
  asf_stream_t *asf_stream = this->asf_header->streams[stream];
  xine_waveformatex  *wavex = (xine_waveformatex *)asf_stream->private_data;

  if (!this->audio_fifo)
    return;

  buf = this->audio_fifo->buffer_pool_alloc (this->audio_fifo);
  if ((int)(asf_stream->private_data_length) > buf->max_size) {
    xprintf(this->stream->xine, XINE_VERBOSITY_LOG,
            "demux_asf: private decoder data length (%d) is greater than fifo buffer length (%d)\n",
            asf_stream->private_data_length, buf->max_size);
    buf->free_buffer(buf);
    this->status = DEMUX_FINISHED;
    return;
  }

  memcpy (buf->content, wavex, asf_stream->private_data_length);

  _x_stream_info_set(this->stream, XINE_STREAM_INFO_AUDIO_FOURCC, wavex->wFormatTag);

  lprintf ("wavex header is %d bytes long\n", asf_stream->private_data_length);

  buf->size = asf_stream->private_data_length;
  buf->type = this->streams[stream].buf_type;
  buf->decoder_flags   = BUF_FLAG_HEADER|BUF_FLAG_STDHEADER|BUF_FLAG_FRAME_END;
  buf->decoder_info[1] = wavex->nSamplesPerSec;
  buf->decoder_info[2] = wavex->wBitsPerSample;
  buf->decoder_info[3] = wavex->nChannels;

  this->audio_fifo->put (this->audio_fifo, buf);
}

#if 0
static unsigned long str2ulong(unsigned char *str) {
  return ( str[0] | (str[1]<<8) | (str[2]<<16) | (str[3]<<24) );
}
#endif

static void asf_send_video_header (demux_asf_t *this, int stream) {

  buf_element_t      *buf;
  asf_demux_stream_t *demux_stream = &this->streams[stream];
  asf_stream_t       *asf_stream = this->asf_header->streams[stream];
  xine_bmiheader     *bih =  (xine_bmiheader *)(asf_stream->private_data + 11);

  _x_stream_info_set(this->stream, XINE_STREAM_INFO_VIDEO_FOURCC, bih->biCompression);

  buf = this->video_fifo->buffer_pool_alloc (this->video_fifo);
  if (((int)(asf_stream->private_data_length) - 11) > buf->max_size) {
    xprintf(this->stream->xine, XINE_VERBOSITY_LOG,
            "demux_asf: private decoder data length (%d) is greater than fifo buffer length (%d)\n",
            asf_stream->private_data_length-10, buf->max_size);
    buf->free_buffer(buf);
    this->status = DEMUX_FINISHED;
    return;
  }

  buf->decoder_flags   = BUF_FLAG_HEADER|BUF_FLAG_STDHEADER|BUF_FLAG_FRAMERATE|
                         BUF_FLAG_FRAME_END;

  buf->decoder_info[0] = 0;

  if (this->asf_header->aspect_ratios[stream].x && this->asf_header->aspect_ratios[stream].y)
  {
    buf->decoder_flags  |= BUF_FLAG_ASPECT;
    buf->decoder_info[1] = bih->biWidth  * this->asf_header->aspect_ratios[stream].x;
    buf->decoder_info[2] = bih->biHeight * this->asf_header->aspect_ratios[stream].y;
  }

  buf->size = asf_stream->private_data_length - 11;
  memcpy (buf->content, bih, buf->size);
  buf->type = this->streams[stream].buf_type;

  this->video_fifo->put (this->video_fifo, buf);


  /* send off the palette, if there is one */
  if (demux_stream->palette_count) {
    xprintf(this->stream->xine, XINE_VERBOSITY_LOG,
	    "demux_asf: stream %d, palette : %d entries\n", stream, demux_stream->palette_count);
    buf = this->video_fifo->buffer_pool_alloc (this->video_fifo);
    buf->decoder_flags = BUF_FLAG_SPECIAL|BUF_FLAG_HEADER;
    buf->decoder_info[1] = BUF_SPECIAL_PALETTE;
    buf->decoder_info[2] = demux_stream->palette_count;
    buf->decoder_info_ptr[2] = &demux_stream->palette;
    buf->size = 0;
    buf->type = this->streams[stream].buf_type;
    this->video_fifo->put (this->video_fifo, buf);
  }
}

static int asf_read_header (demux_asf_t *this) {
  int i;
  {
    uint64_t asf_header_len;
    uint8_t *asf_header_buffer = NULL;
    uint8_t buf[8];
    if (this->input->read (this->input, buf, 8) != 8) {
      xprintf(this->stream->xine, XINE_VERBOSITY_DEBUG, "demux_asf: end of data\n");
      this->status = DEMUX_FINISHED;
      return 0;
    }
    asf_header_len = _X_LE_64 (buf);
    if (asf_header_len > (4 << 20)) {
      xprintf (this->stream->xine, XINE_VERBOSITY_DEBUG,
        "demux_asf: asf_read_header: overly-large header? (%"PRIu64" bytes)\n", asf_header_len);
      return 0;
    }
    asf_header_buffer = malloc (asf_header_len);
    if (!asf_header_buffer)
      return 0;
    if (this->input->read (this->input, asf_header_buffer, asf_header_len) != (int)asf_header_len) {
      free (asf_header_buffer);
      xprintf (this->stream->xine, XINE_VERBOSITY_DEBUG, "demux_asf: end of data\n");
      this->status = DEMUX_FINISHED;
      return 0;
    }
    /* delete previous header */
    if (this->asf_header)
      asf_header_delete (this->asf_header);
    /* the header starts with :
     *   byte  0-15: header guid
     *   byte 16-23: header length
     */
    this->asf_header = asf_header_new (asf_header_buffer, asf_header_len);
    free (asf_header_buffer);
    if (!this->asf_header)
      return 0;
  }
  lprintf("asf header parsing ok\n");

  this->packet_size = this->asf_header->file->packet_size;
  this->packet_count = this->asf_header->file->data_packet_count;

  /* compute stream duration */
  this->length = (this->asf_header->file->send_duration -
                  this->asf_header->file->preroll) / 10000;
  if (this->length < 0)
    this->length = 0;

  /* compute average byterate (needed for seeking) */
  if (this->asf_header->file->max_bitrate)
    this->rate = this->asf_header->file->max_bitrate >> 3;
  else if (this->length)
    this->rate = (int64_t) this->input->get_length(this->input) * 1000 / this->length;

  _x_stream_info_set(this->stream, XINE_STREAM_INFO_BITRATE, this->asf_header->file->max_bitrate);

  for (i = 0; i < this->asf_header->stream_count; i++) {
    asf_stream_t *asf_stream = this->asf_header->streams[i];
    asf_demux_stream_t *demux_stream = &this->streams[i];

    if (!asf_stream) {
      if (this->mode != ASF_MODE_NO_CONTENT) {
	xine_log(this->stream->xine, XINE_LOG_MSG,
		 _("demux_asf: warning: A stream appears to be missing.\n"));
	_x_message(this->stream, XINE_MSG_READ_ERROR,
		   _("Media stream missing?"), NULL);
	this->mode = ASF_MODE_NO_CONTENT;
      }
      return 0;
    }

    if (asf_stream->encrypted_flag) {
      if (this->mode != ASF_MODE_ENCRYPTED_CONTENT) {
	xine_log(this->stream->xine, XINE_LOG_MSG,
		 _("demux_asf: warning: The stream id=%d is encrypted.\n"), asf_stream->stream_number);
	_x_message(this->stream, XINE_MSG_ENCRYPTED_SOURCE,
		   _("Media stream scrambled/encrypted"), NULL);
	this->mode = ASF_MODE_ENCRYPTED_CONTENT;
      }
    }
    switch (asf_stream->stream_type) {
    case GUID_ASF_AUDIO_MEDIA:
    {
      xine_waveformatex *fx = (xine_waveformatex *)asf_stream->private_data;
      if (!fx || (asf_stream->private_data_length < sizeof (*fx)))
        break;
      _x_waveformatex_le2me (fx);
      demux_stream->buf_type = _x_formattag_to_buf_audio (fx->wFormatTag);
      xprintf (this->stream->xine, XINE_VERBOSITY_DEBUG,
        "demux_asf: audio stream #%d: [0x%0x] %dbps %dch %dHz %dbit\n", (int)asf_stream->stream_number,
        (unsigned int)fx->wFormatTag, (int)this->asf_header->bitrates[i], (int)fx->nChannels,
        (int)fx->nSamplesPerSec, (int)fx->wBitsPerSample);
      if (!demux_stream->buf_type) {
        demux_stream->buf_type = BUF_AUDIO_UNKNOWN;
        _x_report_audio_format_tag (this->stream->xine, LOG_MODULE, fx->wFormatTag);
      }

      if  ((asf_stream->error_correction_type == GUID_ASF_AUDIO_SPREAD)
        &&  asf_stream->error_correction_data
        && (asf_stream->error_correction_data_length >= 5)) {
	this->reorder_h = asf_stream->error_correction_data[0];
        this->reorder_w = _X_LE_16 (asf_stream->error_correction_data + 1);
        this->reorder_b = _X_LE_16 (asf_stream->error_correction_data + 3);
        if (!this->reorder_b)
          this->reorder_b = 1;
        this->reorder_w /= this->reorder_b;
        xprintf (this->stream->xine, XINE_VERBOSITY_DEBUG,
          "demux_asf: audio conceal interleave detected (%d x %d x %d)\n",
          this->reorder_w, this->reorder_h, this->reorder_b);
      } else {
	this->reorder_b = this->reorder_h = this->reorder_w = 1;
      }


      _x_meta_info_set(this->stream, XINE_META_INFO_AUDIOCODEC, _x_buf_audio_name(demux_stream->buf_type));

      this->streams[i].fifo        = this->audio_fifo;
      this->streams[i].frag_offset = 0;
      this->streams[i].seq         = 0;
      if (this->reorder_h > 1 && this->reorder_w > 1 ) {
	if( !this->streams[i].buffer )
	  this->streams[i].buffer = malloc( DEFRAG_BUFSIZE );
	this->streams[i].defrag = 1;
      } else
	this->streams[i].defrag = 0;
    }
    break;

    case GUID_ASF_VIDEO_MEDIA:
      {
        /* video private data
         * 11 bytes : header
         * 40 bytes : bmiheader
         * XX bytes : optional palette
         */
	uint32_t width, height;
	/*uint16_t bmiheader_size;*/
	xine_bmiheader *bmiheader;

        if  (!asf_stream->private_data
          || (asf_stream->private_data_length < 11 + sizeof (*bmiheader)))
          break;

	width = _X_LE_32(asf_stream->private_data);
	height = _X_LE_32(asf_stream->private_data + 4);
	/* there is one unknown byte between height and the bmiheader size */
	/*bmiheader_size = _X_LE_16(asf_stream->private_data + 9);*/

	/* FIXME: bmiheader_size must be >= sizeof(xine_bmiheader) */

	bmiheader = (xine_bmiheader *) (asf_stream->private_data + 11);
	_x_bmiheader_le2me(bmiheader);

	/* FIXME: check if (bmi_header_size == bmiheader->biSize) ? */

	demux_stream->buf_type = _x_fourcc_to_buf_video(bmiheader->biCompression);
        {
          uint8_t sf[20];
          _x_tag32_me2str (sf, bmiheader->biCompression);
          xprintf (this->stream->xine, XINE_VERBOSITY_DEBUG,
            "demux_asf: video stream #%d: [%s] %dbps %d x %d\n", (int)asf_stream->stream_number,
            sf, (int)this->asf_header->bitrates[i], (int)width, (int)height);
        }
	if( !demux_stream->buf_type ) {
	  demux_stream->buf_type = BUF_VIDEO_UNKNOWN;
	  _x_report_video_fourcc (this->stream->xine, LOG_MODULE, bmiheader->biCompression);
	}

	_x_meta_info_set(this->stream, XINE_META_INFO_VIDEOCODEC, _x_buf_video_name(demux_stream->buf_type));
	_x_stream_info_set(this->stream, XINE_STREAM_INFO_VIDEO_WIDTH, width);
	_x_stream_info_set(this->stream, XINE_STREAM_INFO_VIDEO_HEIGHT, height);

	this->streams[i].fifo         = this->video_fifo;
	this->streams[i].frag_offset  = 0;
	this->streams[i].defrag       = 0;

	/* load the palette, if there is one */
	demux_stream->palette_count = bmiheader->biClrUsed;

	lprintf ("palette_count: %d\n", demux_stream->palette_count);
	if (demux_stream->palette_count > 256) {
	  lprintf ("number of colours exceeded 256 (%d)", demux_stream->palette_count);
	  demux_stream->palette_count = 256;
	}
	if (((int)(asf_stream->private_data_length) - (int)sizeof (xine_bmiheader) - 11) >= (demux_stream->palette_count * 4)) {
	  int j;
	  uint8_t *palette;

	  /* according to msdn the palette is located here : */
	  palette = (uint8_t *)bmiheader + bmiheader->biSize;

	  /* load the palette */
	  for (j = 0; j < demux_stream->palette_count; j++) {
	    demux_stream->palette[j].b = *(palette + j * 4 + 0);
	    demux_stream->palette[j].g = *(palette + j * 4 + 1);
	    demux_stream->palette[j].r = *(palette + j * 4 + 2);
	  }
	} else {
	  int j;

	  /* generate a greyscale palette */
	  demux_stream->palette_count = 256;
	  for (j = 0; j < demux_stream->palette_count; j++) {
	  demux_stream->palette[j].r = j;
	  demux_stream->palette[j].g = j;
	  demux_stream->palette[j].b = j;
	  }
	}
      }
      break;
    default: ;
    }
  }

  {
    uint8_t b1[16 + 10];
    uint8_t b2[40];
    unsigned int n;
    if (this->input->read (this->input, b1, 16 + 10) != 16 + 10) {
      asf_header_delete (this->asf_header);
      this->asf_header = NULL;
      xprintf (this->stream->xine, XINE_VERBOSITY_LOG, "demux_asf: no data chunk.\n");
      return 0;
    }
    this->first_packet_pos = this->input->get_current_pos (this->input);
    asf_guid_2_str (b2, b1);
    n = _X_LE_32 (b1 + 16);
    xprintf (this->stream->xine, XINE_VERBOSITY_DEBUG,
      "demux_asf: data chunk %s @%" PRId64 ", %u packets of %u bytes each.\n",
      b2, (int64_t)this->first_packet_pos - 16 - 10, n, this->packet_size);
    if ((memcmp (b1, this->asf_header->file->file_id, 16)) || (n != (unsigned int)this->packet_count)) {
      asf_guid_2_str (b2, this->asf_header->file->file_id);
      xprintf (this->stream->xine, XINE_VERBOSITY_DEBUG,
        "demux_asf: warning: announced was %s, %u packets.\n",
        b2, (unsigned int)this->packet_count);
    }
  }
  this->packet_size_left = 0;
  return 1;
}

static int demux_asf_send_headers_common (demux_asf_t *this) {

  /* will get overridden later */
  _x_stream_info_set(this->stream, XINE_STREAM_INFO_HAS_VIDEO, 0);
  _x_stream_info_set(this->stream, XINE_STREAM_INFO_HAS_AUDIO, 0);

  /*
   * initialize asf engine
   */
  this->audio_stream             = -1;
  this->video_stream             = -1;
  this->packet_size              = 0;
  this->seqno                    = 0;

  if (!asf_read_header (this)) {

    xprintf(this->stream->xine, XINE_VERBOSITY_DEBUG, "demux_asf: asf_read_header failed.\n");

    this->status = DEMUX_FINISHED;
    return 1;
  } else {

    /*
     * send start buffer
     */
    _x_demux_control_start(this->stream);

    if (this->asf_header->content) {
      _x_meta_info_set(this->stream, XINE_META_INFO_TITLE, this->asf_header->content->title);
      _x_meta_info_set(this->stream, XINE_META_INFO_ARTIST, this->asf_header->content->author);
      _x_meta_info_set(this->stream, XINE_META_INFO_COMMENT, this->asf_header->content->description);
    }

    /*  Choose the best audio and the best video stream.
     *  Use the bitrate to do the choice.
     */
    asf_header_choose_streams(this->asf_header, -1, &this->video_stream, &this->audio_stream);
    this->audio_id = this->audio_stream == -1 ? -1 : this->asf_header->streams[this->audio_stream]->stream_number;
    this->video_id = this->video_stream == -1 ? -1 : this->asf_header->streams[this->video_stream]->stream_number;
    xprintf (this->stream->xine, XINE_VERBOSITY_DEBUG,
      "demux_asf: video stream_id: %d, audio stream_id: %d\n", this->video_id, this->audio_id);

    if (this->audio_stream != -1) {
      asf_stream_t *s = this->asf_header->streams[this->audio_stream];
      if ((s->error_correction_type == GUID_ASF_AUDIO_SPREAD)
        &&  s->error_correction_data && (s->error_correction_data_length >= 5)) {
        this->reorder_h = s->error_correction_data[0];
        this->reorder_w = _X_LE_16 (s->error_correction_data + 1);
        this->reorder_b = _X_LE_16 (s->error_correction_data + 3);
        if (!this->reorder_b)
          this->reorder_b = 1;
        this->reorder_w /= this->reorder_b;
      } else {
        this->reorder_b = this->reorder_h = this->reorder_w = 1;
      }
      free (this->reorder_temp);
      this->reorder_temp = NULL;
      if ((this->reorder_w > 1) || (this->reorder_h > 1))
        this->reorder_temp = malloc (this->reorder_w * this->reorder_h * this->reorder_b);
      _x_stream_info_set(this->stream, XINE_STREAM_INFO_HAS_AUDIO, 1);
      asf_send_audio_header(this, this->audio_stream);
    }
    if (this->video_stream != -1) {
      _x_stream_info_set(this->stream, XINE_STREAM_INFO_HAS_VIDEO, 1);
      asf_send_video_header(this, this->video_stream);
    }
  }
  return 0;
}

static void asf_reorder (demux_asf_t *this, uint8_t *src, int len) {
  if (this->reorder_temp) {
    int bsize = this->reorder_h * this->reorder_w * this->reorder_b;
    uint8_t *s2 = src;
    int n = len / bsize, x, y;
    while (n--) {
      uint8_t *t = this->reorder_temp;
      for (x = 0; x < this->reorder_w; x++) {
        for (y = 0; y < this->reorder_h; y++) {
          memcpy (t, s2 + (y * this->reorder_w + x) * this->reorder_b, this->reorder_b);
          t += this->reorder_b;
        }
      }
      memcpy (s2, this->reorder_temp, bsize);
      s2 += bsize;
    }
  }
}

/* redefine abs as macro to handle 64-bit diffs.
   i guess llabs may not be available everywhere */
#define abs(x) ( ((x)<0) ? -(x) : (x) )

static void check_newpts (demux_asf_t *this, int64_t pts, int video, int frame_end) {
  int64_t diff;

  (void)frame_end; /* FIXME: use */

  diff = pts - this->last_pts[video];

#ifdef LOG
  if (pts) {
    if (video) {
      printf ("demux_asf: VIDEO: pts = %8"PRId64", diff = %8"PRId64"\n", pts, pts - this->last_pts[video]);
    } else {
      printf ("demux_asf: AUDIO: pts = %8"PRId64", diff = %8"PRId64"\n", pts, pts - this->last_pts[video]);
    }
  }
#endif
  if (pts && (this->send_newpts || (this->last_pts[video] && abs(diff) > WRAP_THRESHOLD))) {

    lprintf ("sending newpts %"PRId64" (video = %d diff = %"PRId64")\n", pts, video, diff);

    if (this->buf_flag_seek) {
      _x_demux_control_newpts(this->stream, pts, BUF_FLAG_SEEK);
      this->buf_flag_seek = 0;
    } else {
      _x_demux_control_newpts(this->stream, pts, 0);
    }

    this->send_newpts         = 0;
    this->last_pts[1 - video] = 0;
  }

  if (pts)
    this->last_pts[video] = pts;

}


static void asf_send_buffer_nodefrag (demux_asf_t *this, asf_demux_stream_t *stream,
				      int frag_offset, int64_t timestamp,
				      int frag_len) {

  int            package_done;

  lprintf ("pts=%"PRId64", off=%d, len=%d, total=%d\n",
           timestamp * 90, frag_offset, frag_len, stream->payload_size);

  if (frag_offset == 0) {
    /* new packet */
    stream->frag_offset = 0;
    lprintf("new packet\n");
  } else {
    if (frag_offset == stream->frag_offset) {
      /* continuing packet */
      lprintf("continuing packet: offset=%d\n", frag_offset);
    } else {
      /* cannot continue current packet: free it */
      xprintf (this->stream->xine, XINE_VERBOSITY_DEBUG, "demux_asf: asf_send_buffer_nodefrag: stream offset: %d, invalid offset: %d\n", stream->frag_offset, frag_offset);
      this->input->seek (this->input, frag_len, SEEK_CUR);
      return ;
    }
  }

  while (frag_len) {
    buf_element_t *buf;
    int bsize;

    buf = stream->fifo->buffer_pool_size_alloc (stream->fifo, frag_len);
    bsize = frag_len < buf->max_size ? frag_len : buf->max_size;

    if (this->input->read (this->input, buf->content, bsize) != bsize) {
      buf->free_buffer (buf);
      xprintf (this->stream->xine, XINE_VERBOSITY_DEBUG, "demux_asf: input buffer starved\n");
      return ;
    }
    lprintf ("data: %d %d %d %d\n", buf->content[0], buf->content[1], buf->content[2], buf->content[3]);

    if (this->input->get_length (this->input) > 0)
      buf->extra_info->input_normpos = (int)((double)this->input->get_current_pos (this->input) *
        65535 / this->input->get_length (this->input));
    buf->extra_info->input_time = timestamp;
    lprintf ("input normpos is %d, input time is %d, rate %d\n",
             buf->extra_info->input_normpos,
             buf->extra_info->input_time,
             this->rate);

    buf->pts  = timestamp * 90;
    buf->type = stream->buf_type;
    buf->size = bsize;
    timestamp = 0;

    if (stream->frag_offset == 0)
      buf->decoder_flags |= BUF_FLAG_FRAME_START;
    stream->frag_offset += bsize;
    frag_len -= bsize;

    package_done = (stream->frag_offset >= stream->payload_size);
    if ((buf->type & BUF_MAJOR_MASK) == BUF_VIDEO_BASE)
      check_newpts (this, buf->pts, PTS_VIDEO, package_done);
    else
      check_newpts (this, buf->pts, PTS_AUDIO, package_done);

    /* test if whole packet read */
    if (package_done) {
      buf->decoder_flags |= BUF_FLAG_FRAME_END;
      lprintf("packet done: offset=%d, payload=%d\n", stream->frag_offset, stream->payload_size);
    }

    lprintf ("buffer type %08x %8d bytes, %8" PRId64 " pts\n",
             buf->type, buf->size, buf->pts);

    stream->fifo->put (stream->fifo, buf);
  }
}

static void asf_send_buffer_defrag (demux_asf_t *this, asf_demux_stream_t *stream,
				    int frag_offset, int64_t timestamp,
				    int frag_len) {

  int            package_done;

  /*
    printf("asf_send_buffer seq=%d frag_offset=%d frag_len=%d\n",
    seq, frag_offset, frag_len );
  */
  lprintf ("asf_send_buffer_defrag: timestamp=%"PRId64", pts=%"PRId64"\n", timestamp, timestamp * 90);

  if (frag_offset == 0) {
    /* new packet */
    lprintf("new packet\n");
    stream->frag_offset = 0;
    stream->timestamp = timestamp;
  } else {
    if (frag_offset == stream->frag_offset) {
      /* continuing packet */
      lprintf("continuing packet: offset=%d\n", frag_offset);
    } else {
      /* cannot continue current packet: free it */
      xprintf (this->stream->xine, XINE_VERBOSITY_DEBUG, "demux_asf: asf_send_buffer_defrag: invalid offset\n");
      this->input->seek (this->input, frag_len, SEEK_CUR);
      return ;
    }
  }

  if( stream->frag_offset + frag_len > DEFRAG_BUFSIZE ) {
    xprintf (this->stream->xine, XINE_VERBOSITY_DEBUG, "demux_asf: buffer overflow on defrag!\n");
  } else {
    if (this->input->read (this->input, &stream->buffer[stream->frag_offset], frag_len) != frag_len) {
      xprintf (this->stream->xine, XINE_VERBOSITY_DEBUG, "demux_asf: input buffer starved\n");
      return ;
    }
    stream->frag_offset += frag_len;
  }

  package_done = (stream->frag_offset >= stream->payload_size);

  if (package_done) {
    uint8_t *p;

    lprintf("packet done: offset=%d, payload=%d\n", stream->frag_offset, stream->payload_size);

    if (stream->fifo == this->audio_fifo &&
        this->reorder_h > 1 && this->reorder_w > 1 ) {
      asf_reorder(this,stream->buffer,stream->frag_offset);
    }

    p = stream->buffer;
    while (stream->frag_offset) {
      buf_element_t *buf;
      int bsize;

      buf = stream->fifo->buffer_pool_size_alloc (stream->fifo, stream->frag_offset);
      bsize = stream->frag_offset < buf->max_size ? stream->frag_offset : buf->max_size;
      xine_fast_memcpy (buf->content, p, bsize);

      if (this->input->get_length (this->input) > 0)
        buf->extra_info->input_normpos = (int)((double)this->input->get_current_pos (this->input) *
          65535 / this->input->get_length (this->input));
      buf->extra_info->input_time = stream->timestamp;

      /* send the same pts for the entire frame */
      buf->pts  = stream->timestamp * 90;

      buf->type = stream->buf_type;
      buf->size = bsize;

      lprintf ("buffer type %08x %8d bytes, %8" PRId64 " pts\n",
	       buf->type, buf->size, buf->pts);

      stream->frag_offset -= bsize;
      p += bsize;

      if ((buf->type & BUF_MAJOR_MASK) == BUF_VIDEO_BASE)
        check_newpts (this, buf->pts, PTS_VIDEO, !stream->frag_offset);
      else
        check_newpts (this, buf->pts, PTS_AUDIO, !stream->frag_offset);

      /* test if whole packet read */
      if (!stream->frag_offset)
        buf->decoder_flags |= BUF_FLAG_FRAME_END;

      stream->fifo->put (stream->fifo, buf);
    }
  }
}

/* return 0 if ok */
static asf_error_t asf_parse_packet_align (demux_asf_t *this) {

  uint64_t current_pos, packet_pos;
  uint32_t mod;
  uint64_t packet_num;


  current_pos = this->input->get_current_pos (this->input);

  /* seek to the beginning of the next packet */
  mod = (current_pos - this->first_packet_pos) % this->packet_size;
  this->packet_size_left = mod ? this->packet_size - mod : 0;
  packet_pos = current_pos + this->packet_size_left;

  if (this->packet_size_left) {
    lprintf("last packet is not finished, %d bytes\n", this->packet_size_left);
    current_pos = this->input->seek (this->input, packet_pos, SEEK_SET);
    if (current_pos != packet_pos) {
      return ASF_SEEK_ERROR;
    }
  }
  this->packet_size_left = 0;

  /* check packet_count */
  packet_num = (packet_pos - this->first_packet_pos) / this->packet_size;
  lprintf("packet_num=%"PRId64", packet_count=%"PRId64"\n", packet_num, this->packet_count);
  if (packet_num >= this->packet_count) {
    /* end of payload data */
    current_pos = this->input->get_current_pos (this->input);
    lprintf("end of payload data, current_pos=%"PRId64"\n", current_pos);
    {
      /* check new asf header */
      if (get_guid(this) == GUID_ASF_HEADER) {
        lprintf("new asf header detected\n");
        _x_demux_control_end(this->stream, 0);
        if (demux_asf_send_headers_common(this))
          return ASF_NEW_STREAM;
      } else {
	lprintf("not an ASF stream or end of stream\n");
        return ASF_EOS;
      }
    }
  }

  return ASF_OK;
}

/* return 0 if ok */
static asf_error_t asf_parse_packet_ecd (demux_asf_t *this, uint32_t  *p_hdr_size) {
  while (1) {
    uint8_t buf[16];
    /* ecd_flags:
     *  bit 7:   ecd_present
     *  bit 6~5: ecd_len_type
     *  bit 4:   ecd_opaque
     *  bit 3~0: ecd_len
     */
    if (this->input->read (this->input, buf, 1) != 1)
      return ASF_EOF;
    *p_hdr_size = 1;

    if ((buf[0] & 0xf0) == 0x80) {

      /* skip ecd */
      int size = buf[0] & 0x0f;
      if (this->input->read (this->input, buf + 1, size) != size)
        return ASF_EOF;
      *p_hdr_size += size;
      return ASF_OK;

    } else {

      /* check if it's a new stream */
      if (this->input->read (this->input, buf + 1, 15) != 15)
        return ASF_EOF;
      *p_hdr_size += 15;
      if (get_guid_id (this, buf) == GUID_ASF_HEADER) {
        lprintf ("new asf header detected\n");
        _x_demux_control_end (this->stream, 0);
        if (demux_asf_send_headers_common (this))
          return ASF_NEW_STREAM;
      } else {
        /* skip invalid packet */
        xprintf (this->stream->xine, XINE_VERBOSITY_DEBUG,
          "demux_asf: skip invalid packet: 0x%02x\n", (unsigned int)buf[0]);
        this->input->seek (this->input, this->packet_size - *p_hdr_size, SEEK_CUR);
      }

    }
  }
}

/* return 0 if ok */
static asf_error_t asf_parse_packet_payload_header (demux_asf_t *this, uint32_t p_hdr_size) {

#ifdef LOG
  unsigned int timestamp;
  unsigned int duration;
#endif

  static const uint8_t sn[128] = {
    0,1, 1,2, 2,3, 4,5,  1,2, 2,3, 3,4, 5, 6,  2,3, 3,4, 4,5,  6, 7,  4,5, 5, 6,  6, 7,  8, 9,
    1,2, 2,3, 3,4, 5,6,  2,3, 3,4, 4,5, 6, 7,  3,4, 4,5, 5,6,  7, 8,  5,6, 6, 7,  7, 8,  9,10,
    2,3, 3,4, 4,5, 6,7,  3,4, 4,5, 5,6, 7, 8,  4,5, 5,6, 6,7,  8, 9,  6,7, 7, 8,  8, 9, 10,11,
    4,5, 5,6, 6,7, 8,9,  5,6, 6,7, 7,8, 9,10,  6,7, 7,8, 8,9, 10,11,  8,9, 9,10, 10,11, 12,13
  };
  uint8_t b[24], *p = b;
  int need;

  if (this->input->read (this->input, b, 2) != 2)
    return ASF_EOF;
  this->packet_len_flags  = b[0];
  this->packet_prop_flags = b[1];

  p += 2;
  /* static const int s[4] = {0, 1, 2, 4};
   * need = s[(b[0] >> 5) & 3] + s[(b[0] >> 1) & 3] + s[(b[0] >> 3) & 3] + 6 + (b[0] & 1);
   */
  need = sn[b[0] & 0x7f] + 6;
  if (this->input->read (this->input, p, need) != need)
    return ASF_EOF;

  /* packet size */
  switch ((b[0] >> 5) & 3) {
    case 1:  this->data_size = *p++; break;
    case 2:  this->data_size = (uint16_t)_X_LE_16 (p); p += 2; break;
    case 3:  this->data_size = (uint32_t)_X_LE_32 (p); p += 4; break;
    default: this->data_size = 0;
  }

  /* sequence */
  switch ((b[0] >> 1) & 3) {
    case 1: p += 1; break;
    case 2: p += 2; break;
    case 3: p += 4; break;
    default: ;
  }

  /* padding size */
  switch ((b[0] >> 3) & 3) {
    case 1:  this->packet_padsize = *p++; break;
    case 2:  this->packet_padsize = (uint16_t)_X_LE_16 (p); p += 2; break;
    case 3:  this->packet_padsize = (uint32_t)_X_LE_32 (p); p += 4; break;
    default: this->packet_padsize = 0;
  }

#ifdef LOG
  timestamp = _X_LE_32 (p); p += 4;
  duration  = _X_LE_16 (p); p += 2;
  lprintf ("timestamp=%u, duration=%u\n", timestamp, duration);
#else
  /* skip the above bytes */
  p += 6;
#endif

  if ((b[0] >> 5) & 3) {
    /* absolute data size */
    lprintf ("absolute data size\n");

    this->packet_padsize = this->packet_size - this->data_size; /* not used */
  } else {
    /* relative data size */
    lprintf ("relative data size\n");

    this->data_size = this->packet_size - this->packet_padsize;
  }

  if (this->packet_padsize > this->packet_size)
    /* skip packet */
    return ASF_INVALID_PAD_SIZE;

  /* Multiple frames */
  if (b[0] & 0x01) {
    this->frame_flag = *p++;
    this->nb_frames = (this->frame_flag & 0x3F);

    lprintf ("multiple frames %d\n", this->nb_frames);
  } else {
    this->frame_flag = 0;
    this->nb_frames = 1;
  }

  p_hdr_size += p - b;

  /* this->packet_size_left = this->packet_size - p_hdr_size; */
  this->packet_size_left = this->data_size - p_hdr_size;
  lprintf ("new packet, size = %d, size_left = %d, flags = 0x%02x, padsize = %d, this->packet_size = %d\n",
	   this->data_size, this->packet_size_left, this->packet_len_flags, this->packet_padsize, this->packet_size);

  return ASF_OK;
}

/* return 0 if ok */
static asf_error_t asf_parse_packet_payload_common (demux_asf_t *this,
  uint8_t raw_id, asf_demux_stream_t **stream, uint32_t *frag_offset, uint32_t *rlen) {
  uint8_t        stream_id;
  uint32_t       seq = 0;
  uint32_t       next_seq = 0;

  static const uint8_t sn[64] = {
    0,1,2,4, 1,2,3,5, 2,3,4,6, 4,5,6, 8, 1,2,3,5, 2,3,4,6, 3,4,5, 7, 5,6, 7, 9,
    2,3,4,6, 3,4,5,7, 4,5,6,8, 6,7,8,10, 4,5,6,8, 5,6,7,9, 6,7,8,10, 8,9,10,12
  };
  uint8_t b[20], *p = b;
  int need;

  need = sn[this->packet_prop_flags & 0x3f];
  if (this->input->read (this->input, b, need) != need)
    return ASF_EOF;

  stream_id  = raw_id & 0x7f;
  lprintf ("got raw_id=%d, stream_id=%d\n", raw_id, stream_id);

  {
    unsigned int i;
    for (i = 0; i < sizeof (this->seen_streams); i++) {
      if (stream_id == this->seen_streams[i])
        break;
      if (255 == this->seen_streams[i]) {
        this->seen_streams[i] = stream_id;
        xprintf (this->stream->xine, XINE_VERBOSITY_DEBUG,
          "demux_asf: seen stream #%d.\n", stream_id);
        break;
      }
    }
  }

  *stream = NULL;
  if (stream_id == this->audio_id)
    *stream = &this->streams[this->audio_stream];
  else if (stream_id == this->video_id)
    *stream = &this->streams[this->video_stream];

  if (*stream) {
    switch ((this->packet_prop_flags >> 4) & 3) {
      case 1:
        seq = *p++;
        (*stream)->seq = (*stream)->seq & 0xff;
        next_seq = ((*stream)->seq + 1) & 0xff;
      break;
      case 2:
        seq = _X_LE_16 (p); p += 2;
        (*stream)->seq = (*stream)->seq & 0xffff;
        next_seq = ((*stream)->seq + 1) & 0xffff;
      break;
      case 3:
        seq = _X_LE_32 (p); p += 4;
        next_seq = (*stream)->seq + 1;
      break;
      default:
        lprintf ("seq=0\n");
        seq = 0;
    }
    /* check seq number */
    lprintf ("stream_id = %d, seq = %d\n", stream_id, seq);
    if ((*stream)->first_seq || (*stream)->skip) {
      next_seq = seq;
      (*stream)->first_seq = 0;
    }
    if ((seq != (uint32_t)((*stream)->seq)) && (seq != next_seq)) {
      xprintf (this->stream->xine, XINE_VERBOSITY_DEBUG,
        "demux_asf: bad seq: seq = %d, next_seq = %d, stream seq = %d!\n", seq, next_seq, (*stream)->seq);
      /* the stream is corrupted, reset the decoder and restart at a new keyframe */
      if ((*stream)->fifo) {
        buf_element_t *buf = (*stream)->fifo->buffer_pool_alloc ((*stream)->fifo);
        buf->type = BUF_CONTROL_RESET_DECODER;
        (*stream)->fifo->put((*stream)->fifo, buf);
      }
      if (stream_id == this->video_id) {
        lprintf ("bad seq: waiting for keyframe\n");
        (*stream)->resync    =  1;
        (*stream)->skip      =  1;
        this->keyframe_found =  0;
      }
    }
    (*stream)->seq = seq;
  } else {
    p += sn[(this->packet_prop_flags >> 4) & 3];
  }

  switch ((this->packet_prop_flags >> 2) & 3) {
    case 1: *frag_offset = *p++; break;
    case 2: *frag_offset = _X_LE_16 (p); p += 2; break;
    case 3: *frag_offset = _X_LE_32 (p); p += 4; break;
    default:
      lprintf ("frag_offset=0\n");
      *frag_offset = 0;
  }

  switch (this->packet_prop_flags & 3) {
    case 1:  *rlen = *p++; break;
    case 2:  *rlen = _X_LE_16 (p); p += 2; break;
    case 3:  *rlen = _X_LE_32 (p); p += 4; break;
    default: *rlen = 0;
  }

  if (*rlen > (uint32_t)this->packet_size_left)
    /* skip packet */
    return ASF_INVALID_RLEN;

  lprintf ("segment header, stream id %02x, frag_offset %d, flags : %02x\n",
          stream_id, *frag_offset, *rlen);

  this->packet_size_left -= p - b;
  return ASF_OK;
}

/* return 0 if ok */
static asf_error_t asf_parse_packet_compressed_payload (demux_asf_t *this,
  asf_demux_stream_t *stream, uint8_t raw_id, uint32_t frag_offset, int64_t *timestamp) {
  uint32_t s_hdr_size = 0;
  uint32_t data_length = 0;
  uint32_t data_sent = 0;

  *timestamp = frag_offset;
  if (*timestamp)
    *timestamp -= this->asf_header->file->preroll;

  frag_offset = 0;

  if (this->packet_len_flags & 0x01) {
    /* multiple frames */
    static const int s[4] = {2, 1, 2, 4};
    int need = 1 + s[(this->frame_flag >> 6) & 3];
    uint8_t b[8];
    if (this->input->read (this->input, b + 3, need) != need)
      return ASF_EOF;
    s_hdr_size += need;
    switch ((this->frame_flag >> 6) & 3) {
      case 1: data_length = b[4]; break;
      default:
        lprintf ("invalid frame_flag %d\n", this->frame_flag);
        /* fall through */
      case 2: data_length = _X_LE_16 (b + 4); break;
      case 3: data_length = _X_LE_32 (b + 4); break;
    }
    lprintf ("reading multiple payload, size = %d\n", data_length);
  } else {
    uint8_t b[1];
    if (this->input->read (this->input, b, 1) != 1)
      return ASF_EOF;
    s_hdr_size += 1;
    data_length = this->packet_size_left - s_hdr_size;
    lprintf ("reading single payload, size = %d\n", data_length);
  }

  if (data_length > (uint32_t)this->packet_size_left)
    /* skip packet */
    return ASF_INVALID_DATA_LENGTH;

  this->packet_size_left -= s_hdr_size;

  while (data_sent < data_length) {
    int object_length;
    {
      uint8_t b[1];
      if (this->input->read (this->input, b, 1) != 1)
        return ASF_EOF;
      object_length = b[0];
    }
    lprintf ("sending grouped object, len = %d\n", object_length);

    if (stream && stream->fifo) {
      stream->payload_size = object_length;

      /* keyframe detection for non-seekable input plugins */
      if (stream->skip && (raw_id & 0x80) && !this->keyframe_found) {
        xprintf(this->stream->xine, XINE_VERBOSITY_DEBUG, "demux_asf: keyframe detected\n");
        this->keyframe_ts = *timestamp;
        this->keyframe_found = 1;
      }

      if (stream->resync && (this->keyframe_found) && (*timestamp >= this->keyframe_ts)) {
        xprintf(this->stream->xine, XINE_VERBOSITY_DEBUG, "demux_asf: stream resynced\n");
        stream->resync = 0;
        stream->skip = 0;
      }

      if (!stream->skip) {
        lprintf ("sending buffer of type %08x\n", stream->buf_type);

        if (stream->defrag)
          asf_send_buffer_defrag (this, stream, 0, *timestamp, object_length);
        else
          asf_send_buffer_nodefrag (this, stream, 0, *timestamp, object_length);
      } else {
        lprintf ("skip object\n");

        this->input->seek (this->input, object_length, SEEK_CUR);
      }
      stream->seq++;
    } else {
      lprintf ("unhandled stream type\n");

      this->input->seek (this->input, object_length, SEEK_CUR);
    }
    data_sent += object_length + 1;
    this->packet_size_left -= object_length + 1;
    *timestamp = 0;
  }
  *timestamp = frag_offset;
  return ASF_OK;
}

/* return 0 if ok */
static asf_error_t asf_parse_packet_payload (demux_asf_t *this,
  asf_demux_stream_t *stream, uint8_t raw_id, uint32_t frag_offset, uint32_t rlen, int64_t *timestamp) {
  uint32_t s_hdr_size = 0;
  uint32_t frag_len;
  uint32_t payload_size = 0;

  *timestamp = 0;
  if (rlen >= 8) {
    uint8_t b[8];
    if (this->input->read (this->input, b, 8) != 8)
      return ASF_EOF;
    payload_size = _X_LE_32 (b);
    *timestamp   = _X_LE_32 (b + 4);
    if (*timestamp)
      *timestamp -= this->asf_header->file->preroll;
    if (stream)
      stream->payload_size = payload_size;
    s_hdr_size += 8;
    rlen -= 8;
  }
  s_hdr_size += rlen;
  if (rlen)
    this->input->seek (this->input, rlen, SEEK_CUR);

  if (this->packet_len_flags & 0x01) {
    static const int s[4] = {2, 1, 2, 4};
    int need = s[(this->frame_flag >> 6) & 3];
    uint8_t b[4];
    if (this->input->read (this->input, b, need) != need)
      return ASF_EOF;
    s_hdr_size += need;
    switch ((this->frame_flag >> 6) & 3) {
      case 1: frag_len = b[0]; break;
      default:
        lprintf ("invalid frame_flag %d\n", this->frame_flag);
        /* fall through */
      case 2: frag_len = _X_LE_16 (b); break;
      case 3: frag_len = _X_LE_32 (b); break;
    }
    lprintf ("reading multiple payload, payload_size=%d, frag_len=%d\n", payload_size, frag_len);
  } else {
    frag_len = this->packet_size_left - s_hdr_size;
    lprintf ("reading single payload, payload_size=%d, frag_len = %d\n", payload_size, frag_len);
  }

  if (frag_len > (uint32_t)this->packet_size_left)
    /* skip packet */
    return ASF_INVALID_FRAGMENT_LENGTH;

  this->packet_size_left -= s_hdr_size;

  if (stream && stream->fifo) {
    if (!frag_offset) {
      /* keyframe detection for non-seekable input plugins */
      if (stream->skip && (raw_id & 0x80) && !this->keyframe_found) {
        xprintf(this->stream->xine, XINE_VERBOSITY_DEBUG, "demux_asf: keyframe detected\n");
        this->keyframe_found = 1;
        this->keyframe_ts = *timestamp;
      }
      if (stream->resync && this->keyframe_found && (*timestamp >= this->keyframe_ts) &&
          !frag_offset) {
        xprintf(this->stream->xine, XINE_VERBOSITY_DEBUG, "demux_asf: stream resynced\n");
        stream->resync = 0;
        stream->skip = 0;
      }
    }

    if (!stream->skip) {
      lprintf ("sending buffer of type %08x\n", stream->buf_type);

      if (stream->defrag)
        asf_send_buffer_defrag (this, stream, frag_offset, *timestamp, frag_len);
      else
        asf_send_buffer_nodefrag (this, stream, frag_offset, *timestamp, frag_len);
    } else {
      lprintf ("skip fragment\n");

      this->input->seek (this->input, frag_len, SEEK_CUR);
    }
  } else {
    lprintf ("unhandled stream type\n");

    this->input->seek (this->input, frag_len, SEEK_CUR);
  }
  this->packet_size_left -= frag_len;
  return ASF_OK;
}

static size_t demux_asf_read_file(demux_asf_t *this, char **pbuf)
{
  char           *buf = NULL;
  size_t          buf_size = 0;
  size_t          buf_used = 0;
  int             len;

  /* read file to memory.
   * warning: dumb code, but hopefuly ok since reference file is small */
  do {
    void *tmp;
    buf_size += 1024;
    tmp = realloc(buf, buf_size+1);
    if (!tmp)
      break;
    buf = tmp;

    len = this->input->read(this->input, &buf[buf_used], buf_size-buf_used);

    if( len > 0 )
      buf_used += len;

    /* 50k of reference file? no way. something must be wrong */
    if( buf_used > 50*1024 )
      break;
  } while( len > 0 );

  if (buf)
    buf[buf_used] = '\0';
  *pbuf = buf;

  return buf_used;
}

/*
 * parse a m$ http reference
 * format :
 * [Reference]
 * Ref1=http://www.blabla.com/blabla
 */
static int demux_asf_parse_http_references( demux_asf_t *this) {
  char           *buf = NULL;
  char           *ptr, *end;
  char           *href = NULL;
  int             free_href = 0;

  demux_asf_read_file(this, &buf);

  ptr = buf;
  if (ptr && !strncmp(ptr, "[Reference]", 11)) {

    const char *const mrl = this->input->get_mrl(this->input);
    if (!strncmp(mrl, "http", 4)) {
      /* never trust a ms server, reopen the same mrl with the mms input plugin
       * some servers are badly configured and return a incorrect reference.
       */
      href = strdup(mrl);
      free_href = 1;
    } else {
      ptr += 11;
      if (*ptr == '\r') ptr ++;
      if (*ptr == '\n') ptr ++;
      href = strchr(ptr, '=');
      if (!href) goto failure;
      href++;
      end = strchr(href, '\r');
      if (!end) goto failure;
      *end = '\0';
    }

    /* replace http by mmsh */
    if (!strncmp(href, "http", 4)) {
      memcpy(href, "mmsh", 4);
    }

    xprintf(this->stream->xine, XINE_VERBOSITY_DEBUG, "demux_asf: http ref: %s\n", href);
    _x_demux_send_mrl_reference (this->stream, 0, href, NULL, 0, 0);

    if (free_href)
      free(href);
  }

failure:
  free (buf);
  this->status = DEMUX_FINISHED;
  return this->status;
}

/*
 * parse a stupid ASF reference in an asx file
 * format : "ASF http://www.blabla.com/blabla"
 */
static int demux_asf_parse_asf_references( demux_asf_t *this) {
  char           *buf = NULL;
  size_t          i;

  demux_asf_read_file(this, &buf);

  if (buf && !strncmp(buf, "ASF ", 4)) {

    /* find the end of the string */
    for (i = 4; buf[i]; i++) {
      if ((buf[i] == ' ') || (buf[i] == '\r') || (buf[i] == '\n')) {
        buf[i] = '\0';
        break;
      }
    }

    xprintf(this->stream->xine, XINE_VERBOSITY_DEBUG, "demux_asf: asf ref: %s\n", buf + 4);
    _x_demux_send_mrl_reference (this->stream, 0, buf + 4, NULL, 0, 0);
  }

  free (buf);

  this->status = DEMUX_FINISHED;
  return this->status;
}


/* .asx playlist parser helper functions */
static uint32_t asx_get_time_value (const xml_node_t *node)
{
  const char *value = xml_parser_get_property (node, "VALUE");

  if (value)
  {
    int hours, minutes;
    double seconds;

    if (sscanf (value, "%d:%d:%lf", &hours, &minutes, &seconds) == 3)
      return hours * 3600000 + minutes * 60000 + seconds * 1000;

    if (sscanf (value, "%d:%lf", &minutes, &seconds) == 3)
      return minutes * 60000 + seconds * 1000;

    /* FIXME: single element is minutes or seconds? */
  }

  return 0; /* value not found */
}

/*
 * parse .asx playlist files
 */
static int demux_asf_parse_asx_references( demux_asf_t *this) {

  char           *buf = NULL;
  size_t          buf_used;
  xml_node_t     *xml_tree, *asx_entry, *asx_ref;
  xml_parser_t   *xml_parser;
  int             result;

  buf_used = demux_asf_read_file(this, &buf);
  if (!buf || buf_used < 1)
    goto failure;

  xml_parser = xml_parser_init_r(buf, buf_used, XML_PARSER_CASE_INSENSITIVE);
  if((result = xml_parser_build_tree_r(xml_parser, &xml_tree)) != XML_PARSER_OK) {
    xml_parser_finalize_r(xml_parser);
    goto failure;
  }

  xml_parser_finalize_r(xml_parser);

  if(!strcasecmp(xml_tree->name, "ASX")) {
    /* Attributes: VERSION, PREVIEWMODE, BANNERBAR
     * Child elements: ABSTRACT, AUTHOR, BASE, COPYRIGHT, DURATION, ENTRY,
                       ENTRYREF, MOREINFO, PARAM, REPEAT, TITLE
     */

    /*const char *base_href = NULL;*/

    for (asx_entry = xml_tree->child; asx_entry; asx_entry = asx_entry->next)
    {
      /*const char *ref_base_href = base_href;*/

      if (!strcasecmp (asx_entry->name, "ENTRY"))
      {
        /* Attributes: CLIENTSKIP, SKIPIFREF
         * Child elements: ABSTRACT, AUTHOR, BASE, COPYRIGHT, DURATION,
                           ENDMARKER, MOREINFO, PARAM, REF, STARTMARKER,
                           STARTTIME, TITLE
         */
        const char *href = NULL;
        const char *title = NULL;
        uint32_t start_time = ~0u;
        uint32_t duration = ~0u;

        for (asx_ref = asx_entry->child; asx_ref; asx_ref = asx_ref->next)
        {
          if (!strcasecmp(asx_ref->name, "REF"))
          {
            xml_node_t *asx_sub;
            /* Attributes: HREF
             * Child elements: DURATION, ENDMARKER, STARTMARKER, STARTTIME
             */

            /* FIXME: multiple REFs => alternative streams
             * (and per-ref start times and durations?).
             * Just the one title, though.
             */
            href = xml_parser_get_property (asx_ref, "HREF");

            for (asx_sub = asx_ref->child; asx_sub; asx_sub = asx_sub->next)
            {
              if (!strcasecmp (asx_sub->name, "STARTTIME"))
                start_time = asx_get_time_value (asx_sub);
              else if (!strcasecmp (asx_sub->name, "DURATION"))
                duration = asx_get_time_value (asx_sub);
            }
          }

          else if (!strcasecmp (asx_ref->name, "TITLE"))
          {
            if (!title)
              title = asx_ref->data;
          }

          else if (!strcasecmp (asx_ref->name, "STARTTIME"))
          {
            if (start_time == ~0u)
              start_time = asx_get_time_value (asx_ref);
          }

          else if (!strcasecmp (asx_ref->name, "DURATION"))
          {
            if (duration == ~0u)
              duration = asx_get_time_value (asx_ref);
          }

#if 0
          else if (!strcasecmp (asx_ref->name, "BASE"))
            /* Attributes: HREF */
            ref_base_href = xml_parser_get_property (asx_entry, "HREF");
#endif
        }

        /* FIXME: prepend ref_base_href to href */
        if (href && *href)
          _x_demux_send_mrl_reference (this->stream, 0, href, title,
                                       start_time == ~0u ? 0 : start_time,
                                       duration == ~0u ? ~0u : duration);
      }

      else if (!strcasecmp (asx_entry->name, "ENTRYREF"))
      {
        /* Attributes: HREF, CLIENTBIND */
        const char *href = xml_parser_get_property (asx_entry, "HREF");
        if (href && *href)
          _x_demux_send_mrl_reference (this->stream, 0, href, NULL, 0, -1);
      }

#if 0
      else if (!strcasecmp (asx_entry->name, "BASE"))
        /* Attributes: HREF */
        base_href = xml_parser_get_property (asx_entry, "HREF");
#endif
    }
  }
  else
    xprintf(this->stream->xine, XINE_VERBOSITY_DEBUG,
	    "demux_asf: Unsupported XML type: '%s'.\n", xml_tree->name);

  xml_parser_free_tree(xml_tree);
failure:
  free(buf);

  this->status = DEMUX_FINISHED;
  return this->status;
}


/*
 * xine specific functions start here
 */

static int demux_asf_send_chunk (demux_plugin_t *this_gen) {
  demux_asf_t *this = (demux_asf_t *) this_gen;

  switch (this->mode) {
    case ASF_MODE_ASX_REF:
      return demux_asf_parse_asx_references(this);

    case ASF_MODE_HTTP_REF:
      return demux_asf_parse_http_references(this);

    case ASF_MODE_ASF_REF:
      return demux_asf_parse_asf_references(this);

    case ASF_MODE_ENCRYPTED_CONTENT:
    case ASF_MODE_NO_CONTENT:
      this->status = DEMUX_FINISHED;
      return this->status;

    default:
    {
      asf_error_t e;
      uint32_t header_size = 0;

      e = asf_parse_packet_align (this);
      if (e) {
        xprintf (this->stream->xine, XINE_VERBOSITY_DEBUG,
          "demux_asf: asf_parse_packet_align: %s.\n", error_strings[e]);
        this->status = DEMUX_FINISHED;
        return this->status;
      }
      e = asf_parse_packet_ecd (this, &header_size);
      if (e) {
        xprintf (this->stream->xine, XINE_VERBOSITY_DEBUG,
          "demux_asf: asf_parse_packet_ecd: %s.\n", error_strings[e]);
        this->status = DEMUX_FINISHED;
        return this->status;
      }
      e = asf_parse_packet_payload_header (this, header_size);
      if (e) {
        xprintf (this->stream->xine, XINE_VERBOSITY_DEBUG,
          "demux_asf: asf_parse_packet_payload_header: %s.\n", error_strings[e]);
        this->status = DEMUX_FINISHED;
        return this->status;
      }

      for (this->frame = 0; this->frame < (this->nb_frames & 0x3f); this->frame++) {
        asf_demux_stream_t *stream = NULL;
        int64_t  ts = 0;
        uint32_t rlen = 0;
        uint32_t frag_offset = 0;
        uint8_t  raw_id;
        {
          uint8_t b[1];
          if (this->input->read (this->input, b, 1) != 1)
            break;
          raw_id = b[0];
        }
        this->packet_size_left -= 1;

        e = asf_parse_packet_payload_common (this, raw_id, &stream, &frag_offset, &rlen);
        if (e) {
          xprintf (this->stream->xine, XINE_VERBOSITY_DEBUG,
            "demux_asf: asf_parse_packet_payload_common: %s.\n", error_strings[e]);
          break;
        }
        if (rlen == 1) {
          e = asf_parse_packet_compressed_payload (this, stream, raw_id, frag_offset, &ts);
          if (e) {
            xprintf (this->stream->xine, XINE_VERBOSITY_DEBUG,
              "demux_asf: asf_parse_packet_compressed_payload: %s.\n", error_strings[e]);
            break;
          }
        } else {
          e = asf_parse_packet_payload (this, stream, raw_id, frag_offset, rlen, &ts);
          if (e) {
            xprintf (this->stream->xine, XINE_VERBOSITY_DEBUG,
              "demux_asf: asf_parse_packet_payload: %s.\n", error_strings[e]);
            break;
          }
        }
      }
      return this->status;
    }
  }
}

static void demux_asf_dispose (demux_plugin_t *this_gen) {

  demux_asf_t *this = (demux_asf_t *) this_gen;

  if (this->asf_header) {
    int i;

    for (i=0; i<this->asf_header->stream_count; i++) {
      asf_demux_stream_t *asf_stream;

      asf_stream = &this->streams[i];
      if (asf_stream->buffer) {
        free (asf_stream->buffer);
        asf_stream->buffer = NULL;
      }
    }

    asf_header_delete (this->asf_header);
  }

  free (this->reorder_temp);
  free (this);
}

static int demux_asf_get_status (demux_plugin_t *this_gen) {
  demux_asf_t *this = (demux_asf_t *) this_gen;

  return this->status;
}


static void demux_asf_send_headers (demux_plugin_t *this_gen) {
  demux_asf_t *this = (demux_asf_t *) this_gen;
  asf_guid_t guid;

  this->video_fifo     = this->stream->video_fifo;
  this->audio_fifo     = this->stream->audio_fifo;

  this->last_pts[0]    = 0;
  this->last_pts[1]    = 0;

  this->status         = DEMUX_OK;

  if (this->input->get_capabilities (this->input) & INPUT_CAP_SEEKABLE)
    this->input->seek (this->input, 0, SEEK_SET);

  if ((this->mode == ASF_MODE_ASX_REF) ||
      (this->mode == ASF_MODE_HTTP_REF) ||
      (this->mode == ASF_MODE_ASF_REF)) {
    _x_demux_control_start(this->stream);
    return;
  }

  guid = get_guid(this);
  if (guid != GUID_ASF_HEADER) {
    xprintf(this->stream->xine, XINE_VERBOSITY_DEBUG, "demux_asf: file doesn't start with an asf header\n");
    this->status = DEMUX_FINISHED;
    return;
  }

  demux_asf_send_headers_common(this);

  lprintf ("send header done\n");
}

static int demux_asf_seek (demux_plugin_t *this_gen,
			    off_t start_pos, int start_time, int playing) {

  demux_asf_t *this = (demux_asf_t *) this_gen;
  int i;

  lprintf ("demux_asf_seek: start_pos=%"PRId64", start_time=%d\n",
	   start_pos, start_time);

  this->status = DEMUX_OK;

  if (this->mode != ASF_MODE_NORMAL) {
    return this->status;
  }

  /*
   * seek to start position
   */
  for(i = 0; i < this->asf_header->stream_count; i++) {
    this->streams[i].frag_offset =  0;
    this->streams[i].first_seq   =  1;
    this->streams[i].seq         =  0;
    this->streams[i].timestamp   =  0;
  }
  this->last_pts[PTS_VIDEO] = 0;
  this->last_pts[PTS_AUDIO] = 0;
  this->keyframe_ts = 0;
  this->keyframe_found = 0;

  /* engine sync stuff */
  this->send_newpts   = 1;
  this->buf_flag_seek = 1;

  if (this->input->get_capabilities(this->input) & INPUT_CAP_SEEKABLE) {
    int state;

    _x_demux_flush_engine(this->stream);

    start_time /= 1000;
    start_pos = (off_t) ( (double) start_pos / 65535 *
                this->input->get_length (this->input) );

    if ( (!start_pos) && (start_time))
      start_pos = start_time * this->rate;

    if (start_pos < this->first_packet_pos)
      start_pos = this->first_packet_pos;

    /*
     * Find the previous keyframe
     *
     * states : 0  start, search a video keyframe
     *          1  video keyframe found, search an audio packet
     *          2  no video stream, search an audio packet
     *          5  end
     */
    state = 0;

    /* no video stream */
    if (this->video_stream == -1) {
      if (this->audio_stream == -1) {
        lprintf ("demux_asf_seek: no video stream, no audio stream\n");
        return this->status;
      } else {
        lprintf ("demux_asf_seek: no video stream\n");
        state = 2;
      }
    }

    /* force the demuxer to not send data to decoders */

    if (this->video_stream >= 0) {
      this->streams[this->video_stream].skip = 1;
      this->streams[this->video_stream].resync = 0;
    }
    if (this->audio_stream >= 0) {
      this->streams[this->audio_stream].skip = 1;
      this->streams[this->audio_stream].resync = 0;
    }

    start_pos -= (start_pos - this->first_packet_pos) % this->packet_size;
    while ((start_pos >= this->first_packet_pos) && (state != 5)) {
      asf_error_t e;
      uint32_t header_size;

      /* seek to the beginning of the previous packet */
      lprintf ("demux_asf_seek: seek back\n");

      if (this->input->seek (this->input, start_pos, SEEK_SET) != start_pos) {
        xprintf (this->stream->xine, XINE_VERBOSITY_DEBUG, "demux_asf: demux_asf_seek: seek failed\n");
        goto error;
      }
      header_size = 0;
      e = asf_parse_packet_ecd (this, &header_size);
      if (e) {
        xprintf (this->stream->xine, XINE_VERBOSITY_DEBUG,
          "demux_asf: asf_parse_packet_ecd: %s.\n", error_strings[e]);
        this->status = DEMUX_FINISHED;
        return this->status;
      }
      e = asf_parse_packet_payload_header (this, header_size);
      if (e) {
        xprintf (this->stream->xine, XINE_VERBOSITY_DEBUG,
          "demux_asf: asf_parse_packet_payload_header: %s.\n", error_strings[e]);
        this->status = DEMUX_FINISHED;
        return this->status;
      }

      for (this->frame = 0; this->frame < (this->nb_frames & 0x3f); this->frame++) {
        asf_demux_stream_t *stream = NULL;
        int64_t  ts;
        uint32_t rlen = 0;
        uint32_t frag_offset = 0;
        uint8_t  raw_id, stream_id;
        {
          uint8_t b[1];
          if (this->input->read (this->input, b, 1) != 1)
            break;
          raw_id = b[0];
        }
        this->packet_size_left -= 1;

        lprintf ("demux_asf_seek: raw_id = %d\n", (int)raw_id);

        stream_id = raw_id & 0x7f;
        e = asf_parse_packet_payload_common (this, raw_id, &stream, &frag_offset, &rlen);
        if (e) {
          xprintf (this->stream->xine, XINE_VERBOSITY_DEBUG,
            "demux_asf: asf_parse_packet_payload_common: %s.\n", error_strings[e]);
          break;
        }
        if (rlen == 1) {
          e = asf_parse_packet_compressed_payload (this, stream, raw_id, frag_offset, &ts);
          if (e) {
            xprintf (this->stream->xine, XINE_VERBOSITY_DEBUG,
              "demux_asf: asf_parse_packet_compressed_payload: %s.\n", error_strings[e]);
            break;
          }
        } else {
          e = asf_parse_packet_payload (this, stream, raw_id, frag_offset, rlen, &ts);
          if (e) {
            xprintf (this->stream->xine, XINE_VERBOSITY_DEBUG,
              "demux_asf: asf_parse_packet_payload: %s.\n", error_strings[e]);
            break;
          }
        }

        if (state == 0) {
          if (this->keyframe_found) {
            if (this->audio_stream == -1) {
              lprintf ("demux_asf_seek: no audio stream\n");
              state = 5;
            }
            state = 1; /* search an audio packet with pts < this->keyframe_pts */

            lprintf ("demux_asf_seek: keyframe found at %"PRId64", timestamp = %"PRId64"\n", start_pos, ts);
            check_newpts (this, ts * 90, 1, 0);
          }
        } else if (state == 1) {
          if ((stream_id == this->audio_id) && ts && (ts <= this->keyframe_ts)) {
            lprintf ("demux_asf_seek: audio packet found at %"PRId64", ts = %"PRId64"\n", start_pos, ts);
            state = 5; /* end */
            break;
          }
        } else if (state == 2) {
          if ((stream_id == this->audio_id) && !frag_offset) {
            this->keyframe_found = 1;
            this->keyframe_ts = ts;
            state = 5; /* end */
            lprintf ("demux_asf_seek: audio packet found at %"PRId64", timestamp = %"PRId64"\n", start_pos, ts);
            check_newpts (this, ts * 90, 0, 0);
          }
        }
      }
      start_pos -= this->packet_size;
    }
    if (state != 5) {
      xprintf(this->stream->xine, XINE_VERBOSITY_DEBUG, "demux_asf: demux_asf_seek: beginning of the stream\n");
      this->input->seek (this->input, this->first_packet_pos, SEEK_SET);
      this->keyframe_found = 1;
    } else {
      this->input->seek (this->input, start_pos + this->packet_size, SEEK_SET);
    }
    lprintf ("demux_asf_seek: keyframe_found=%d, keyframe_ts=%"PRId64"\n",
             this->keyframe_found, this->keyframe_ts);
    if (this->video_stream >= 0) {
      this->streams[this->video_stream].resync = 1;
      this->streams[this->video_stream].skip   = 1;
    }
    if (this->audio_stream >= 0) {
      this->streams[this->audio_stream].resync = 1;
      this->streams[this->audio_stream].skip   = 1;
    }
  } else if (!playing && this->input->seek_time != NULL) {
    if (start_pos && !start_time)
      start_time = this->length * start_pos / 65535;

    this->input->seek_time (this->input, start_time, SEEK_SET);

    this->keyframe_ts = 0;
    this->keyframe_found = 0; /* means next keyframe */
    if (this->video_stream >= 0) {
      this->streams[this->video_stream].resync = 1;
      this->streams[this->video_stream].skip   = 1;
    }
    if (this->audio_stream >= 0) {
      this->streams[this->audio_stream].resync = 0;
      this->streams[this->audio_stream].skip   = 0;
    }
  } else {
    /* "streaming" mode */
    this->keyframe_ts = 0;
    this->keyframe_found = 0; /* means next keyframe */
    if (this->video_stream >= 0) {
      this->streams[this->video_stream].resync = 1;
      this->streams[this->video_stream].skip   = 1;
    }
    if (this->audio_stream >= 0) {
      this->streams[this->audio_stream].resync = 0;
      this->streams[this->audio_stream].skip   = 0;
    }
  }
  return this->status;

error:
  this->status = DEMUX_FINISHED;
  return this->status;
}

static int demux_asf_get_stream_length (demux_plugin_t *this_gen) {

  demux_asf_t *this = (demux_asf_t *) this_gen;

  return this->length;
}

static uint32_t demux_asf_get_capabilities(demux_plugin_t *this_gen) {
  (void)this_gen;
  return DEMUX_CAP_NOCAP;
}

static int demux_asf_get_optional_data(demux_plugin_t *this_gen,
					void *data, int data_type) {
  (void)this_gen;
  (void)data;
  (void)data_type;

  return DEMUX_OPTIONAL_UNSUPPORTED;
}

static demux_plugin_t *open_plugin (demux_class_t *class_gen,
				    xine_stream_t *stream,
				    input_plugin_t *input) {

  demux_asf_t       *this;
  uint8_t            buf[MAX_PREVIEW_SIZE+1];
  int                len;

  switch (stream->content_detection_method) {
  case METHOD_BY_CONTENT:

    /*
     * try to get a preview of the data
     */

    len = _x_demux_read_header (input, buf, MAX_PREVIEW_SIZE);
    if (len < 16)
      return NULL;

    if (asf_guid_2_num (buf) != GUID_ASF_HEADER) {
      buf[len] = '\0';
      if( !strstr(buf,"asx") &&
          !strstr(buf,"ASX") &&
          strncmp(buf,"[Reference]", 11) &&
          strncmp(buf,"ASF ", 4) &&
	  memcmp(buf, "\x30\x26\xB2\x75", 4)
	  )
        return NULL;
    }

    lprintf ("file starts with an asf header\n");

    break;

  case METHOD_BY_MRL:
  case METHOD_EXPLICIT:
  break;

  default:
    xprintf (stream->xine, XINE_VERBOSITY_DEBUG,
	     "demux_asf: warning, unknown method %d\n", stream->content_detection_method);
    return NULL;
  }

  this = calloc (1, sizeof (demux_asf_t));
  if (!this)
    return NULL;

  this->reorder_temp = NULL;

  this->stream = stream;
  this->input  = input;

  this->audio_id = -1;
  this->video_id = -1;

  memset (this->seen_streams, 255, sizeof (this->seen_streams));

  /*
   * check for reference stream
   */
  this->mode = ASF_MODE_NORMAL;
  len = input->get_optional_data (input, buf, INPUT_OPTIONAL_DATA_PREVIEW);
  if ( (len == INPUT_OPTIONAL_UNSUPPORTED) &&
       (input->get_capabilities (input) & INPUT_CAP_SEEKABLE) ) {
    input->seek (input, 0, SEEK_SET);
    len=input->read (input, buf, 1024);
  }
  if(len > 0) {
    buf[len] = '\0';
    if( strstr((char*)buf,"asx") || strstr((char*)buf,"ASX") )
      this->mode = ASF_MODE_ASX_REF;
    if( strstr((char*)buf,"[Reference]") )
      this->mode = ASF_MODE_HTTP_REF;
    if( strstr((char*)buf,"ASF ") )
      this->mode = ASF_MODE_ASF_REF;
  }

  this->demux_plugin.send_headers      = demux_asf_send_headers;
  this->demux_plugin.send_chunk        = demux_asf_send_chunk;
  this->demux_plugin.seek              = demux_asf_seek;
  this->demux_plugin.dispose           = demux_asf_dispose;
  this->demux_plugin.get_status        = demux_asf_get_status;
  this->demux_plugin.get_stream_length = demux_asf_get_stream_length;
  this->demux_plugin.get_capabilities  = demux_asf_get_capabilities;
  this->demux_plugin.get_optional_data = demux_asf_get_optional_data;
  this->demux_plugin.demux_class       = class_gen;

  this->status = DEMUX_FINISHED;

  return &this->demux_plugin;
}

static void *init_class (xine_t *xine, const void *data) {

  (void)xine;
  (void)data;

  static const demux_class_t demux_asf_class = {
    .open_plugin     = open_plugin,
    .description     = N_("ASF demux plugin"),
    .identifier      = "ASF",
    .mimetypes       =
      "video/x-ms-asf: asf: ASF stream;"
      "video/x-ms-wmv: wmv: Windows Media Video;"
      "audio/x-ms-wma: wma: Windows Media Audio;"
      "application/vnd.ms-asf: asf: ASF stream;"
      "application/x-mplayer2: asf,asx,asp: mplayer2;"
      "video/x-ms-asf-plugin: asf,asx,asp: mms animation;"
      "video/x-ms-wvx: wvx: wmv metafile;"
      "video/x-ms-wax: wva: wma metafile;",
    /* asx, wvx, wax are metafile or playlist */
    .extensions      = "asf wmv wma asx wvx wax",
    .dispose         = NULL,
  };

  return (void *)&demux_asf_class;
}


/*
 * exported plugin catalog entry
 */
static const demuxer_info_t demux_info_asf = {
  .priority = 10,
};

const plugin_info_t xine_plugin_info[] EXPORTED = {
  /* type, API, "name", version, special_info, init_function */
  { PLUGIN_DEMUX, 27, "asf", XINE_VERSION_CODE, &demux_info_asf, init_class },
  { PLUGIN_NONE, 0, NULL, 0, NULL, NULL }
};
