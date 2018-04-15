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

#ifdef HAVE_ICONV
#include <iconv.h>
#endif

#include <string.h>
#include <stdio.h>
#include <stdlib.h>


#define LOG_MODULE "asfheader"
#define LOG_VERBOSE
/*
#define LOG
*/

#include <xine/xineutils.h>
#include "bswap.h"
#include "asfheader.h"

#ifndef HAVE_ICONV

/* dummy conversion perserving ASCII only */

#define iconv_open(TO, FROM) 0
#define iconv(CD, INBUF, INLEFT, OUTBUF, OUTLEFT) iconv_internal(INBUF, INLEFT, OUTBUF, OUTLEFT)
#define iconv_close(CD)
#ifdef ICONV_CONST
#  undef ICONV_CONST
#endif
#define ICONV_CONST const

typedef int iconv_t;

size_t iconv_internal(const char **inbuf, size_t *inbytesleft, char **outbuf, size_t *outbytesleft) {
  size_t i, n;
  const char *ins;
  char *outs;

  n = *inbytesleft / 2 > *outbytesleft ? *outbytesleft : *inbytesleft / 2;
  for (i = n, ins = *inbuf, outs = *outbuf; i > 0; i--) {
    outs[0] = ((ins[0] & 0x80) || ins[1]) ? '?' : ins[0];
    ins += 2;
    outs++;
  }
  *inbuf = ins;
  *outbuf = outs;
  (*inbytesleft) -= (2 * n);
  (*outbytesleft) -= n;

  return 0;
}
#endif


typedef struct asf_header_internal_s asf_header_internal_t;
struct asf_header_internal_s {
  asf_header_t            pub;

  /* private part */
  int                     number_count;
  uint16_t                numbers[ASF_MAX_NUM_STREAMS];
  uint8_t                *bitrate_pointers[ASF_MAX_NUM_STREAMS];
};


typedef struct asf_reader_s asf_reader_t;
struct asf_reader_s {
  uint8_t *buffer;
  size_t   pos;
  size_t   size;
};


static void asf_reader_init(asf_reader_t *reader, uint8_t *buffer, int size) {
  reader->buffer = buffer;
  reader->pos = 0;
  reader->size = size;
}

#if 0
static int asf_reader_get_8(asf_reader_t *reader, uint8_t *value) {
  if ((reader->size - reader->pos) < 1)
    return 0;
  *value = *(reader->buffer + reader->pos);
  reader->pos += 1;
  return 1;
}
#endif

static int asf_reader_get_16(asf_reader_t *reader, uint16_t *value) {
  if ((reader->size - reader->pos) < 2)
    return 0;
  *value = _X_LE_16(reader->buffer + reader->pos);
  reader->pos += 2;
  return 1;
}

static int asf_reader_get_32(asf_reader_t *reader, uint32_t *value) {
  if ((reader->size - reader->pos) < 4)
    return 0;
  *value = _X_LE_32(reader->buffer + reader->pos);
  reader->pos += 4;
  return 1;
}

static int asf_reader_get_64(asf_reader_t *reader, uint64_t *value) {
  if ((reader->size - reader->pos) < 8)
    return 0;
  *value = _X_LE_64(reader->buffer + reader->pos);
  reader->pos += 8;
  return 1;
}

static int asf_reader_get_guid (asf_reader_t *reader, uint8_t *value) {
  if ((reader->size - reader->pos) < 16)
    return 0;

  memcpy (value, reader->buffer + reader->pos, 16);
  reader->pos += 16;
  return 1;
}

static uint8_t *asf_reader_get_bytes(asf_reader_t *reader, size_t size) {
  uint8_t *buffer;

  if ((reader->size - reader->pos) < size)
    return NULL;
  if (! (buffer = malloc(size)) )
    return NULL;
  memcpy(buffer, reader->buffer + reader->pos, size);
  reader->pos += size;
  return buffer;
}

/* get an utf8 string */
static char *asf_reader_get_string(asf_reader_t *reader, size_t size, iconv_t cd) {
  char *inbuf, *outbuf;
  size_t inbytesleft, outbytesleft;
  char scratch[2048];

  if ((size == 0) ||((reader->size - reader->pos) < size))
    return NULL;

  inbuf = (char *)reader->buffer + reader->pos;
  inbytesleft = size;
  outbuf = scratch;
  outbytesleft = sizeof(scratch);
  reader->pos += size;
  if (iconv (cd, (ICONV_CONST char **)&inbuf, &inbytesleft, &outbuf, &outbytesleft) != (size_t)-1) {
    return strdup(scratch);
  } else {
    lprintf("iconv error\n");
    return NULL;
  }
}

static int asf_reader_skip(asf_reader_t *reader, size_t size) {
  if ((reader->size - reader->pos) < size) {
    reader->pos = reader->size;
    return 0;
  }
  reader->pos += size;
  return size;
}

static uint8_t *asf_reader_get_buffer(asf_reader_t *reader) {
  return (reader->buffer + reader->pos);
}

static int asf_reader_eos(asf_reader_t *reader) {
  if (reader->pos < reader->size)
    return 0;
  else
    return 1;
}

static size_t asf_reader_get_size(asf_reader_t *reader) {
  return reader->size - reader->pos;
}

/* Manage id mapping */
static int asf_header_get_stream_id(asf_header_t *header_pub, uint16_t stream_number) {
  asf_header_internal_t *header = (asf_header_internal_t *)header_pub;
  int i;
  
  /* linear search */
  for (i = 0; i < header->number_count; i++) {
    if (stream_number == header->numbers[i]) {
      lprintf("asf_header_get_stream_id: id found: %d\n", i);
      return i;
    }
  }

  /* not found */
  if (header->number_count >= ASF_MAX_NUM_STREAMS)
    return -1;

  header->numbers[header->number_count] = stream_number;
  header->number_count++;
  return header->number_count - 1;
}

static int asf_header_parse_file_properties(asf_header_t *header, uint8_t *buffer, int buffer_len) {
  asf_reader_t reader;
  asf_file_t *asf_file;
  uint32_t flags = 0;

  if (buffer_len < 80) {
    lprintf("invalid asf file properties object\n");
    return 0;
  }

  if (! (asf_file = malloc(sizeof(asf_file_t))) ) {
    lprintf("cannot allocate asf_file_struct\n");
    return 0;
  }

  asf_reader_init(&reader, buffer, buffer_len);
 
  asf_reader_get_guid(&reader, asf_file->file_id);
  asf_reader_get_64(&reader, &asf_file->file_size);

  /* creation date */
  asf_reader_skip(&reader, 8);
  asf_reader_get_64(&reader, &asf_file->data_packet_count);
  asf_reader_get_64(&reader, &asf_file->play_duration);
  asf_reader_get_64(&reader, &asf_file->send_duration);
  asf_reader_get_64(&reader, &asf_file->preroll);
  
  asf_reader_get_32(&reader, &flags);
  asf_reader_get_32(&reader, &asf_file->packet_size);

  /* duplicated packet size */
  asf_reader_skip(&reader, 4);
  asf_reader_get_32(&reader, &asf_file->max_bitrate);
  
  asf_file->broadcast_flag = flags & 0x1;
  asf_file->seekable_flag = flags & 0x2;

  header->file = asf_file;

  lprintf("File properties\n");
  lprintf("  file_id:                           %04X\n", _X_LE_16(&asf_file->file_id[4]));
  lprintf("  file_size:                         %"PRIu64"\n", asf_file->file_size);
  lprintf("  data_packet_count:                 %"PRIu64"\n", asf_file->data_packet_count);
  lprintf("  play_duration:                     %"PRIu64"\n", asf_file->play_duration);
  lprintf("  send_duration:                     %"PRIu64"\n", asf_file->send_duration);
  lprintf("  preroll:                           %"PRIu64"\n", asf_file->preroll);
  lprintf("  broadcast_flag:                    %d\n", asf_file->broadcast_flag);
  lprintf("  seekable_flag:                     %d\n", asf_file->seekable_flag);
  lprintf("  packet_size:                       %"PRIu32"\n", asf_file->packet_size);
  lprintf("  max_bitrate:                       %"PRIu32"\n", asf_file->max_bitrate);
  return 1;
}

static void asf_header_delete_stream_properties(asf_stream_t *asf_stream) {
  if (asf_stream) {
    if (asf_stream->private_data)
      free(asf_stream->private_data);
    if (asf_stream->error_correction_data)
      free(asf_stream->error_correction_data);
    free(asf_stream);
  }
}

static int asf_header_parse_stream_properties(asf_header_t *header, uint8_t *buffer, int buffer_len) {
  asf_reader_t reader;
  uint16_t flags = 0;
  uint32_t junk;
  uint8_t guid[16];
  asf_stream_t *asf_stream = NULL;
  int stream_id;

  if (buffer_len < 54)
    goto exit_error;

  if (! (asf_stream = malloc(sizeof(asf_stream_t))) )
    goto exit_error;

  asf_stream->private_data = NULL;
  asf_stream->error_correction_data = NULL;

  asf_reader_init(&reader, buffer, buffer_len);

  asf_reader_get_guid (&reader, guid);
  asf_stream->stream_type = asf_guid_2_num (guid);
  asf_reader_get_guid (&reader, guid);
  asf_stream->error_correction_type = asf_guid_2_num (guid);

  asf_reader_get_64(&reader, &asf_stream->time_offset);
  asf_reader_get_32(&reader, &asf_stream->private_data_length);
  asf_reader_get_32(&reader, &asf_stream->error_correction_data_length);

  asf_reader_get_16(&reader, &flags);
  asf_stream->stream_number = flags & 0x7F;
  asf_stream->encrypted_flag = flags >> 15;

  asf_reader_get_32(&reader, &junk);

  asf_stream->private_data = asf_reader_get_bytes(&reader, asf_stream->private_data_length);
  if (!asf_stream->private_data)
    goto exit_error;

  asf_stream->error_correction_data = asf_reader_get_bytes(&reader, asf_stream->error_correction_data_length);
  if (!asf_stream->error_correction_data)
    goto exit_error;

  lprintf("Stream_properties\n");
  lprintf("  stream_number:                     %d\n", asf_stream->stream_number);
  lprintf("  stream_type:                       %s\n", asf_guid_name (asf_stream->stream_type));
  lprintf("  error_correction_type:             %s\n", asf_guid_name (asf_stream->error_correction_type));
  lprintf("  time_offset:                       %"PRIu64"\n", asf_stream->time_offset);
  lprintf("  private_data_length:               %"PRIu32"\n", asf_stream->private_data_length);
  lprintf("  error_correction_data_length:      %"PRIu32"\n", asf_stream->error_correction_data_length);
  lprintf("  encrypted_flag:                    %d\n", asf_stream->encrypted_flag);

  stream_id = asf_header_get_stream_id(header, asf_stream->stream_number);
  if (stream_id >= 0) {
    header->streams[stream_id] = asf_stream;
    header->stream_count++;
  } else {
    asf_header_delete_stream_properties(asf_stream);
  }
  return 1;

exit_error:
  asf_header_delete_stream_properties(asf_stream);
  return 0;
}

static void asf_header_delete_stream_extended_properties(asf_stream_extension_t *asf_stream_extension) {
  int i;

  if (asf_stream_extension->stream_name_count > 0) {
    for (i = 0; i < asf_stream_extension->stream_name_count; i++) {
      free(asf_stream_extension->stream_names[i]);
    }
    free(asf_stream_extension->stream_names);
  }
  free(asf_stream_extension);
}

static int asf_header_parse_stream_extended_properties(asf_header_t *header, uint8_t *buffer, int buffer_len) {
  asf_reader_t reader;
  uint32_t flags = 0;
  uint16_t stream_number = 0;
  int i;
  int stream_id;
  asf_stream_extension_t *asf_stream_extension;

  if (buffer_len < 64)
    return 0;

  if (! (asf_stream_extension = malloc(sizeof(asf_stream_extension_t))) )
    return 0;

  asf_reader_init(&reader, buffer, buffer_len);

  asf_reader_get_64(&reader, &asf_stream_extension->start_time);
  asf_reader_get_64(&reader, &asf_stream_extension->end_time);

  asf_reader_get_32(&reader, &asf_stream_extension->data_bitrate);
  asf_reader_get_32(&reader, &asf_stream_extension->buffer_size);
  asf_reader_get_32(&reader, &asf_stream_extension->initial_buffer_fullness);
  asf_reader_get_32(&reader, &asf_stream_extension->alternate_data_bitrate);
  asf_reader_get_32(&reader, &asf_stream_extension->alternate_buffer_size);
  asf_reader_get_32(&reader, &asf_stream_extension->alternate_initial_buffer_fullness);
  asf_reader_get_32(&reader, &asf_stream_extension->max_object_size);

  /* 4 flags */
  asf_reader_get_32(&reader, &flags);
  asf_stream_extension->reliable_flag = flags  & 1;
  asf_stream_extension->seekable_flag = (flags >> 1) & 1;
  asf_stream_extension->no_cleanpoints_flag = (flags >> 2) & 1;
  asf_stream_extension->resend_live_cleanpoints_flag = (flags >> 3) & 1;

  asf_reader_get_16(&reader, &stream_number);

  asf_reader_get_16(&reader, &asf_stream_extension->language_id);
  asf_reader_get_64(&reader, &asf_stream_extension->average_time_per_frame);

  asf_reader_get_16(&reader, &asf_stream_extension->stream_name_count);
  asf_reader_get_16(&reader, &asf_stream_extension->payload_extension_system_count);

  /* get stream names */
  if (asf_stream_extension->stream_name_count) {
    asf_stream_extension->stream_names = malloc (asf_stream_extension->stream_name_count * sizeof(void*));
    for (i = 0; i < asf_stream_extension->stream_name_count; i++) {
      uint16_t lang_index, length = 0;
      asf_reader_get_16(&reader, &lang_index);
      asf_reader_get_16(&reader, &length);
      asf_stream_extension->stream_names[i] = (char*)asf_reader_get_bytes(&reader, length); /* store them */
    }
  }

  /* skip payload extensions */
  if (asf_stream_extension->payload_extension_system_count) {
    for (i = 0; i < asf_stream_extension->payload_extension_system_count; i++) {
      uint32_t length = 0;
      asf_reader_skip (&reader, 16 + 2); /* guid, data_size */
      asf_reader_get_32(&reader, &length);
      asf_reader_skip(&reader, length);
    }
  }

  stream_id = asf_header_get_stream_id(header, stream_number);
  if (stream_id >= 0) {
    header->stream_extensions[stream_id] = asf_stream_extension;
  }

  /* embeded stream properties */
  if (asf_reader_get_size(&reader) >= 24) {
    uint8_t guid[16];
    uint64_t object_length = 0;

    asf_reader_get_guid (&reader, guid);
    asf_reader_get_64(&reader, &object_length);

    /* check length validity */
    if (asf_reader_get_size(&reader) == (object_length - 24)) {
      asf_guid_t object_id = asf_guid_2_num (guid);
      switch (object_id) {
        case GUID_ASF_STREAM_PROPERTIES:
          asf_header_parse_stream_properties(header, asf_reader_get_buffer(&reader), object_length - 24);
          break;
        default:
          lprintf ("unexpected object\n");
          break;
      }
    } else {
      lprintf ("invalid object length\n");
    }
  }

  lprintf("Stream extension properties\n");
  lprintf("  stream_number:                     %"PRIu16"\n", stream_number);
  lprintf("  start_time:                        %"PRIu64"\n", asf_stream_extension->start_time);
  lprintf("  end_time:                          %"PRIu64"\n", asf_stream_extension->end_time);
  lprintf("  data_bitrate:                      %"PRIu32"\n", asf_stream_extension->data_bitrate);
  lprintf("  buffer_size:                       %"PRIu32"\n", asf_stream_extension->buffer_size);
  lprintf("  initial_buffer_fullness:           %"PRIu32"\n", asf_stream_extension->initial_buffer_fullness);
  lprintf("  alternate_data_bitrate:            %"PRIu32"\n", asf_stream_extension->alternate_data_bitrate);
  lprintf("  alternate_buffer_size:             %"PRIu32"\n", asf_stream_extension->alternate_buffer_size);
  lprintf("  alternate_initial_buffer_fullness: %"PRIu32"\n", asf_stream_extension->alternate_initial_buffer_fullness);
  lprintf("  max_object_size:                   %"PRIu32"\n", asf_stream_extension->max_object_size);
  lprintf("  language_id:                       %"PRIu16"\n", asf_stream_extension->language_id);
  lprintf("  average_time_per_frame:            %"PRIu64"\n", asf_stream_extension->average_time_per_frame);
  lprintf("  stream_name_count:                 %"PRIu16"\n", asf_stream_extension->stream_name_count);
  lprintf("  payload_extension_system_count:    %"PRIu16"\n", asf_stream_extension->payload_extension_system_count);
  lprintf("  reliable_flag:                     %d\n", asf_stream_extension->reliable_flag);
  lprintf("  seekable_flag:                     %d\n", asf_stream_extension->seekable_flag);
  lprintf("  no_cleanpoints_flag:               %d\n", asf_stream_extension->no_cleanpoints_flag);
  lprintf("  resend_live_cleanpoints_flag:      %d\n", asf_stream_extension->resend_live_cleanpoints_flag);

  if (stream_id < 0) {
    asf_header_delete_stream_extended_properties(asf_stream_extension);
  }

  return 1;
}

static int asf_header_parse_stream_bitrate_properties(asf_header_t *header_pub, uint8_t *buffer, int buffer_len) {
  asf_header_internal_t *header = (asf_header_internal_t *)header_pub;
  asf_reader_t reader;
  uint16_t bitrate_count = 0;
  int i;
  int stream_id;

  if (buffer_len < 2)
    return 0;

  asf_reader_init(&reader, buffer, buffer_len);
  asf_reader_get_16(&reader, &bitrate_count);

  if (buffer_len < (2 + 6 * bitrate_count))
    return 0;

  lprintf ("  bitrate count: %d\n", bitrate_count);

  for(i = 0; i < bitrate_count; i++) {
    uint16_t flags = 0;
    uint32_t bitrate = 0;
    int stream_number;
    uint8_t *bitrate_pointer;

    asf_reader_get_16(&reader, &flags);
    stream_number = flags & 0x7f;

    bitrate_pointer = asf_reader_get_buffer(&reader);
    asf_reader_get_32(&reader, &bitrate);
    lprintf ("  stream num %d, bitrate %"PRIu32"\n", stream_number, bitrate);

    stream_id = asf_header_get_stream_id(&header->pub, stream_number);
    if (stream_id >= 0) {
      header->pub.bitrates[stream_id] = bitrate;
      header->bitrate_pointers[stream_id] = bitrate_pointer;
    }
  }
  return 1;
}

static int asf_header_parse_metadata(asf_header_t *header_pub, uint8_t *buffer, int buffer_len)
{
  asf_header_internal_t *header = (asf_header_internal_t *)header_pub;
  asf_reader_t reader;
  uint16_t i, records_count = 0;
  iconv_t iconv_cd;

  if (buffer_len < 2)
    return 0;

  if ((iconv_cd = iconv_open ("UTF-8", "UCS-2LE")) == (iconv_t)-1)
    return 0;

  asf_reader_init(&reader, buffer, buffer_len);
  asf_reader_get_16(&reader, &records_count);

  for (i = 0; i < records_count; i++)
  {
    uint16_t index, stream = 0, name_len = 0, data_type;
    uint32_t data_len = 0;
    int stream_id;

    asf_reader_get_16 (&reader, &index); 
    asf_reader_get_16 (&reader, &stream); 
    stream &= 0x7f;
    asf_reader_get_16 (&reader, &name_len); 
    asf_reader_get_16 (&reader, &data_type); 
    asf_reader_get_32 (&reader, &data_len); 

    stream_id = asf_header_get_stream_id (&header->pub, stream);

    if (data_len >= 4)
    {
      char *name = asf_reader_get_string (&reader, name_len, iconv_cd);
      if (name && !strcmp (name, "AspectRatioX"))
      {
        asf_reader_get_32 (&reader, &header->pub.aspect_ratios[stream_id].x);
        data_len -= 4;
      }
      else if (name && !strcmp (name, "AspectRatioY"))
      {
        asf_reader_get_32 (&reader, &header->pub.aspect_ratios[stream_id].y);
        data_len -= 4;
      }
      free (name);
      asf_reader_skip (&reader, data_len);
    }
    else
      asf_reader_skip (&reader, data_len + name_len);
  }

  iconv_close (iconv_cd);
  return 1;
}

static int asf_header_parse_header_extension(asf_header_t *header, uint8_t *buffer, int buffer_len) {
  asf_reader_t reader;

  uint32_t data_length;

  if (buffer_len < 22)
    return 0;

  asf_reader_init(&reader, buffer, buffer_len);

  asf_reader_skip (&reader, 16 + 2); /* guid + junk */
  asf_reader_get_32(&reader, &data_length);

  lprintf("parse_asf_header_extension: length: %"PRIu32"\n", data_length);

  while (!asf_reader_eos(&reader)) {

    uint8_t guid[16];
    asf_guid_t object_id;
    uint64_t object_length = 0, object_data_length;

    if (asf_reader_get_size(&reader) < 24) {
      printf("invalid buffer size\n");
      return 0;
    }

    asf_reader_get_guid (&reader, guid);
    asf_reader_get_64(&reader, &object_length);

    object_data_length = object_length - 24;
    object_id = asf_guid_2_num (guid);
    switch (object_id) {
      case GUID_EXTENDED_STREAM_PROPERTIES:
        asf_header_parse_stream_extended_properties(header, asf_reader_get_buffer(&reader), object_data_length);
        break;
      case GUID_METADATA:
        asf_header_parse_metadata(header, asf_reader_get_buffer(&reader), object_data_length);
        break;
      case GUID_ADVANCED_MUTUAL_EXCLUSION:
      case GUID_GROUP_MUTUAL_EXCLUSION:
      case GUID_STREAM_PRIORITIZATION:
      case GUID_BANDWIDTH_SHARING:
      case GUID_LANGUAGE_LIST:
      case GUID_METADATA_LIBRARY:
      case GUID_INDEX_PARAMETERS:
      case GUID_MEDIA_OBJECT_INDEX_PARAMETERS:
      case GUID_TIMECODE_INDEX_PARAMETERS:
      case GUID_ADVANCED_CONTENT_ENCRYPTION:
      case GUID_COMPATIBILITY:
	    case GUID_ASF_PADDING:
        break;
      default:
        lprintf ("unexpected object\n");
        break;
    }
    asf_reader_skip(&reader, object_data_length);
  }
  return 1;
}

static int asf_header_parse_content_description(asf_header_t *header_pub, uint8_t *buffer, int buffer_len) {
  asf_header_internal_t *header = (asf_header_internal_t *)header_pub;
  asf_reader_t reader;
  asf_content_t *content;
  uint16_t title_length = 0, author_length = 0, copyright_length = 0, description_length = 0, rating_length = 0;
  iconv_t iconv_cd;

  if (buffer_len < 10)
    return 0;

  content = calloc(1, sizeof(asf_content_t));
  if (!content)
    return 0;

  if ( (iconv_cd = iconv_open("UTF-8", "UCS-2LE")) == (iconv_t)-1 ) {
    free(content);
    return 0;
  }

  asf_reader_init(&reader, buffer, buffer_len);
  asf_reader_get_16(&reader, &title_length);
  asf_reader_get_16(&reader, &author_length);
  asf_reader_get_16(&reader, &copyright_length);
  asf_reader_get_16(&reader, &description_length);
  asf_reader_get_16(&reader, &rating_length);

  content->title = asf_reader_get_string(&reader, title_length, iconv_cd);
  content->author = asf_reader_get_string(&reader, author_length, iconv_cd);
  content->copyright = asf_reader_get_string(&reader, copyright_length, iconv_cd);
  content->description = asf_reader_get_string(&reader, description_length, iconv_cd);
  content->rating = asf_reader_get_string(&reader, rating_length, iconv_cd);

  lprintf("title: %d chars: \"%s\"\n", title_length, content->title);
  lprintf("author: %d chars: \"%s\"\n", author_length, content->author);
  lprintf("copyright: %d chars: \"%s\"\n", copyright_length, content->copyright);
  lprintf("description: %d chars: \"%s\"\n", description_length, content->description);
  lprintf("rating: %d chars: \"%s\"\n", rating_length, content->rating);

  header->pub.content = content;

  iconv_close(iconv_cd);
  return 1;
}


asf_header_t *asf_header_new (uint8_t *buffer, int buffer_len) {

  asf_header_internal_t *asf_header;
  asf_reader_t reader;
  uint32_t object_count;
  uint16_t junk;

  lprintf("parsing_asf_header\n");
  if (buffer_len < 6) {
    printf("invalid buffer size\n");
    return NULL;
  }

  if (! (asf_header = calloc(1, sizeof(asf_header_internal_t))) )
    return NULL;

  asf_reader_init(&reader, buffer, buffer_len);
  asf_reader_get_32(&reader, &object_count);
  asf_reader_get_16(&reader, &junk);

  while (!asf_reader_eos(&reader)) {

    uint8_t guid[16];
    asf_guid_t object_id;
    uint64_t object_length = 0, object_data_length;

    if (asf_reader_get_size(&reader) < 24) {
      printf("invalid buffer size\n");
      goto exit_error;
    }

    asf_reader_get_guid (&reader, guid);
    asf_reader_get_64(&reader, &object_length);

    object_data_length = object_length - 24;

    object_id = asf_guid_2_num (guid);
    switch (object_id) {
    
      case GUID_ASF_FILE_PROPERTIES:
        asf_header_parse_file_properties(&asf_header->pub, asf_reader_get_buffer(&reader), object_data_length);
        break;

      case GUID_ASF_STREAM_PROPERTIES:
        asf_header_parse_stream_properties(&asf_header->pub, asf_reader_get_buffer(&reader), object_data_length);
        break;

      case GUID_ASF_STREAM_BITRATE_PROPERTIES:
        asf_header_parse_stream_bitrate_properties(&asf_header->pub, asf_reader_get_buffer(&reader), object_data_length);
        break;

	    case GUID_ASF_HEADER_EXTENSION:
        asf_header_parse_header_extension(&asf_header->pub, asf_reader_get_buffer(&reader), object_data_length);
        break;

	    case GUID_ASF_CONTENT_DESCRIPTION:
        asf_header_parse_content_description(&asf_header->pub, asf_reader_get_buffer(&reader), object_data_length);
        break;

	    case GUID_ASF_CODEC_LIST:
	    case GUID_ASF_SCRIPT_COMMAND:
	    case GUID_ASF_MARKER:
	    case GUID_ASF_BITRATE_MUTUAL_EXCLUSION:
	    case GUID_ASF_ERROR_CORRECTION:
	    case GUID_ASF_EXTENDED_CONTENT_DESCRIPTION:
	    case GUID_ASF_EXTENDED_CONTENT_ENCRYPTION:
	    case GUID_ASF_PADDING:
        break;
    
      default:
        lprintf ("unexpected object\n");
        break;
    }
    asf_reader_skip(&reader, object_data_length);
  }

  /* basic checks */
  if (!asf_header->pub.file) {
    lprintf("no file object present\n");
    goto exit_error;
  }
  if (!asf_header->pub.content) {
    lprintf("no content object present\n");
    asf_header->pub.content = calloc(1, sizeof(asf_content_t));
    if (!asf_header->pub.content)
      goto exit_error;
  }

  return &asf_header->pub;

exit_error:
  asf_header_delete(&asf_header->pub);
  return NULL;
}


static void asf_header_delete_file_properties(asf_file_t *asf_file) {
  free(asf_file);
}

static void asf_header_delete_content(asf_content_t *asf_content) {
  if (asf_content->title)
    free(asf_content->title);
  if (asf_content->author)
    free(asf_content->author);
  if (asf_content->copyright)
    free(asf_content->copyright);
  if (asf_content->description)
    free(asf_content->description);
  if (asf_content->rating)
    free(asf_content->rating);
  free(asf_content);
}

void asf_header_delete (asf_header_t *header_pub) {
  asf_header_internal_t *header = (asf_header_internal_t *)header_pub;
  int i;

  if (header->pub.file)
    asf_header_delete_file_properties(header->pub.file);

  if (header->pub.content)
    asf_header_delete_content(header->pub.content);

  for (i = 0; i < ASF_MAX_NUM_STREAMS; i++) {
    if (header->pub.streams[i])
      asf_header_delete_stream_properties(header->pub.streams[i]);
    if (header->pub.stream_extensions[i])
      asf_header_delete_stream_extended_properties(header->pub.stream_extensions[i]);
  }
  
  free(header);
}

/* Given a bandwidth, select the best stream */
static int asf_header_choose_stream (asf_header_internal_t *header, int stream_type,
                                     uint32_t bandwidth) {
  int i;
  int max_lt, min_gt;

  max_lt = min_gt = -1;
  for (i = 0; i < header->pub.stream_count; i++) {
    if (header->pub.streams[i]->stream_type == stream_type) {
      if (header->pub.bitrates[i] <= bandwidth) {
        if ((max_lt == -1) || (header->pub.bitrates[i] > header->pub.bitrates[max_lt]))
          max_lt = i;
      } else {
        if ((min_gt == -1) || (header->pub.bitrates[i] < header->pub.bitrates[min_gt]))
          min_gt = i;
      }
    }
  }

  return (max_lt != -1) ? max_lt : min_gt;
}

void asf_header_choose_streams (asf_header_t *header_pub, uint32_t bandwidth,
                                int *video_id, int *audio_id) {
  asf_header_internal_t *header = (asf_header_internal_t *)header_pub;
  uint32_t bandwidth_left;

  *video_id = *audio_id = -1;
  bandwidth_left = bandwidth;

  lprintf("%d streams, bandwidth %"PRIu32"\n", header->pub.stream_count, bandwidth_left);

  /* choose a video stream adapted to the user bandwidth */
  *video_id = asf_header_choose_stream (header, GUID_ASF_VIDEO_MEDIA, bandwidth_left);
  if (*video_id != -1) {
    if (header->pub.bitrates[*video_id] < bandwidth_left) {
      bandwidth_left -= header->pub.bitrates[*video_id];
    } else {
      bandwidth_left = 0;
    }
    lprintf("selected video stream %d, bandwidth left: %"PRIu32"\n",
      header->pub.streams[*video_id]->stream_number, bandwidth_left);
  } else {
    lprintf("no video stream\n");
  }

  /* choose a audio stream adapted to the user bandwidth */
  *audio_id = asf_header_choose_stream (header, GUID_ASF_AUDIO_MEDIA, bandwidth_left);
  if (*audio_id != -1) {
    if (header->pub.bitrates[*audio_id] < bandwidth_left) {
      bandwidth_left -= header->pub.bitrates[*audio_id];
    } else {
      bandwidth_left = 0;
    }
    lprintf("selected audio stream %d, bandwidth left: %"PRIu32"\n",
      header->pub.streams[*audio_id]->stream_number, bandwidth_left);
  } else {
    lprintf("no audio stream\n");
  }
}

void asf_header_disable_streams (asf_header_t *header_pub, int video_id, int audio_id) {
  asf_header_internal_t *header = (asf_header_internal_t *)header_pub;
  int i;

  for (i = 0; i < header->pub.stream_count; i++) {
    asf_guid_t stream_type = header->pub.streams[i]->stream_type;

    if (((stream_type == GUID_ASF_VIDEO_MEDIA) && (i != video_id)) ||
      ((stream_type == GUID_ASF_AUDIO_MEDIA) && (i != audio_id))) {
      uint8_t *bitrate_pointer = header->bitrate_pointers[i];
      /* disable  the stream */
      lprintf("stream %d disabled\n", header->pub.streams[i]->stream_number);
      *bitrate_pointer++ = 0;
      *bitrate_pointer++ = 0;
      *bitrate_pointer++ = 0;
      *bitrate_pointer = 0;
    }
  }
}

static const asf_guid_t sorted_nums[] = {
  GUID_ERROR,
  GUID_ASF_NO_ERROR_CORRECTION,
  GUID_ASF_JFIF_MEDIA,
  GUID_ASF_MUTEX_BITRATE,
  GUID_ASF_MARKER,
  GUID_ASF_MUTEX_UKNOWN,
  GUID_ASF_RESERVED_1,
  GUID_ASF_EXTENDED_CONTENT_ENCRYPTION,
  GUID_ASF_RESERVED_MARKER,
  GUID_ASF_FILE_TRANSFER_MEDIA,
  GUID_ASF_SCRIPT_COMMAND,
  GUID_ASF_HEADER,
  GUID_ASF_CONTENT_DESCRIPTION,
  GUID_ADVANCED_CONTENT_ENCRYPTION,
  GUID_ASF_ERROR_CORRECTION,
  GUID_ASF_DATA,
  GUID_ASF_CODEC_LIST,
  GUID_GROUP_MUTUAL_EXCLUSION,
  GUID_ASF_AUDIO_MEDIA,
  GUID_ASF_EXTENDED_CONTENT_DESCRIPTION,
  GUID_ASF_AUDIO_CONCEAL_NONE,
  GUID_ASF_CODEC_COMMENT1_HEADER,
  GUID_ASF_AUDIO_SPREAD,
  GUID_STREAM_PRIORITIZATION,
  GUID_COMPATIBILITY,
  GUID_TIMECODE_INDEX_PARAMETERS,
  GUID_ASF_PADDING,
  GUID_ASF_SIMPLE_INDEX,
  GUID_ASF_STREAM_PROPERTIES,
  GUID_METADATA_LIBRARY,
  GUID_ASF_FILE_PROPERTIES,
  GUID_LANGUAGE_LIST,
  GUID_MEDIA_OBJECT_INDEX_PARAMETERS,
  GUID_ASF_HEADER_EXTENSION,
  GUID_ASF_COMMAND_MEDIA,
  GUID_ASF_VIDEO_MEDIA,
  GUID_EXTENDED_STREAM_PROPERTIES,
  GUID_ASF_STREAM_BITRATE_PROPERTIES,
  GUID_ADVANCED_MUTUAL_EXCLUSION,
  GUID_TIMECODE_INDEX,
  GUID_ASF_2_0_HEADER,
  GUID_INDEX,
  GUID_ASF_BITRATE_MUTUAL_EXCLUSION,
  GUID_INDEX_PARAMETERS,
  GUID_ASF_DEGRADABLE_JPEG_MEDIA,
  GUID_ASF_BINARY_MEDIA,
  GUID_ASF_RESERVED_SCRIPT_COMMNAND,
  GUID_BANDWIDTH_SHARING,
  GUID_METADATA,
  GUID_MEDIA_OBJECT_INDEX
};

static const uint8_t sorted_guids[] = {
  0x00,0x00,0x00,0x00, 0x00,0x00, 0x00,0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, /* GUID_ERROR */
  0x00,0x57,0xfb,0x20, 0x55,0x5b, 0xcf,0x11, 0xa8, 0xfd, 0x00, 0x80, 0x5f, 0x5c, 0x44, 0x2b, /* GUID_ASF_NO_ERROR_CORRECTION */
  0x00,0xe1,0x1b,0xb6, 0x4e,0x5b, 0xcf,0x11, 0xa8, 0xfd, 0x00, 0x80, 0x5f, 0x5c, 0x44, 0x2b, /* GUID_ASF_JFIF_MEDIA */
  0x01,0x2a,0xe2,0xd6, 0xda,0x35, 0xd1,0x11, 0x90, 0x34, 0x00, 0xa0, 0xc9, 0x03, 0x49, 0xbe, /* GUID_ASF_MUTEX_BITRATE */
  0x01,0xcd,0x87,0xf4, 0x51,0xa9, 0xcf,0x11, 0x8e, 0xe6, 0x00, 0xc0, 0x0c, 0x20, 0x53, 0x65, /* GUID_ASF_MARKER */
  0x02,0x2a,0xe2,0xd6, 0xda,0x35, 0xd1,0x11, 0x90, 0x34, 0x00, 0xa0, 0xc9, 0x03, 0x49, 0xbe, /* GUID_ASF_MUTEX_UKNOWN */
  0x11,0xd2,0xd3,0xab, 0xba,0xa9, 0xcf,0x11, 0x8e, 0xe6, 0x00, 0xc0, 0x0c, 0x20, 0x53, 0x65, /* GUID_ASF_RESERVED_1 */
  0x14,0xe6,0x8a,0x29, 0x22,0x26, 0x17,0x4c, 0xb9, 0x35, 0xda, 0xe0, 0x7e, 0xe9, 0x28, 0x9c, /* GUID_ASF_EXTENDED_CONTENT_ENCRYPTION */
  0x20,0xdb,0xfe,0x4c, 0xf6,0x75, 0xcf,0x11, 0x9c, 0x0f, 0x00, 0xa0, 0xc9, 0x03, 0x49, 0xcb, /* GUID_ASF_RESERVED_MARKER */
  0x2c,0x22,0xbd,0x91, 0x1c,0xf2, 0x7a,0x49, 0x8b, 0x6d, 0x5a, 0xa8, 0x6b, 0xfc, 0x01, 0x85, /* GUID_ASF_FILE_TRANSFER_MEDIA */
  0x30,0x1a,0xfb,0x1e, 0x62,0x0b, 0xd0,0x11, 0xa3, 0x9b, 0x00, 0xa0, 0xc9, 0x03, 0x48, 0xf6, /* GUID_ASF_SCRIPT_COMMAND */
  0x30,0x26,0xb2,0x75, 0x8e,0x66, 0xcf,0x11, 0xa6, 0xd9, 0x00, 0xaa, 0x00, 0x62, 0xce, 0x6c, /* GUID_ASF_HEADER */
  0x33,0x26,0xb2,0x75, 0x8e,0x66, 0xcf,0x11, 0xa6, 0xd9, 0x00, 0xaa, 0x00, 0x62, 0xce, 0x6c, /* GUID_ASF_CONTENT_DESCRIPTION */
  0x33,0x85,0x05,0x43, 0x81,0x69, 0xe6,0x49, 0x9b, 0x74, 0xad, 0x12, 0xcb, 0x86, 0xd5, 0x8c, /* GUID_ADVANCED_CONTENT_ENCRYPTION */
  0x35,0x26,0xb2,0x75, 0x8e,0x66, 0xcf,0x11, 0xa6, 0xd9, 0x00, 0xaa, 0x00, 0x62, 0xce, 0x6c, /* GUID_ASF_ERROR_CORRECTION */
  0x36,0x26,0xb2,0x75, 0x8e,0x66, 0xcf,0x11, 0xa6, 0xd9, 0x00, 0xaa, 0x00, 0x62, 0xce, 0x6c, /* GUID_ASF_DATA */
  0x40,0x52,0xd1,0x86, 0x1d,0x31, 0xd0,0x11, 0xa3, 0xa4, 0x00, 0xa0, 0xc9, 0x03, 0x48, 0xf6, /* GUID_ASF_CODEC_LIST */
  0x40,0x5a,0x46,0xd1, 0x79,0x5a, 0x38,0x43, 0xb7, 0x1b, 0xe3, 0x6b, 0x8f, 0xd6, 0xc2, 0x49, /* GUID_GROUP_MUTUAL_EXCLUSION */
  0x40,0x9e,0x69,0xf8, 0x4d,0x5b, 0xcf,0x11, 0xa8, 0xfd, 0x00, 0x80, 0x5f, 0x5c, 0x44, 0x2b, /* GUID_ASF_AUDIO_MEDIA */
  0x40,0xa4,0xd0,0xd2, 0x07,0xe3, 0xd2,0x11, 0x97, 0xf0, 0x00, 0xa0, 0xc9, 0x5e, 0xa8, 0x50, /* GUID_ASF_EXTENDED_CONTENT_DESCRIPTION */
  0x40,0xa4,0xf1,0x49, 0xce,0x4e, 0xd0,0x11, 0xa3, 0xac, 0x00, 0xa0, 0xc9, 0x03, 0x48, 0xf6, /* GUID_ASF_AUDIO_CONCEAL_NONE */
  0x41,0x52,0xd1,0x86, 0x1d,0x31, 0xd0,0x11, 0xa3, 0xa4, 0x00, 0xa0, 0xc9, 0x03, 0x48, 0xf6, /* GUID_ASF_CODEC_COMMENT1_HEADER */
  0x50,0xcd,0xc3,0xbf, 0x8f,0x61, 0xcf,0x11, 0x8b, 0xb2, 0x00, 0xaa, 0x00, 0xb4, 0xe2, 0x20, /* GUID_ASF_AUDIO_SPREAD */
  0x5b,0xd1,0xfe,0xd4, 0xd3,0x88, 0x4f,0x45, 0x81, 0xf0, 0xed, 0x5c, 0x45, 0x99, 0x9e, 0x24, /* GUID_STREAM_PRIORITIZATION */
  0x5d,0x8b,0xf1,0x26, 0x84,0x45, 0xec,0x47, 0x9f, 0x5f, 0x0e, 0x65, 0x1f, 0x04, 0x52, 0xc9, /* GUID_COMPATIBILITY */
  0x6d,0x49,0x5e,0xf5, 0x97,0x97, 0x5d,0x4b, 0x8c, 0x8b, 0x60, 0x4d, 0xf9, 0x9b, 0xfb, 0x24, /* GUID_TIMECODE_INDEX_PARAMETERS */
  0x74,0xd4,0x06,0x18, 0xdf,0xca, 0x09,0x45, 0xa4, 0xba, 0x9a, 0xab, 0xcb, 0x96, 0xaa, 0xe8, /* GUID_ASF_PADDING */
  0x90,0x08,0x00,0x33, 0xb1,0xe5, 0xcf,0x11, 0x89, 0xf4, 0x00, 0xa0, 0xc9, 0x03, 0x49, 0xcb, /* GUID_ASF_SIMPLE_INDEX */
  0x91,0x07,0xdc,0xb7, 0xb7,0xa9, 0xcf,0x11, 0x8e, 0xe6, 0x00, 0xc0, 0x0c, 0x20, 0x53, 0x65, /* GUID_ASF_STREAM_PROPERTIES */
  0x94,0x1c,0x23,0x44, 0x98,0x94, 0xd1,0x49, 0xa1, 0x41, 0x1d, 0x13, 0x4e, 0x45, 0x70, 0x54, /* GUID_METADATA_LIBRARY */
  0xa1,0xdc,0xab,0x8c, 0x47,0xa9, 0xcf,0x11, 0x8e, 0xe4, 0x00, 0xc0, 0x0c, 0x20, 0x53, 0x65, /* GUID_ASF_FILE_PROPERTIES */
  0xa9,0x46,0x43,0x7c, 0xe0,0xef, 0xfc,0x4b, 0xb2, 0x29, 0x39, 0x3e, 0xde, 0x41, 0x5c, 0x85, /* GUID_LANGUAGE_LIST */
  0xad,0x3b,0x20,0x6b, 0x11,0x3f, 0xe4,0x48, 0xac, 0xa8, 0xd7, 0x61, 0x3d, 0xe2, 0xcf, 0xa7, /* GUID_MEDIA_OBJECT_INDEX_PARAMETERS */
  0xb5,0x03,0xbf,0x5f, 0x2e,0xa9, 0xcf,0x11, 0x8e, 0xe3, 0x00, 0xc0, 0x0c, 0x20, 0x53, 0x65, /* GUID_ASF_HEADER_EXTENSION */
  0xc0,0xcf,0xda,0x59, 0xe6,0x59, 0xd0,0x11, 0xa3, 0xac, 0x00, 0xa0, 0xc9, 0x03, 0x48, 0xf6, /* GUID_ASF_COMMAND_MEDIA */
  0xc0,0xef,0x19,0xbc, 0x4d,0x5b, 0xcf,0x11, 0xa8, 0xfd, 0x00, 0x80, 0x5f, 0x5c, 0x44, 0x2b, /* GUID_ASF_VIDEO_MEDIA */
  0xcb,0xa5,0xe6,0x14, 0x72,0xc6, 0x32,0x43, 0x83, 0x99, 0xa9, 0x69, 0x52, 0x06, 0x5b, 0x5a, /* GUID_EXTENDED_STREAM_PROPERTIES */
  0xce,0x75,0xf8,0x7b, 0x8d,0x46, 0xd1,0x11, 0x8d, 0x82, 0x00, 0x60, 0x97, 0xc9, 0xa2, 0xb2, /* GUID_ASF_STREAM_BITRATE_PROPERTIES */
  0xcf,0x49,0x86,0xa0, 0x75,0x47, 0x70,0x46, 0x8a, 0x16, 0x6e, 0x35, 0x35, 0x75, 0x66, 0xcd, /* GUID_ADVANCED_MUTUAL_EXCLUSION */
  0xd0,0x3f,0xb7,0x3c, 0x4a,0x0c, 0x03,0x48, 0x95, 0x3d, 0xed, 0xf7, 0xb6, 0x22, 0x8f, 0x0c, /* GUID_TIMECODE_INDEX */
  0xd1,0x29,0xe2,0xd6, 0xda,0x35, 0xd1,0x11, 0x90, 0x34, 0x00, 0xa0, 0xc9, 0x03, 0x49, 0xbe, /* GUID_ASF_2_0_HEADER */
  0xd3,0x29,0xe2,0xd6, 0xda,0x35, 0xd1,0x11, 0x90, 0x34, 0x00, 0xa0, 0xc9, 0x03, 0x49, 0xbe, /* GUID_INDEX */
  0xdc,0x29,0xe2,0xd6, 0xda,0x35, 0xd1,0x11, 0x90, 0x34, 0x00, 0xa0, 0xc9, 0x03, 0x49, 0xbe, /* GUID_ASF_BITRATE_MUTUAL_EXCLUSION */
  0xdf,0x29,0xe2,0xd6, 0xda,0x35, 0xd1,0x11, 0x90, 0x34, 0x00, 0xa0, 0xc9, 0x03, 0x49, 0xbe, /* GUID_INDEX_PARAMETERS */
  0xe0,0x7d,0x90,0x35, 0x15,0xe4, 0xcf,0x11, 0xa9, 0x17, 0x00, 0x80, 0x5f, 0x5c, 0x44, 0x2b, /* GUID_ASF_DEGRADABLE_JPEG_MEDIA */
  0xe2,0x65,0xfb,0x3a, 0xef,0x47, 0xf2,0x40, 0xac, 0x2c, 0x70, 0xa9, 0x0d, 0x71, 0xd3, 0x43, /* GUID_ASF_BINARY_MEDIA */
  0xe3,0xcb,0x1a,0x4b, 0x0b,0x10, 0xd0,0x11, 0xa3, 0x9b, 0x00, 0xa0, 0xc9, 0x03, 0x48, 0xf6, /* GUID_ASF_RESERVED_SCRIPT_COMMNAND */
  0xe6,0x09,0x96,0xa6, 0x7b,0x51, 0xd2,0x11, 0xb6, 0xaf, 0x00, 0xc0, 0x4f, 0xd9, 0x08, 0xe9, /* GUID_BANDWIDTH_SHARING */
  0xea,0xcb,0xf8,0xc5, 0xaf,0x5b, 0x77,0x48, 0x84, 0x67, 0xaa, 0x8c, 0x44, 0xfa, 0x4c, 0xca, /* GUID_METADATA */
  0xf8,0x03,0xb1,0xfe, 0xad,0x12, 0x64,0x4c, 0x84, 0x0f, 0x2a, 0x1d, 0x2f, 0x7a, 0xd4, 0x8c, /* GUID_MEDIA_OBJECT_INDEX */
};

asf_guid_t asf_guid_2_num (const uint8_t *guid) {
  int b = 0, m = -1, l, e = sizeof (sorted_guids) >> 4;
  do {
    const uint8_t *p, *q;
    int i;
    l = m;
    m = (b + e) >> 1;
    p = sorted_guids + (m << 4);
    q = guid;
    i = 16;
    do {
      int d = (int)*q++ - (int)*p++;
      if (d < 0) {
        e = m;
        break;
      } else if (d > 0) {
        b = m;
        break;
      }
      i--;
    } while (i);
    if (!i)
      return sorted_nums[m];
  } while (m != l);
  return sorted_nums[0];
}

static const char tab_hex[16] = "0123456789abcdef";

void asf_guid_2_str (uint8_t *str, const uint8_t *guid) {
  *str++ = tab_hex[guid[3] >> 4];
  *str++ = tab_hex[guid[3] & 15];
  *str++ = tab_hex[guid[2] >> 4];
  *str++ = tab_hex[guid[2] & 15];
  *str++ = tab_hex[guid[1] >> 4];
  *str++ = tab_hex[guid[1] & 15];
  *str++ = tab_hex[guid[0] >> 4];
  *str++ = tab_hex[guid[0] & 15];
  *str++ = '-';
  *str++ = tab_hex[guid[5] >> 4];
  *str++ = tab_hex[guid[5] & 15];
  *str++ = tab_hex[guid[4] >> 4];
  *str++ = tab_hex[guid[4] & 15];
  *str++ = '-';
  *str++ = tab_hex[guid[7] >> 4];
  *str++ = tab_hex[guid[7] & 15];
  *str++ = tab_hex[guid[6] >> 4];
  *str++ = tab_hex[guid[6] & 15];
  *str++ = '-';
  guid += 8;
  int i = 8;
  while (i > 0) {
    *str++ = tab_hex[guid[0] >> 4];
    *str++ = tab_hex[guid[0] & 15];
    guid++;
    i--;
  }
  *str = 0;
}


static const char * guid_names[] = {
  "error",
  /* base ASF objects */
  "header",
  "data",
  "simple index",
  "index",
  "media object index",
  "timecode index",
  /* header ASF objects */
  "file properties",
  "stream header",
  "header extension",
  "codec list",
  "script command",
  "marker",
  "bitrate mutual exclusion",
  "error correction",
  "content description",
  "extended content description",
  "stream bitrate properties", /* (http://get.to/sdp) */
  "extended content encryption",
  "padding",
  /* stream properties object stream type */
  "audio media",
  "video media",
  "command media",
  "JFIF media (JPEG)",
  "Degradable JPEG media",
  "File Transfer media",
  "Binary media",
  /* stream properties object error correction */
  "no error correction",
  "audio spread",
  /* mutual exclusion object exlusion type */
  "mutex bitrate",
  "mutex unknown",
  /* header extension */
  "reserved_1",
  /* script command */
  "reserved script command",
  /* marker object */
  "reserved marker",
  /* various */
  "audio conceal none",
  "codec comment1 header",
  "asf 2.0 header",
  /* header extension GUIDs */ 
  "extended stream properties",
  "advanced mutual exclusion",
  "group mutual exclusion",
  "stream prioritization",
  "bandwidth sharing",
  "language list",
  "metadata",
  "metadata library",
  "index parameters",
  "media object index parameters",
  "timecode index parameters",
  "advanced content encryption",
  /* exotic stuff */
  "compatibility",
};

const char *asf_guid_name (asf_guid_t num) {
  if ((num < 0) || (num >= GUID_END))
    num = GUID_ERROR;
  return guid_names[num];
}

