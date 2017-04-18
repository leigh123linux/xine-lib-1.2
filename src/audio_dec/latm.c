/*
 * Copyright (C) 2017 the xine project
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
 * AAC LATM parser and demuxer by Torsten Jager <t.jager@gmx.de>
 * Limitations:
 *  - LATM v0 and v1 only
 *  - 1 program / 1 layer of the possible 16 / 8 (DVB)
 *
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <sys/types.h>
#include <string.h>
#include <stdlib.h>

#define LOG_MODULE "latm"
#define LOG_VERBOSE
/*
#define LOG
*/

#if (defined (__GNUC__) && (__GNUC__ >= 4)) || defined (__clang__)
#  if defined (__i386) || defined (__i386__)
#    define BEBF_X86_32_ASM
#  endif
#  define bebf_UNUSED __attribute__((unused))
#  ifdef WORDS_BIGENDIAN
#    define bebf_ADJ32(v) (v)
#  else
#    define bebf_ADJ32(v) ((uint32_t)__builtin_bswap32 ((int32_t)(v)))
#  endif
#else
#  define bebf_UNUSED
#  ifdef WORDS_BIGENDIAN
#    define bebf_ADJ32(v) (v)
#  else
#    define bebf_ADJ32(v) (((v) >> 24) | (((v) & 0xff0000) >> 8) | (((v) & 0xff00) << 8) | ((v) << 24))
#  endif
#endif

/*************************************************************************
* Big Endian BitFile helper                                              *
*************************************************************************/

typedef struct {
  uint64_t readcache; /* left justified */
  const uint32_t *readptr, *readstop;
  const uint8_t *readstart;
  int32_t readbits, writebits; /* # of invalid bits in cache */
  uint64_t writecache;
  uint32_t *writeptr;
  uint8_t *writestart;
} bebf_t;

/**
 * Jump to stream reader bit position.
 * @param bebf pointer to bebf_t struct to use.
 * @param nbits new position in bits from the beginning.
 */
static void bebf_UNUSED bebf_seek (bebf_t *bebf, uint32_t nbits) {
  const uint32_t *p = (const uint32_t *)(((uintptr_t)bebf->readstart + (nbits >> 3)) & ~(uintptr_t)3);
  uint32_t n = (int32_t)nbits - (((const uint8_t *)p - bebf->readstart) << 3);
  bebf->readbits = n;
  if (p < bebf->readstop) {
    uint64_t b = bebf_ADJ32 (p[0]);
    b <<= 32;
    b |= bebf_ADJ32 (p[1]);
    bebf->readcache = b << n;
    bebf->readptr = p + 2;
  } else {
    bebf->readcache = 0;
    bebf->readptr = bebf->readstop;
  }
}
  
/**
 * Align to next byte boundary.
 * @param bebf pointer to bebf_t struct to use.
 */
static void bebf_UNUSED bebf_align (bebf_t *bebf) {
  uint32_t n = (64 - bebf->readbits) & 7;
  if (n) {
    bebf->readcache <<= n;
    bebf->readbits += n;
  }
}
  
/**
 * Initialize big endian bit stream reader.
 * @param bebf pointer to bebf_t struct to use.
 * @param rp   pointer to read stream, padded by at least 8 bytes.
 * @param nbytes byte length of input.
 */
static void bebf_UNUSED bebf_set_read (bebf_t *bebf, const uint8_t *rp, uint32_t nbytes) {
  bebf->readstart = rp;
  bebf->readstop = (const uint32_t *)(((uintptr_t)rp + nbytes + 3) & ~(uintptr_t)3);
  bebf_seek (bebf, 0);
}

/**
 * Initialize big endian bit stream writer.
 * @param bebf pointer to bebf_t struct to use.
 * @param wp   pointer to 4 byte aligned write buffer.
 */
static void bebf_UNUSED bebf_set_write (bebf_t *bebf, uint8_t *wp) {
  bebf->writestart = wp;
  bebf->writeptr = (uint32_t *)wp;
  bebf->writecache = 0;
  bebf->writebits = 64;
}

/**
 * Get 1 .. 32 bits of data from read stream.
 * @param bebf  pointer to bebf_t struct to use.
 * @param nbits bit count.
 * @return      the requested data, right justified.
 */
static uint32_t bebf_UNUSED bebf_get (bebf_t *bebf, uint32_t nbits) {
  uint32_t b;
  if (bebf->readbits > 31) {
    bebf->readbits -= 32;
    if (bebf->readptr < bebf->readstop) {
      uint32_t v = bebf_ADJ32 (*(bebf->readptr)++);
      bebf->readcache |= (uint64_t)v << bebf->readbits;
    }
  }
  bebf->readbits += nbits;
  b = bebf->readcache >> 32;
  bebf->readcache <<= nbits;
  return b >> (32 - nbits);
}

/**
 * Sniff 1 .. 32 bits of data from read stream, not changing the stream position.
 * @param bebf  pointer to bebf_t struct to use.
 * @param nbits bit count.
 * @return      the requested data, right justified.
 */
static uint32_t bebf_UNUSED bebf_sniff (bebf_t *bebf, uint32_t nbits) {
  uint32_t b;
  if (bebf->readbits > 31) {
    bebf->readbits -= 32;
    if (bebf->readptr < bebf->readstop) {
      uint32_t v = bebf_ADJ32 (*(bebf->readptr)++);
      bebf->readcache |= (uint64_t)v << bebf->readbits;
    }
  }
  bebf->readbits += nbits;
  b = bebf->readcache >> 32;
  return b >> (32 - nbits);
}

/**
 * Discard 1 .. 32 bits of data from read stream.
 * @param bebf  pointer to bebf_t struct to use.
 * @param nbits bit count.
 */
static void bebf_UNUSED bebf_skip (bebf_t *bebf, uint32_t nbits) {
  if (bebf->readbits > 31) {
    bebf->readbits -= 32;
    if (bebf->readptr < bebf->readstop) {
      uint32_t v = bebf_ADJ32 (*(bebf->readptr)++);
      bebf->readcache |= (uint64_t)v << bebf->readbits;
    }
  }
  bebf->readbits += nbits;
  bebf->readcache <<= nbits;
}

/**
 * Tell current read stream position.
 * @param bebf pointer to bebf_t struct to use.
 * @return     stream position in bits.
 */
static uint32_t bebf_UNUSED bebf_tell (bebf_t *bebf) {
  return (((const uint8_t *)bebf->readptr - bebf->readstart) << 3) + bebf->readbits - 64;
}

/**
 * Seek forward to right after next appearence of sync pattern.
 * @param bebf    pointer to bebf_t struct to use.
 * @param pattern the sync pattern to search for, right justified.
 * @param pbits   bit length of pattern.
 * @return        1 (found), 0 (end of bitfile reached).
 */
static int bebf_UNUSED bebf_sync (bebf_t *bebf, uint32_t pattern, uint32_t pbits) {
  uint32_t _pat = pattern << (32 - pbits);
  uint32_t _mask = ~0 << (32 - pbits);
  if (bebf->readbits > 31) {
    bebf->readbits -= 32;
    if (bebf->readptr < bebf->readstop) {
      uint32_t v = bebf_ADJ32 (*(bebf->readptr)++);
      bebf->readcache |= (uint64_t)v << bebf->readbits;
    }
  }
  while (1) {
    if (bebf->readbits > 31) {
      uint32_t v;
      bebf->readbits -= 32;
      if (bebf->readptr >= bebf->readstop)
        return 0;
      v = bebf_ADJ32 (*(bebf->readptr)++);
      bebf->readcache |= v;
    }
    if (((bebf->readcache >> 32) & _mask) == _pat)
      break;
    bebf->readbits++;
    bebf->readcache <<= 1;
  }
  bebf->readbits += pbits;
  bebf->readcache <<= pbits;
  return 1;
}


/**
 * Write 1 .. 32 bits of data to write stream.
 * @param bebf  pointer to bebf_t struct to use.
 * @param bits  The data to write, right justified.
 * @param nbits bit count.
 */
static void bebf_UNUSED bebf_put (bebf_t *bebf, uint32_t bits, uint32_t nbits) {
  if (bebf->writebits < 33) {
    *(bebf->writeptr)++ = bebf_ADJ32 (bebf->writecache >> 32);
    bebf->writecache <<= 32;
    bebf->writebits += 32;
  }
  bebf->writebits -= nbits;
  bebf->writecache |= (uint64_t)bits << bebf->writebits;
}

/**
 * Flush out any unsaved data to write stream.
 * @param bebf  pointer to bebf_t struct to use.
 * @return      byte count of write buffer so far.
 */
static size_t bebf_UNUSED bebf_flush (bebf_t *bebf) {
  if (bebf->writebits < 33) {
    *(bebf->writeptr)++ = bebf_ADJ32 (bebf->writecache >> 32);
    bebf->writecache <<= 32;
    bebf->writebits += 32;
  }
  if (bebf->writebits < 64) {
    *(bebf->writeptr) = bebf_ADJ32 (bebf->writecache >> 32);
  }
  return ((uint8_t *)bebf->writeptr - bebf->writestart) + ((64 - bebf->writebits + 7) >> 3);
}

/**
 * Copy bits from read stream to write stream verbatim, and try to be fast.
 * @param bebf  pointer to bebf_t struct to use.
 * @param nbits bit count.
 */
static void bebf_UNUSED bebf_copy (bebf_t *bebf, uint32_t nbits) {
  const uint32_t *p = bebf->readptr;
  uint32_t *q = bebf->writeptr;
  /* refill read */
  if (bebf->readbits > 31) {
    bebf->readbits -= 32;
    bebf->readcache |= (uint64_t)bebf_ADJ32 (p[0]) << bebf->readbits;
    p++;
  }
  /* flush write */
  if (bebf->writebits < 33) {
    *q = bebf_ADJ32 (bebf->writecache >> 32);
    bebf->writecache <<= 32;
    q++;
    bebf->writebits += 32;
  }
  /* staying inside cache? */
  if (bebf->writebits >= (int32_t)nbits) {
    bebf->readptr = p;
    bebf->writeptr = q;
    bebf->writebits -= nbits;
    bebf->writecache |= (bebf->readcache >> (64 - nbits)) << bebf->writebits;
    bebf->readcache <<= nbits;
    bebf->readbits += nbits;
    return;
  }
  /* align write */
  if (bebf->writebits < 64) {
    uint32_t n = bebf->writebits - 32;
    uint32_t b = bebf->readcache >> (64 - n);
    b |= bebf->writecache >> 32;
    *q = bebf_ADJ32 (b);
    q++;
    bebf->readcache <<= n;
    bebf->readbits += n;
    bebf->writebits = 64;
    nbits -= n;
    if (bebf->readbits > 31) {
      bebf->readbits -= 32;
      bebf->readcache |= (uint64_t)bebf_ADJ32 (p[0]) << bebf->readbits;
      p++;
    }
  }
  /* fast blocks */
  if (nbits & ~31) {
    uint32_t words = nbits >> 5;
    switch ((64 - bebf->readbits) & 31) {
      /* If only my old Amiga blitter could see this :-)) */
#ifdef BEBF_X86_32_ASM
#  define bebf_shift(nn) { \
  uint32_t hi = bebf->readcache >> 32, lo = bebf->readcache, dummy; \
  do { \
    __asm__ __volatile__ ( \
      "movl\t%1, %4\n\t" \
      "shldl\t%5, %0, %1\n\t" \
      "bswap\t%4\n\t" \
      "movl\t(%2), %0\n\t" \
      "bswap\t%0\n\t" \
      "movl\t%4, (%3)\n\t" \
      "addl\t$4, %2\n\t" \
      "leal\t4(%3), %3\n\t" \
      "shldl\t%6, %0, %1\n\t" \
      "sall\t%6, %0" \
      : "=r" (lo), "=r" (hi), "=r" (p), "=r" (q), "=r" (dummy) \
      : "I" (nn), "I" (32 - nn), "0" (lo), "1" (hi), "2" (p), "3" (q) \
      : "cc"); \
  } while (--words); \
  bebf->readcache = ((uint64_t)hi << 32) | lo; \
}
#else
#  define bebf_shift(nn) { \
  uint64_t b = bebf->readcache; \
  do {*q++ = bebf_ADJ32 (b >> 32); b <<= nn; b |= bebf_ADJ32 (*p++); b <<= 32 - nn;} while (--words); \
  bebf->readcache = b; \
}
#endif
      case  1: bebf_shift ( 1); break;
      case  2: bebf_shift ( 2); break;
      case  3: bebf_shift ( 3); break;
      case  4: bebf_shift ( 4); break;
      case  5: bebf_shift ( 5); break;
      case  6: bebf_shift ( 6); break;
      case  7: bebf_shift ( 7); break;
      case  9: bebf_shift ( 9); break;
      case 10: bebf_shift (10); break;
      case 11: bebf_shift (11); break;
      case 12: bebf_shift (12); break;
      case 13: bebf_shift (13); break;
      case 14: bebf_shift (14); break;
      case 15: bebf_shift (15); break;
      case 17: bebf_shift (17); break;
      case 18: bebf_shift (18); break;
      case 19: bebf_shift (19); break;
      case 20: bebf_shift (20); break;
      case 21: bebf_shift (21); break;
      case 22: bebf_shift (22); break;
      case 23: bebf_shift (23); break;
      case 25: bebf_shift (25); break;
      case 26: bebf_shift (26); break;
      case 27: bebf_shift (27); break;
      case 28: bebf_shift (28); break;
      case 29: bebf_shift (29); break;
      case 30: bebf_shift (30); break;
      case 31: bebf_shift (31); break;
      default:
        memcpy (q, (const uint8_t *)p - ((64 - bebf->readbits) >> 3), words << 2);
        p += words;
        q += words;
        bebf->readcache = (((uint64_t)bebf_ADJ32 (p[-2]) << 32) | bebf_ADJ32 (p[-1])) << bebf->readbits;
    }
    nbits &= 31;
  }
  /* tail bits */
  if (nbits) {
    bebf->writecache = (bebf->readcache >> (64 - nbits)) << (64 - nbits);
    bebf->writebits = 64 - nbits;
    bebf->readcache <<= nbits;
    bebf->readbits += nbits;
  } else {
    bebf->writecache = 0;
  }
  bebf->readptr = p;
  bebf->writeptr = q;
}

/*************************************************************************
* AAC config data parser                                                 *
*************************************************************************/

typedef enum {
  AOT_AAC_MAIN        = 1, /* Main */
  AOT_AAC_LC          = 2, /* Low Complexity */
  AOT_AAC_SSR         = 3, /* Scalable Sample Rate */
  AOT_AAC_LTP         = 4, /* Long Term Prediction */
  AOT_SBR             = 5, /* Spectral Band Replication */
  AOT_AAC_SCALABLE    = 6, /* Scalable */
  AOT_TWINVQ          = 7, /* Twin Vector Quantizer */
  AOT_CELP            = 8, /* Code Excited Linear Prediction */
  AOT_HVXC            = 9, /* Harmonic Vector eXcitation Coding */
  AOT_ER_AAC_LC       = 17, /* Error Resilient variants */
  AOT_ER_AAC_LTP      = 19,
  AOT_ER_AAC_SCALABLE = 20,
  AOT_ER_TWINVQ       = 21,
  AOT_ER_BSAC         = 22, /* Bit-Sliced Arithmetic Coding */
  AOT_ER_AAC_LD       = 23, /* Low Delay */
  AOT_ER_CELP         = 24,
  AOT_ER_HVXC         = 25,
  AOT_ER_HILN         = 26, /* Harmonic and Individual Lines plus Noise */
  AOT_ER_PARAM        = 27, /* Parametric */
  AOT_SSC             = 28, /* SinuSoidal Coding */
  AOT_PS              = 29, /* Parametric Stereo */
  AOT_L1              = 32, /* Layer 1 */
  AOT_L2              = 33, /* Layer 2 */
  AOT_L3              = 34, /* Layer 3 */
  AOT_ALS             = 36, /* Audio LosslesS */
  AOT_ER_AAC_ELD      = 39  /* Enhanced Low Delay */
} bebf_aot_t;

#define AOTF_SHORT 0x0001 /* frameLengthFlag */
#define AOTF_CORE  0x0002 /* dependsOnCoreCoder */
#define AOTF_EXT1  0x0004 /* extensionFlag1 */
#define AOTF_LAYER 0x0008 /* layerNr */
#define AOTF_CHAN  0x0010 /* custom channel layout */
#define AOTF_SUBFR 0x0020 /* subFrames */
#define AOTF_RESIL 0x0040 /* errorResilienceFlags */
#define AOTF_LDSBR 0x0080 /* lowDelaySbr */
#define AOTF_ELD   0x0100 /* enhancedLowDelay */
#define AOTF_EPCNF 0x0200 /* epConfig */

static const uint32_t bebf_latm_flags[40] = {
  0,
  /* AOT_AAC_MAIN        */ AOTF_SHORT|AOTF_CORE|AOTF_EXT1|AOTF_CHAN,
  /* AOT_AAC_LC          */ AOTF_SHORT|AOTF_CORE|AOTF_EXT1|AOTF_CHAN,
  /* AOT_AAC_SSR         */ AOTF_SHORT|AOTF_CORE|AOTF_EXT1|AOTF_CHAN,
  /* AOT_AAC_LTP         */ AOTF_SHORT|AOTF_CORE|AOTF_EXT1|AOTF_CHAN,
  /* AOT_SBR             */ 0,
  /* AOT_AAC_SCALABLE    */ AOTF_SHORT|AOTF_CORE|AOTF_EXT1|AOTF_LAYER|AOTF_CHAN,
  /* AOT_TWINVQ          */ 0,
  /* AOT_CELP            */ 0,
  /* AOT_HVXC            */ 0,
  0, 0, 0, 0, 0, 0, 0,
  /* AOT_ER_AAC_LC       */ AOTF_SHORT|AOTF_CORE|AOTF_EXT1|AOTF_CHAN|AOTF_RESIL|AOTF_EPCNF,
  0,
  /* AOT_ER_AAC_LTP      */ AOTF_SHORT|AOTF_CORE|AOTF_EXT1|AOTF_CHAN|AOTF_RESIL|AOTF_EPCNF,
  /* AOT_ER_AAC_SCALABLE */ AOTF_SHORT|AOTF_CORE|AOTF_EXT1|AOTF_LAYER|AOTF_CHAN|AOTF_RESIL|AOTF_EPCNF,
  /* AOT_ER_TWINVQ       */ 0,
  /* AOT_ER_BSAC         */ AOTF_SHORT|AOTF_CORE|AOTF_EXT1|AOTF_CHAN|AOTF_SUBFR,
  /* AOT_ER_AAC_LD       */ AOTF_SHORT|AOTF_CORE|AOTF_EXT1|AOTF_CHAN|AOTF_RESIL|AOTF_EPCNF,
  /* AOT_ER_CELP         */ 0,
  /* AOT_ER_HVXC         */ 0,
  /* AOT_ER_HILN         */ 0,
  /* AOT_ER_PARAM        */ 0,
  /* AOT_SSC             */ 0,
  /* AOT_PS              */ 0,
  0, 0,
  /* AOT_L1              */ 0,
  /* AOT_L2              */ 0,
  /* AOT_L3              */ 0,
  0,
  /* AOT_ALS             */ 0,
  0, 0,
  /* AOT_ER_AAC_ELD      */ AOTF_SHORT|AOTF_RESIL|AOTF_LDSBR|AOTF_ELD|AOTF_EPCNF
};

#define BEBF_LATM_GOT_CONF 1
#define BEBF_LATM_GOT_FRAME 2

typedef struct {
  bebf_t     bebf;
  uint8_t   *frame;
  uint32_t   framelen, fbuflen;
  uint8_t   *config;
  uint32_t   conflen, confbuflen;
  uint32_t   version;
  uint32_t   frame_len_type, frame_len;
  bebf_aot_t object_type, object_type2;
  uint32_t   samplerate_index, samplerate;
  uint32_t   samplerate_index2, samplerate2;
  uint32_t   samples;
  uint32_t   channel_conf, numchannels;
  uint32_t   channel_conf2;
  int32_t    sbr, ps;
} bebf_latm_t;

/**
 * Parse and export AAC configuration data.
 * @param latm  pointer to bebf_latm_t struct.
 * @param nbits count of input bits or 0.
 * @return      BEBF_LATM_GOT_CONF or 0.
 */
static int bebf_UNUSED bebf_latm_configure (bebf_latm_t *latm, uint32_t nbits) {
  /* Aargh - LATM v0 does not provide config length, so we need to fully parse it ... */
  static const uint32_t rates[16] = {
    96000, 88200, 64000, 48000, 44100, 32000, 24000, 22050, 16000, 12000, 11025, 8000, 7350, 0, 0, 0
  };
  static const uint8_t channels[16] = { 0, 1, 2, 3, 4, 5, 6, 8, 0, 0, 0, 0, 0, 0, 0, 0 };
  uint32_t conf_pos = bebf_tell (&latm->bebf), conf_pos2;
  uint32_t n, ext1_flag, flags, ret = 0;
  /* object type */
  latm->object_type = bebf_get (&latm->bebf, 5);
  if (latm->object_type == 31)
    latm->object_type = bebf_get (&latm->bebf, 6) + 32;
  /* sample rate */
  latm->samplerate_index = bebf_get (&latm->bebf, 4);
  latm->samplerate = latm->samplerate_index == 15 ? bebf_get (&latm->bebf, 24) : rates[latm->samplerate_index];
  /* channels */
  latm->channel_conf = bebf_get (&latm->bebf, 4);
  latm->numchannels = channels[latm->channel_conf];
  /* explicit sbr/ps */
  latm->sbr = latm->ps = -1;
  n = 0;
  if (latm->object_type == AOT_SBR) {
    n = 1;
  } else if (latm->object_type == AOT_PS) {
    if ((latm->channel_conf == 1) && (bebf_sniff (&latm->bebf, 9) & 0x0ff)) {
      latm->ps = 1;
      n = 1;
    }
  }
  if (n) {
    latm->object_type2 = AOT_SBR;
    latm->sbr = 1;
    latm->samplerate_index2 = bebf_get (&latm->bebf, 4);
    latm->samplerate2 = latm->samplerate_index2 == 15 ? bebf_get (&latm->bebf, 24) : rates[latm->samplerate_index2];
    latm->object_type = bebf_get (&latm->bebf, 5);
    if (latm->object_type == 31)
      latm->object_type = bebf_get (&latm->bebf, 6) + 32;
    if (latm->object_type == AOT_ER_BSAC)
      latm->channel_conf2 = bebf_get (&latm->bebf, 4);
  } else {
    latm->object_type2 = 0;
    latm->samplerate_index2 = 0;
    latm->samplerate2 = 0;
  }
  /* implicit ps */
  if (latm->ps == -1) {
    if (!latm->sbr || (latm->object_type != AOT_AAC_LC))
      latm->ps = 0;
  }
  /* more specific info */
  conf_pos2 = bebf_tell (&latm->bebf);
  /* lossless audio */
  if (latm->object_type == AOT_ALS) {
    bebf_skip (&latm->bebf, 5);
    conf_pos2 += 5;
    if (bebf_sniff (&latm->bebf, 24) != 0x414c53) { /* "ALS" */
      bebf_skip (&latm->bebf, 24);
      conf_pos2 += 24;
    }
    /* sniff first frame head for correct settings */
    if (bebf_get (&latm->bebf, 32) == 0x414c5300) { /* "ALS\0" */
      latm->samplerate = bebf_get (&latm->bebf, 32);
      bebf_skip (&latm->bebf, 32); /* numSamples */
      latm->channel_conf = 0;
      latm->numchannels = bebf_get (&latm->bebf, 16) + 1;
    }
  }
  n = latm->object_type;
  if (n > 39)
    n = 0;
  flags = bebf_latm_flags[n];
  if (flags & AOTF_SHORT) /* frameLengthFlag */
    latm->samples = bebf_get (&latm->bebf, 1) ? 960 : 1024;
  if (flags & AOTF_CORE) { /* dependsOnCoreCoder */
    if (bebf_get (&latm->bebf, 1))
      bebf_skip (&latm->bebf, 14); /* coreCoderDelay */
  }
  ext1_flag = 2;
  if (flags & AOTF_EXT1) /* extensionFlag1 */
    ext1_flag = bebf_get (&latm->bebf, 1);
  if (flags & AOTF_LAYER) /* layerNr */
    bebf_skip (&latm->bebf, 3);
  if (flags & AOTF_CHAN) { /* custom channel layout */
    if (!latm->channel_conf) {
      int front, side, back, lfe, data, coupling, n;
      /* program config element */
      bebf_skip (&latm->bebf, 10); /* elementInstanceTag, objectType, Freq */
      n        = bebf_get (&latm->bebf, 4 + 4 + 4 + 2 + 3 + 4);
      front    =  n >> (4 + 4 + 2 + 3 + 4);
      side     = (n >> (    4 + 2 + 3 + 4)) & 15;
      back     = (n >> (        2 + 3 + 4)) & 15;
      lfe      = (n >> (            3 + 4)) &  3;
      data     = (n >>                  4 ) &  7;
      coupling =  n                         & 15;
      latm->numchannels = front + side + back + lfe;
      if (bebf_get (&latm->bebf, 1))
        bebf_skip (&latm->bebf, 4); /* mono downmix */
      if (bebf_get (&latm->bebf, 1))
        bebf_skip (&latm->bebf, 4); /* stereo downmix */
      if (bebf_get (&latm->bebf, 1))
        bebf_skip (&latm->bebf, 3); /* matrix downmix */
      bebf_seek (&latm->bebf, bebf_tell (&latm->bebf) + 5 * (front + side + back + coupling) + 4 * (lfe + data));
      /* comment text */
      bebf_align (&latm->bebf);
      n = bebf_get (&latm->bebf, 8);
      bebf_seek (&latm->bebf, bebf_tell (&latm->bebf) + 8 * n);
    }
  }
  if (ext1_flag) {
    if (flags & AOTF_SUBFR) /* subFrames */
      bebf_skip (&latm->bebf, 5 + 11); /* numSubFrames, layerLength */
    if (flags & AOTF_RESIL) /* errorResilienceFlags */
      bebf_skip (&latm->bebf, 3);
    if (ext1_flag == 1)
      bebf_skip (&latm->bebf, 1); /* extFlag3 */
  }
  if (flags & AOTF_LDSBR) /* lowDelaySbr */
    bebf_skip (&latm->bebf, 1);
  if (flags & AOTF_ELD) { /* enhancedLowDelay */
    while (bebf_get (&latm->bebf, 4)) {
      n = bebf_get (&latm->bebf, 4);
      if (n == 15) {
        n += bebf_get (&latm->bebf, 8);
        if (n == 231)
          n += bebf_get (&latm->bebf, 16);
      }
      bebf_seek (&latm->bebf, bebf_tell (&latm->bebf) + 8 * n);
    }
  }
  if (flags & AOTF_EPCNF) /* epConfig */
    bebf_skip (&latm->bebf, 2);
  conf_pos2 = bebf_tell (&latm->bebf);
  /* export config */
  do {
    size_t l = (conf_pos2 - conf_pos + 7) >> 3;
    if (l > latm->confbuflen) {
      free (latm->config);
      latm->confbuflen = (3 * l / 2 + 7) & ~(size_t)7;
      latm->config = malloc (2 * (latm->confbuflen));
    }
    if (!latm->config)
      break;
    bebf_set_write (&latm->bebf, latm->config);
    bebf_seek (&latm->bebf, conf_pos);
    bebf_copy (&latm->bebf, conf_pos2 - conf_pos);
    bebf_flush (&latm->bebf);
    ret = BEBF_LATM_GOT_CONF;
    if (latm->conflen == l) {
      if (!memcmp (latm->config, latm->config + latm->confbuflen, l))
        ret = 0;
    }
    if (ret)
      memcpy (latm->config + latm->confbuflen, latm->config, l);
    latm->conflen = l;
  } while (0);
  bebf_seek (&latm->bebf, nbits ? conf_pos + nbits : conf_pos2);
  return ret;
}

/*************************************************************************
* AAC LATM demuxer                                                       *
*************************************************************************/

/**
 * Demultiplex 1 LATM packet.
 * @param latm   pointer to bebf_latm_t struct.
 * @param in     pointer to packet.
 * @param nbytes byte length of packet.
 * @return       BEBF_LATM_GOT_* flags.
 */
static int bebf_UNUSED bebf_latm_demux (bebf_latm_t *latm, const uint8_t *in, uint32_t nbytes) {
  uint32_t ret = 0;
  bebf_set_read (&latm->bebf, in, nbytes);
  {
    /* latm sync */
    uint32_t n = bebf_get (&latm->bebf, 24);
    if ((n >> 13) != 0x2b7)
      return 0;
    n &= 0x1fff;
    n += 3;
    if (n < nbytes)
      nbytes = n;
  }
  if (bebf_get (&latm->bebf, 1)) {
    /* same settings */
    if (!latm->conflen)
      return 0;
  } else {
    /* new settings */
    latm->version = bebf_get (&latm->bebf, 1);
    if (latm->version)
      latm->version += bebf_get (&latm->bebf, 1);
    if (latm->version < 2) {
      uint32_t n;
      /* some yet not mattering values */
      if (latm->version) {
        n = bebf_get (&latm->bebf, 2);
        bebf_skip (&latm->bebf, 8 * n + 8); /* taraFullness */
      }
      bebf_skip (&latm->bebf, 1 + 6); /* allStreamSameTimeFraming, numSubFrames */
      /* part count (DVB uses 1:1) */
      if (bebf_get (&latm->bebf, 4)) /* numPrograms */
        return 0;
      if (bebf_get (&latm->bebf, 3)) /* numLayers */
        return 0;
      /* config len */
      n = 0;
      if (latm->version) {
        n = bebf_get (&latm->bebf, 2);
        n = bebf_get (&latm->bebf, 8 * n + 8);
      }
      /* extract config */
      ret |= bebf_latm_configure (latm, n);
      /* frame len */
      latm->frame_len_type = n = bebf_get (&latm->bebf, 3);
      if (n < 3) {
        if (n == 0)
          bebf_skip (&latm->bebf, 8); /* latmBufferFullness */
        else if (n == 1)
          latm->frame_len = bebf_get (&latm->bebf, 9); /* fixed frameLength */
      } else {
        if (n <= 5)
          bebf_skip (&latm->bebf, 6); /* CELP frame len index */
        else
          bebf_skip (&latm->bebf, 1); /* HVXC frame len index */
      }
      /* more extra stuff */
      if (bebf_get (&latm->bebf, 1)) { /* other data present */
        if (latm->version) {
          n = bebf_get (&latm->bebf, 2);
          bebf_skip (&latm->bebf, 8 * n + 8);
        } else {
          do {
            n = bebf_get (&latm->bebf, 1);
            bebf_skip (&latm->bebf, 8);
          } while (n);
        }
      }
      if (bebf_get (&latm->bebf, 1)) /* crc present */
        bebf_skip (&latm->bebf, 8);
    }
  }
  if (latm->version < 2) { /* payload len */
    uint32_t l = 0;
    if (latm->frame_len_type == 0) {
      uint32_t n;
      do {
        n = bebf_get (&latm->bebf, 8);
        l += n;
      } while (n == 255);
      latm->frame_len = l; /* variable frameLength */
    } else if (latm->frame_len_type == 1) {
      l = latm->frame_len;
    } else if (latm->frame_len & 1) { /* 3, 5, 7 */
      bebf_skip (&latm->bebf, 2); /* mux_slot_len_coded */
    }
  }
  /*
   * Export frame.
   * @#?!*#$!! Looks like LATM actually appends the frame contents to the
   * bitstream without gap. They dont win much by not byte aligning it there!!
   * Some possible workarounds:
   *  - Merge LATM demuxer and AAC decoder. Terribly complex code, ff does it
   *    that way.
   *  - Tell main decoding function to skip first 0..7 bits. Very easy, but
   *    wont work with external libfaad.
   *  - Bit shift whole frame. Thats what we do here.
   */
  do {
    size_t n = nbytes - (bebf_tell (&latm->bebf) >> 3);
    if (n > latm->fbuflen) {
      free (latm->frame);
      latm->fbuflen = (3 * n / 2 + 7) & ~(size_t)7;
      latm->frame = malloc (latm->fbuflen);
    }
    if (!latm->frame)
      break;
    bebf_set_write (&latm->bebf, latm->frame);
    bebf_copy (&latm->bebf, 8 * nbytes - bebf_tell (&latm->bebf));
    latm->framelen = bebf_flush (&latm->bebf);
    ret |= BEBF_LATM_GOT_FRAME;
  } while (0);
  return ret;
}

/*************************************************************************
* AAC LATM parser                                                        *
*************************************************************************/

static void bebf_UNUSED bebf_latm_open (bebf_latm_t *latm) {
  memset (latm, 0, sizeof (*latm));
}

static void bebf_UNUSED bebf_latm_close (bebf_latm_t *latm) {
  free (latm->config);
  latm->config = NULL;
  free (latm->frame);
  latm->frame = NULL;
  latm->fbuflen = 0;
  latm->framelen = 0;
}

typedef enum {
  BEBF_LATM_NEED_MORE_DATA,
  BEBF_LATM_IS_RAW,
  BEBF_LATM_IS_ADTS,
  BEBF_LATM_IS_LATM,
  BEBF_LATM_IS_UNKNOWN
} bebf_latm_parser_status_t;

/**
 * Test if really LATM.
 * @param in      pointer to input data.
 * @param nbytes  byte count.
 * @return        status.
 */
static bebf_latm_parser_status_t bebf_UNUSED bebf_latm_test (const uint8_t *in, int nbytes) {
  uint32_t word = 0;
  int n = nbytes;
#define BEBF_TEST_MAX (2 * (0x1fff + 3))
  if (n > BEBF_TEST_MAX)
    n = BEBF_TEST_MAX;
  while (n--) {
    word <<= 8;
    word |= *in++;
    if ((word & 0xfff60000) == 0xfff00000) do {
      /* ADTS */
      int size;
      if (n < 7 - 4)
        break;
      size = ((word << 11) | ((uint32_t)in[0] << 3) | (in[1] >> 5)) & 0x1fff;
      if ((size < 7) || (n < size + 7 - 4))
        break;
      if ((in[size - 4] != 0xff) || ((in[size + 1 - 4] & 0xf6) != 0xf0))
        break;
      return BEBF_LATM_IS_ADTS;
    } while (0);
    if ((word & 0xffe00000) == 0x56e00000) do {
      /* LATM */
      int size = ((word >> 8) & 0x1fff) + 3;
      if (n < size + 3 - 4)
        break;
      if ((in[size - 4] != 0x56) || ((in[size + 1 - 4] & 0xe0) != 0xe0))
        break;
      return BEBF_LATM_IS_LATM;
    } while (0);
  }
  return nbytes < BEBF_TEST_MAX ? BEBF_LATM_NEED_MORE_DATA : BEBF_LATM_IS_UNKNOWN;
}

/**
 * Parse and demux LATM.
 * @param latm    pointer to bebf_latm_t struct.
 * @param in      pointer to input data.
 * @param nbytes  pointer to byte count, will be adjusted to consumed.
 * @return        BEBF_LATM_GOT_* flags.
 */
static int bebf_UNUSED bebf_latm_parse (bebf_latm_t *latm, const uint8_t *in, int *nbytes) {
  const uint8_t *p = in;
  int n = *nbytes, size;
  /* discard leading garbage */
  n -= 2;
  while (n >= 0) {
    if (p[0] == 0x56) {
      if ((p[1] & 0xe0) == 0xe0)
        break;
    }
    p++;
    n--;
  }
  n += 2;
  *nbytes = p - in;
  if (n < 3) {
    if ((n == 1) && (p[0] != 0x56))
      *nbytes += 1;
    return 0;
  }
  size = ((((uint32_t)p[1] << 8) | p[2]) & 0x1fff) + 3;
  if (n < size)
    return 0;
  *nbytes += size;
  return bebf_latm_demux (latm, p, size);
}
#undef LOG_MODULE


