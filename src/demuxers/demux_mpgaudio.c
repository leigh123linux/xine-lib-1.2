/*
 * Copyright (C) 2000-2007 the xine project
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
 * demultiplexer for mpeg audio (i.e. mp3) streams
 *
 * mp3 file structure:
 *   [id3v2][Xing|Vbri] Frame1 Frame2 Frame3...FrameX [Lyrics][id3v2][id3v1]
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>

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
#include "id3.h"

/* 2 preview buffers are enough,
 * the first mp3 frame is potentially a vbr header (xing, vbri)
 * the second mp3 frame is sent to the decoder
 */
#define NUM_PREVIEW_BUFFERS  2

#define WRAP_THRESHOLD       120000

#define FOURCC_TAG BE_FOURCC
#define RIFF_CHECK_BYTES 1024
#define RIFF_TAG FOURCC_TAG('R', 'I', 'F', 'F')
#define AVI_TAG FOURCC_TAG('A', 'V', 'I', ' ')
#define CDXA_TAG FOURCC_TAG('C', 'D', 'X', 'A')
#define MPEG_MARKER ME_FOURCC( 0x00, 0x00, 0x01, 0xBA )


/* Xing header stuff */
#define XING_TAG FOURCC_TAG('X', 'i', 'n', 'g')
#define XING_FRAMES_FLAG     0x0001
#define XING_BYTES_FLAG      0x0002
#define XING_TOC_FLAG        0x0004
#define XING_VBR_SCALE_FLAG  0x0008
#define XING_TOC_LENGTH      100

/* Xing header stuff */
#define VBRI_TAG FOURCC_TAG('V', 'B', 'R', 'I')

/* mp3 frame struct */
typedef struct {
  /* header */
  double               duration;
  uint32_t             size;                 /* in bytes */
  uint32_t             bitrate;              /* in bit per second */
  uint16_t             freq;                 /* in Hz */

  uint8_t              layer;

  uint8_t              version_idx:2;        /* 0: mpeg1, 1: mpeg2, 2: mpeg2.5 */
  uint8_t              lsf_bit:1;
  uint8_t              channel_mode:3;
} mpg_audio_frame_t;

/* Xing Vbr Header struct */
typedef struct {
  uint32_t             flags;
  uint32_t             stream_frames;
  uint32_t             stream_size;
  uint8_t              toc[XING_TOC_LENGTH];
  uint32_t             vbr_scale;
} xing_header_t;

/* Vbri Vbr Header struct */
typedef struct {
  uint16_t             version;
  uint16_t             delai;
  uint16_t             quality;
  uint32_t             stream_size;
  uint32_t             stream_frames;
  uint16_t             toc_entries;
  uint16_t             toc_scale_factor;
  uint16_t             entry_size;
  uint16_t             entry_frames;
  int                 *toc;
} vbri_header_t;

/* demuxer instance struct */
typedef struct {

  demux_plugin_t       demux_plugin;
  xine_stream_t       *stream;
  fifo_buffer_t       *audio_fifo;
  input_plugin_t      *input;
  int                  status;

  uint32_t             stream_length;    /* in seconds */
  int                  br;               /* bitrate in bits/second */
  uint32_t             blocksize;

  mpg_audio_frame_t    cur_frame;
  double               cur_time;         /* in milliseconds */

  off_t                mpg_frame_start;  /* offset */
  off_t                mpg_frame_end;    /* offset */
  off_t                mpg_size;         /* in bytes */

  int                  check_vbr_header;
  xing_header_t       *xing_header;
  vbri_header_t       *vbri_header;
  
} demux_mpgaudio_t ;

/* demuxer class struct */
typedef struct {

  demux_class_t     demux_class;

  /* class-wide, global variables here */

  xine_t           *xine;

} demux_mpgaudio_class_t;

/*
 * Parse a mp3 frame
 * return 1 on success
 */
static int parse_frame_header(mpg_audio_frame_t *const frame, const uint8_t *const buf) {
  /* bitrate table[mpeg version][layer][bitrate index]
   * values stored in kbps
   */
  static const uint16_t mp3_bitrates[3][3][16] = {
    { {0, 32, 64, 96, 128, 160, 192, 224, 256, 288, 320, 352, 384, 416, 448,},
      {0, 32, 48, 56,  64,  80,  96, 112, 128, 160, 192, 224, 256, 320, 384,},
      {0, 32, 40, 48,  56,  64,  80,  96, 112, 128, 160, 192, 224, 256, 320,} },
    { {0, 32, 48, 56,  64,  80,  96, 112, 128, 144, 160, 176, 192, 224, 256,},
      {0,  8, 16, 24,  32,  40,  48,  56,  64,  80,  96, 112, 128, 144, 160,},
      {0,  8, 16, 24,  32,  40,  48,  56,  64,  80,  96, 112, 128, 144, 160,} },
    { {0, 32, 48, 56,  64,  80,  96, 112, 128, 144, 160, 176, 192, 224, 256,},
      {0,  8, 16, 24,  32,  40,  48,  56,  64,  80,  96, 112, 128, 144, 160,},
      {0,  8, 16, 24,  32,  40,  48,  56,  64,  80,  96, 112, 128, 144, 160,} }
  };

  /* frequency table[mpeg version][frequence index] (in KHz) */
  static const uint16_t mp3_freqs[3][3] = {
    { 44100, 48000, 32000 },
    { 22050, 24000, 16000 },
    { 11025, 12000,  8000 }
  };

  /* samples per frame table[mpeg version][layer] */
  static const uint16_t mp3_samples[3][3] = {
    { 384, 1152, 1152 },
    { 384, 1152, 576 },
    { 384, 1152, 576 }
  };

  struct {
    uint16_t  mpeg25_bit:1;
    uint16_t  bitrate_idx:4;
    uint16_t  freq_idx:3;
    uint16_t  padding_bit:1;
    uint16_t  channel_mode:3;

#if 0 /* Unused */
    uint16_t  protection_bit:1;
    uint16_t  private_bit:1;
    uint16_t  mode_extension:3;
    uint16_t  copyright:1;
    uint16_t  original:1;
#endif

#if defined(OPT_STRICT)
    uint16_t  emphasis:3;
#endif
  } frame_header;

  const uint32_t head = _X_BE_32(buf);
  const uint16_t frame_sync = head >> 21;

  lprintf("header: %08X\n", head);
  if (frame_sync != 0x7ff) {
    lprintf("invalid frame sync\n");
    return 0;
  }

  frame_header.mpeg25_bit     = (head >> 20) & 0x1;
  frame->lsf_bit        = (head >> 19) & 0x1;
  if (!frame_header.mpeg25_bit) {
    if (frame->lsf_bit) {
      lprintf("reserved mpeg25 lsf combination\n");
      return 0;
    } else
      frame->version_idx = 2;  /* MPEG Version 2.5 */
  } else {
    if (!frame->lsf_bit)
      frame->version_idx = 1;  /* MPEG Version 2 */
    else
      frame->version_idx = 0;  /* MPEG Version 1 */
  }

  frame->layer = 4 - ((head >> 17) & 0x3);
  if (frame->layer == 4) {
    lprintf("reserved layer\n");
    return 0;
  }

  frame_header.bitrate_idx    = (head >> 12) & 0xf;
  if ((frame_header.bitrate_idx == 0) || (frame_header.bitrate_idx == 15)) {
    lprintf("invalid bitrate index\n");
    return 0;
  }

  frame_header.freq_idx       = (head >> 10) & 0x3;
  if (frame_header.freq_idx == 3) {
    lprintf("invalid frequence index\n");
    return 0;
  }

  frame_header.padding_bit    = (head >>  9) & 0x1;
  frame_header.channel_mode   = (head >>  6) & 0x3;

#if 0 /* Unused */
  frame_header.protection_bit = (head >> 16) & 0x1;
  frame_header.private_bit    = (head >>  8) & 0x1;
  frame_header.mode_extension = (head >>  4) & 0x3;
  frame_header.copyright      = (head >>  3) & 0x1;
  frame_header.original       = (head >>  2) & 0x1;
#endif

#if defined(OPT_STRICT)
  frame_header.emphasis       =  head        & 0x3;

  /*
   * ISO/IEC 11172-3 says this is a reserved emphasis value, but
   * streams exist which use it anyway. Since the value is not important
   * to the decoder proper, we allow it unless OPT_STRICT is defined.
   */
  if (frame_header.emphasis == 2) {
    lprintf("reserved emphasis\n");
    return 0;
  }
#endif
  
  {
    const uint16_t samples = mp3_samples[frame->version_idx][frame->layer - 1];
    frame->bitrate = mp3_bitrates[frame->version_idx][frame->layer - 1][frame_header.bitrate_idx] * 1000;
    frame->freq    = mp3_freqs[frame->version_idx][frame_header.freq_idx];
  
    frame->size  = samples * (frame->bitrate / 8);
    frame->size /= frame->freq;
    /* Padding: only if padding_bit is set; 4 bytes for Layer 1 and 1 byte for others */
    frame->size += ( frame_header.padding_bit ? ( frame->layer == 1 ? 4 : 1 ) : 0 );

    frame->duration  = 1000.0f * (double)samples / (double)frame->freq;
  }

  lprintf("mpeg %d, layer %d\n", frame->version_idx + 1, frame->layer);
  lprintf("bitrate: %d bps, samplerate: %d Hz\n", frame->bitrate, frame->freq);
  lprintf("length: %d bytes, %f ms\n", frame->size, frame->duration);
  lprintf("padding: %d bytes\n", ( frame_header.padding_bit ? ( frame->layer == 1 ? 4 : 1 ) : 0 ));
  return 1;
}

/*
 * Parse a Xing header
 * return the Xing header or NULL on error
 */
static xing_header_t* parse_xing_header(mpg_audio_frame_t *frame,
                                        uint8_t *buf, int bufsize) {

#ifdef LOG
  int i;
#endif
  uint8_t *ptr = buf;
  xing_header_t *xing;

  xing = xine_xmalloc (sizeof (xing_header_t));
  if (!xing)
    return NULL;

  /* offset of the Xing header */
  if (frame->lsf_bit) {
    if (frame->channel_mode != 3)
      ptr += (32 + 4);
    else
      ptr += (17 + 4);
  } else {
    if (frame->channel_mode != 3)
      ptr += (17 + 4);
    else
      ptr += (9 + 4);
  }
  
  if (ptr >= (buf + bufsize - 4)) return 0;
  lprintf("checking %08X\n", *ptr);
  if (_X_BE_32(ptr) == XING_TAG) {
    lprintf("Xing header found\n");
    ptr += 4;
    
    if (ptr >= (buf + bufsize - 4)) return 0;
    xing->flags = _X_BE_32(ptr); ptr += 4;

    if (xing->flags & XING_FRAMES_FLAG) {
      if (ptr >= (buf + bufsize - 4)) return 0;
      xing->stream_frames = _X_BE_32(ptr); ptr += 4;
      lprintf("stream frames: %d\n", xing->stream_frames);
    }
    if (xing->flags & XING_BYTES_FLAG) {
      if (ptr >= (buf + bufsize - 4)) return 0;
      xing->stream_size = _X_BE_32(ptr); ptr += 4;
      lprintf("stream size: %d\n", xing->stream_size);
    }
    if (xing->flags & XING_TOC_FLAG) {
      lprintf("toc found\n");
      if (ptr >= (buf + bufsize - XING_TOC_LENGTH)) return 0;

      memcpy(xing->toc, ptr, XING_TOC_LENGTH);
#ifdef LOG
      for (i = 0; i < XING_TOC_LENGTH; i++) {
        lprintf("%d ", xing->toc[i]);
      }
      lprintf("\n");
#endif
      ptr += XING_TOC_LENGTH;
    }
    xing->vbr_scale = -1;
    if (xing->flags & XING_VBR_SCALE_FLAG) {
      if (ptr >= (buf + bufsize - 4)) return 0;
      xing->vbr_scale = _X_BE_32(ptr);
      lprintf("vbr_scale: %d\n", xing->vbr_scale);
    }

    return xing;
  } else {
    lprintf("Xing header not found\n");
    free(xing);
    return NULL;
  }
}

/*
 * Parse a Vbri header
 * return the Vbri header or NULL on error
 */
static vbri_header_t* parse_vbri_header(mpg_audio_frame_t *frame,
                                        uint8_t *buf, int bufsize) {

  int i;
  uint8_t *ptr = buf;
  vbri_header_t *vbri;

  vbri = xine_xmalloc (sizeof (vbri_header_t));
  if (!vbri)
    return NULL;

  ptr += (32 + 4);
  
  if ((ptr + 4) >= (buf + bufsize)) return 0;
  lprintf("Checking %08X\n", *ptr);
  if (_X_BE_32(ptr) == VBRI_TAG) {
    lprintf("Vbri header found\n");
    ptr += 4;
    
    if ((ptr + 22) >= (buf + bufsize)) return 0;
    vbri->version           = _X_BE_16(ptr); ptr += 2;
    vbri->delai             = _X_BE_16(ptr); ptr += 2;
    vbri->quality           = _X_BE_16(ptr); ptr += 2;
    vbri->stream_size       = _X_BE_32(ptr); ptr += 4;
    vbri->stream_frames     = _X_BE_32(ptr); ptr += 4;
    vbri->toc_entries       = _X_BE_16(ptr); ptr += 2;
    vbri->toc_scale_factor  = _X_BE_16(ptr); ptr += 2;
    vbri->entry_size        = _X_BE_16(ptr); ptr += 2;
    vbri->entry_frames      = _X_BE_16(ptr); ptr += 2;
    lprintf("version: %d\n", vbri->version);
    lprintf("delai: %d\n", vbri->delai);
    lprintf("quality: %d\n", vbri->quality);
    lprintf("stream_size: %d\n", vbri->stream_size);
    lprintf("stream_frames: %d\n", vbri->stream_frames);
    lprintf("toc_entries: %d\n", vbri->toc_entries);
    lprintf("toc_scale_factor: %d\n", vbri->toc_scale_factor);
    lprintf("entry_size: %d\n", vbri->entry_size);
    lprintf("entry_frames: %d\n", vbri->entry_frames);

    if ((ptr + (vbri->toc_entries + 1) * vbri->entry_size) >= (buf + bufsize)) return 0;
    vbri->toc = xine_xcalloc ((vbri->toc_entries + 1), sizeof(int));
    if (!vbri->toc) {
      free (vbri);
      return NULL;
    }

    lprintf("toc entries: %d\n", vbri->toc_entries);
    for (i = 0; i <= vbri->toc_entries; i++) {
      int j;
      uint32_t value = 0;

      for (j = 0; j < vbri->entry_size; j++) {
        value = (value << 8) | *(ptr + i * vbri->entry_size + j);
      }
      vbri->toc[i] = value;
#ifdef LOG
      printf("%d ", vbri->toc[i]);
#endif
    }
#ifdef LOG
    printf("\n");
#endif
    {
      int64_t toc_stream_size = 0;

      /* Compute the stream size using the toc */
      for (i = 0; i <= vbri->toc_entries; i++) {
        toc_stream_size += vbri->toc[i];
      }
      lprintf("stream size from toc: %"PRId64"\n", toc_stream_size);
    }

    return vbri;
  } else {
    lprintf("Vbri header not found\n");
    free (vbri);
    return NULL;
  }
}

/*
 * Parse a mp3 frame paylod
 * return 1 on success, 0 on error
 */
static int parse_frame_payload(demux_mpgaudio_t *this,
                               uint8_t *frame_header,
                               int decoder_flags) {
  buf_element_t *buf;
  off_t          frame_pos, len;
  uint64_t       pts = 0;

  frame_pos = this->input->get_current_pos(this->input) - 4;
  lprintf("frame_pos = %"PRId64"\n", frame_pos);

  buf = this->audio_fifo->buffer_pool_alloc(this->audio_fifo);

  if (this->cur_frame.size > buf->max_size) {
    xprintf(this->stream->xine, XINE_VERBOSITY_LOG,
            "demux_mpgaudio: frame size is greater than fifo buffer size\n");
    buf->free_buffer(buf);
    return 0;
  }
  
  /* the decoder needs the frame header */
  memcpy(buf->mem, frame_header, 4);

  len = this->input->read(this->input, buf->mem + 4, this->cur_frame.size - 4);
  if (len != (this->cur_frame.size - 4)) {
    buf->free_buffer(buf);
    return 0;
  }

  if (this->check_vbr_header) {
    this->check_vbr_header = 0;
    this->mpg_frame_start = frame_pos;
    this->xing_header = parse_xing_header(&this->cur_frame, buf->mem, this->cur_frame.size);
    if (this->xing_header) {
      buf->free_buffer(buf);
      return 1;
    }
    this->vbri_header = parse_vbri_header(&this->cur_frame, buf->mem, this->cur_frame.size);
    if (this->vbri_header) {
      buf->free_buffer(buf);
      return 1;
    }
  }
  

  pts = (int64_t)(this->cur_time * 90.0f);

  if (this->stream_length)
    buf->extra_info->input_normpos = (this->cur_time * 65535.0f) / this->stream_length;

  buf->extra_info->input_time = this->cur_time;
  buf->pts                    = pts;
  buf->size                   = len + 4;
  buf->content                = buf->mem;
  buf->type                   = BUF_AUDIO_MPEG;
  buf->decoder_info[0]        = 1;
  buf->decoder_flags          = decoder_flags|BUF_FLAG_FRAME_END;

  this->audio_fifo->put(this->audio_fifo, buf);
  lprintf("send buffer: pts=%"PRId64"\n", pts);  
  this->cur_time += this->cur_frame.duration;
  return 1;
}


/* Scan through the preview buffer to find a potential
 * 32-bit MP3 frame header.
 * return 1 if found, 0 if not found
 */
static int sniff_buffer_looks_like_mp3 (uint8_t *buf, int buflen)
{
  int offset;
  mpg_audio_frame_t frame;

  if (buf == NULL)
    return 0;

  for (offset = 0; (offset + 4) < buflen; offset++) {

    if (parse_frame_header(&frame, buf + offset)) {
      size_t size = frame.size;

      /* Since one frame is available, is there another frame
       * just to be sure this is more likely to be a real MP3
       * buffer? */
      offset += size;

      if (offset + 4 >= buflen) {
        return 0;
      }

      if (parse_frame_header(&frame, buf + offset)) {
        lprintf("mpeg audio frame detected\n");
        return 1;
      }
      break;
    }
  }
  return 0;
}

/*
 * Read a mp3 frame header (4 bytes)
 */
static int read_frame_header(demux_mpgaudio_t *this, uint8_t *header_buf, int bytes) {
  off_t len;
  int i;
  
  for (i = 0; i < (4 - bytes); i++) {
    header_buf[i] = header_buf[i + bytes];
  }

  len = this->input->read(this->input, header_buf + 4 - bytes, bytes);
  if (len != ((off_t) bytes)) {
    return 0;
  }
  return 1;
}

/*
 * Parse next mp3 frame
 */
static int demux_mpgaudio_next (demux_mpgaudio_t *this, int decoder_flags, int send_header) {
  uint8_t  header_buf[4];
  int      bytes = 4;

  for (;;) {

    if (read_frame_header(this, header_buf, bytes)) {

      if (parse_frame_header(&this->cur_frame, header_buf)) {

	/* send header buffer */
	if ( send_header ) {
	  buf_element_t *buf;
	  
	  buf = this->audio_fifo->buffer_pool_alloc(this->audio_fifo);
	  
	  buf->type = BUF_AUDIO_MPEG;
	  buf->decoder_flags = BUF_FLAG_HEADER|BUF_FLAG_STDHEADER|BUF_FLAG_FRAME_END;
	  
	  buf->decoder_info[0] = 0;
	  buf->decoder_info[1] = this->cur_frame.freq;
	  buf->decoder_info[2] = 0; /* bits_per_sample */
	  
	  /* Only for channel_mode == 3 (mono) there is one channel, for any other case, there are 2 */
	  buf->decoder_info[3] = ( this->cur_frame.channel_mode == 3 ) ? 1 : 2;
	  
	  buf->size = 0; /* No extra header data */
	  
	  this->audio_fifo->put(this->audio_fifo, buf);
	}
	
        return parse_frame_payload(this, header_buf, decoder_flags);
        
      } else if ( id3v2_istag(header_buf) ) {
	if (!id3v2_parse_tag(this->input, this->stream, header_buf)) {
          xprintf(this->stream->xine, XINE_VERBOSITY_LOG,
                  LOG_MODULE ": ID3V2 tag parsing error\n");
          bytes = 1; /* resync */
        } else {
          bytes = 4;
        }
      } else {
        /* skip */
        bytes = 1;
      }

    } else {
      lprintf("read error\n");
      return 0;
    }
  }
}

static int demux_mpgaudio_send_chunk (demux_plugin_t *this_gen) {

  demux_mpgaudio_t *this = (demux_mpgaudio_t *) this_gen;

  if (!demux_mpgaudio_next (this, 0, 0))
    this->status = DEMUX_FINISHED;

  return this->status;
}

static int demux_mpgaudio_get_status (demux_plugin_t *this_gen) {
  demux_mpgaudio_t *this = (demux_mpgaudio_t *) this_gen;

  return this->status;
}

static int demux_mpgaudio_read_head(input_plugin_t *input, uint8_t *buf) {

  int       bs = 0;

  if (INPUT_IS_SEEKABLE(input)) {
    input->seek(input, 0, SEEK_SET);

    bs = input->read(input, buf, MAX_PREVIEW_SIZE);

    lprintf("stream is seekable\n");

  } else if ((input->get_capabilities(input) & INPUT_CAP_PREVIEW) != 0) {

    lprintf("input plugin provides preview\n");

    bs = input->get_optional_data (input, buf, INPUT_OPTIONAL_DATA_PREVIEW);

  } else {
    lprintf("not seekable, no preview\n");
    return 0;
  }
  return bs;
}

/*
 * mp3 stream detection
 * return 1 if detected, 0 otherwise
 */
static int detect_mpgaudio_file(input_plugin_t *input) {
  mpg_audio_frame_t frame;
  uint8_t buf[MAX_PREVIEW_SIZE];
  int preview_len;
  uint32_t head;

  preview_len = demux_mpgaudio_read_head(input, buf);
  if (preview_len < 4)
    return 0;

  head = _X_ME_32(buf);

  lprintf("got preview %08x\n", head);

  if ((head == ID3V22_TAG) ||
      (head == ID3V23_TAG) ||
      (head == ID3V24_TAG)) {
    /* check if a mp3 frame follows the tag
     * id3v2 are not specific to mp3 files,
     * flac files can contain id3v2 tags
     */
    int tag_size = _X_BE_32_synchsafe(&buf[6]);
    lprintf("try to skip id3v2 tag (%d bytes)\n", tag_size);
    if ((10 + tag_size) >= preview_len) {
      lprintf("cannot skip id3v2 tag\n");
      return 0;
    }
    if ((10 + tag_size + 4) >= preview_len) {
      lprintf("cannot read mp3 frame header\n");
      return 0;
    }
    if (!parse_frame_header(&frame, &buf[10 + tag_size])) {
      lprintf ("invalid mp3 frame header\n");
      return 0;
    } else {
      lprintf ("a valid mp3 frame follows the id3v2 tag\n");
    }
  } else if (head == MPEG_MARKER) {
    return 0;
  } else if (!sniff_buffer_looks_like_mp3(buf, preview_len)) {
    lprintf ("sniff_buffer_looks_like_mp3 failed\n");
    return 0;
  }
  return 1;
}

static void demux_mpgaudio_send_headers (demux_plugin_t *this_gen) {

  demux_mpgaudio_t *this = (demux_mpgaudio_t *) this_gen;
  int i;

  this->stream_length = 0;
  this->status        = DEMUX_OK;

  _x_stream_info_set(this->stream, XINE_STREAM_INFO_HAS_VIDEO, 0);
  _x_stream_info_set(this->stream, XINE_STREAM_INFO_HAS_AUDIO, 1);

  _x_demux_control_start (this->stream);

  /* read id3 info only from inputs with seeking and without "live" flag */
  if (INPUT_IS_SEEKABLE(this->input)) {
    off_t pos;

    /* check ID3 v1 at the end of the stream */
    pos = this->input->get_length(this->input) - 128;
    if(pos > 0) {
      if (pos == this->input->seek (this->input, pos, SEEK_SET))
        id3v1_parse_tag (this->input, this->stream);
    }

    /* seek back to the beginning */
    this->input->seek (this->input, 0, SEEK_SET);

    /*
     * send preview buffers
     */
    this->check_vbr_header = 1;
    for (i = 0; i < NUM_PREVIEW_BUFFERS; i++) {
      if (!demux_mpgaudio_next (this, BUF_FLAG_PREVIEW, i == 0)) {
        break;
      }
    }
    
    if (this->xing_header) {
      xing_header_t *xing = this->xing_header;

      this->mpg_size = xing->stream_size;
      this->mpg_frame_end = this->mpg_frame_start + this->mpg_size;
      this->stream_length = (double)xing->stream_frames * this->cur_frame.duration;
      /* compute abr */
      if (this->stream_length) {
        this->br = ((uint64_t)xing->stream_size * 8 * 1000) / this->stream_length;
      }
      
    } else if (this->vbri_header) {
      vbri_header_t *vbri = this->vbri_header;

      this->mpg_size = vbri->stream_size;
      this->mpg_frame_end = this->mpg_frame_start + this->mpg_size;
      this->stream_length = (double)vbri->stream_frames * this->cur_frame.duration;
      /* compute abr */
      if (this->stream_length) {
        this->br = ((uint64_t)vbri->stream_size * 8 * 1000) / this->stream_length;
      }
    }
 
    /* Set to default if Vbr header is incomplete or not present */
    if (!this->br) {
      /* assume CBR */
      this->br = this->cur_frame.bitrate;
    }
    if (!this->mpg_frame_end) {
      this->mpg_frame_end = this->input->get_length(this->input);
    }
    if (!this->mpg_size) {
      this->mpg_size = this->mpg_frame_end - this->mpg_frame_start;
    }
    if (!this->stream_length && this->br) {
      this->stream_length = (this->mpg_size * 1000) / (this->br / 8);
    }

    _x_stream_info_set(this->stream, XINE_STREAM_INFO_BITRATE, this->br);
    _x_stream_info_set(this->stream, XINE_STREAM_INFO_AUDIO_BITRATE, this->br);
    lprintf("frame_start: %"PRId64", frame_end: %"PRId64"\n",
            this->mpg_frame_start, this->mpg_frame_end);
    lprintf("stream size: %"PRId64", mp3 size: %"PRId64"\n",
            this->input->get_length(this->input),
            this->mpg_size);
    lprintf("stream_length: %d ms\n", this->stream_length);

    /* set codec infos here
     * the decoder doesn't know if the stream is VBR
     */
    {
      char scratch_buf[256];
      char *mpeg_ver[3] = {"1", "2", "2.5"};
      
      snprintf(scratch_buf, 256, "MPEG %s Layer %1d%s",
               mpeg_ver[this->cur_frame.version_idx], this->cur_frame.layer,
               (this->xing_header)? " VBR" : " CBR" );
      _x_meta_info_set_utf8(this->stream, XINE_META_INFO_AUDIOCODEC, scratch_buf);
    }
  } else {
    for (i = 0; i < NUM_PREVIEW_BUFFERS; i++) {
      if (!demux_mpgaudio_next (this, BUF_FLAG_PREVIEW, i == 0)) {
        break;
      }
    }
  }

  this->status = DEMUX_OK;
}

/*
 * interpolate in Xing TOC to get file seek point in bytes
 * return the stream offset of the seekpoint
 */
static off_t xing_get_seek_point(xing_header_t *xing, int time, int stream_length)
{
  off_t seekpoint;
  int a;
  float fa, fb, fx;
  float percent;

  percent = ((float)time * 100.0f)/ (float)stream_length;
  if (percent < 0.0f)   percent = 0.0f;
  if (percent > 100.0f) percent = 100.0f;

  a = (int)percent;
  if (a > 99) a = 99;
  fa = xing->toc[a];
  if (a < 99) {
      fb = xing->toc[a + 1];
  } else {
      fb = 256.0f;
  }

  fx = fa + (fb - fa) * (percent - a);
  seekpoint = (off_t)((1.0f / 256.0f) * fx * xing->stream_size);

  return seekpoint;
}

/*
 * Interpolate in Vbri TOC to get file seek point in bytes
 * return the stream offset of the seekpoint
 */
static off_t vbri_get_seek_point(vbri_header_t *vbri, int time, int stream_length)
{
  double fa, fb, fx;
  double toc_entry;
  int i;
  int a;

  toc_entry = ((float)time * (float)(vbri->toc_entries + 1)) /
              (float)stream_length;
  lprintf("time: %d, stream length: %d, toc entry: %f\n",
          time, stream_length, toc_entry);
  if (toc_entry < 0.0f)
    toc_entry = 0.0f;
  if (toc_entry > (float)vbri->toc_entries)
    toc_entry = (float)vbri->toc_entries;

  a = (int)toc_entry;
  if (a > (vbri->toc_entries - 1))
    a = vbri->toc_entries - 1;

  /* compute the stream offset of the toc entry */
  fa = 0.0f;
  for (i = 0; i < a; i++) {
    fa += (double)vbri->toc[i];
  }
  /* compute the stream offset of the next toc entry */
  fb = fa + (double)vbri->toc[a];

  /* interpolate */
  fx = fa + (fb - fa) * (toc_entry - (double)a);

  return (off_t)fx;
}

/*
 * Seeking function
 * Try to use the Vbr header if present.
 * If no Vbr header is present then use a CBR formula
 *
 * Position seek is relative to the total time of the stream, the position
 * is converted to a time at the beginning of the function
 */
static int demux_mpgaudio_seek (demux_plugin_t *this_gen,
                                off_t start_pos, int start_time, int playing) {

  demux_mpgaudio_t *this = (demux_mpgaudio_t *) this_gen;
  off_t seek_pos = this->mpg_frame_start;

  if ((this->input->get_capabilities(this->input) & INPUT_CAP_SEEKABLE) != 0) {
    /* Convert position seek to time seek */
    if (!start_time) {
      start_time = (int)((double)start_pos * (double)this->stream_length / 65535.0f);
      lprintf("position seek: start_pos=%"PRId64" => start_time=%d\n", start_pos, start_time);
    }

    if (start_time < 0)
      start_time = 0;
    if (start_time > this->stream_length)
      start_time = this->stream_length;

    if (this->stream_length > 0) {
      if (this->xing_header &&
          (this->xing_header->flags & (XING_TOC_FLAG | XING_BYTES_FLAG))) {
        seek_pos += xing_get_seek_point(this->xing_header, start_time, this->stream_length);
        lprintf("time seek: xing: time=%d, pos=%"PRId64"\n", start_time, seek_pos);
      } else if (this->vbri_header) {
        seek_pos += vbri_get_seek_point(this->vbri_header, start_time, this->stream_length);
        lprintf("time seek: vbri: time=%d, pos=%"PRId64"\n", start_time, seek_pos);
      } else {
        /* cbr */
        seek_pos += ((double)start_time / 1000.0) * ((double)this->br / 8.0);
        lprintf("time seek: cbr: time=%d, pos=%"PRId64"\n", start_time, seek_pos);
      }
    }
    /* assume seeking is always perfect... */
    this->cur_time = start_time;
    this->input->seek (this->input, seek_pos, SEEK_SET);
    
    if (playing) {
      _x_demux_flush_engine(this->stream);
    }
    _x_demux_control_newpts(this->stream,
                            (int64_t)(this->cur_time * 90.0f),
                             (playing) ? BUF_FLAG_SEEK : 0);
  }
  this->status = DEMUX_OK;

  return this->status;
}

static int demux_mpgaudio_get_stream_length (demux_plugin_t *this_gen) {
  demux_mpgaudio_t *this = (demux_mpgaudio_t *) this_gen;

  if (this->stream_length > 0) {
    return this->stream_length;
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
                                    input_plugin_t *input) {

  demux_mpgaudio_t *this;

  lprintf("trying to open %s...\n", input->get_mrl(input));

  switch (stream->content_detection_method) {

  case METHOD_BY_CONTENT: {
    if (!detect_mpgaudio_file(input))
      return NULL;
  }
  break;

  case METHOD_BY_MRL:
  case METHOD_EXPLICIT:
  break;
  
  default:
    return NULL;
  }
  
  this = xine_xmalloc (sizeof (demux_mpgaudio_t));

  this->demux_plugin.send_headers      = demux_mpgaudio_send_headers;
  this->demux_plugin.send_chunk        = demux_mpgaudio_send_chunk;
  this->demux_plugin.seek              = demux_mpgaudio_seek;
  this->demux_plugin.dispose           = default_demux_plugin_dispose;
  this->demux_plugin.get_status        = demux_mpgaudio_get_status;
  this->demux_plugin.get_stream_length = demux_mpgaudio_get_stream_length;
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
void *demux_mpgaudio_init_class (xine_t *xine, void *data) {
  
  demux_mpgaudio_class_t     *this;
  
  this         = xine_xmalloc (sizeof (demux_mpgaudio_class_t));
  this->xine   = xine;

  this->demux_class.open_plugin     = open_plugin;
  this->demux_class.description     = N_("MPEG audio demux plugin");
  this->demux_class.identifier      = "MPEGAUDIO";
  if( _x_decoder_available(this->xine, BUF_AUDIO_MPEG) ) {
    this->demux_class.mimetypes = 
      "audio/mpeg2: mp2: MPEG audio;"
      "audio/x-mpeg2: mp2: MPEG audio;"
      "audio/mpeg3: mp3: MPEG audio;"
      "audio/x-mpeg3: mp3: MPEG audio;"
      "audio/mpeg: mpa,abs,mpega: MPEG audio;"
      "audio/x-mpeg: mpa,abs,mpega: MPEG audio;"
      "audio/x-mpegurl: mp3: MPEG audio;"
      "audio/mpegurl: mp3: MPEG audio;"
      "audio/mp3: mp3: MPEG audio;"
      "audio/x-mp3: mp3: MPEG audio;";
    this->demux_class.extensions    = "mp3 mp2 mpa mpega";
  } else {
    this->demux_class.mimetypes     = NULL;
    this->demux_class.extensions    = NULL;
  }
  this->demux_class.dispose         = default_demux_class_dispose;

  return this;
}
