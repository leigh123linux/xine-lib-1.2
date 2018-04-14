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

#ifndef ASFHEADER_H
#define ASFHEADER_H

#include <inttypes.h>

/*
 * define asf GUIDs (list from avifile)
 */
typedef enum {
  GUID_ERROR = 0,
  /* base ASF objects */
  GUID_ASF_HEADER,
  GUID_ASF_DATA,
  GUID_ASF_SIMPLE_INDEX,
  GUID_INDEX,
  GUID_MEDIA_OBJECT_INDEX,
  GUID_TIMECODE_INDEX,
  /* header ASF objects */
  GUID_ASF_FILE_PROPERTIES,
  GUID_ASF_STREAM_PROPERTIES,
  GUID_ASF_HEADER_EXTENSION,
  GUID_ASF_CODEC_LIST,
  GUID_ASF_SCRIPT_COMMAND,
  GUID_ASF_MARKER,
  GUID_ASF_BITRATE_MUTUAL_EXCLUSION,
  GUID_ASF_ERROR_CORRECTION,
  GUID_ASF_CONTENT_DESCRIPTION,
  GUID_ASF_EXTENDED_CONTENT_DESCRIPTION,
  GUID_ASF_STREAM_BITRATE_PROPERTIES,
  GUID_ASF_EXTENDED_CONTENT_ENCRYPTION,
  GUID_ASF_PADDING,
  /* stream properties object stream type */
  GUID_ASF_AUDIO_MEDIA,
  GUID_ASF_VIDEO_MEDIA,
  GUID_ASF_COMMAND_MEDIA,
  GUID_ASF_JFIF_MEDIA,
  GUID_ASF_DEGRADABLE_JPEG_MEDIA,
  GUID_ASF_FILE_TRANSFER_MEDIA,
  GUID_ASF_BINARY_MEDIA,
  /* stream properties object error correction type */
  GUID_ASF_NO_ERROR_CORRECTION,
  GUID_ASF_AUDIO_SPREAD,
  /* mutual exclusion object exlusion type */
  GUID_ASF_MUTEX_BITRATE,
  GUID_ASF_MUTEX_UKNOWN,
  /* header extension */
  GUID_ASF_RESERVED_1,
  /* script command */
  GUID_ASF_RESERVED_SCRIPT_COMMNAND,
  /* marker object */
  GUID_ASF_RESERVED_MARKER,
  /* various */
  GUID_ASF_AUDIO_CONCEAL_NONE,
  GUID_ASF_CODEC_COMMENT1_HEADER,
  GUID_ASF_2_0_HEADER,

  GUID_EXTENDED_STREAM_PROPERTIES,
  GUID_ADVANCED_MUTUAL_EXCLUSION,
  GUID_GROUP_MUTUAL_EXCLUSION,
  GUID_STREAM_PRIORITIZATION,
  GUID_BANDWIDTH_SHARING,
  GUID_LANGUAGE_LIST,
  GUID_METADATA,
  GUID_METADATA_LIBRARY,
  GUID_INDEX_PARAMETERS,
  GUID_MEDIA_OBJECT_INDEX_PARAMETERS,
  GUID_TIMECODE_INDEX_PARAMETERS,
  GUID_ADVANCED_CONTENT_ENCRYPTION,
  GUID_COMPATIBILITY,
  GUID_END
} asf_guid_t;

#if 0
/* asf stream types. currently using asf_guid_t instead. */
typedef enum {
  ASF_STREAM_TYPE_UNKNOWN = 0,
  ASF_STREAM_TYPE_AUDIO,
  ASF_STREAM_TYPE_VIDEO,
  ASF_STREAM_TYPE_CONTROL,
  ASF_STREAM_TYPE_JFIF,
  ASF_STREAM_TYPE_DEGRADABLE_JPEG,
  ASF_STREAM_TYPE_FILE_TRANSFER,
  ASF_STREAM_TYPE_BINARY
} asf_stream_type_t;
#endif

#define ASF_MAX_NUM_STREAMS     23

/* TJ. Globally Unique IDentifiction (GUID) is originally defined as
 * uint32_t Data1; uint16_t Data2; uint16_t Data3; uint8_t Data4[8];
 * stored in little endian byte order.
 * This is fine with x86 but inefficient at big endian machines.
 * We only compare GUIDs against hard-coded constants here,
 * so lets use plain uint8_t[16] instead.
 */

typedef struct asf_header_s asf_header_t;
typedef struct asf_file_s asf_file_t;
typedef struct asf_content_s asf_content_t;
typedef struct asf_stream_s asf_stream_t;
typedef struct asf_stream_extension_s asf_stream_extension_t;

struct asf_header_s {
  asf_file_t             *file;
  asf_content_t          *content;
  int                     stream_count;

  asf_stream_t           *streams[ASF_MAX_NUM_STREAMS];
  asf_stream_extension_t *stream_extensions[ASF_MAX_NUM_STREAMS];
  uint32_t                bitrates[ASF_MAX_NUM_STREAMS];
  struct { uint32_t x, y; } aspect_ratios[ASF_MAX_NUM_STREAMS];
};

struct asf_file_s {
  uint8_t  file_id[16];
  uint64_t file_size;              /* in bytes */
  uint64_t data_packet_count;
  uint64_t play_duration;          /* in 100 nanoseconds unit */
  uint64_t send_duration;          /* in 100 nanoseconds unit */
  uint64_t preroll;                /* in 100 nanoseconds unit */

  uint32_t packet_size;
  uint32_t max_bitrate;

  uint8_t  broadcast_flag;
  uint8_t  seekable_flag;
};

/* ms unicode strings */
struct asf_content_s {
  char     *title;
  char     *author;
  char     *copyright;
  char     *description;
  char     *rating;
};

struct asf_stream_s {
  uint16_t   stream_number;
  asf_guid_t stream_type;
  asf_guid_t error_correction_type;
  uint64_t   time_offset;

  uint32_t   private_data_length;
  uint8_t   *private_data;

  uint32_t   error_correction_data_length;
  uint8_t   *error_correction_data;

  uint8_t    encrypted_flag;
};

struct asf_stream_extension_s {
  uint64_t start_time;
  uint64_t end_time;
  uint32_t data_bitrate;
  uint32_t buffer_size;
  uint32_t initial_buffer_fullness;
  uint32_t alternate_data_bitrate;
  uint32_t alternate_buffer_size;
  uint32_t alternate_initial_buffer_fullness;
  uint32_t max_object_size;

  uint8_t  reliable_flag;
  uint8_t  seekable_flag;
  uint8_t  no_cleanpoints_flag;
  uint8_t  resend_live_cleanpoints_flag;

  uint16_t language_id;
  uint64_t average_time_per_frame;

  uint16_t stream_name_count;
  uint16_t payload_extension_system_count;

  char   **stream_names;
};

asf_guid_t asf_guid_2_num (const uint8_t *guid);
void asf_guid_2_str (uint8_t *str, const uint8_t *guid);
const char *asf_guid_name (asf_guid_t num);

asf_header_t *asf_header_new (uint8_t *buffer, int buffer_len) XINE_MALLOC;
void asf_header_choose_streams (asf_header_t *header, uint32_t bandwidth,
                                int *video_id, int *audio_id);
void asf_header_disable_streams (asf_header_t *header,
                                 int video_id, int audio_id);
void asf_header_delete (asf_header_t *header);


#endif
