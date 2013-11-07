/*
 * Copyright (C) 2013 the xine project
 * Copyright (C) 2013 Petri Hintukainen <phintuka@users.sourceforge.net>
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
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdlib.h>
#include <string.h>

#include <libavformat/avformat.h>
#include <libavformat/avio.h>

#define LOG_MODULE "libavformat"
#define LOG_VERBOSE
/*
#define LOG
*/

#include <xine/xine_internal.h>
#include <xine/xineutils.h>
#include <xine/input_plugin.h>
#include <xine/demux.h>

#include "ffmpeg_decoder.h"

#include "ff_video_list.h"
#include "ff_audio_list.h"

/*
 * avformat dummy input plugin
 */

typedef struct {
  input_plugin_t   input_plugin;

  char            *mrl;         /* 'public' mrl without authentication credentials */
  AVFormatContext *fmt_ctx;

} avformat_input_plugin_t;

static off_t input_avformat_read (input_plugin_t *this_gen, void *buf_gen, off_t len) {
  return 0;
}

static buf_element_t *input_avformat_read_block (input_plugin_t *this_gen, fifo_buffer_t *fifo, off_t todo) {
  return NULL;
}

static off_t input_avformat_get_length (input_plugin_t *this_gen) {
  return -1;
}

static uint32_t input_avformat_get_capabilities (input_plugin_t *this_gen) {
  return INPUT_CAP_NOCAP;
}

static uint32_t input_avformat_get_blocksize (input_plugin_t *this_gen) {
  return 0;
}

static off_t input_avformat_get_current_pos (input_plugin_t *this_gen) {
  return 0;
}

static off_t input_avformat_seek (input_plugin_t *this_gen, off_t offset, int origin) {
  return -1;
}

static const char* input_avformat_get_mrl (input_plugin_t *this_gen) {
  avformat_input_plugin_t *this = (avformat_input_plugin_t *) this_gen;

  return this->mrl;
}

static int input_avformat_get_optional_data (input_plugin_t *this_gen,
                                                    void *data, int data_type) {
  avformat_input_plugin_t *this = (avformat_input_plugin_t *) this_gen;

  switch (data_type) {
    case INPUT_OPTIONAL_DATA_DEMUXER:
      if (this->fmt_ctx) {
        if (data) {
          *(const char **)data = DEMUX_AVFORMAT_ID;
        }
        return INPUT_OPTIONAL_SUCCESS;
      }
      break;

    case INPUT_OPTIONAL_DATA_fmt_ctx:
      *((AVFormatContext **)data) = this->fmt_ctx;
      this->fmt_ctx = NULL;
      return INPUT_OPTIONAL_SUCCESS;
  }

  return INPUT_OPTIONAL_UNSUPPORTED;
}

static int input_avformat_open (input_plugin_t *this_gen) {
  return 1;
}

static void input_avformat_dispose (input_plugin_t *this_gen ) {
  avformat_input_plugin_t *this = (avformat_input_plugin_t *) this_gen;

  avformat_close_input(&this->fmt_ctx);
  _x_freep (&this->mrl);
  free (this_gen);
}

/*
 * avformat input class
 */

static input_plugin_t *input_avformat_get_instance (input_class_t *cls_gen, xine_stream_t *stream, const char *mrl) {

  const int proto_len = strlen(DEMUX_AVFORMAT_ID"+");

  if (!mrl || !*mrl) {
    return NULL;
  }

  /* accept only mrls with protocol part */
  if (!strchr(mrl, ':') || (strchr(mrl, '/') < strchr(mrl, ':'))) {
    return NULL;
  }

  /* always accept own protocol */
  /* avformat+http://... --> use avformat instead of xine native input/demux plugins */
  if (!strncasecmp (mrl, DEMUX_AVFORMAT_ID"+", proto_len)) {
    mrl += proto_len;
  }

  /* rtsp lower transport */

  AVDictionary    *options = NULL;
  char            *real_mrl = NULL;

  if (!strncmp(mrl, "rtsp+tcp", 8)) {
    av_dict_set(&options, "rtsp_transport", "tcp", 0);
    real_mrl = strdup(mrl);
    memmove(real_mrl + 4, real_mrl + 8, strlen(real_mrl) - 8 + 1);
  }
  if (!strncmp(mrl, "rtsp+http", 9)) {
    av_dict_set(&options, "rtsp_transport", "http", 0);
    memmove(real_mrl + 4, real_mrl + 9, strlen(real_mrl) - 9 + 1);
  }

  /* open input file, and allocate format context */

  AVFormatContext *fmt_ctx = NULL;
  int error;

  if ((error = avformat_open_input(&fmt_ctx, real_mrl ? real_mrl : mrl, NULL, &options)) < 0) {
    char buf[80] = "";
    if (!av_strerror(error, buf, sizeof(buf))) {
      xprintf (stream->xine, XINE_VERBOSITY_LOG, LOG_MODULE": Could not open source '%s': %s\n", mrl, buf);
    } else {
      xprintf (stream->xine, XINE_VERBOSITY_LOG, LOG_MODULE": Could not open source '%s'\n", mrl);
    }
    free(real_mrl);
    return NULL;
  }

  _x_freep(&real_mrl);

  /* create xine input plugin */

  avformat_input_plugin_t *this;

  this = calloc(1, sizeof(avformat_input_plugin_t));
  this->mrl           = _x_mrl_remove_auth(mrl);
  this->fmt_ctx       = fmt_ctx;

  this->input_plugin.open              = input_avformat_open;
  this->input_plugin.get_capabilities  = input_avformat_get_capabilities;
  this->input_plugin.read              = input_avformat_read;
  this->input_plugin.read_block        = input_avformat_read_block;
  this->input_plugin.seek              = input_avformat_seek;
  this->input_plugin.get_current_pos   = input_avformat_get_current_pos;
  this->input_plugin.get_length        = input_avformat_get_length;
  this->input_plugin.get_blocksize     = input_avformat_get_blocksize;
  this->input_plugin.get_mrl           = input_avformat_get_mrl;
  this->input_plugin.get_optional_data = input_avformat_get_optional_data;
  this->input_plugin.dispose           = input_avformat_dispose;
  this->input_plugin.input_class       = cls_gen;

  /* do not expose authentication credentials in title (if title is not set, it defaults to mrl in xine-ui) */
  _x_meta_info_set(stream, XINE_META_INFO_TITLE, this->mrl);

  return &this->input_plugin;
}

void *init_avformat_input_plugin (xine_t *xine, void *data) {

  input_class_t  *this;

  this = calloc(1, sizeof(input_class_t));

  pthread_once( &once_control, init_once_routine );

  this->get_instance      = input_avformat_get_instance;
  this->description       = N_("libavformat input plugin");
  this->identifier        = DEMUX_AVFORMAT_ID;
  this->get_dir           = NULL;
  this->get_autoplay_list = NULL;
  this->dispose           = default_input_class_dispose;
  this->eject_media       = NULL;

  return this;
}

input_info_t input_info_avformat = {
  -2   /* priority */
};

/*
 * avformat demux plugin
 */

typedef struct {
  demux_plugin_t        demux_plugin;

  xine_stream_t        *stream;
  int                   status;

  AVFormatContext *fmt_ctx;
  int              video_stream_idx; /* selected avformat video stream */
  int              audio_stream_idx; /* selected avformat audio stream */

  uint32_t         xine_video_type;
  uint32_t         xine_audio_type;

  /* detect discontinuity */
  int64_t               last_pts;
  int                   send_newpts;

} avformat_demux_plugin_t;

/*
 * TODO:
 *  - multiple audio streams
 *  - subtitle streams
 *  - seeking
 *  - metadata
 */

#define WRAP_THRESHOLD 360000

static void check_newpts(avformat_demux_plugin_t *this, int64_t pts) {

  int64_t diff = this->last_pts - pts;
  if (this->send_newpts || (this->last_pts && abs(diff) > WRAP_THRESHOLD)) {

    _x_demux_control_newpts(this->stream, pts, 0);
    this->send_newpts = 0;
    this->last_pts = pts;
  }
}

static uint32_t video_codec_lookup(avformat_demux_plugin_t *this, int id) {

  int i;
  for (i = 0; i < sizeof(ff_video_lookup)/sizeof(ff_codec_t); i++) {
    if (ff_video_lookup[i].id == id) {
      xprintf (this->stream->xine, XINE_VERBOSITY_LOG,
               LOG_MODULE": found video codec '%s'\n", ff_video_lookup[i].name);
      return ff_video_lookup[i].type;
    }
  }

  return 0;
}

static uint32_t audio_codec_lookup(avformat_demux_plugin_t *this, int id) {

  int i;
  for (i = 0; i < sizeof(ff_audio_lookup)/sizeof(ff_codec_t); i++) {
    if (ff_audio_lookup[i].id == id) {
      xprintf (this->stream->xine, XINE_VERBOSITY_LOG,
               LOG_MODULE": found audio codec '%s'\n", ff_audio_lookup[i].name);
      return ff_audio_lookup[i].type;
    }
  }

  switch (id) {
    case AV_CODEC_ID_PCM_S16LE:
      return BUF_AUDIO_LPCM_LE;
    case AV_CODEC_ID_PCM_S16BE:
      return BUF_AUDIO_LPCM_BE;
  }

  return 0;
}

static int find_avformat_streams(avformat_demux_plugin_t *this) {

  /* find avformat streams */

  this->video_stream_idx = av_find_best_stream(this->fmt_ctx, AVMEDIA_TYPE_VIDEO, -1, -1, NULL, 0);
  this->audio_stream_idx = av_find_best_stream(this->fmt_ctx, AVMEDIA_TYPE_AUDIO, -1, -1, NULL, 0);

  if (this->video_stream_idx < 0 && this->audio_stream_idx < 0) {
    xprintf (this->stream->xine, XINE_VERBOSITY_LOG,
             LOG_MODULE": Could not find supported audio or video stream in the input\n");
    return 0;
  }

  /* map streams to xine types */

  if (this->video_stream_idx >= 0) {
    AVStream *st = this->fmt_ctx->streams[this->video_stream_idx];
    this->xine_video_type = video_codec_lookup(this, st->codec->codec_id);

    if (!this->xine_video_type) {
      this->video_stream_idx = -1;
      xprintf (this->stream->xine, XINE_VERBOSITY_LOG,
               LOG_MODULE": ffmpeg video codec id %d --> NO xine buffer type\n", st->codec->codec_id);
    } else {
      xprintf (this->stream->xine, XINE_VERBOSITY_LOG,
               LOG_MODULE": ffmpeg video codec id %d --> xine buffer type 0x%08x\n", st->codec->codec_id, this->xine_video_type);
    }
  }

  if (this->audio_stream_idx >= 0) {
    AVStream *st = this->fmt_ctx->streams[this->audio_stream_idx];

    this->xine_audio_type = audio_codec_lookup(this, st->codec->codec_id);
    if (!this->xine_audio_type) {
      this->audio_stream_idx = -1;
      xprintf (this->stream->xine, XINE_VERBOSITY_LOG,
               LOG_MODULE": ffmpeg audio codec id %d --> NO xine buffer type\n", st->codec->codec_id);
    } else {
      xprintf (this->stream->xine, XINE_VERBOSITY_LOG,
               LOG_MODULE": ffmpeg audio codec id %d --> xine buffer type 0x%08x\n", st->codec->codec_id, this->xine_audio_type);
    }
  }

  if (!this->xine_video_type && !this->xine_audio_type) {
    xprintf (this->stream->xine, XINE_VERBOSITY_LOG,
             LOG_MODULE": Could not find matching xine buffer types, aborting\n");
    return 0;
  }

  /* TODO: set metadata */
#ifdef LOG
  /* dump metadata */
  AVDictionaryEntry *tag = NULL;
  while ((tag = av_dict_get(this->fmt_ctx->metadata, "", tag, AV_DICT_IGNORE_SUFFIX)))
    printf("   %s=%s\n", tag->key, tag->value);
#endif

  return 1;
}

static void send_headers_audio(avformat_demux_plugin_t *this) {

  AVCodecContext    *ctx = this->fmt_ctx->streams[this->audio_stream_idx]->codec;
  buf_element_t     *buf = this->stream->audio_fifo->buffer_pool_alloc (this->stream->audio_fifo);
  size_t             extradata_size = ctx->extradata_size;
  xine_waveformatex *fmt = (xine_waveformatex *)buf->content;

  if (!ctx->extradata || extradata_size + sizeof(xine_waveformatex) > buf->max_size) {
    extradata_size = 0;
  }

  _x_stream_info_set(this->stream, XINE_STREAM_INFO_AUDIO_FOURCC, ctx->codec_tag);

  fmt->cbSize          = extradata_size;
  fmt->nBlockAlign     = ctx->block_align;
  fmt->nAvgBytesPerSec = ctx->bit_rate / 8;

  if (extradata_size) {
    memcpy(buf->content + sizeof(xine_waveformatex), ctx->extradata, extradata_size);
  }

  buf->type = this->xine_audio_type;
  buf->size = extradata_size + sizeof(xine_waveformatex);
  buf->decoder_info[1] = ctx->sample_rate;
  buf->decoder_info[2] = ctx->bits_per_coded_sample;
  buf->decoder_info[3] = ctx->channels;
  buf->decoder_flags = BUF_FLAG_HEADER | BUF_FLAG_STDHEADER | BUF_FLAG_FRAME_END;

  this->stream->audio_fifo->put (this->stream->audio_fifo, buf);
}

static void send_headers_video(avformat_demux_plugin_t *this) {

  AVCodecContext *ctx = this->fmt_ctx->streams[this->video_stream_idx]->codec;
  buf_element_t  *buf = this->stream->video_fifo->buffer_pool_alloc (this->stream->video_fifo);
  size_t          extradata_size = ctx->extradata_size;
  xine_bmiheader *bih = (xine_bmiheader *)buf->content;

  if (!ctx->extradata || extradata_size + sizeof(xine_bmiheader) > buf->max_size) {
    extradata_size = 0;
  }

  _x_stream_info_set(this->stream, XINE_STREAM_INFO_VIDEO_FOURCC, ctx->codec_tag);

  bih->biSize     = sizeof(xine_bmiheader) + extradata_size;
  bih->biBitCount = ctx->bits_per_coded_sample;
  bih->biWidth    = ctx->width;
  bih->biHeight   = ctx->height;

  if (extradata_size) {
    memcpy(buf->content + sizeof(xine_bmiheader), ctx->extradata, extradata_size);
  }

  buf->type = this->xine_video_type;
  buf->size = extradata_size + sizeof(xine_bmiheader);
  buf->decoder_flags = BUF_FLAG_HEADER | BUF_FLAG_STDHEADER | BUF_FLAG_FRAME_END;

  this->stream->video_fifo->put (this->stream->video_fifo, buf);
}

static int send_avpacket(avformat_demux_plugin_t *this)
{
  int64_t  stream_pos    = avio_tell(this->fmt_ctx->pb);
  int64_t  stream_length = avio_size(this->fmt_ctx->pb);
  AVPacket pkt;
  uint32_t buffer_type = 0;
  fifo_buffer_t *fifo = NULL;

  av_init_packet(&pkt);
  pkt.data = NULL;
  pkt.size = 0;

  /* read frame from the file */
  if (av_read_frame(this->fmt_ctx, &pkt) < 0) {
    return -1;
  }

  /* map to xine fifo / buffer type */
  if (this->video_stream_idx >= 0 && pkt.stream_index == this->video_stream_idx) {
    fifo = this->stream->video_fifo;
    buffer_type = this->xine_video_type;
  } else if (this->audio_stream_idx >= 0 && pkt.stream_index == this->audio_stream_idx) {
    fifo = this->stream->audio_fifo;
    buffer_type = this->xine_audio_type;
  } else {
    //fprintf(stderr, "??? PACK\n");
  }

  /* send to decoder */
  if (fifo) {
    int64_t  pts = 0;
    float    input_normpos = (stream_length > 0 && stream_pos > 0) ? (int)(65535 * stream_pos / stream_length) : 0;
    int      total_time    = (int)((int64_t)this->fmt_ctx->duration * 1000 / AV_TIME_BASE);
    int      input_time    = input_normpos * total_time / 65535;

    if (pkt.pts != AV_NOPTS_VALUE) {
      AVStream *stream = this->fmt_ctx->streams[pkt.stream_index];
      pts = (int64_t)(pkt.pts * stream->time_base.num * 90000 / stream->time_base.den);
      check_newpts(this, pts);
    }

    _x_demux_send_data(fifo, pkt.data, pkt.size, pts, buffer_type, 0/*decoder_flags*/,
                       input_normpos, input_time, total_time, 0/*frame_number*/);
  }

  av_free_packet(&pkt);

  return 1;
}

/*
 * demux interface
 */

static int demux_avformat_get_status (demux_plugin_t *this_gen) {
  avformat_demux_plugin_t *this = (avformat_demux_plugin_t *) this_gen;

  return this->status;
}

static int demux_avformat_get_stream_length (demux_plugin_t *this_gen) {
  avformat_demux_plugin_t *this = (avformat_demux_plugin_t *) this_gen;

  if (this->fmt_ctx) {
    return (int)((int64_t)this->fmt_ctx->duration * 1000 / AV_TIME_BASE);
  }

  return -1;
}

static uint32_t demux_avformat_get_capabilities(demux_plugin_t *this_gen) {
  return DEMUX_CAP_NOCAP;
}

static int demux_avformat_get_optional_data(demux_plugin_t *this_gen,
                                            void *data, int data_type) {
  return DEMUX_OPTIONAL_UNSUPPORTED;
}

static int demux_avformat_send_chunk (demux_plugin_t *this_gen) {
  avformat_demux_plugin_t *this = (avformat_demux_plugin_t *) this_gen;

  if (send_avpacket (this) < 0) {
    this->status = DEMUX_FINISHED;
  } else {
    this->status = DEMUX_OK;
  }

  return this->status;
}

static void demux_avformat_send_headers (demux_plugin_t *this_gen) {
  avformat_demux_plugin_t *this = (avformat_demux_plugin_t *) this_gen;

  _x_demux_control_start(this->stream);

  if (this->audio_stream_idx >= 0) {
    _x_stream_info_set(this->stream, XINE_STREAM_INFO_HAS_AUDIO, 1);
    send_headers_audio(this);
  }

  if (this->video_stream_idx >= 0) {
    _x_stream_info_set(this->stream, XINE_STREAM_INFO_HAS_VIDEO, 1);
    send_headers_video(this);
  }

  this->send_newpts = 1;
  this->status      = DEMUX_OK;
}

static int demux_avformat_seek (demux_plugin_t *this_gen,
                                off_t start_pos, int start_time, int playing) {

  avformat_demux_plugin_t *this = (avformat_demux_plugin_t *) this_gen;

  /* TODO */

  return this->status;
}

static void demux_avformat_dispose (demux_plugin_t *this_gen) {
  avformat_demux_plugin_t *this = (avformat_demux_plugin_t *) this_gen;

  avformat_close_input(&this->fmt_ctx);
  free (this_gen);
}

/*
 * demux class
 */

static int pb_input_read_packet(void *opaque, uint8_t *buf, int buf_size) {
  input_plugin_t *input = (input_plugin_t *)opaque;
  return input->read(input, buf, buf_size);
}

static int64_t pb_input_seek(void *opaque, int64_t offset, int whence) {
  input_plugin_t *input = (input_plugin_t *)opaque;
  return input->seek(input, offset, whence);
}

static AVIOContext *get_io_context(xine_stream_t *stream, input_plugin_t *input)
{
  AVIOContext *pb = NULL;

  if (!strcmp(input->input_class->identifier, INPUT_AVIO_ID)) {

    /* get AVIOContext from avio input plugin */
    if (input->get_optional_data(input, &pb, INPUT_OPTIONAL_DATA_pb) != INPUT_OPTIONAL_SUCCESS || !pb) {
      xprintf (stream->xine, XINE_VERBOSITY_LOG, LOG_MODULE": could not get AVIOContext from input plugin\n");
      return NULL;
    }
    xprintf (stream->xine, XINE_VERBOSITY_LOG, LOG_MODULE": got AVIOContext from input plugin\n");

  } else {

    /* create AVIO wrapper for native input plugin */
    xprintf (stream->xine, XINE_VERBOSITY_LOG, LOG_MODULE": creating AVIOContext wrapper for input plugin\n");
    pb = avio_alloc_context(av_malloc(4096), 4096, 0/*write_flag*/, input, pb_input_read_packet, NULL, pb_input_seek);
  }

  avio_seek(pb, 0, SEEK_SET);

  return pb;
}

static AVFormatContext *get_format_context(xine_stream_t *stream, input_plugin_t *input)
{
  AVFormatContext *fmt_ctx = NULL;

  if (!strcmp(input->input_class->identifier, DEMUX_AVFORMAT_ID)) {

    /* get AVFormatContext from input plugin */
    if (input->get_optional_data(input, &fmt_ctx, INPUT_OPTIONAL_DATA_fmt_ctx) != INPUT_OPTIONAL_SUCCESS || !fmt_ctx) {
      xprintf (stream->xine, XINE_VERBOSITY_LOG, LOG_MODULE": could not get AVFormatContext from input plugin\n");
      return NULL;
    }
    xprintf (stream->xine, XINE_VERBOSITY_LOG, LOG_MODULE": got AVFormtContext from input plugin\n");

  } else {

    /* create and open AVFormatContext */

    AVIOContext *pb = get_io_context(stream, input);
    if (!pb) {
      return NULL;
    }

    fmt_ctx = avformat_alloc_context();
    fmt_ctx->pb = pb;

    if (avformat_open_input(&fmt_ctx, input->get_mrl(input), NULL, NULL) < 0) {
      xprintf (stream->xine, XINE_VERBOSITY_LOG, LOG_MODULE": could not open AVFormatContext for source '%s'\n", input->get_mrl(input));
      return NULL;
    }
  }

  return fmt_ctx;
}

/*
 * demux class interface
 */

static demux_plugin_t *open_demux_avformat_plugin (demux_class_t *class_gen,
                                                   xine_stream_t *stream,
                                                   input_plugin_t *input) {

  /* get AVFormatContext */
  AVFormatContext *fmt_ctx = get_format_context(stream, input);
  if (!fmt_ctx) {
    return NULL;
  }

  /* retrieve stream information */
  if (avformat_find_stream_info(fmt_ctx, NULL) < 0) {
    xprintf (stream->xine, XINE_VERBOSITY_LOG, LOG_MODULE": could not find stream information\n");
    avformat_close_input(&fmt_ctx);
    return NULL;
  }

  /* dump input information to stderr */
  av_dump_format(fmt_ctx, 0, input->get_mrl(input), 0);


  /* initialize xine demuxer */

  avformat_demux_plugin_t *this;

  this         = calloc(1, sizeof(avformat_demux_plugin_t));
  this->stream = stream;

  this->demux_plugin.send_headers      = demux_avformat_send_headers;
  this->demux_plugin.send_chunk        = demux_avformat_send_chunk;
  this->demux_plugin.seek              = demux_avformat_seek;
  this->demux_plugin.dispose           = demux_avformat_dispose;
  this->demux_plugin.get_status        = demux_avformat_get_status;
  this->demux_plugin.get_stream_length = demux_avformat_get_stream_length;
  this->demux_plugin.get_capabilities  = demux_avformat_get_capabilities;
  this->demux_plugin.get_optional_data = demux_avformat_get_optional_data;
  this->demux_plugin.demux_class       = class_gen;

  this->status = DEMUX_FINISHED;
  this->fmt_ctx = fmt_ctx;

  /* check if the stream can be played */
  if (!find_avformat_streams(this)) {
    xprintf (stream->xine, XINE_VERBOSITY_LOG, LOG_MODULE": could not find any playable streams\n");
    demux_avformat_dispose(&this->demux_plugin);
    return NULL;
  }

  return &this->demux_plugin;
}

void *init_avformat_demux_plugin (xine_t *xine, void *data) {
  demux_class_t     *this;

  this  = calloc(1, sizeof(demux_class_t));

  this->open_plugin     = open_demux_avformat_plugin;
  this->description     = N_("libavformat demux plugin");
  this->identifier      = DEMUX_AVFORMAT_ID;
  this->mimetypes       = NULL;
  this->extensions      = "";
  this->dispose         = default_demux_class_dispose;

  return this;
}

demuxer_info_t demux_info_avformat = {
  -1                       /* priority */
};
