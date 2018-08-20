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

/**
 * @file
 * @brief Declaration of internal, private functions for xine-lib.
 *
 * @internal These functions should not be used by neither plugins nor
 * frontends.
 */

#ifndef XINE_PRIVATE_H__
#define XINE_PRIVATE_H__

#ifndef XINE_LIBRARY_COMPILE
# error xine_private.h is for libxine private use only!
#endif
#if defined(HAVE_CONFIG_H) && !defined(__XINE_LIB_CONFIG_H__)
#  error config.h not included
#endif

#include <xine/xine_internal.h>

#if SUPPORT_ATTRIBUTE_VISIBILITY_INTERNAL
# define INTERNAL __attribute__((visibility("internal")))
#elif SUPPORT_ATTRIBUTE_VISIBILITY_DEFAULT
# define INTERNAL __attribute__((__visibility__("default")))
#else
# define INTERNAL
#endif

#if defined (__GNUC__) && (__GNUC__ > 4 || (__GNUC__ == 4 && __GNUC_MINOR__ >= 6 ))
#    define XINE_DISABLE_DEPRECATION_WARNINGS _Pragma("GCC diagnostic ignored \"-Wdeprecated-declarations\"")
#    define XINE_ENABLE_DEPRECATION_WARNINGS  _Pragma("GCC diagnostic warning \"-Wdeprecated-declarations\"")
#else
#    define XINE_DISABLE_DEPRECATION_WARNINGS
#    define XINE_ENABLE_DEPRECATION_WARNINGS
#endif

#ifdef __cplusplus
# define EXTERN_C_START extern "C" {
# define EXTERN_C_STOP  }
#else
# define EXTERN_C_START
# define EXTERN_C_STOP
#endif

EXTERN_C_START

/**
 * @defgroup load_plugins Plugins loading
 * @brief Functions related with plugins loading.
 */

/**
 * @ingroup load_plugins
 * @brief Load plugins into catalog
 * @param this xine instance
 *
 * All input and demux plugins will be fully loaded and initialized.
 * Decoder plugins are loaded on demand. Video/audio output plugins
 * have special load/probe functions
 */
int _x_scan_plugins (xine_t *this) INTERNAL;

/**
 * @ingroup load_plugins
 * @brief Dispose (shutdown) all currently loaded plugins
 * @param this xine instance
 */
void _x_dispose_plugins (xine_t *this) INTERNAL;

void _x_free_video_driver (xine_t *xine, vo_driver_t **driver) INTERNAL;
void _x_free_audio_driver (xine_t *xine, ao_driver_t **driver) INTERNAL;

///@{
/**
 * @defgroup
 * @brief find and instantiate input and demux plugins
 */
input_plugin_t *_x_find_input_plugin (xine_stream_t *stream, const char *mrl) INTERNAL;
demux_plugin_t *_x_find_demux_plugin_last_probe(xine_stream_t *stream, const char *last_demux_name, input_plugin_t *input) INTERNAL;
input_plugin_t *_x_rip_plugin_get_instance (xine_stream_t *stream, const char *filename) INTERNAL;
input_plugin_t *_x_cache_plugin_get_instance (xine_stream_t *stream) INTERNAL;
void _x_free_input_plugin (xine_stream_t *stream, input_plugin_t *input) INTERNAL;
///@}

///@{
/**
 * @defgroup
 * @brief  create decoder fifos and threads
*/

int _x_video_decoder_init           (xine_stream_t *stream) INTERNAL;
void _x_video_decoder_shutdown      (xine_stream_t *stream) INTERNAL;

int _x_audio_decoder_init           (xine_stream_t *stream) INTERNAL;
void _x_audio_decoder_shutdown      (xine_stream_t *stream) INTERNAL;
///@}

/**
 * @brief Benchmark available memcpy methods
 */
void xine_probe_fast_memcpy(xine_t *xine) INTERNAL;

/**
 * @brief Make file descriptors and sockets uninheritable
 */
int _x_set_file_close_on_exec(int fd) INTERNAL;

int _x_set_socket_close_on_exec(int s) INTERNAL;


#if defined(HAVE_PTHREAD_RWLOCK)
#  define xine_rwlock_t                pthread_rwlock_t
#  define xine_rwlock_init_default(l)  pthread_rwlock_init (l, NULL)
#  define xine_rwlock_rdlock(l)        pthread_rwlock_rdlock (l)
#  define xine_rwlock_tryrdlock(l)     pthread_rwlock_tryrdlock (l)
#  define xine_rwlock_timedrdlock(l,t) pthread_rwlock_timedrdlock (l, t)
#  define xine_rwlock_wrlock(l)        pthread_rwlock_wrlock (l)
#  define xine_rwlock_trywrlock(l)     pthread_rwlock_trywrlock (l)
#  define xine_rwlock_timedwrlock(l,t) pthread_rwlock_timedwrlock (l, t)
#  define xine_rwlock_unlock(l)        pthread_rwlock_unlock (l)
#  define xine_rwlock_destroy(l)       pthread_rwlock_destroy (l)
#else
#  define xine_rwlock_t                pthread_mutex_t
#  define xine_rwlock_init_default(l)  pthread_mutex_init (l, NULL)
#  define xine_rwlock_rdlock(l)        pthread_mutex_lock (l)
#  define xine_rwlock_tryrdlock(l)     pthread_mutex_trylock (l)
#  define xine_rwlock_timedrdlock(l,t) pthread_mutex_timedlock (l, t)
#  define xine_rwlock_wrlock(l)        pthread_mutex_lock (l)
#  define xine_rwlock_trywrlock(l)     pthread_mutex_trylock (l)
#  define xine_rwlock_timedwrlock(l,t) pthread_mutex_timedlock (l, t)
#  define xine_rwlock_unlock(l)        pthread_mutex_unlock (l)
#  define xine_rwlock_destroy(l)       pthread_mutex_destroy (l)
#endif

#ifdef HAVE_POSIX_TIMERS
#  define xine_gettime(t) clock_gettime (CLOCK_REALTIME, t)
#else
static inline int xine_gettime (struct timespec *ts) {
  struct timeval tv;
  int r;
  r = gettimeofday (&tv, NULL);
  if (!r) {
    ts->tv_sec  = tv.tv_sec;
    ts->tv_nsec = tv.tv_usec * 1000;
  }
  return r;
}
#endif

static inline int32_t xine_str2int32 (const char **s) {
  const uint8_t *p = (const uint8_t *)*s;
  uint8_t z;
  int32_t v;
  do {
    z = *p;
    if (!z) {
      *s = (const char *)p;
      return 0;
    }
    p++;
    z ^= '0';
  } while ((z > 9) && (z != ('-' ^ '0')));
  if (z == ('-' ^ '0')) {
    v = 0;
    while (1) {
      z = *p++ ^ '0';
      if (z > 9)
        break;
      v = 10 * v - z;
    }
  } else {
    v = 0;
    do {
      v = 10 * v + z;
      z = *p++ ^ '0';
    } while (z <= 9);
  }
  *s = (const char *)(p - 1);
  return v;
}

static inline uint32_t xine_str2uint32 (const char **s) {
  const uint8_t *p = (const uint8_t *)*s;
  uint8_t z;
  uint32_t v;
  do {
    z = *p;
    if (!z) {
      *s = (const char *)p;
      return 0;
    }
    p++;
    z ^= '0';
  } while (z > 9);
  v = 0;
  do {
    v = 10u * v + z;
    z = *p++ ^ '0';
  } while (z <= 9);
  *s = (const char *)(p - 1);
  return v;
}

static inline uint64_t xine_str2uint64 (const char **s) {
  const uint8_t *p = (const uint8_t *)*s;
  uint8_t z;
  uint64_t v;
#if defined(__WORDSIZE) && (__WORDSIZE == 32)
  uint32_t u;
#endif
  do {
    z = *p;
    if (!z) {
      *s = (const char *)p;
      return 0;
    }
    p++;
    z ^= '0';
  } while (z > 9);
#if defined(__WORDSIZE) && (__WORDSIZE == 32)
  u = 0;
  do {
    u = 10u * u + z;
    z = *p++ ^ '0';
    if (z > 9) {
      *s = (const char *)(p - 1);
      return u;
    }
  } while (!(u & 0xf0000000));
  v = u;
#else
  v = 0;
#endif
  do {
    v = (v << 3) + (v << 1) + z;
    z = *p++ ^ '0';
  } while (z <= 9);
  *s = (const char *)(p - 1);
  return v;
}

#define XINE_MAX_INT32_STR 13
static inline void xine_int32_2str (char **s, int32_t v) {
  uint8_t b[24], *t = b + 11, *q = (uint8_t *)*s;
  uint32_t u;
  if (v < 0) {
    *q++ = '-';
    u = -v;
  } else {
    u = v;
  }
  *t = 0;
  do {
    *--t = u % 10u + '0';
    u /= 10u;
  } while (u);
  memcpy (q, t, 12);
  *s = (char *)(q + (b + 11 - t));
}

static inline void xine_uint32_2str (char **s, uint32_t v) {
  uint8_t b[24], *t = b + 11, *q = (uint8_t *)*s;
  *t = 0;
  do {
    *--t = v % 10u + '0';
    v /= 10u;
  } while (v);
  memcpy (q, t, 12);
  *s = (char *)(q + (b + 11 - t));
}

#define XINE_MAX_INT64_STR 21
static inline void xine_uint64_2str (char **s, uint64_t v) {
  uint8_t b[44], *t = b + 21, *q = (uint8_t *)*s;
  *t = 0;
  do {
    *--t = v % 10u + '0';
    v /= 10u;
  } while (v);
  memcpy (q, t, 21);
  *s = (char *)(q + (b + 21 - t));
}

/* A little helper for integers whose size is not obvious, like off_t and time_t. */
#define xine_uint2str(s,v) do { \
  if (sizeof (v) == 64) \
    xine_uint64_2str (s, v); \
  else \
    xine_uint32_2str (s, v); \
} while (0)

EXTERN_C_STOP

#endif


