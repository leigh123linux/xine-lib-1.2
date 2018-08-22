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
 */

/*
 * Real Media File Demuxer by Mike Melanson (melanson@pcisys.net)
 *   improved by James Stembridge (jstembridge@users.sourceforge.net)
 * For more information regarding the Real file format, visit:
 *   http://www.pcisys.net/~melanson/codecs/
 *
 * video packet sub-demuxer ported from mplayer code (www.mplayerhq.hu):
 *   Real parser & demuxer
 *
 *   (C) Alex Beregszaszi <alex@naxine.org>
 *
 *   Based on FFmpeg's libav/rm.c.
 *
 * Reworked by Torsten Jager <t.jager@gmx.de>
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#ifdef HAVE_MALLOC_H
#include <malloc.h>
#endif

#define LOG_MODULE "demux_real"
#define LOG_VERBOSE
/*
#define LOG
*/

#include "group_video.h"

#include <xine/xine_internal.h>
#include <xine/xineutils.h>
#include <xine/compat.h>
#include <xine/demux.h>
#include "bswap.h"

#include "demux_real_common.h"

#define FOURCC_TAG BE_FOURCC
#define PROP_TAG  FOURCC_TAG('P', 'R', 'O', 'P')
#define MDPR_TAG  FOURCC_TAG('M', 'D', 'P', 'R')
#define CONT_TAG  FOURCC_TAG('C', 'O', 'N', 'T')
#define DATA_TAG  FOURCC_TAG('D', 'A', 'T', 'A')
#define RA_TAG    FOURCC_TAG('.', 'r', 'a', 0xfd)
#define VIDO_TAG  FOURCC_TAG('V', 'I', 'D', 'O')

#define PREAMBLE_SIZE 8
#define REAL_SIGNATURE_SIZE 8
#define DATA_CHUNK_HEADER_SIZE 10
#define DATA_PACKET_HEADER_SIZE 12
#define INDEX_CHUNK_HEADER_SIZE 20
#define INDEX_RECORD_SIZE 14

#define PN_KEYFRAME_FLAG 0x0002

#define MAX_VIDEO_STREAMS 10
#define MAX_AUDIO_STREAMS 8

#define FRAGMENT_TAB_SIZE 256

typedef struct {
  uint16_t   object_version;

  uint16_t   stream_number;
  uint32_t   max_bit_rate;
  uint32_t   avg_bit_rate;
  uint32_t   max_packet_size;
  uint32_t   avg_packet_size;
  uint32_t   start_time;
  uint32_t   preroll;
  uint32_t   duration;
  size_t     stream_name_size;
  char      *stream_name;
  size_t     mime_type_size;
  char      *mime_type;
  size_t     type_specific_len;
  uint8_t   *type_specific_data;
} mdpr_t;

typedef struct {
  uint32_t timestamp;
  uint32_t offset;
  uint32_t packetno;
} real_index_entry_t;

typedef struct {
  uint32_t             fourcc;
  uint32_t             buf_type;
  uint32_t             format;
  /* seek index */
  int                  index_entries;
  real_index_entry_t  *index;
  /* stream info */
  mdpr_t              *mdpr;
  /* interleaver type and settings */
  uint32_t             intl;
  int                  sps, cfs, w, h;
  int                  block_align;
  size_t               frame_size;
  uint8_t             *frame_buffer;
  uint32_t             frame_num_bytes;
  uint32_t             sub_packet_cnt;

  uint32_t             audio_time;
} real_stream_t;

typedef struct {
  demux_plugin_t       demux_plugin;
  /* xine engine */
  xine_stream_t       *stream;
  fifo_buffer_t       *video_fifo;
  fifo_buffer_t       *audio_fifo;
  /* an input or 2 ;-) */
  input_plugin_t      *input, *in1, *in2;
  off_t                startpos1;
  off_t                startpos2;
  uint32_t             pos1, endpos1, pos2, endpos2;
  uint32_t             lasttime1, lasttime2;
  /* seek */
  off_t                index_start;
  off_t                data_start;
  off_t                data_size;
  unsigned int         duration;
  /* top level chunk parser */
  unsigned int         current_data_chunk_packet_count;
  unsigned int         next_data_chunk_offset;
  unsigned int         data_chunk_size;
  /* multirate database */
  int                  num_audio_streams;
  int                  num_video_streams;
  real_stream_t        audio_streams[MAX_AUDIO_STREAMS];
  real_stream_t        video_streams[MAX_VIDEO_STREAMS];
  /* and what we use of it */
  real_stream_t       *audio_stream;
  real_stream_t       *video_stream;
  int                  audio_id;
  int                  video_id;
  /* audio deobfuscation */
  int                  audio_need_keyframe;
  /* video deobfuscation */
  uint32_t            *fragment_tab;
  int                  fragment_tab_max;
  int                  fragment_size;
  int                  fragment_count;
  int                  old_seqnum;
  /* mp4 video depacketizer */
  buf_element_t       *vbuf;
  uint32_t             vtime;
  uint32_t             vkeyframe;
  /* timeline */
  off_t                avg_bitrate;
  int64_t              last_pts[2];
  int                  send_newpts;
  /* redirection */
  int                  reference_mode;
  /* our status */
  int                  status;
} demux_real_t ;


static void real_parse_index(demux_real_t *this) {

  off_t   next_index_chunk = this->index_start;
  off_t   original_pos     = this->input->get_current_pos(this->input);
  uint8_t index_chunk_header[INDEX_CHUNK_HEADER_SIZE];
  int     i;

  while(next_index_chunk) {
    uint16_t version;
    uint32_t entries;
    uint16_t stream_num;
    uint8_t *q;
    real_index_entry_t **index = NULL;
    lprintf("reading index chunk at %"PRIX64"\n", next_index_chunk);

    /* Seek to index chunk */
    this->input->seek(this->input, next_index_chunk, SEEK_SET);

    /* Read index chunk header */
    if(this->input->read(this->input, index_chunk_header, INDEX_CHUNK_HEADER_SIZE)
       != INDEX_CHUNK_HEADER_SIZE) {
      lprintf("index chunk header not read\n");
      break;
    }

    /* Check chunk is actually an index chunk */
    if(!_x_is_fourcc(&index_chunk_header[0], "INDX")) {
      lprintf("expected index chunk found chunk type: %.4s\n", &index_chunk_header[0]);
      break;
    }

    /* Check version */
    version = _X_BE_16(&index_chunk_header[8]);
    if(version != 0) {
      xprintf(this->stream->xine, XINE_VERBOSITY_DEBUG,
	      "demux_real: unknown object version in INDX: 0x%04x\n", version);
      break;
    }

    /* Read data from header */
    entries          = _X_BE_32(&index_chunk_header[10]);
    stream_num       = _X_BE_16(&index_chunk_header[14]);
    next_index_chunk = _X_BE_32(&index_chunk_header[16]);

    /* Find which stream this index is for */
    for(i = 0; i < this->num_video_streams; i++) {
      if(stream_num == this->video_streams[i].mdpr->stream_number) {
	index = &this->video_streams[i].index;
	this->video_streams[i].index_entries = entries;
	lprintf("found index chunk for video stream with num %d\n", stream_num);
	break;
      }
    }

    if(!index) {
      for(i = 0; i < this->num_audio_streams; i++) {
	if(stream_num == this->audio_streams[i].mdpr->stream_number) {
	  index = &this->audio_streams[i].index;
	  this->audio_streams[i].index_entries = entries;
	  lprintf("found index chunk for audio stream with num %d\n", stream_num);
	  break;
	}
      }
    }

    q = NULL;
    if (index && entries && (entries < (1 << 18))) {
      free (*index);
      *index = NULL;
      /* Allocate memory for index */
      if (sizeof (real_index_entry_t) > INDEX_RECORD_SIZE)
        q = malloc (entries * sizeof (real_index_entry_t));
      else
        q = malloc (entries * INDEX_RECORD_SIZE);
    }
    if (index && entries && q) {
      real_index_entry_t *e = (real_index_entry_t *)q;
      if (sizeof (real_index_entry_t) > INDEX_RECORD_SIZE)
        q += entries * (sizeof (real_index_entry_t) - INDEX_RECORD_SIZE);
      /* Read index */
      if (this->input->read (this->input, q, entries * INDEX_RECORD_SIZE) != entries * INDEX_RECORD_SIZE) {
        lprintf ("index record not read\n");
        free (e);
        *index = NULL;
      } else {
        *index = e;
        while (entries--) {
          e->timestamp = _X_BE_32 (q + 2);
          e->offset    = _X_BE_32 (q + 6);
          e->packetno  = _X_BE_32 (q + 10);
          e++;
          q += INDEX_RECORD_SIZE;
        }
      }
    } else {
      lprintf("unused index chunk with %d entries for stream num %d\n",
	      entries, stream_num);
    }
  }

  /* Seek back to position before index reading */
  this->input->seek(this->input, original_pos, SEEK_SET);
}

static mdpr_t *real_parse_mdpr (const char *data, uint32_t size) {
  const uint8_t *p, *p1, *p2, *p3;
  uint8_t *q;
  mdpr_t *mdpr;
  uint32_t sall, s1, s2, s3;
  /* get size needed */
  if (size < 38)
    return NULL;
  p = (const uint8_t *)data + 32;
  sall = size - 38;
  s1 = *p++;
  if (sall < s1)
    return NULL;
  p1 = p;
  p += s1;
  sall -= s1;
  s2 = *p++;
  if (sall < s2)
    return NULL;
  p2 = p;
  p += s2;
  sall -= s2;
  s3 = _X_BE_32 (p); p += 4;
  if (sall < s3)
    return NULL;
  p3 = p;
  /* now build */
  q = malloc (sizeof (mdpr_t) + s1 + s2 + s3 + 8);
  if (!q)
    return NULL;
  mdpr = (mdpr_t *)q;
  p = (const uint8_t *)data;
  mdpr->object_version  = _X_BE_16 (p);
  mdpr->stream_number   = _X_BE_16 (p +  2);
  mdpr->max_bit_rate    = _X_BE_32 (p +  4);
  mdpr->avg_bit_rate    = _X_BE_32 (p +  8);
  mdpr->max_packet_size = _X_BE_32 (p + 12);
  mdpr->avg_packet_size = _X_BE_32 (p + 16);
  mdpr->start_time      = _X_BE_32 (p + 20);
  mdpr->preroll         = _X_BE_32 (p + 24);
  mdpr->duration        = _X_BE_32 (p + 28);
  q += sizeof (mdpr_t);
#define ALIGN4(q) q = (uint8_t *)(((uintptr_t)q + 3) & ~(uintptr_t)3)
  memcpy (q, p1, s1);
  mdpr->stream_name = (char *)q;
  mdpr->stream_name_size = s1;
  q += s1;
  *q++ = 0;
  ALIGN4 (q);
  memcpy (q, p2, s2);
  mdpr->mime_type = (char *)q;
  mdpr->mime_type_size = s2;
  q += s2;
  *q++ = 0;
  ALIGN4 (q);
  memcpy (q, p3, s3);
  mdpr->type_specific_data = q;
  mdpr->type_specific_len  = s3;
#undef ALIGN4

  lprintf("MDPR: stream number: %i\n", mdpr->stream_number);
  lprintf("MDPR: maximal bit rate: %i\n", mdpr->max_bit_rate);
  lprintf("MDPR: average bit rate: %i\n", mdpr->avg_bit_rate);
  lprintf("MDPR: largest packet size: %i bytes\n", mdpr->max_packet_size);
  lprintf("MDPR: average packet size: %i bytes\n", mdpr->avg_packet_size);
  lprintf("MDPR: start time: %i\n", mdpr->start_time);
  lprintf("MDPR: pre-buffer: %i ms\n", mdpr->preroll);
  lprintf("MDPR: duration of stream: %i ms\n", mdpr->duration);
  lprintf("MDPR: stream name: %s\n", mdpr->stream_name);
  lprintf("MDPR: mime type: %s\n", mdpr->mime_type);
  lprintf("MDPR: type specific data:\n");
#ifdef LOG
  xine_hexdump(mdpr->type_specific_data, mdpr->type_specific_len);
#endif
  return mdpr;
}

static void real_free_mdpr (mdpr_t *mdpr) {
  free (mdpr);
}

static int real_parse_audio_specific_data (demux_real_t *this, real_stream_t *stream) {
  uint8_t *p, *e;
  uint32_t version;
  uint32_t head_size, flavor, coded_frame_size, h, frame_size, subpacket_size;
  uint32_t samplerate, samplesize, channels, intl, fourcc;
  uint32_t codecdata_length;

  p = stream->mdpr->type_specific_data;
  e = p + stream->mdpr->type_specific_len;
  if (p + 6 > e) goto truncated;
  version = p[5];

  if (version == 3) {
    frame_size       = 240;
    samplerate       = 8000;
    samplesize       = 16;
    channels         = 1;
    intl             = 0;
    coded_frame_size = 0;
    h                = 0;
    subpacket_size   = 0;
    if (p + 11 > e) goto truncated;
    p = e - 5;
    fourcc = _X_ME_32 (p);
  } else {
    if (p + 48 > e) goto truncated;
    p += 20;
    head_size        = _X_BE_16 (p); p += 2;
    flavor           = _X_BE_16 (p); p += 2;
    coded_frame_size = _X_BE_32 (p); p += 4;
    p += 12;
    h                = _X_BE_16 (p); p += 2;
    frame_size       = _X_BE_16 (p); p += 2;
    subpacket_size   = _X_BE_16 (p); p += 2;
    p += 2;
    if (version == 5) p += 6;
    if (p + 8 > e) goto truncated;
    samplerate       = _X_BE_16 (p); p += 2;
    p += 2;
    samplesize       = _X_BE_16 (p); p += 2;
    channels         = _X_BE_16 (p); p += 2;
    if (version == 5) {
      if (p + 8 > e) goto truncated;
      intl   = _X_ME_32 (p); p += 4; /* integer ID's */
      fourcc = _X_ME_32 (p); p += 4;
    } else {
      if (p + 10 > e) goto truncated;
      intl   = _X_ME_32 (p + 1); p += p[0] + 1; /* string ID's */
      fourcc = _X_ME_32 (p + 1); p += p[0] + 1;
    }
    stream->block_align = frame_size;
    switch (fourcc) {
      case ME_FOURCC('d','n','e','t'):
        /* AC3 with swapped byte order */
      break;
      case ME_FOURCC('1','4','_','4'):
        stream->block_align = 20;
      break;
      case ME_FOURCC('2','8','_','8'):
        stream->block_align = coded_frame_size;
      break;
      case ME_FOURCC('s','i','p','r'):
      case ME_FOURCC('a','t','r','c'):
      case ME_FOURCC('c','o','o','k'):
        p += 3;
        if (version == 5) p += 1;
        if (p + 4 > e) goto truncated;
        codecdata_length = _X_BE_32 (p); p += 4;
        stream->block_align = intl == ME_FOURCC('g','e','n','r') ? subpacket_size : coded_frame_size;
      break;
      case ME_FOURCC('r','a','a','c'):
      case ME_FOURCC('r','a','c','p'):
        /* AAC */
        p += 3;
        if (version == 5) p += 1;
        if (p + 4 > e) goto truncated;
        codecdata_length = _X_BE_32 (p); p += 4;
        fourcc = ME_FOURCC('m','p','4','a');
      break;
    }
  }
  if (0) {
    truncated:
    xprintf (this->stream->xine, XINE_VERBOSITY_LOG, "demux_real: truncated MDPR chunk\n");
    return 0;
  }
  /* return 4CC */
  stream->fourcc = fourcc;
  /* return interleaver type and settings */
  if (!((intl == ME_FOURCC('g','e','n','r')) || (intl == ME_FOURCC('I','n','t','4'))
    || (intl == ME_FOURCC('s','i','p','r')))) intl = 0;
  stream->intl = intl;

  stream->sps  = subpacket_size;
  stream->w    = frame_size;
  stream->h    = h;
  stream->cfs  = coded_frame_size;

  {
    char b[20];
    _x_tag32_me2str (b, fourcc);
    xprintf (this->stream->xine, XINE_VERBOSITY_DEBUG,
      "demux_real: stream #%d: audio v%d, [%s] %ubps, %dch, %dHz, %dbit, start %d\n",
      (int)stream->mdpr->stream_number, version, b, (unsigned int)stream->mdpr->avg_bit_rate,
      channels, samplerate, samplesize, (int)stream->mdpr->start_time);
    if (intl) {
      _x_tag32_me2str (b, intl);
      xprintf (this->stream->xine, XINE_VERBOSITY_DEBUG,
        "            interleaved [%s] %dx%d, align %d, sps %d, cfs %d\n",
        b, stream->w, stream->h, stream->block_align, stream->sps, stream->cfs);
    }
  }
  /*
   * when stream->sps is set it used to do this:
   *   stream->frame_size      = stream->w / stream->sps * stream->h * stream->sps;
   * but it looks pointless? the compiler will probably optimise it away, I suppose?
   */
  free (stream->frame_buffer);
  if (stream->w < 32768 && stream->h < 32768) {
    stream->frame_size = stream->w * stream->h;
    stream->frame_buffer = calloc (stream->frame_size, 1);
  } else {
    stream->frame_size = 0;
    stream->frame_buffer = NULL;
  }

  stream->frame_num_bytes = 0;
  stream->sub_packet_cnt = 0;

  /* XXX: decoder parses these again - skip "unused" warning */
  (void)head_size;
  (void)flavor;
  (void)codecdata_length;

  if (!stream->frame_buffer) xprintf (this->stream->xine, XINE_VERBOSITY_LOG,
    "demux_real: failed to allocate the audio frame buffer!\n");
  return 1;
}

static void real_parse_headers (demux_real_t *this) {

  char           preamble[PREAMBLE_SIZE];
  unsigned int   chunk_type = 0;
  unsigned int   chunk_size;
  unsigned int   audio_bitrate = 0;
  unsigned int   video_bitrate = 0;
  real_stream_t  *stream;

  if (INPUT_IS_SEEKABLE(this->input))
    this->input->seek (this->input, 0, SEEK_SET);

  {
    uint8_t signature[REAL_SIGNATURE_SIZE];
    if (this->input->read(this->input, signature, REAL_SIGNATURE_SIZE) !=
	REAL_SIGNATURE_SIZE) {

      lprintf ("signature not read\n");
      this->status = DEMUX_FINISHED;
      return;
    }

    if ( !_x_is_fourcc(signature, ".RMF") ) {
      this->status = DEMUX_FINISHED;
      lprintf ("signature not found '%.4s'\n", signature);
      return;
    }

    /* skip to the start of the first chunk and start traversing */
    chunk_size = _X_BE_32(&signature[4]);
  }

  this->data_start = 0;
  this->data_size = 0;
  this->num_video_streams = 0;
  this->num_audio_streams = 0;

  this->input->seek(this->input, chunk_size-8, SEEK_CUR);

  /* iterate through chunks and gather information until the first DATA
   * chunk is located */
  while (chunk_type != DATA_TAG) {

    if (this->input->read(this->input, preamble, PREAMBLE_SIZE) !=
	PREAMBLE_SIZE) {
      this->status = DEMUX_FINISHED;
      return;
    }
    chunk_type = _X_BE_32(&preamble[0]);
    chunk_size = _X_BE_32(&preamble[4]);

    lprintf ("chunktype %.4s len %d\n", (char *) &chunk_type, chunk_size);
    switch (chunk_type) {

    case PROP_TAG:
    case MDPR_TAG:
    case CONT_TAG:
      {
	if (chunk_size < PREAMBLE_SIZE+1) {
	  this->status = DEMUX_FINISHED;
	  return;
	}
	chunk_size -= PREAMBLE_SIZE;
	uint8_t *const chunk_buffer = malloc(chunk_size);
	if (! chunk_buffer ||
	    this->input->read(this->input, chunk_buffer, chunk_size) !=
	    chunk_size) {
	  free (chunk_buffer);
	  this->status = DEMUX_FINISHED;
	  return;
	}

	uint16_t version = _X_BE_16(&chunk_buffer[0]);

	if (chunk_type == PROP_TAG) {

	  if(version != 0) {
	    xprintf(this->stream->xine, XINE_VERBOSITY_DEBUG,
		    "demuxe_real: unknown object version in PROP: 0x%04x\n", version);
	    free(chunk_buffer);
	    this->status = DEMUX_FINISHED;
	    return;
	  }

	  this->duration      = _X_BE_32(&chunk_buffer[22]);
	  this->index_start   = _X_BE_32(&chunk_buffer[30]);
	  this->data_start    = _X_BE_32(&chunk_buffer[34]);
	  this->avg_bitrate   = _X_BE_32(&chunk_buffer[6]);

	  lprintf("PROP: duration: %d ms\n", this->duration);
	  lprintf("PROP: index start: %"PRIX64"\n", this->index_start);
	  lprintf("PROP: data start: %"PRIX64"\n", this->data_start);
	  lprintf("PROP: average bit rate: %"PRId64"\n", this->avg_bitrate);

	  if (this->avg_bitrate<1)
	    this->avg_bitrate = 1;

	  _x_stream_info_set(this->stream, XINE_STREAM_INFO_BITRATE,
                             this->avg_bitrate);

	} else if (chunk_type == MDPR_TAG) {

          mdpr_t *mdpr;
          uint32_t fourcc = 0, isvideo = 0;

          if (version != 0) {
            xprintf (this->stream->xine, XINE_VERBOSITY_DEBUG,
              "demux_real: unknown object version in MDPR: 0x%04x\n", version);
            free (chunk_buffer);
	    continue;
	  }
          mdpr = real_parse_mdpr ((char *)chunk_buffer, chunk_size);
          if (!mdpr) {
            xprintf (this->stream->xine, XINE_VERBOSITY_DEBUG,
              "demux_real: invalid MDPR\n");
            free (chunk_buffer);
            continue;
          }

	  lprintf ("parsing type specific data...\n");
	  /* identify by mime type */
          if (!strcmp (mdpr->mime_type, "audio/X-MP3-draft-00")) {
            fourcc = ME_FOURCC ('a','d','u',0x55);
          } else if (!strcmp (mdpr->mime_type, "audio/mpeg4-generic")) {
            fourcc = ME_FOURCC ('m','p','4','a');
          } else if (!strcmp (mdpr->mime_type, "video/MP4V-ES")) {
            fourcc = ME_FOURCC ('m','p','4','v');
            isvideo = 1;
          }
          if (fourcc) {
            if (isvideo) {
              if (this->num_video_streams == MAX_VIDEO_STREAMS) {
                xprintf (this->stream->xine, XINE_VERBOSITY_DEBUG,
                  "demux_real: maximum number of video stream exceeded\n");
                goto unknown;
              }
              stream = &this->video_streams[this->num_video_streams];
              /* stream->format = _X_BE_32(mdpr->type_specific_data + 30); */
            } else {
              char b[20];
              if (this->num_audio_streams == MAX_AUDIO_STREAMS) {
                xprintf (this->stream->xine, XINE_VERBOSITY_DEBUG,
                  "demux_real: maximum number of audio stream exceeded\n");
                goto unknown;
              }
              stream = &this->audio_streams[this->num_audio_streams];
              stream->mdpr = mdpr;
              _x_tag32_me2str (b, fourcc);
              xprintf (this->stream->xine, XINE_VERBOSITY_DEBUG,
                "demux_real: stream #%d: audio [%s] %ubps, start %d\n",
                (int)mdpr->stream_number, b, (unsigned int)mdpr->avg_bit_rate, (int)mdpr->start_time);
            }
          } else if ((mdpr->type_specific_len >= 4) && (_X_BE_32 (mdpr->type_specific_data) == RA_TAG)) {
	    /* identify by type specific data */
	    if(this->num_audio_streams == MAX_AUDIO_STREAMS) {
	      xprintf(this->stream->xine, XINE_VERBOSITY_DEBUG,
		      "demux_real: maximum number of audio stream exceeded\n");
	      goto unknown;
	    }
            stream = &this->audio_streams[this->num_audio_streams];
            stream->mdpr = mdpr;
            if (!real_parse_audio_specific_data (this, stream))
              goto unknown;
            fourcc = stream->fourcc;

          } else if ((mdpr->type_specific_len >= 34) && (_X_BE_32 (mdpr->type_specific_data + 4) == VIDO_TAG)) {

	    if(this->num_video_streams == MAX_VIDEO_STREAMS) {
	      xprintf(this->stream->xine, XINE_VERBOSITY_DEBUG,
		      "demux_real: maximum number of video stream exceeded\n");
	      goto unknown;
	    }

            isvideo = 1;
	    lprintf ("video detected\n");
            fourcc = _X_ME_32 (mdpr->type_specific_data + 8);

            stream = &this->video_streams[this->num_video_streams];
            stream->format = _X_BE_32 (mdpr->type_specific_data + 30);
          }

          if (fourcc) {
            if (isvideo) {
              char b[20];
              _x_tag32_me2str (b, fourcc);
              xprintf (this->stream->xine, XINE_VERBOSITY_DEBUG,
                "demux_real: stream #%d: video [%s] %ubps, start %d\n",
                (int)mdpr->stream_number, b, (unsigned int)mdpr->avg_bit_rate, (int)mdpr->start_time);
              stream->index    = NULL;
              stream->mdpr     = mdpr;
              stream->fourcc   = fourcc;
              stream->buf_type = _x_fourcc_to_buf_video (fourcc);
              if (!stream->buf_type)
                _x_report_video_fourcc (this->stream->xine, LOG_MODULE, fourcc);
              if (mdpr->avg_bit_rate > video_bitrate) { /* select best video */
                video_bitrate = mdpr->avg_bit_rate;
                this->video_stream = stream;
              }
              this->num_video_streams++;
            } else {
              stream->index    = NULL;
              stream->fourcc   = fourcc;
              stream->buf_type = _x_formattag_to_buf_audio (fourcc);
              if (mdpr->avg_bit_rate > audio_bitrate) { /* select best audio */
                audio_bitrate = mdpr->avg_bit_rate;
                this->audio_stream = stream;
              }
              this->num_audio_streams++;
            }
          } else {
            lprintf ("unrecognised mdpr or type specific data\n");
            unknown:
            real_free_mdpr (mdpr);
          }

	} else if (chunk_type == CONT_TAG) {

	  if(version != 0) {
	    xprintf(this->stream->xine, XINE_VERBOSITY_DEBUG,
		    "demux_real: unknown object version in CONT: 0x%04x\n", version);
	    free(chunk_buffer);
	    continue;
	  }

	  int stream_ptr = 2;
#define SET_METADATA_STRING(type) \
  do { \
    const uint16_t field_size = _X_BE_16(&chunk_buffer[stream_ptr]); \
    stream_ptr += 2; \
    _x_meta_info_n_set(this->stream, type, (char *)&chunk_buffer[stream_ptr], field_size); \
    stream_ptr += field_size; \
  } while(0)

	  /* load the title string */
	  SET_METADATA_STRING(XINE_META_INFO_TITLE);
	
	  /* load the author string */
	  SET_METADATA_STRING(XINE_META_INFO_ARTIST);

	  /* load the copyright string as the year */
	  SET_METADATA_STRING(XINE_META_INFO_YEAR);

	  /* load the comment string */
	  SET_METADATA_STRING(XINE_META_INFO_COMMENT);
	}

	free(chunk_buffer);
      }
      break;

    case DATA_TAG: {
      uint8_t data_chunk_header[DATA_CHUNK_HEADER_SIZE];

      if (this->input->read(this->input, data_chunk_header,
                            DATA_CHUNK_HEADER_SIZE) != DATA_CHUNK_HEADER_SIZE) {
        this->status = DEMUX_FINISHED;
        return ;
      }

      /* check version */
      const uint16_t version = _X_BE_16(&data_chunk_header[0]);
      if(version != 0) {
          xprintf(this->stream->xine, XINE_VERBOSITY_DEBUG,
                  "demux_real: unknown object version in DATA: 0x%04x\n", version);
          this->status = DEMUX_FINISHED;
          return;
      }

      this->current_data_chunk_packet_count = _X_BE_32(&data_chunk_header[2]);
      this->next_data_chunk_offset = _X_BE_32(&data_chunk_header[6]);
      this->data_chunk_size = chunk_size;
    }
    break;

    default:
      /* this should not occur, but in case it does, skip the chunk */
      lprintf("skipping a chunk!\n");
      this->input->seek(this->input, chunk_size - PREAMBLE_SIZE, SEEK_CUR);
      break;

    }
  }

  /* Read index tables */
  if(INPUT_IS_SEEKABLE(this->input))
    real_parse_index(this);

  /* Simple stream selection case - 0/1 audio/video streams */
  if (!this->video_stream)
    this->video_stream = (this->num_video_streams == 1) ? &this->video_streams[0] : NULL;
  if (!this->audio_stream)
    this->audio_stream = (this->num_audio_streams == 1) ? &this->audio_streams[0] : NULL;

  /* Last resort in the case of multiple audio/video streams:
   * select the first streams found in the file */
  if ((!this->video_stream && (this->num_video_streams > 1))
    || (!this->audio_stream && (this->num_audio_streams > 1))) {
    int   len, offset;
    off_t original_pos = 0;
    char  search_buffer[MAX_PREVIEW_SIZE];

    /* Get data to search through for stream chunks */
    if(INPUT_IS_SEEKABLE(this->input)) {
      original_pos = this->input->get_current_pos(this->input);

      if((len = this->input->read(this->input, search_buffer, MAX_PREVIEW_SIZE)) <= 0) {
        xprintf(this->stream->xine, XINE_VERBOSITY_DEBUG,
                "demux_real: failed to read header\n");
        this->status = DEMUX_FINISHED;
        return;
      }

      offset = 0;
    } else if((this->input->get_capabilities(this->input) & INPUT_CAP_PREVIEW) != 0) {
      if((len = this->input->get_optional_data(this->input, search_buffer, INPUT_OPTIONAL_DATA_PREVIEW)) <= 0) {
        xprintf(this->stream->xine, XINE_VERBOSITY_DEBUG,
                "demux_real: failed to read header\n");
        this->status = DEMUX_FINISHED;
        return;
      }

      /* Preview data starts at the beginning of the file */
      offset = this->data_start + 18;
    } else {
      xprintf(this->stream->xine, XINE_VERBOSITY_DEBUG,
              "demux_real: unable to search for correct stream\n");
      this->status = DEMUX_FINISHED;
      return;
    }

    while((offset < len) &&
          ((!this->video_stream && (this->num_video_streams > 0)) ||
           (!this->audio_stream && (this->num_audio_streams > 0)))) {
      int      i;

      /* Check for end of the data chunk */
      if (_x_is_fourcc(&search_buffer[offset], "INDX") || _x_is_fourcc(&search_buffer[offset], "DATA"))
	break;

      const int stream = _X_BE_16(&search_buffer[offset + 4]);

      for(i = 0; !this->video_stream && (i < this->num_video_streams); i++) {
        if(stream == this->video_streams[i].mdpr->stream_number) {
          this->video_stream = &this->video_streams[i];
          lprintf("selecting video stream: %d\n", stream);
        }
      }

      for(i = 0; !this->audio_stream && (i < this->num_audio_streams); i++) {
        if(stream == this->audio_streams[i].mdpr->stream_number) {
          this->audio_stream = &this->audio_streams[i];
          lprintf("selecting audio stream: %d\n", stream);
        }
      }

      offset += _X_BE_16(&search_buffer[offset + 2]);
    }

    if(INPUT_IS_SEEKABLE(this->input))
      this->input->seek(this->input, original_pos, SEEK_SET);
  }

  /* Let the user know if we haven't managed to detect what streams to play */
  if((!this->video_stream && this->num_video_streams) ||
     (!this->audio_stream && this->num_audio_streams)) {
    xprintf(this->stream->xine, XINE_VERBOSITY_LOG,
            "demux_real: unable to determine which audio/video streams to play\n");
    this->status = DEMUX_FINISHED;
    return;
  }

  this->audio_id  = this->audio_stream ? this->audio_stream->mdpr->stream_number : -1;
  this->video_id  = this->video_stream ? this->video_stream->mdpr->stream_number : -1;
  this->pos1      = 18;
  this->endpos1   = chunk_size;
  this->pos2      = 18;
  this->endpos2   = chunk_size;
  this->startpos1 = this->input->get_current_pos (this->input) - 18;

  /* noninterleaved file (1 DATA chunk per stream) ? */
  if (INPUT_IS_SEEKABLE (this->input)) {
    uint32_t need = (this->audio_stream ? 1 : 0) | (this->video_stream ? 2 : 0);
    off_t here = this->startpos1;
    off_t last_pos = here + 18;
    this->input->seek (this->input, here, SEEK_SET);
    while (need) {
      uint8_t b[20];
      uint32_t size;
      if (this->input->read (this->input, b, 18) != 18)
        break;
      size = _X_BE_32 (b + 4);
      if (_X_ME_32 (b) == ME_FOURCC('D','A','T','A')) {
        int n = 32;
        while (--n) {
          int stream, psize;
          if (this->input->read (this->input, b, 12) != 12)
            break;
          psize = _X_BE_16 (b + 2);
          if (_X_BE_16 (b) < 2) {
            stream = _X_BE_16 (b + 4);
            if ((need & 1) && (stream == this->audio_id)) {
              this->startpos1 = here;
              this->endpos1 = size;
              need &= ~1;
              if (!need)
                break;
            } else if ((need & 2) && (stream == this->video_id)) {
              this->startpos2 = here;
              this->endpos2 = size;
              need &= ~2;
              if (!need)
                break;
            }
          }
          this->input->seek (this->input, psize - 12, SEEK_CUR);
        }
      }
      here += size;
      this->input->seek (this->input, here, SEEK_SET);
    }
    if (this->startpos1)
      xprintf (this->stream->xine, XINE_VERBOSITY_DEBUG,
        "demux_real: audio stream #%d found in DATA chunk @%"PRId64".\n",
        this->audio_id, (int64_t)this->startpos1);
    if (this->startpos2)
      xprintf (this->stream->xine, XINE_VERBOSITY_DEBUG,
        "demux_real: video stream #%d found in DATA chunk @%"PRId64".\n",
        this->video_id, (int64_t)this->startpos2);
    if (this->startpos1) {
      this->input->seek (this->input, this->startpos1 + 18, SEEK_SET);
      if (this->startpos2) {
        if (this->startpos1 != this->startpos2) {
          /* Noninterleaved file. Try cloning input. */
          if (this->input->get_capabilities (this->input) & INPUT_CAP_CLONE) {
            input_plugin_t *in2 = NULL;
            if (this->input->get_optional_data (this->input, &in2, INPUT_OPTIONAL_DATA_CLONE) == INPUT_OPTIONAL_SUCCESS)
              this->in2 = in2;
          }
          if (this->in2) {
            this->in2->seek (this->in2, this->startpos2 + 18, SEEK_SET);
            this->lasttime2 = 0;
          }
        }
      }
    } else {
      if (this->startpos2) {
        this->input->seek (this->input, this->startpos2 + 18, SEEK_SET);
        this->startpos1 = this->startpos2;
        this->endpos1 = this->endpos2;
      } else {
        /* should not happen */
        this->input->seek (this->input, last_pos, SEEK_SET);
      }
    }
  }

  /* should not happen */
  if (!this->endpos1)
    this->endpos1 = 1;
  if (!this->endpos2)
    this->endpos2 = 1;

  /* Send headers and set meta info */
  if (this->video_stream) {
    _x_stream_info_set(this->stream, XINE_STREAM_INFO_HAS_VIDEO, 1);
    _x_stream_info_set(this->stream, XINE_STREAM_INFO_VIDEO_FOURCC,
                         this->video_stream->fourcc);
    _x_stream_info_set(this->stream, XINE_STREAM_INFO_VIDEO_BITRATE,
                         this->video_stream->mdpr->avg_bit_rate);
  } else _x_stream_info_set(this->stream, XINE_STREAM_INFO_HAS_VIDEO, 0);

  if (this->audio_stream) {
    _x_stream_info_set(this->stream, XINE_STREAM_INFO_HAS_AUDIO, 1);
    _x_stream_info_set(this->stream, XINE_STREAM_INFO_AUDIO_FOURCC,
                         this->audio_stream->fourcc);
    _x_stream_info_set(this->stream, XINE_STREAM_INFO_AUDIO_BITRATE,
                         this->audio_stream->mdpr->avg_bit_rate);
  } else _x_stream_info_set(this->stream, XINE_STREAM_INFO_HAS_AUDIO, 0);

  /* send start buffers */
  _x_demux_control_start(this->stream);

  if(this->video_stream) {
    static const uint8_t h[] = {
      0x00,0x00,0x01,0xb0,0xf3,0x00,0x00,0x01,
      0xb5,0x0e,0xe0,0x40,0xc0,0xcf,0x00,0x00,
      0x01,0x00,0x00,0x00,0x01,0x20,0x00,0x84,
      0x40,0x07,0xa8,0x50,0x20,0xf0,0xa2,0x1f
    };
    const uint8_t *info = this->video_stream->mdpr->type_specific_data;
    uint32_t ilen = this->video_stream->mdpr->type_specific_len;
    buf_element_t *buf;
    /* Check for recognised codec*/
    if(!this->video_stream->buf_type)
      this->video_stream->buf_type = BUF_VIDEO_UNKNOWN;
    /* Send header */
    buf = this->video_fifo->buffer_pool_alloc (this->video_fifo);
    buf->content = buf->mem;
    if (!ilen && (this->video_stream->fourcc == ME_FOURCC ('m','p','4','v'))) {
      xine_bmiheader *bih;
      xprintf (this->stream->xine, XINE_VERBOSITY_LOG,
        "demux_real: missing mp4v config, trying generic 320x240...\n");
      buf->decoder_flags = BUF_FLAG_HEADER | BUF_FLAG_STDHEADER | BUF_FLAG_FRAME_END;
      bih = (xine_bmiheader *)buf->content;
      memset (bih, 0, sizeof (xine_bmiheader));
      bih->biSize   = sizeof (xine_bmiheader);
      bih->biWidth  = 320;
      bih->biHeight = 240;
      buf->size     = sizeof (xine_bmiheader);
      buf->type     = this->video_stream->buf_type;
      this->video_fifo->put (this->video_fifo, buf);
      buf = this->video_fifo->buffer_pool_alloc (this->video_fifo);
      info = h;
      ilen = sizeof (h);
    }
    if (ilen > (uint32_t)buf->max_size)
      ilen = buf->max_size;
    if (ilen)
      memcpy (buf->content, info, ilen);
    buf->size = ilen;
    buf->decoder_flags = BUF_FLAG_HEADER | BUF_FLAG_FRAME_END;
    buf->extra_info->input_normpos = 0;
    buf->extra_info->input_time    = 0;
    buf->type = this->video_stream->buf_type;
    this->video_fifo->put (this->video_fifo, buf);

    /* Allocate fragment offset table */
    this->fragment_tab = calloc(FRAGMENT_TAB_SIZE, sizeof(uint32_t));
    this->fragment_tab_max = FRAGMENT_TAB_SIZE;
  }

  if(this->audio_stream) {
    /* Check for recognised codec */
    if(!this->audio_stream->buf_type)
      this->audio_stream->buf_type = BUF_AUDIO_UNKNOWN;

    /* Send headers */
    if(this->audio_fifo) {
      mdpr_t        *const mdpr = this->audio_stream->mdpr;

      buf_element_t *buf = this->audio_fifo->buffer_pool_alloc (this->audio_fifo);

      buf->type           = this->audio_stream->buf_type;
      buf->decoder_flags  = BUF_FLAG_HEADER | BUF_FLAG_FRAME_END;

      if (!mdpr->type_specific_len) {
	/* we dont have anything to tell, maybe it still works */
        buf->decoder_flags |= BUF_FLAG_STDHEADER;
	buf->size = 0;
	buf->decoder_info[0] = 0;
	buf->decoder_info[1] = 0;
	buf->decoder_info[2] = 0;
	buf->decoder_info[3] = 0;
      } else if (buf->type == BUF_AUDIO_AAC) {
      /* For AAC we send two header buffers, the first is a standard audio
       * header giving bits per sample, sample rate and number of channels.
       * The second is the codec initialisation data found at the end of
       * the type specific data for the audio stream */
        if (mdpr->type_specific_len >= 62) {
          uint16_t version = _X_BE_16 (mdpr->type_specific_data + 4);
          if (version != 5) {
            xprintf (this->stream->xine, XINE_VERBOSITY_DEBUG,
              "demux_real: unsupported audio header version for AAC: %d\n", version);
            buf->free_buffer (buf);
            goto unsupported;
          }
          buf->decoder_info[1] = _X_BE_16 (mdpr->type_specific_data + 54);
          buf->decoder_info[2] = _X_BE_16 (mdpr->type_specific_data + 58);
          buf->decoder_info[3] = _X_BE_16 (mdpr->type_specific_data + 60);
          buf->decoder_flags  |= BUF_FLAG_STDHEADER;
          buf->content         = NULL;
          buf->size            = 0;

          if (mdpr->type_specific_len >= 80) {
            uint32_t s;
            this->audio_fifo->put (this->audio_fifo, buf);
            buf = this->audio_fifo->buffer_pool_alloc (this->audio_fifo);
            s = _X_BE_32 (mdpr->type_specific_data + 74);
            if (s >= 1)
              s -= 1;
            if (s > mdpr->type_specific_len - 79)
              s = mdpr->type_specific_len - 79;
            buf->type                = this->audio_stream->buf_type;
            buf->decoder_flags       = BUF_FLAG_HEADER|BUF_FLAG_FRAME_END|BUF_FLAG_SPECIAL;
            buf->decoder_info[1]     = BUF_SPECIAL_DECODER_CONFIG;
            buf->decoder_info[2]     = s;
            buf->decoder_info_ptr[2] = buf->content;
            buf->size                = 0;
            memcpy (buf->content, mdpr->type_specific_data + 79, s);
          }
        }
      } else {
        memcpy (buf->content, mdpr->type_specific_data, mdpr->type_specific_len);
        buf->size = mdpr->type_specific_len;
      }

      this->audio_fifo->put (this->audio_fifo, buf);
    }

unsupported: ;
  }
}


/* very naive approach for parsing ram files. it will extract known
 * mrls directly so it should work for simple smil files too.
 * no attempt is made to support smil features:
 * http://service.real.com/help/library/guides/production/htmfiles/smil.htm
 */
static int demux_real_parse_references( demux_real_t *this) {

  char           *buf = NULL;
  int             buf_size = 0;
  int             buf_used = 0;
  int             len, i, j;
  int             alternative = 0;
  int             comment = 0;


  lprintf("parsing references\n");

  /* read file to memory.
   * warning: dumb code, but hopefuly ok since reference file is small */
  do {
    buf_size += 1024;
    buf = realloc(buf, buf_size+1);

    len = this->input->read(this->input, &buf[buf_used], buf_size-buf_used);

    if( len > 0 )
      buf_used += len;

    /* 50k of reference file? no way. something must be wrong */
    if( buf_used > 50*1024 )
      break;
  } while( len > 0 );

  if(buf_used)
    buf[buf_used] = '\0';

  lprintf("received %d bytes [%s]\n", buf_used, buf);

  if (!strncmp(buf,"http://",7))
  {
    i = 0;
    while (buf[i])
    {
      j = i;
      while (buf[i] && !isspace(buf[i]))
	++i; /* skip non-space */
      len = buf[i];
      buf[i] = 0;
      if (strncmp (buf + j, "http://", 7) || (i - j) < 8)
        break; /* stop at the first non-http reference */
      lprintf("reference [%s] found\n", buf + j);
      _x_demux_send_mrl_reference (this->stream, 0, buf + j, NULL, 0, 0);
      buf[i] = (char) len;
      while (buf[i] && isspace(buf[i]))
	++i; /* skip spaces */
    }
  }
  else for (i = 0; i < buf_used; ++i)
  {
    /* "--stop--" is used to have pnm alternative for old real clients
     * new real clients will stop processing the file and thus use
     * rtsp protocol.
     */
    if( !strncmp(&buf[i],"--stop--",8) )
      alternative++;

    /* rpm files can contain comments which should be skipped */
    if( !strncmp(&buf[i],"<!--",4) )
      comment = 1;

    if( !strncmp(&buf[i],"-->",3) )
      comment = 0;

    if( (!strncmp(&buf[i],"pnm://",6) || !strncmp(&buf[i],"rtsp://",7)) &&
        !comment ) {
      for(j=i; buf[j] && buf[j] != '"' && !isspace(buf[j]); j++ )
        ;
      buf[j]='\0';
      lprintf("reference [%s] found\n", &buf[i]);

      _x_demux_send_mrl_reference (this->stream, alternative,
                                   &buf[i], NULL, 0, 0);

      i = j;
    }
  }

  free(buf);

  this->status = DEMUX_FINISHED;
  return this->status;
}

/* redefine abs as macro to handle 64-bit diffs.
   i guess llabs may not be available everywhere */
#define abs(x) ( ((x)<0) ? -(x) : (x) )

#define WRAP_THRESHOLD           220000
#define PTS_AUDIO                0
#define PTS_VIDEO                1
#define PTS_BOTH                 2

static void check_newpts (demux_real_t *this, int64_t pts, int video, int preview) {
  const int64_t diff = pts - this->last_pts[video];

  if (preview)
    return;

  /* Metronom does not strictly follow audio pts. They usually are too coarse
     for seamless playback. Instead, it takes the latest discontinuity as a
     starting point. This can lead to terrible lags for our very long audio frames.
     So let's make sure audio has the last word here. */
  if (this->send_newpts > video) {
    _x_demux_control_newpts (this->stream, pts, BUF_FLAG_SEEK);
    this->send_newpts         = video;
    this->last_pts[video]     = pts;
    this->last_pts[1 - video] = 0;
  } else if (pts && (this->last_pts[video]) && (abs (diff) > WRAP_THRESHOLD)) {
    _x_demux_control_newpts (this->stream, pts, 0);
    this->send_newpts         = 0;
    this->last_pts[1 - video] = 0;
  }

  if (pts)
    this->last_pts[video] = pts;
}

static uint32_t real_get_reordered_pts (demux_real_t *this, uint8_t *hdr, uint32_t dts) {
  int      pict_type; /* I1/I2/P/B-frame */
  uint32_t t, pts;
  /* lower 13 bits of pts are stored within the frame */
  pict_type = hdr[0];
  t = ((((uint32_t)hdr[1] << 8) | hdr[2]) << 8) | hdr[3];
  switch (this->video_stream->buf_type) {
    case BUF_VIDEO_RV20:
      pict_type >>= 6;
      t         >>= 10;
    break;
    case BUF_VIDEO_RV30:
      pict_type >>= 3;
      t         >>= 7;
    break;
    case BUF_VIDEO_RV40:
      pict_type >>= 5;
      t         >>= 6;
    break;
    default:
      xprintf(this->stream->xine, XINE_VERBOSITY_DEBUG,
              "demux_real: can't fix timestamp for buf type 0x%08x\n",
              this->video_stream->buf_type);
      return (dts);
    break;
  }

  pict_type &= 3;
  t &= 0x1fff;
  pts = (dts & (~0x1fff)) | t;
  /* snap to dts +/- 4.095 seconds */
  if (dts + 0x1000 < pts) pts -= 0x2000;
  else if (dts > pts + 0x1000) pts += 0x2000;
  if (this->stream->xine->verbosity == XINE_VERBOSITY_DEBUG + 1) {
    xprintf (this->stream->xine, XINE_VERBOSITY_DEBUG + 1,
      "demux_real: video pts: %d.%03d:%04d -> %d.%03d (%d)\n",
      dts / 1000, dts % 1000, t, pts / 1000, pts % 1000, pict_type);
  }
  return (pts);
}

static void demux_real_flush (demux_real_t *this) {
  if (this->vbuf) {
    /* flush previous frame */
    this->vbuf->free_buffer (this->vbuf);
    this->vbuf = NULL;
  }
}

static int demux_real_send_chunk(demux_plugin_t *this_gen) {

  demux_real_t   *this = (demux_real_t *) this_gen;

  uint32_t timestamp;
  uint32_t stream;
  uint32_t size;
  uint32_t flags;

  int64_t pts;
  int             keyframe, input_time = 0;
  int             normpos;
  uint32_t        bytes;

  if(this->reference_mode)
    return demux_real_parse_references(this);

  {
    uint8_t header[DATA_PACKET_HEADER_SIZE + 2];
    uint32_t version;
    /* load a header from wherever the stream happens to be pointing */
    if (this->input->read(this->input, header, DATA_PACKET_HEADER_SIZE) != DATA_PACKET_HEADER_SIZE) {
      xprintf (this->stream->xine, XINE_VERBOSITY_DEBUG,
        "demux_real: failed to read data packet header\n");
      goto fail0;
    }
    /* Check to see if we've gone past the end of the data chunk */
    if (_x_is_fourcc(&header[0], "INDX") || _x_is_fourcc(&header[0], "DATA")) {
      lprintf("finished reading data chunk\n");
      goto fail0;
    }
    bytes = DATA_PACKET_HEADER_SIZE;
    if (this->input == this->in1)
      normpos = (uint64_t)this->pos1 * 0xffff / this->endpos1;
    else
      normpos = (uint64_t)this->pos2 * 0xffff / this->endpos2;
    /* check version */
    version = _X_BE_16 (&header[0]);
    if (version > 1) {
      xprintf(this->stream->xine, XINE_VERBOSITY_DEBUG,
        "demux_real: unknown object version in data packet: 0x%04x\n", version);
      goto fail0;
    }
    /* read the packet information */
    stream = _X_BE_16 (&header[4]);
    size   = _X_BE_16 (&header[2]);
    if (size < DATA_PACKET_HEADER_SIZE)
      goto fail0;
    size -= DATA_PACKET_HEADER_SIZE;
    timestamp = _X_BE_32 (&header[6]);
    pts       = (int64_t)timestamp * 90;
    /* Data packet header with version 1 contains 1 extra byte */
    if (version == 0) {
      header[12] = 0;
      keyframe = header[11] & PN_KEYFRAME_FLAG;
    } else {
      if (this->input->read (this->input, header + DATA_PACKET_HEADER_SIZE, 1) != 1)
        goto fail0;
      keyframe = header[DATA_PACKET_HEADER_SIZE] & PN_KEYFRAME_FLAG;
      size--;
      bytes += 1;
    }
    header[13] = 0;
    flags = _X_BE_32 (header + 10);
  }
  lprintf ("packet of stream %d, 0x%X bytes @ %"PRIX64", pts = %"PRId64"%s\n",
    stream, size,
    this->input == this->in1 ? this->startpos1 + this->pos1 : this->startpos2 + this->pos2,
    pts, keyframe ? ", keyframe" : "");

  if (this->video_stream && ((int)stream == this->video_id)) {

    int            vpkg_seqnum = -1;
    int            vpkg_subseq = 0;
    buf_element_t *buf;
    uint32_t       decoder_flags;

    lprintf ("video chunk detected.\n");

    pts = (int64_t) timestamp * 90;

    if (this->video_stream->fourcc == ME_FOURCC ('m','p','4','v')) {
      int left = size;
#ifdef DEBUG_MP4V
      int64_t here = this->input->get_current_pos (this->input);
      xprintf (this->stream->xine, XINE_VERBOSITY_DEBUG,
        "demux_real: mp4v @%" PRId64 ", %u bytes, time %u, flags 0x%08x.\n", here, size, timestamp, flags);
#else
      (void)flags;
#endif
      buf = this->vbuf;
      if (timestamp != this->vtime) {
        if (buf) {
          /* finish previous frame */
          buf->decoder_flags |= BUF_FLAG_FRAME_END;
          this->video_fifo->put (this->video_fifo, buf);
          this->vbuf = buf = NULL;
        }
        /* start new frame */
        this->vtime = timestamp;
        this->vkeyframe = keyframe ? BUF_FLAG_KEYFRAME : 0;
      }
      if (!buf) {
        buf = this->video_fifo->buffer_pool_size_alloc (this->video_fifo, size);
        this->vbuf = buf;
        buf->size = 0;
        buf->type = this->video_stream->buf_type;
        buf->decoder_flags = BUF_FLAG_FRAME_START | this->vkeyframe;
        buf->pts = pts;
        if (pts) {
          check_newpts (this, pts, PTS_VIDEO, 0);
          pts = 0;
        }
      }
      while (left) {
        int part = buf->max_size - buf->size;
        if (part > left)
          part = left;
        if (this->input->read (this->input, buf->content + buf->size, part) != part) {
          xprintf (this->stream->xine, XINE_VERBOSITY_DEBUG,
            "demux_real: failed to read video");
          break;
        }
        buf->size += part;
        buf->extra_info->input_normpos = normpos;
        buf->extra_info->input_time    = timestamp;
        buf->extra_info->total_time    = this->duration;
        bytes += part;
        if (buf->size == buf->max_size) {
          this->video_fifo->put (this->video_fifo, buf);
          buf = this->video_fifo->buffer_pool_size_alloc (this->video_fifo, size);
          buf->size = 0;
          buf->type = this->video_stream->buf_type;
          buf->decoder_flags = this->vkeyframe;
          this->vbuf = buf;
        }
        left -= part;
      }
    }
    else

    /* sub-demuxer */

    while (size > 2) {

      /*
       * read packet header
       * bit 7: 1=last block in block chain
       * bit 6: 1=short header (only one block?)
       */

      int vpkg_header, vpkg_length, vpkg_offset;
      uint8_t b[16], *p = b, *q = b;
#define NEEDBYTES(n) { \
  int need = n - (q - p); \
  if (need > 0) { \
    if (this->input->read (this->input, q, need) != need) \
      goto fail0; \
    q += need; \
  } \
}
      NEEDBYTES (2);
      vpkg_header = *p++;
      lprintf ("vpkg_hdr: %02x (size=%d)\n", vpkg_header, size);

      if (0x40 == (vpkg_header & 0xc0)) {

        /* seems to be a very short header (2 bytes), purpose of the second byte yet unknown */
        lprintf ("bummer == %02X\n", (unsigned int)*p);
        p++;
        vpkg_offset = 0;
        vpkg_length = size - 2;

      } else {

        if (0 == (vpkg_header & 0x40)) {
          /* sub-seqnum (bits 0-6: number of fragment. bit 7: ???) */
          vpkg_subseq = (*p++) & 0x7f;
        }

        /* size of the complete packet. bit 14 is always one (same applies to the offset) */
        NEEDBYTES (5);
        vpkg_length = _X_BE_16 (p); p += 2;
        if (!(vpkg_length & 0xC000)) {
          vpkg_length <<= 16;
          vpkg_length |= _X_BE_16 (p); p += 2;
        } else
          vpkg_length &= 0x3fff;

	/*
	 * offset of the following data inside the complete packet
	 * Note: if (hdr&0xC0)==0x80 then offset is relative to the
	 * _end_ of the packet, so it's equal to fragment size!!!
	 */
        NEEDBYTES (3);
        vpkg_offset = _X_BE_16 (p); p += 2;
        if (!(vpkg_offset & 0xC000)) {
          NEEDBYTES (3);
          vpkg_offset <<= 16;
          vpkg_offset |= _X_BE_16 (p); p += 2;
        } else
          vpkg_offset &= 0x3fff;

        vpkg_seqnum = *p++;
      }

      bytes += p - b;
      size  -= p - b;
#undef NEEDBYTES

      /* not (yet) needed */
      (void)vpkg_subseq;

      lprintf ("seq=%d, offset=%d, length=%d, size=%d, frag size=%d, flags=%02x\n",
               vpkg_seqnum, vpkg_offset, vpkg_length, size, this->fragment_size,
               vpkg_header);

      if (vpkg_seqnum != this->old_seqnum) {
        lprintf ("new seqnum\n");

	this->fragment_size = 0;
	this->old_seqnum = vpkg_seqnum;
      }

      /* if we have a seekable stream then use the timestamp for the data
       * packet for more accurate seeking - if not then estimate time using
       * average bitrate */
      if(this->video_stream->index)
        input_time = timestamp;
      else
        input_time = (int)((int64_t) this->input->get_current_pos(this->input)
                           * 8 * 1000 / this->avg_bitrate);

      decoder_flags = keyframe ? BUF_FLAG_KEYFRAME : 0;

      if (this->fragment_size == 0) {
	lprintf ("new packet starting\n");

        /* send fragment offset table */
        if(this->fragment_count) {
          lprintf("sending fragment offset table\n");

          buf = this->video_fifo->buffer_pool_alloc(this->video_fifo);

          buf->decoder_flags = BUF_FLAG_SPECIAL | BUF_FLAG_FRAME_END;
          buf->decoder_info[1] = BUF_SPECIAL_RV_CHUNK_TABLE;
          buf->decoder_info[2] = this->fragment_count - 1;
          buf->decoder_info_ptr[2] = buf->content;
          buf->decoder_info[3] = 0;
          buf->size = 0;
          buf->type = this->video_stream->buf_type;

          xine_fast_memcpy(buf->content, this->fragment_tab,
                           this->fragment_count*8);

          this->video_fifo->put(this->video_fifo, buf);

          this->fragment_count = 0;
        }

	decoder_flags |= BUF_FLAG_FRAME_START;
      } else {
	lprintf ("continuing packet \n");
      }

      /* add entry to fragment offset table */
      this->fragment_tab[2*this->fragment_count]   = 1;
      this->fragment_tab[2*this->fragment_count+1] = this->fragment_size;
      this->fragment_count++;

      /*
       * calc size of fragment
       */

      int fragment_size;
      switch(vpkg_header & 0xc0) {
      case 0x80:
	fragment_size = vpkg_offset;
	break;
      case 0x00:
	fragment_size = size;
	break;
      default:
	fragment_size = vpkg_length;
	break;
      }
      lprintf ("fragment size is %d\n", fragment_size);

      /*
       * read fragment_size bytes of data
       */

      int n = fragment_size;
      while(n) {
        buf = this->video_fifo->buffer_pool_alloc(this->video_fifo);

	buf->size = MIN(n, buf->max_size);

        buf->decoder_flags = decoder_flags;
        decoder_flags &= ~BUF_FLAG_FRAME_START;

        buf->type = this->video_stream->buf_type;

        if(this->input->read(this->input, buf->content, buf->size) < buf->size) {
          xprintf(this->stream->xine, XINE_VERBOSITY_DEBUG,
                  "demux_real: failed to read video fragment");
          buf->free_buffer(buf);
          goto fail0;
        }
        bytes += buf->size;

        /* RV30 and RV40 streams contain some fragments that shouldn't be passed
         * to the decoder. The purpose of these fragments is unknown, but
         * realplayer doesn't appear to pass them to the decoder either */
        if((n == fragment_size) &&
           (((buf->type == BUF_VIDEO_RV30) && (buf->content[0] & 0x20)) ||
            ((buf->type == BUF_VIDEO_RV40) && (buf->content[0] & 0x80)))) {
          lprintf("ignoring fragment\n");

          /* Discard buffer and skip over rest of fragment */
          buf->free_buffer(buf);
          this->input->seek(this->input, n - buf->size, SEEK_CUR);
          this->fragment_count--;

          break;
        }

        /* if the video stream has b-frames fix the timestamps */
        if((this->video_stream->format >= 0x20200002) &&
           (buf->decoder_flags & BUF_FLAG_FRAME_START))
          pts = (int64_t)real_get_reordered_pts (this, buf->content, timestamp) * 90;

        /* this test was moved from ffmpeg video decoder.
         * fixme: is pts only valid on frame start? */
        if (buf->decoder_flags & BUF_FLAG_FRAME_START) {
          buf->pts = pts;
          check_newpts (this, pts, PTS_VIDEO, 0);
        } else buf->pts = 0;
        pts = 0;

        buf->extra_info->input_normpos = normpos;
        buf->extra_info->input_time    = input_time;
        buf->extra_info->total_time    = this->duration;

        this->video_fifo->put(this->video_fifo, buf);

        n -= buf->size;
      }

      size -= fragment_size;
      lprintf ("size left %d\n", size);

      this->fragment_size += fragment_size;

      if (this->fragment_size >= vpkg_length) {
        lprintf ("fragment finished (%d/%d)\n", this->fragment_size, vpkg_length);
        this->fragment_size = 0;
      }

    } /* while(size>2) */

  } else if (this->audio_fifo && this->audio_stream && ((int)stream == this->audio_id)) {

    lprintf ("audio chunk detected.\n");

    if(this->audio_need_keyframe && !keyframe)
      goto discard;
    else
      this->audio_need_keyframe = 0;

    /* speed up when not debugging */
    if (this->stream->xine->verbosity == XINE_VERBOSITY_DEBUG + 1) {
      xprintf (this->stream->xine, XINE_VERBOSITY_DEBUG + 1,
        "demux_real: audio pts: %d.%03d %s%s\n",
        timestamp / 1000, timestamp % 1000,
        keyframe ? "*" : " ",
        this->audio_stream->sub_packet_cnt ? " " : "s");
    }

    /* cook audio frames are fairly long (almost 2 seconds). For obfuscation
       purposes, they are sent as multiple fragments in intentionally wrong order.
       The first sent fragment has the timestamp for the whole frame.

       Sometimes, the remaining fragments all carry the same time, and appear
       immediately thereafter. This is easy.

       Sometimes, the remaining fragments carry fake timestamps interpolated across
       the frame duration. Consequently, they will be muxed between the next few
       video frames. We get the complete frame ~2 seconds late. This is ugly.
       Let's be careful not to trap metronom into a big lag. */
    if (!this->audio_stream->sub_packet_cnt)
      this->audio_stream->audio_time = timestamp;
    else
      timestamp = this->audio_stream->audio_time;
    /* nasty kludge, maybe this is somewhere in mdpr? */
    if (this->audio_stream->buf_type == BUF_AUDIO_COOK)
      timestamp += 120;
    pts = (int64_t) timestamp * 90;

    /* if we have a seekable stream then use the timestamp for the data
     * packet for more accurate seeking - if not then estimate time using
     * average bitrate */
    if(this->audio_stream->index)
      input_time = timestamp;
    else
      input_time = (int)((int64_t) this->input->get_current_pos(this->input)
                         * 8 * 1000 / this->avg_bitrate);

    check_newpts (this, pts, PTS_AUDIO, 0);

    /* Each packet of AAC is made up of several AAC frames preceded by a
     * header defining the size of the frames in bits (!) */
    if(this->audio_stream->buf_type == BUF_AUDIO_AAC) {
      int i, frames, sizes[16];
      uint8_t b[32];

      /* Upper 4 bits of second byte is frame count */
      if (this->input->read (this->input, b, 2) != 2)
        goto fail0;
      bytes += 2;
      frames = b[1] >> 4;

      /* 2 bytes per frame size */
      if (this->input->read (this->input, b, 2 * frames) != 2 * frames)
        goto fail0;
      bytes += 2 * frames;
      for (i = 0; i < frames; i++)
        sizes[i] = (_X_BE_16 (b + 2 * i) + 7) >> 3;

      for(i = 0; i < frames; i++) {
        if(_x_demux_read_send_data(this->audio_fifo, this->input, sizes[i], pts,
             this->audio_stream->buf_type, !i && keyframe ? BUF_FLAG_KEYFRAME : 0, normpos,
             input_time, this->duration, 0) < 0) {

          xprintf(this->stream->xine, XINE_VERBOSITY_DEBUG,
                  "demux_real: failed to read AAC frame\n");

          goto fail0;
        }
        bytes += sizes[i];
        pts = 0; /* Only set pts on first frame */
      }
    } else if (this->audio_stream->intl) {
      /* reorder */
      uint8_t * buffer = this->audio_stream->frame_buffer;
      const int sps = this->audio_stream->sps;
      const int sph = this->audio_stream->h;
      const int cfs = this->audio_stream->cfs;
      const int w = this->audio_stream->w;
      const int spc = this->audio_stream->sub_packet_cnt;
      int x;
      const int fs = this->audio_stream->frame_size;

      if (!buffer) {
        this->status = DEMUX_FINISHED;
        return this->status;
      }

      switch (this->audio_stream->intl) {
      case ME_FOURCC('I','n','t','4'):
	for (x = 0; x < sph / 2; x++) {
	  int pos = x * 2 * w + spc * cfs;
	  if (pos + cfs > fs || this->input->read (this->input, buffer + pos, cfs) < cfs) {
	    xprintf(this->stream->xine, XINE_VERBOSITY_DEBUG,
		    "demux_real: failed to read audio chunk\n");

            goto fail0;
	  }
          bytes += cfs;
	}
	break;
      case ME_FOURCC('g','e','n','r'):
	for (x = 0; x < w / sps; x++) {
	  int pos = sps * (sph * x + ((sph + 1) / 2) * (spc & 1) + (spc >> 1));
	  if (pos + sps > fs || this->input->read (this->input, buffer + pos, sps) < sps) {
	    xprintf(this->stream->xine, XINE_VERBOSITY_DEBUG,
		    "demux_real: failed to read audio chunk\n");

            goto fail0;
	  }
          bytes += sps;
	}
	break;
      case ME_FOURCC('s','i','p','r'):
        {
          int pos = spc * w;
          if (pos + w > fs || this->input->read (this->input, buffer + pos, w) < w) {
            xprintf(this->stream->xine, XINE_VERBOSITY_DEBUG,
              "demux_real: failed to read audio chunk\n");
            goto fail0;
          }
        }
        bytes += w;
	if (spc == sph - 1)
	  demux_real_sipro_swap ((char *)buffer, sph * w * 2 / 96);
	break;
      }
      this->audio_stream->sub_packet_cnt++;
      if ((int)this->audio_stream->sub_packet_cnt == sph) {
	this->audio_stream->sub_packet_cnt = 0;
        xprintf (this->stream->xine, XINE_VERBOSITY_DEBUG + 1,
          "demux_real: sending audio %d.%03d\n", input_time / 1000, input_time % 1000);
	_x_demux_send_data(this->audio_fifo, buffer, this->audio_stream->frame_size,
          pts, this->audio_stream->buf_type, keyframe ? BUF_FLAG_KEYFRAME : 0, normpos,
          input_time, this->duration, 0);
      }
    } else {
      if(_x_demux_read_send_data(this->audio_fifo, this->input, size, pts,
           this->audio_stream->buf_type, keyframe ? BUF_FLAG_KEYFRAME : 0, normpos,
           input_time, this->duration, 0) < 0) {

        xprintf(this->stream->xine, XINE_VERBOSITY_DEBUG,
                "demux_real: failed to read audio chunk\n");

        goto fail0;
      }
      bytes += size;

      /* FIXME: dp->flags = (flags & 0x2) ? 0x10 : 0; */
    }

  } else {

    /* discard */
    lprintf ("chunk not detected; discarding.\n");

discard:
    this->input->seek(this->input, size, SEEK_CUR);
    bytes += size;

  }

  /* Every good demuxer also is a muxer ;-) */
  if (this->input == this->in1) {
    this->lasttime1 = timestamp;
    this->pos1     += bytes;
  } else {
    this->lasttime2 = timestamp;
    this->pos2     += bytes;
  }
  if (this->in2) {
    if (this->lasttime1 > this->lasttime2)
      this->input = this->in2;
    else
      this->input = this->in1;
  }

#if 0

  this->current_data_chunk_packet_count--;

  /* check if it's time to reload */
  if (!this->current_data_chunk_packet_count &&
      this->next_data_chunk_offset) {
    unsigned char   data_chunk_header[DATA_CHUNK_HEADER_SIZE];

    /* seek to the next DATA chunk offset */
    this->input->seek(this->input, this->next_data_chunk_offset + PREAMBLE_SIZE, SEEK_SET);

    /* load the rest of the DATA chunk header */
    if (this->input->read(this->input, data_chunk_header,
      DATA_CHUNK_HEADER_SIZE) != DATA_CHUNK_HEADER_SIZE) {
      this->status = DEMUX_FINISHED;
      return this->status;
    }
    lprintf ("**** found next DATA tag\n");
    this->current_data_chunk_packet_count = _X_BE_32(&data_chunk_header[2]);
    this->next_data_chunk_offset = _X_BE_32(&data_chunk_header[6]);
  }

  if (!this->current_data_chunk_packet_count) {
    this->status = DEMUX_FINISHED;
    return this->status;
  }

#endif

  return this->status;

  fail0:
  if (this->vbuf) {
    /* finish last frame */
    this->vbuf->decoder_flags |= BUF_FLAG_FRAME_END;
    this->video_fifo->put (this->video_fifo, this->vbuf);
    this->vbuf = NULL;
  }
  if (this->input == this->in1) {
    this->lasttime1 = 0xffffffff;
    if (this->lasttime2 == 0xffffffff) {
      this->status = DEMUX_FINISHED;
      return this->status;
    }
    if (this->in2)
      this->input = this->in2;
  } else {
    this->lasttime2 = 0xffffffff;
    this->input = this->in1;
    if (this->lasttime1 == 0xffffffff) {
      this->status = DEMUX_FINISHED;
      return this->status;
    }
  }

  return this->status;
}

static void demux_real_send_headers(demux_plugin_t *this_gen) {

  demux_real_t *this = (demux_real_t *) this_gen;

  this->video_fifo = this->stream->video_fifo;
  this->audio_fifo = this->stream->audio_fifo;

  this->status = DEMUX_OK;

  this->last_pts[0]   = 0;
  this->last_pts[1]   = 0;
  this->send_newpts   = PTS_BOTH;

  this->avg_bitrate   = 1;



  /* send init info to decoders */

  this->input->seek (this->input, 0, SEEK_SET);

  _x_stream_info_set(this->stream, XINE_STREAM_INFO_HAS_VIDEO, 0);
  _x_stream_info_set(this->stream, XINE_STREAM_INFO_HAS_AUDIO, 0);

  if( !this->reference_mode ) {
    real_parse_headers (this);
  } else {
    if ((this->input->get_capabilities (this->input) & INPUT_CAP_SEEKABLE) != 0)
      this->input->seek (this->input, 0, SEEK_SET);
  }
}

static int demux_real_find_time (real_index_entry_t *index, int n, uint32_t t) {
  int b = 0, l, m = -1, e = n;
  if (e <= 0)
    return -1;
  do {
    l = m;
    m = (b + e) >> 1;
    if (t < index[m].timestamp)
      e = m;
    else
      b = m;
  } while (l != m);
  return m;
}

static int demux_real_find_offs (real_index_entry_t *index, int n, uint32_t offs) {
  int b = 0, l, m = -1, e = n;
  if (e <= 0)
    return -1;
  do {
    l = m;
    m = (b + e) >> 1;
    if (offs < index[m].offset)
      e = m;
    else
      b = m;
  } while (l != m);
  return m;
}

static int demux_real_seek (demux_plugin_t *this_gen,
                             off_t start_pos, int start_time, int playing) {

  demux_real_t       *this = (demux_real_t *) this_gen;

  lprintf("seek start_pos=%d, start_time=%d, playing=%d\n",
          (int)start_pos, start_time, playing);

  demux_real_flush (this);

  this->input = this->in1;

  do {
    int have;

    if (!(this->input->get_capabilities (this->input) & INPUT_CAP_SEEKABLE)) {
      if (playing || !this->input->seek_time)
        return this->status;
      /* RTSP supports only time based seek */
      if (start_pos && !start_time)
        start_time = (int64_t)this->duration * start_pos / 65535;
      this->input->seek_time (this->input, start_time, SEEK_SET);
      break;
    }

    if (!start_pos && !start_time) {
      /* We can always return to the beginning. */
      this->lasttime1 = 0;
      this->pos1 = 18;
      this->pos2 = 18;
      this->in1->seek (this->in1, this->startpos1 + 18, SEEK_SET);
      if (this->in2) {
        this->lasttime2 = 0;
        this->in2->seek (this->in2, this->startpos2 + 18, SEEK_SET);
      }
      break;
    }

    have = ((this->audio_stream && this->audio_stream->index) ? 1 : 0)
         | ((this->video_stream && this->video_stream->index) ? 2 : 0);
    if (!have)
      return this->status;

    if (this->in2) {
      uint32_t pos1, pos2;
      int i1, i2;
      /* Noninterleaved seek. We need both indices. */
      if (have != 3)
        return this->status;
      if (start_time) {
        i1 = demux_real_find_time (this->audio_stream->index, this->audio_stream->index_entries, start_time);
        i2 = demux_real_find_time (this->video_stream->index, this->video_stream->index_entries, start_time);
        if ((i1 < 0) || (i2 < 0))
          return this->status;
      } else { /* start_pos */
        pos1 = (uint32_t)((double)start_pos / 65535.0 * this->endpos1) + this->startpos1;
        pos2 = (uint32_t)((double)start_pos / 65535.0 * this->endpos2) + this->startpos2;
        i1 = demux_real_find_offs (this->audio_stream->index, this->audio_stream->index_entries, pos1);
        i2 = demux_real_find_offs (this->video_stream->index, this->video_stream->index_entries, pos2);
        if ((i1 < 0) || (i2 < 0))
          return this->status;
      }
      pos1 = this->audio_stream->index[i1].offset;
      pos2 = this->video_stream->index[i2].offset;
      this->input->seek (this->in1, pos1, SEEK_SET);
      this->input->seek (this->in2, pos2, SEEK_SET);
      this->pos1 = pos1 - this->startpos1;
      this->pos2 = pos2 - this->startpos2;
      this->lasttime1 = 0;
      this->lasttime2 = 0;
      break;
    }

    {
      real_index_entry_t *index, *other_index = NULL;
      int i, entries;
      start_pos = (off_t)((double) start_pos / 65535 * this->input->get_length (this->input));
      /* video index has priority over audio index */
      if (have & 2) {
        index = this->video_stream->index;
        entries = this->video_stream->index_entries;
        if (have & 1)
          other_index = this->audio_stream->index;
      } else {
        index = this->audio_stream->index;
        entries = this->audio_stream->index_entries;
      }
      if (start_pos)
        i = demux_real_find_offs (index, entries, start_pos);
      else /* start_time */
        i = demux_real_find_time (index, entries, start_time);
      if (i < 0)
        return this->status;
      /* make sure we don't skip past audio/video at start of file */
      if ((i == 0) && other_index && (other_index[0].offset < index[0].offset))
        index = other_index;
      this->input->seek (this->input, index[i].offset, SEEK_SET);
      this->pos1 = index[i].offset - this->startpos1;
      this->lasttime1 = 0;
      break;
    }
  } while (0);

  this->send_newpts         = PTS_BOTH;
  this->old_seqnum          = -1;
  this->fragment_size       = 0;
  this->fragment_count      = 0;
  this->audio_need_keyframe = 1;
  if (this->audio_stream)
    this->audio_stream->sub_packet_cnt = 0;

  if (playing)
    _x_demux_flush_engine (this->stream);

  this->status          = DEMUX_OK;
  return this->status;
}

static void demux_real_dispose (demux_plugin_t *this_gen) {
  demux_real_t *this = (demux_real_t *) this_gen;
  int i;

  demux_real_flush (this);

  if (this->in2)
    this->in2->dispose (this->in2);

  for(i = 0; i < this->num_video_streams; i++) {
    real_free_mdpr(this->video_streams[i].mdpr);
    free(this->video_streams[i].index);
  }

  for(i = 0; i < this->num_audio_streams; i++) {
    real_free_mdpr(this->audio_streams[i].mdpr);
    free(this->audio_streams[i].index);
    free(this->audio_streams[i].frame_buffer);
  }

  free(this->fragment_tab);
  free(this);
}

static int demux_real_get_status (demux_plugin_t *this_gen) {
  demux_real_t *this = (demux_real_t *) this_gen;

  return this->status;
}

static int demux_real_get_stream_length (demux_plugin_t *this_gen) {
  demux_real_t *this = (demux_real_t *) this_gen;

  /* duration is stored in the file as milliseconds */
  return this->duration;
}

static uint32_t demux_real_get_capabilities (demux_plugin_t *this_gen) {
  (void)this_gen;
  return DEMUX_CAP_NOCAP;
}

static int demux_real_get_optional_data(demux_plugin_t *this_gen,
					void *data, int data_type) {
  (void)this_gen;
  (void)data;
  (void)data_type;
  return DEMUX_OPTIONAL_UNSUPPORTED;
}

/* help function to discover stream type. returns:
 * -1 if couldn't read
 *  0 if not known.
 *  1 if normal stream.
 *  2 if reference stream.
 */
static int real_check_stream_type(input_plugin_t *input)
{
  uint8_t buf[1024];
  off_t len = _x_demux_read_header(input, buf, sizeof(buf));

  if ( len < 4 )
    return -1;

  if ( memcmp(buf, "\x2eRMF", 4) == 0 )
    return 1;

#define my_strnstr(haystack, haystacklen, needle) \
  memmem(haystack, haystacklen, needle, sizeof(needle))

  if( my_strnstr(buf, len, "pnm://") || my_strnstr(buf, len, "rtsp://") ||
      my_strnstr(buf, len, "<smil>") || !strncmp((char *)buf, "http://", MIN(7, len)) )
    return 2;

  return 0;
}

static demux_plugin_t *open_plugin (demux_class_t *class_gen, xine_stream_t *stream,
                                    input_plugin_t *input) {

  demux_real_t *this;
  /* discover stream type */
  const int stream_type = real_check_stream_type(input);

  if ( stream_type < 0 )
    return NULL;

  switch (stream->content_detection_method) {

  case METHOD_BY_CONTENT:
    if ( stream_type < 1 )
      return NULL;

    lprintf ("by content accepted.\n");
    break;

  case METHOD_BY_MRL:
  case METHOD_EXPLICIT:
    break;

  default:
    return NULL;
  }

  this = calloc (1, sizeof (demux_real_t));
  if (!this)
    return NULL;

  this->status              = 0;
  this->num_audio_streams   = 0;
  this->num_video_streams   = 0;
  this->audio_stream        = NULL;
  this->video_stream        = NULL;
  this->startpos1      = 0;
  this->startpos2      = 0;
  this->audio_need_keyframe = 0;
  this->current_data_chunk_packet_count = 0;
  this->last_pts[0]         = 0;
  this->last_pts[1]         = 0;
  this->send_newpts         = 0;
  this->fragment_count      = 0;
  this->fragment_tab        = NULL;
  this->fragment_tab_max    = 0;
  this->vbuf                = NULL;
  this->vtime               = 0;
  this->vkeyframe           = 0;
  this->reference_mode      = 0;
  this->in2                 = NULL;
  this->lasttime1           = 0;

  this->stream = stream;
  this->input  =
  this->in1    = input;

  this->lasttime2 = 0xffffffff;

  if(stream_type == 2){
    this->reference_mode = 1;
    lprintf("reference stream detected\n");
  }

  this->demux_plugin.send_headers      = demux_real_send_headers;
  this->demux_plugin.send_chunk        = demux_real_send_chunk;
  this->demux_plugin.seek              = demux_real_seek;
  this->demux_plugin.dispose           = demux_real_dispose;
  this->demux_plugin.get_status        = demux_real_get_status;
  this->demux_plugin.get_stream_length = demux_real_get_stream_length;
  this->demux_plugin.get_capabilities  = demux_real_get_capabilities;
  this->demux_plugin.get_optional_data = demux_real_get_optional_data;
  this->demux_plugin.demux_class       = class_gen;

  return &this->demux_plugin;
}

void *demux_real_init_class (xine_t *xine, const void *data) {
  /* We explicitely dont need these here. */
  (void)xine;
  (void)data;

  static const demux_class_t demux_real_class = {
    .open_plugin     = open_plugin,
    .description     = N_("RealMedia file demux plugin"),
    .identifier      = "Real",
    .mimetypes       =
      "audio/x-pn-realaudio: ra, rm, ram: Real Media file;"
      "audio/x-pn-realaudio-plugin: rpm: Real Media plugin file;"
      "audio/x-real-audio: ra, rm, ram: Real Media file;"
      "application/vnd.rn-realmedia: ra, rm, ram: Real Media file;",
    .extensions      = "rm rmvb ram",
    .dispose         = NULL,
  };

  return (void *)&demux_real_class;
}
