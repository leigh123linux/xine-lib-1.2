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
 * ID3 tag parser
 *
 * Supported versions: v1, v1.1, v2.2, v2.3, v2.4
 * TODO:
 *   v2.2:  unsynchronize
 *   v2.3:  unsynchronize
 *          unzip support
 *   v2.4:  unsynchronize
 *          unzip support
 *
 * ID3v2 specs: http://www.id3.org/
 *
 * $Id: id3.c,v 1.13 2007/03/03 00:58:52 dgp85 Exp $
 */
 
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#define LOG_MODULE "id3"
#define LOG_VERBOSE
/*
#define LOG
*/

#include "xine_internal.h"
#include "xineutils.h"
#include "bswap.h"
#include "id3.h"

#define ID3_GENRE_COUNT 126
static const char* const id3_genre[] =
  {"Blues", "Classic Rock", "Country", "Dance", "Disco",
   "Funk", "Grunge", "Hip-Hop", "Jazz", "Metal",
   "New Age", "Oldies", "Other", "Pop", "R&B",
   "Rap", "Reggae", "Rock", "Techno", "Industrial",
   "Alternative", "Ska", "Death Metal", "Pranks", "Soundtrack",
   "Euro-Techno", "Ambient", "Trip-Hop", "Vocal", "Jazz+Funk",
   "Fusion", "Trance", "Classical", "Instrumental", "Acid",
   "House", "Game", "Sound Clip", "Gospel", "Noise",
   "AlternRock", "Bass", "Soul", "Punk", "Space",
   "Meditative", "Instrumental Pop", "Instrumental Rock", "Ethnic", "Gothic",
   "Darkwave", "Techno-Industrial", "Electronic", "Pop-Folk", "Eurodance",
   "Dream", "Southern Rock", "Comedy", "Cult", "Gangsta",
   "Top 40", "Christian Rap", "Pop/Funk", "Jungle", "Native American",
   "Cabaret", "New Wave", "Psychadelic", "Rave", "Showtunes",
   "Trailer", "Lo-Fi", "Tribal", "Acid Punk", "Acid Jazz",
   "Polka", "Retro", "Musical", "Rock & Roll", "Hard Rock",
   "Folk", "Folk-Rock", "National Folk", "Swing", "Fast Fusion",
   "Bebob", "Latin", "Revival", "Celtic", "Bluegrass",
   "Avantgarde", "Gothic Rock", "Progressive Rock", "Psychedelic Rock", "Symphonic Rock",
   "Slow Rock", "Big Band", "Chorus", "Easy Listening", "Acoustic",
   "Humour", "Speech", "Chanson", "Opera", "Chamber Music",
   "Sonata", "Symphony", "Booty Bass", "Primus", "Porn Groove",
   "Satire", "Slow Jam", "Club", "Tango", "Samba",
   "Folklore", "Ballad", "Power Ballad", "Rhythmic Soul", "Freestyle",
   "Duet", "Punk Rock", "Drum Solo", "A capella", "Euro-House",
   "Dance Hall" };

#define ID3_ENCODING_COUNT 4
static const char* const id3_encoding[] = {
   "ISO-8859-1",   /* 0x00 */
   "UTF-16",       /* 0x01 */
   "UTF-16BE",     /* 0x02 */
   "UTF-8"};       /* 0x03 */
   
int id3v1_parse_tag (input_plugin_t *input, xine_stream_t *stream) {

  off_t len;
  id3v1_tag_t tag;
  char track[4];

  /* id3v1 */
  len = input->read (input, (char *)&tag, 128);

  if (len == 128) {

    if ( (tag.tag[0]=='T') && (tag.tag[1]=='A') && (tag.tag[2]=='G') ) {

      lprintf("id3v1 tag found\n");
      _x_meta_info_n_set(stream, XINE_META_INFO_TITLE, tag.title, 30);
      _x_meta_info_n_set(stream, XINE_META_INFO_ARTIST, tag.artist, 30);
      _x_meta_info_n_set(stream, XINE_META_INFO_ALBUM, tag.album, 30);
      _x_meta_info_n_set(stream, XINE_META_INFO_COMMENT, tag.comment, 30);

      /* check for a track number: ID3v1.1, which is a clever hack on ID3v1 */
      if (tag.comment[28] == 0 && tag.comment[29] != 0) {
        snprintf(track, 4, "%d", (unsigned char)tag.comment[29]);
        _x_meta_info_set(stream, XINE_META_INFO_TRACK_NUMBER, track);
      }

      if (tag.genre < ID3_GENRE_COUNT) {
        _x_meta_info_set(stream, XINE_META_INFO_GENRE, id3_genre[tag.genre]);
      }
      
    }
    return 1;
  } else {
    return 0;
  }
}

/* id3v2 "genre" parsing code. what a ugly format ! */
static int id3v2_parse_genre(char* dest, char *src, int len) {
  int state = 0;
  char *buf = dest;
  unsigned int index = 0;
  
  while (*src) {
    lprintf("state=%d\n", state);
    if ((buf - dest) >= len)
      return 0;
      
    switch (state) {
      case 0:
        /* initial state */
        if (*src == '(') {
          state = 1;
          index = 0;
          src++;
        } else {
          *buf = *src;
          buf++; src++;
        }
        break;
      case 1:
        /* ( */
        if (*src == '(') {
          *buf = *src;
          buf++; src++;
          state = 0;
        } else if (*src == 'R') {
          src++;
          state = 2;
        } else if (*src == 'C') {
          src++;
          state = 3;
        } else if ((*src >= '0') && (*src <= '9')) {
          index = 10 * index + (*src - '0');
          src++;
        } else if (*src == ')') {
          if (index < ID3_GENRE_COUNT) {
            strncpy(buf, id3_genre[index], len - (buf - dest));
            buf += strlen(id3_genre[index]);
          } else {
            lprintf("invalid index: %d\n", index);
          }
          src++;
          state = 0;
        } else {
          lprintf("parsing error\n");
          return 0;
        }
        break;
      case 2:
        /* (R */
        if (*src == 'X') {
          src++;
          state = 4;
        } else {
          lprintf("parsing error\n");
          return 0;
        }
        break;
      case 3:
        /* (C */
        if (*src == 'R') {
          strncpy(dest, id3_genre[index], len - (buf - dest));
          buf += strlen(id3_genre[index]);
          src++;
          state = 5;
        } else {
          lprintf("parsing error\n");
          return 0;
        }
        break;
      case 4:
        /* (RX */
        if (*src == ')') {
          strncpy(dest, "Remix", len - (buf - dest));
          buf += strlen("Remix");
          src++;
          state = 0;
        } else {
          lprintf("parsing error\n");
          return 0;
        }
        break;
      case 5:
        /* (CR */
        if (*src == ')') {
          strncpy(dest, "Cover", len - (buf - dest));
          buf += strlen("Cover");
          src++;
          state = 0;
        } else {
          lprintf("parsing error\n");
          return 0;
        }
        break;
    }
  }
  if ((buf - dest) >= len) {
    return 0;
  } else {
    *buf = '\0';
  }
  return 1;
}

static int id3v2_parse_header(input_plugin_t *input, uint8_t *mp3_frame_header,
                              id3v2_header_t *tag_header) {
  uint8_t buf[6];

  tag_header->id = BE_32(mp3_frame_header);
  if (input->read (input, buf, 6) == 6) {
    tag_header->revision = buf[0];
    tag_header->flags    = buf[1];
    tag_header->size     = BE_32_synchsafe(&buf[2]);

    lprintf("tag: ID3 v2.%d.%d\n", mp3_frame_header[3], tag_header->revision);
    lprintf("flags: %d\n", tag_header->flags);
    lprintf("size: %d\n", tag_header->size);
    return 1;
  } else {
    return 0;
  }
}

/* id3 v2.2 */

static int id3v22_parse_frame_header(input_plugin_t *input,
                                     id3v22_frame_header_t *frame_header) {
  uint8_t buf[ID3V22_FRAME_HEADER_SIZE];
  int len;

  len  = input->read (input, buf, ID3V22_FRAME_HEADER_SIZE);
  if (len == ID3V22_FRAME_HEADER_SIZE) {
    frame_header->id   = (buf[0] << 16) + (buf[1] << 8) + buf[2];

    frame_header->size = BE_24_synchsafe(&buf[3]);

    lprintf("frame: %c%c%c: size: %d\n", buf[0], buf[1], buf[2],
            frame_header->size);

    return 1;
  } else {
    return 0;
  }
}

static int id3v22_interp_frame(input_plugin_t *input,
                               xine_stream_t *stream,
                               id3v22_frame_header_t *frame_header) {
  char *buf;
  int enc;
  
  buf = malloc(frame_header->size + 1);
  if (buf == NULL) {
    lprintf("malloc error");
    return 0;
  }

  if (input->read (input, buf, frame_header->size) == frame_header->size) {
    buf[frame_header->size] = 0;
    enc = buf[0];
    if( enc >= ID3_ENCODING_COUNT )
      enc = 0;

    switch (frame_header->id) {
      case ( FOURCC_TAG(0, 'T', 'C', 'O') ):
        {
          char tmp[1024];
          
          if (id3v2_parse_genre(tmp, buf + 1, 1024)) {
            _x_meta_info_set(stream, XINE_META_INFO_GENRE, tmp);
          }
        }
        break;

      case ( FOURCC_TAG(0, 'T', 'T', '2') ):
        _x_meta_info_set_generic(stream, XINE_META_INFO_TITLE, buf + 1, id3_encoding[enc]);
        break;

      case ( FOURCC_TAG(0, 'T', 'P', '1') ):
        _x_meta_info_set_generic(stream, XINE_META_INFO_ARTIST, buf + 1, id3_encoding[enc]);
        break;

      case ( FOURCC_TAG(0, 'T', 'A', 'L') ):
        _x_meta_info_set_generic(stream, XINE_META_INFO_ALBUM, buf + 1, id3_encoding[enc]);
        break;

      case ( FOURCC_TAG(0, 'T', 'Y', 'E') ):
        _x_meta_info_set_generic(stream, XINE_META_INFO_YEAR, buf + 1, id3_encoding[enc]);
        break;

      case ( FOURCC_TAG(0, 'C', 'O', 'M') ):
        _x_meta_info_set_generic(stream, XINE_META_INFO_COMMENT, buf + 1 + 3, id3_encoding[enc]);
        break;

      case ( FOURCC_TAG(0, 'T', 'R', 'K') ):
        _x_meta_info_set(stream, XINE_META_INFO_TRACK_NUMBER, buf + 1);
        break;

      default:
        lprintf("unhandled frame\n");
    }

    free(buf);
    return 1;
  } else {
    lprintf("read error\n");
    free(buf);
    return 0;
  }
}


int id3v22_parse_tag(input_plugin_t *input,
                     xine_stream_t *stream,
                     int8_t *mp3_frame_header) {
  id3v2_header_t tag_header;
  id3v22_frame_header_t tag_frame_header;
  int pos = 0;

  if (id3v2_parse_header(input, mp3_frame_header, &tag_header)) {

    if (tag_header.flags & ID3V22_ZERO_FLAG) {
      /* invalid flags */
      xprintf(stream->xine, XINE_VERBOSITY_DEBUG, 
              "id3: invalid header flags (%02x)\n", tag_header.flags);
      input->seek (input, tag_header.size - pos, SEEK_CUR);
      return 0;
    }
    if (tag_header.flags & ID3V22_COMPRESS_FLAG) {
      /* compressed tag: not supported */
      xprintf(stream->xine, XINE_VERBOSITY_DEBUG, 
              "id3: compressed tags are not supported\n");
      input->seek (input, tag_header.size - pos, SEEK_CUR);
      return 0;
    }
    if (tag_header.flags & ID3V22_UNSYNCH_FLAG) {
      /* unsynchronized tag: not supported */
      xprintf(stream->xine, XINE_VERBOSITY_DEBUG, 
              "id3: unsynchronized tags are not supported\n");
      input->seek (input, tag_header.size - pos, SEEK_CUR);
      return 0;
    }
    /* frame parsing */
    while ((pos + ID3V22_FRAME_HEADER_SIZE) <= tag_header.size) {
      if (id3v22_parse_frame_header(input, &tag_frame_header)) {
        pos += ID3V22_FRAME_HEADER_SIZE;
        if (tag_frame_header.id && tag_frame_header.size) {
          if ((pos + tag_frame_header.size) <= tag_header.size) {
            if (!id3v22_interp_frame(input, stream, &tag_frame_header)) {
              xprintf(stream->xine, XINE_VERBOSITY_DEBUG, 
                      "id3: invalid frame content\n");
            }
          } else {
            xprintf(stream->xine, XINE_VERBOSITY_DEBUG, 
                    "id3: invalid frame header\n");
            input->seek (input, tag_header.size - pos, SEEK_CUR);
            return 1;
          }
          pos += tag_frame_header.size;
        } else {
          /* end of frames, the rest is padding */
          input->seek (input, tag_header.size - pos, SEEK_CUR);
          return 1;
        }
      } else {
        xprintf(stream->xine, XINE_VERBOSITY_DEBUG, 
                "id3: id3v2_parse_frame_header problem\n");
        return 0;
      }
    }
    return 1;
  } else {
    xprintf(stream->xine, XINE_VERBOSITY_DEBUG, "id3: id3v2_parse_header problem\n");
    return 0;
  }
}

/* id3 v2.3 */

static int id3v23_parse_frame_header(input_plugin_t *input,
                                     id3v23_frame_header_t *frame_header) {
  uint8_t buf[ID3V23_FRAME_HEADER_SIZE];
  int len;

  len  = input->read (input, buf, ID3V23_FRAME_HEADER_SIZE);
  if (len == ID3V23_FRAME_HEADER_SIZE) {
    frame_header->id    = BE_32(buf);
    frame_header->size  = BE_32(&buf[4]);
    frame_header->flags = BE_16(buf + 8);

    lprintf("frame: %c%c%c%c, size: %d, flags: %X\n", buf[0], buf[1], buf[2], buf[3],
            frame_header->size, frame_header->flags);

    return 1;
  } else {
    return 0;
  }
}

static int id3v23_parse_frame_ext_header(input_plugin_t *input,
                                         id3v23_frame_ext_header_t *frame_ext_header) {
  uint8_t buf[14];

  if (input->read (input, buf, 4) == 4) {
  
    frame_ext_header->size  = BE_32_synchsafe(&buf[0]);
    
    if (frame_ext_header->size == 6) {
      if (input->read (input, buf + 4, 6) == 6) {
        frame_ext_header->flags = BE_16(buf + 4);
        frame_ext_header->padding_size = BE_32(buf + 6);
        frame_ext_header->crc = 0;
      } else {
        return 0;
      }        

    } else if (frame_ext_header->size == 10) {
      if (input->read (input, buf + 4, 10) == 10) {
        frame_ext_header->flags = BE_16(buf + 4);
        frame_ext_header->padding_size = BE_32(buf + 6);
        frame_ext_header->crc = BE_32(buf + 10);
      } else {
        return 0;
      }

    } else {
      lprintf("invalid ext header size: %d\n", frame_ext_header->size);
      return 0;
    }

    lprintf("ext header: size: %d, flags: %X, padding_size: %d, crc: %d\n",
            frame_ext_header->size, frame_ext_header->flags,
            frame_ext_header->padding_size, frame_ext_header->crc);
    return 1;
  } else {
    return 0;
  }
}

static int id3v23_interp_frame(input_plugin_t *input,
                               xine_stream_t *stream,
                               id3v23_frame_header_t *frame_header) {
  char *buf;
  int enc;

  buf = malloc(frame_header->size + 1);
  if (buf == NULL) {
    lprintf("malloc error");
    return 0;
  }

  if (input->read (input, buf, frame_header->size) == frame_header->size) {
    buf[frame_header->size] = 0;
    enc = buf[0];
    if( enc >= ID3_ENCODING_COUNT )
      enc = 0;

    switch (frame_header->id) {
      case ( FOURCC_TAG('T', 'C', 'O', 'N') ):
        {
          char tmp[1024];
          
          if (id3v2_parse_genre(tmp, buf + 1, 1024)) {
            _x_meta_info_set(stream, XINE_META_INFO_GENRE, tmp);
          }
        }
        break;

      case ( FOURCC_TAG('T', 'I', 'T', '2') ):
        _x_meta_info_set_generic(stream, XINE_META_INFO_TITLE, buf + 1, id3_encoding[enc]);
        break;

      case ( FOURCC_TAG('T', 'P', 'E', '1') ):
        _x_meta_info_set_generic(stream, XINE_META_INFO_ARTIST, buf + 1, id3_encoding[enc]);
        break;

      case ( FOURCC_TAG('T', 'A', 'L', 'B') ):
        _x_meta_info_set_generic(stream, XINE_META_INFO_ALBUM, buf + 1, id3_encoding[enc]);
        break;

      case ( FOURCC_TAG('T', 'Y', 'E', 'R') ):
        _x_meta_info_set_generic(stream, XINE_META_INFO_YEAR, buf + 1, id3_encoding[enc]);
        break;

      case ( FOURCC_TAG('C', 'O', 'M', 'M') ):
        _x_meta_info_set_generic(stream, XINE_META_INFO_COMMENT, buf + 1 + 3, id3_encoding[enc]);
        break;

      case ( FOURCC_TAG('T', 'R', 'C', 'K') ):
        _x_meta_info_set(stream, XINE_META_INFO_TRACK_NUMBER, buf + 1);
        break;

      default:
        lprintf("unhandled frame\n");
    }

    free(buf);
    return 1;
  } else {
    lprintf("read error\n");
    free(buf);
    return 0;
  }
}

int id3v23_parse_tag(input_plugin_t *input,
                     xine_stream_t *stream,
                     int8_t *mp3_frame_header) {
  id3v2_header_t tag_header;
  id3v23_frame_header_t tag_frame_header;
  id3v23_frame_ext_header_t tag_frame_ext_header;
  int pos = 0;

  if (id3v2_parse_header(input, mp3_frame_header, &tag_header)) {

    if (tag_header.flags & ID3V23_ZERO_FLAG) {
      /* invalid flags */
      xprintf(stream->xine, XINE_VERBOSITY_DEBUG, 
              "id3: invalid header flags (%02x)\n", tag_header.flags);
      input->seek (input, tag_header.size - pos, SEEK_CUR);
      return 0;
    }
    if (tag_header.flags & ID3V23_UNSYNCH_FLAG) {
      /* unsynchronized tag: not supported */
      xprintf(stream->xine, XINE_VERBOSITY_DEBUG, 
              "id3: unsynchronized tags are not supported\n");
      input->seek (input, tag_header.size - pos, SEEK_CUR);
      return 0;
    }
    if (tag_header.flags & ID3V23_EXT_HEADER_FLAG) {
      /* extended header */
      if (!id3v23_parse_frame_ext_header(input, &tag_frame_ext_header)) {
        return 0;
      }
    }
    /* frame parsing */
    while ((pos + ID3V23_FRAME_HEADER_SIZE) <= tag_header.size) {
      if (id3v23_parse_frame_header(input, &tag_frame_header)) {
        pos += ID3V23_FRAME_HEADER_SIZE;
        if (tag_frame_header.id && tag_frame_header.size) {
          if ((pos + tag_frame_header.size) <= tag_header.size) {
            if (!id3v23_interp_frame(input, stream, &tag_frame_header)) {
              xprintf(stream->xine, XINE_VERBOSITY_DEBUG, 
                      "id3: invalid frame content\n");
            }
          } else {
            xprintf(stream->xine, XINE_VERBOSITY_DEBUG, 
                    "id3: invalid frame header\n");
            input->seek (input, tag_header.size - pos, SEEK_CUR);
            return 1;
          }
          pos += tag_frame_header.size;
        } else {
          /* end of frames, the rest is padding */
          input->seek (input, tag_header.size - pos, SEEK_CUR);
          return 1;
        }
      } else {
        xprintf(stream->xine, XINE_VERBOSITY_DEBUG, 
                "id3: id3v2_parse_frame_header problem\n");
        return 0;
      }
    }
    return 1;
  } else {
    xprintf(stream->xine, XINE_VERBOSITY_DEBUG, "id3v23: id3v2_parse_header problem\n");
    return 0;
  }
}

/* id3 v2.4 */

/* id3v2 "genre" parsing code. what a ugly format ! */
static int id3v24_parse_genre(char* dest, char *src, int len) {
  int index = 0;
  
  dest[0] = '\0';
  if (sscanf(src, "%2d", &index) == 1) {
    if (index < ID3_GENRE_COUNT) {
      strncpy(dest, id3_genre[index], len);
      dest[len - 1] = '\0';
    } else {
      lprintf("invalid index: %d\n", index);
    }
  }
  return 1;
}

static int id3v24_parse_frame_header(input_plugin_t *input,
                                     id3v24_frame_header_t *frame_header) {
  uint8_t buf[ID3V24_FRAME_HEADER_SIZE];
  int len;

  len  = input->read (input, buf, ID3V24_FRAME_HEADER_SIZE);
  if (len == ID3V24_FRAME_HEADER_SIZE) {
    frame_header->id    = BE_32(buf);
    frame_header->size  = BE_32_synchsafe(&buf[4]);
    frame_header->flags = BE_16(&buf[8]);

    lprintf("frame: %c%c%c%c, size: %d, flags: %X\n", buf[0], buf[1], buf[2], buf[3],
            frame_header->size, frame_header->flags);

    return 1;
  } else {
    return 0;
  }
}

static int id3v24_parse_ext_header(input_plugin_t *input,
                                   id3v24_frame_ext_header_t *frame_ext_header) {
  uint8_t buf[5];

  if (input->read (input, buf, 4) == 4) {
 
    frame_ext_header->size  = BE_32_synchsafe(&buf[0]);

    if (input->read (input, buf, 2) == 2) {
      uint8_t flags_size = buf[0];
      if (flags_size != 1) {
        lprintf(
                "id3: invalid extended header, flag size must be 1\n");
        return 0;
      }
      frame_ext_header->flags = buf[1];
      if (frame_ext_header->flags & ID3V24_EXT_ZERO_FLAG) {
        lprintf(
                "id3: invalid extended header, unknown flags\n");
        return 0;
      }
      /* Update flag */
      if (frame_ext_header->flags & ID3V24_EXT_UPDATE_FLAG) {
        if (input->read (input, buf, 1) == 1) {
          uint8_t data_length = buf[0];
          if (data_length != 0) {
        lprintf(
                    "id3: invalid extended header, unexpected update flag data length\n");
            return 0;
          }
        } else {
          return 0;
        }
      }
      /* Crc flag */
      if (frame_ext_header->flags & ID3V24_EXT_CRC_FLAG) {
        if (input->read (input, buf, 1) == 1) {
          uint8_t data_length = buf[0];
          if (data_length != 5) {
            lprintf(
                    "id3: invalid extended header, unexpected crc flag data length\n");
            return 0;
          }
          if (input->read (input, buf, data_length) == data_length) {
            /* ignore crc */
            frame_ext_header->crc = BE_35_synchsafe(buf);
          }
        } else {
          return 0;
        }
      }
      /* Restriction flag */
      if (frame_ext_header->flags & ID3V24_EXT_RESTRICTIONS_FLAG) {
        if (input->read (input, buf, 1) == 1) {
          uint8_t data_length = buf[0];
          if (data_length != 1) {
            lprintf(
                    "id3: invalid extended header, unexpected restriction flag data length\n");
            return 0;
          }
          if (input->read (input, buf, data_length) == data_length) {
            /* ignore restrictions */
            frame_ext_header->restrictions = buf[0];
          }
        } else {
          return 0;
        }
      }
    } else {
      return 0;
    }
    lprintf("ext header: size: %d, flags: %X, crc: %d, restrictions: %8X\n",
            frame_ext_header->size, frame_ext_header->flags,
            frame_ext_header->crc, frame_ext_header->restrictions);
    return 1;
  } else {
    return 0;
  }
}

static int id3v24_interp_frame(input_plugin_t *input,
                               xine_stream_t *stream,
                               id3v24_frame_header_t *frame_header) {
  char *buf;
  int enc;

  buf = malloc(frame_header->size + 1);
  if (buf == NULL) {
    lprintf("malloc error");
    return 0;
  }

  if (input->read (input, buf, frame_header->size) == frame_header->size) {
    buf[frame_header->size] = 0;
    enc = buf[0];
    if( enc >= ID3_ENCODING_COUNT )
      enc = 0;
      
    lprintf("data: %s\n", buf+1);

    switch (frame_header->id) {
      case ( FOURCC_TAG('T', 'C', 'O', 'N') ):
        {
          char tmp[1024];
          
          if (id3v24_parse_genre(tmp, buf + 1, 1024)) {
            _x_meta_info_set(stream, XINE_META_INFO_GENRE, tmp);
          }
        }
        break;

      case ( FOURCC_TAG('T', 'I', 'T', '2') ):
        _x_meta_info_set_generic(stream, XINE_META_INFO_TITLE, buf + 1, id3_encoding[enc]);
        break;

      case ( FOURCC_TAG('T', 'P', 'E', '1') ):
        _x_meta_info_set_generic(stream, XINE_META_INFO_ARTIST, buf + 1, id3_encoding[enc]);
        break;

      case ( FOURCC_TAG('T', 'A', 'L', 'B') ):
        _x_meta_info_set_generic(stream, XINE_META_INFO_ALBUM, buf + 1, id3_encoding[enc]);
        break;

      case ( FOURCC_TAG('T', 'Y', 'E', 'R') ):
        _x_meta_info_set_generic(stream, XINE_META_INFO_YEAR, buf + 1, id3_encoding[enc]);
        break;

      case ( FOURCC_TAG('C', 'O', 'M', 'M') ):
        _x_meta_info_set_generic(stream, XINE_META_INFO_COMMENT, buf + 1 + 3, id3_encoding[enc]);
        break;

      case ( FOURCC_TAG('T', 'R', 'C', 'K') ):
        _x_meta_info_set(stream, XINE_META_INFO_TRACK_NUMBER, buf + 1);
        break;

      default:
        lprintf("unhandled frame\n");
    }

    free(buf);
    return 1;
  } else {
    lprintf("read error\n");
    free(buf);
    return 0;
  }
}

int id3v24_parse_tag(input_plugin_t *input,
                     xine_stream_t *stream,
                     int8_t *mp3_frame_header) {
  id3v2_header_t tag_header;
  id3v24_frame_header_t tag_frame_header;
  id3v24_frame_ext_header_t tag_frame_ext_header;
  int pos = 0;

  if (id3v2_parse_header(input, mp3_frame_header, &tag_header)) {

    if (tag_header.flags & ID3V24_ZERO_FLAG) {
      /* invalid flags */
      xprintf(stream->xine, XINE_VERBOSITY_DEBUG, 
              "id3: invalid header flags (%02x)\n", tag_header.flags);
      input->seek (input, tag_header.size - pos, SEEK_CUR);
      return 0;
    }
    if (tag_header.flags & ID3V24_UNSYNCH_FLAG) {
      /* it just means that all frames must be flagged UNSYNCH */
    }
    if (tag_header.flags & ID3V24_EXPERIMENTAL_FLAG) {
      /* it just means that the tag is in a experimental stage */
    }
    if (tag_header.flags & ID3V24_EXT_HEADER_FLAG) {
      /* extended header */
      if (!id3v24_parse_ext_header(input, &tag_frame_ext_header)) {
        return 0;
      }
    }
    /* frame parsing */
    while ((pos + ID3V24_FRAME_HEADER_SIZE) <= tag_header.size) {
      if (id3v24_parse_frame_header(input, &tag_frame_header)) {
        pos += ID3V24_FRAME_HEADER_SIZE;
        if (tag_frame_header.id && tag_frame_header.size) {
          if ((pos + tag_frame_header.size) <= tag_header.size) {
            if (!id3v24_interp_frame(input, stream, &tag_frame_header)) {
              xprintf(stream->xine, XINE_VERBOSITY_DEBUG, 
                      "id3: invalid frame content\n");
            }
          } else {
            xprintf(stream->xine, XINE_VERBOSITY_DEBUG, 
                    "id3: invalid frame header\n");
            input->seek (input, tag_header.size - pos, SEEK_CUR);
            return 1;
          }
          pos += tag_frame_header.size;
        } else {
          /* end of frames, the rest is padding */
          input->seek (input, tag_header.size - pos, SEEK_CUR);
          return 1;
        }
      } else {
        xprintf(stream->xine, XINE_VERBOSITY_DEBUG, 
                "id3: id3v2_parse_frame_header problem\n");
        return 0;
      }
    }
    if (tag_header.flags & ID3V24_FOOTER_FLAG) {
      /* ignore footer */
      input->seek (input, ID3V24_FOOTER_SIZE, SEEK_CUR);
    }
    return 1;
  } else {
    xprintf(stream->xine, XINE_VERBOSITY_DEBUG, "id3v23: id3v2_parse_header problem\n");
    return 0;
  }
}

int id3v2_parse_tag(input_plugin_t *input,
		    xine_stream_t *stream,
		    int8_t *mp3_frame_header) {
  _x_assert(mp3_frame_header[0] == 'I' && mp3_frame_header[1] == 'D' && mp3_frame_header[2] == '3');

  int result = 0;

  switch(mp3_frame_header[3]) {
  case 2:
    xprintf(stream->xine, XINE_VERBOSITY_LOG, "ID3V2.2 tag\n");
    result = id3v22_parse_tag(input, stream, mp3_frame_header);
    break;
   
  case 3:
    xprintf(stream->xine, XINE_VERBOSITY_LOG, "ID3V2.3 tag\n");
    result = id3v23_parse_tag(input, stream, mp3_frame_header);
    break;

  case 4:
    xprintf(stream->xine, XINE_VERBOSITY_LOG, "ID3V2.4 tag\n");
    result = id3v24_parse_tag(input, stream, mp3_frame_header);
    break;

  default:
    xprintf(stream->xine, XINE_VERBOSITY_LOG, "Unknown ID3v2 version: 0x%02x.\n", mp3_frame_header[3]);
  }

  return result;
}
