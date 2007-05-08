#ifndef _XINE_OS_INTERNAL_H
#define _XINE_OS_INTERNAL_H

/* When using Apple's GCC, __APPLE__ will be defined.  This is a whole lot of
 * messiness, but it's necessary in order to perform universal builds properly.
 * It's meant to over-ride configure time stuff that would be different at
 * compile time.
 */
#if defined(__APPLE__) && defined(XINE_MACOSX_UNIVERSAL_BINARY)

#if !defined(__ppc__) && !defined(__ppc64__) && !defined(__i386__) && !defined(__x86_64__)
#error unrecognized/unsupported CPU type building for Apple Darwin
#endif

/* First get rid of anything that initial configure might have set */
#undef  ARCH_PPC
#undef  ARCH_X86
#undef  ARCH_X86_32
#undef  ARCH_X86_64
#undef  BITFIELD_LSBF
#undef  ENABLE_ALTIVEC
#undef  FPM_64BIT
#undef  FPM_DEFAULT
#undef  FPM_INTEL
#undef  FPM_PPC
#undef  HAVE_MMX
#undef  HOST_ARCH
#undef  HOST_DARWIN
#undef  SIZEOF_INT
#undef  SIZEOF_LONG
#undef  SIZEOF_LONG_LONG
#undef  WORDS_BIGENDIAN

#define HOST_DARWIN 1

/* WORDS_BIGENDIAN (replaces AC_C_BIGENDIAN autoconf test at compile time) */
#include <machine/endian.h>
#if BYTE_ORDER == BIG_ENDIAN
#define WORDS_BIGENDIAN 1
#endif

/* __ppc__, __ppc64__, __i386__, __x86_64__ are interesting arch macros */
#if defined(__ppc__)
#define ARCH_PPC
#define FPM_PPC             1
#define HOST_ARCH           "darwin/powerpc"
#define SIZEOF_INT          4
#define SIZEOF_LONG         4
#define SIZEOF_LONG_LONG    8
#elif defined(__ppc64__)
#define ARCH_PPC
#define FPM_64BIT           1
#define SIZEOF_INT          4
#define SIZEOF_LONG         8
#define SIZEOF_LONG_LONG    8
#define HOST_ARCH           "darwin/powerpc64"
#elif defined(__i386__)
#define ARCH_X86
#define ARCH_X86_32
#define BITFIELD_LSBF
#define FPM_INTEL           1
#define HAVE_MMX
#define HOST_ARCH           "darwin/i386"
#define SIZEOF_INT          4
#define SIZEOF_LONG         4
#define SIZEOF_LONG_LONG    8
#elif defined(__x86_64__)
#define ARCH_X86
#define ARCH_X86_64
#define BITFIELD_LSBF
#define FPM_64BIT           1
#define HAVE_MMX
#define HOST_ARCH           "darwin/x64_64"
#define SIZEOF_INT          4
#define SIZEOF_LONG         8
#define SIZEOF_LONG_LONG    8
#endif

#endif  /* __APPLE__ */

#include <stddef.h>
#include <stdarg.h>

#ifdef HOST_OS_DARWIN
  /* Darwin (Mac OS X) needs __STDC_LIBRARY_SUPPORTED__ for SCNx64 and
   * SCNxMAX macros */
#  ifndef __STDC_LIBRARY_SUPPORTED__
#    define __STDC_LIBRARY_SUPPORTED__
#  endif /* __STDC_LIBRARY_SUPPORTED__ */
#endif

#if defined (__SVR4) && defined (__sun)
#  include <sys/int_types.h>
#endif

#include <inttypes.h>


#if defined(WIN32) || defined(__CYGWIN__)
#  define XINE_PATH_SEPARATOR_STRING ";"
#  define XINE_PATH_SEPARATOR_CHAR ';'
#  define XINE_DIRECTORY_SEPARATOR_STRING "\\"
#  define XINE_DIRECTORY_SEPARATOR_CHAR '\\'
#else
#  define XINE_PATH_SEPARATOR_STRING ":"
#  define XINE_PATH_SEPARATOR_CHAR ':'
#  define XINE_DIRECTORY_SEPARATOR_STRING "/"
#  define XINE_DIRECTORY_SEPARATOR_CHAR '/'
#endif


/* replacement of strndup */
#ifndef HAVE_STRNDUP
#define HAVE_STRNDUP
#define strndup(S, N) xine_private_strndup((S), (N))
char *xine_private_strndup(const char *s, size_t n);
#endif

/* replacement of basename */
#ifndef HAVE_BASENAME
#define HAVE_BASENAME
#define basename(PATH) xine_private_basename((PATH))
char *xine_private_basename(char *path);
#endif

/* replacement of hstrerror */
#ifndef HAVE_HSTRERROR
#define HAVE_HSTRERROR
#define hstrerror(ERR) xine_private_hstrerror((ERR))
const char *xine_private_hstrerror(int err);
#endif

/* replacement of setenv */
#ifndef HAVE_SETENV
#define HAVE_SETENV
#define setenv(NAME, VALUE, OVERWRITE) xine_private_setenv((NAME), (VALUE))
int xine_private_setenv(const char *name, const char *value);
#endif

/* replacement of strtok_r */
#ifndef HAVE_STRTOK_R
#define HAVE_STRTOK_R
#define strtok_r(S, DELIM, PTRPTR) xine_private_strtok_r((S), (DELIM), (PTRPTR))
char *xine_private_strtok_r(char *s, const char *delim, char **ptrptr);
#endif

/* replacement of gettimeofday */
#ifndef HAVE_GETTIMEOFDAY
#  define HAVE_GETTIMEOFDAY
#  ifdef WIN32
#    include <winsock.h>
struct timezone;
#  else
#    include <sys/time.h>
#  endif
#  define gettimeofday(TV, TZ) xine_private_gettimeofday((TV))
int xine_private_gettimeofday(struct timeval *tv);
#endif

/* replacement of strpbrk */
#ifndef HAVE_STRPBRK
#define HAVE_STRPBRK
#define strpbrk(S, ACCEPT) xine_private_strpbrk((S), (ACCEPT))
char *xine_private_strpbrk(const char *s, const char *accept);
#endif

/* replacement of strsep */
#ifndef HAVE_STRSEP
#define HAVE_STRSEP
#define strsep(STRINGP, DELIM) xine_private_strsep((STRINGP), (DELIM))
char *xine_private_strsep(char **stringp, const char *delim);
#endif

/* replacement of timegm */
#ifndef HAVE_TIMEGM
#define HAVE_TIMEGM
#include <time.h>
#define timegm(TM) xine_private_timegm((TM))
time_t xine_private_timegm(struct tm *tm);
#endif

/* replacement of unsetenv */
#ifndef HAVE_UNSETENV
#define HAVE_UNSETENV
#define unsetenv(NAME) xine_private_unsetenv((NAME))
void xine_private_unsetenv(const char *name);
#endif

/* replacement of asprintf & vasprintf */
#ifndef HAVE_ASPRINTF
#define HAVE_ASPRINTF
#ifdef __GNUC__
  #define asprintf(STRINGPP, FORMAT, ARGS...) xine_private_asprintf((STRINGPP), FORMAT, ##ARGS)
#elif defined (_MSC_VER)
  #define asprintf(STRINGPP, FORMATARGS) xine_private_asprintf((STRINGPP), FORMATARGS)
#else
  #define asprintf(STRINGPP, FORMAT, ...) xine_private_asprintf((STRINGPP), FORMAT, __VA_ARGS__)
#endif
#define vasprintf(STRINGPP, FORMAT, VA_ARG) xine_private_vasprintf((STRINGPP), (FORMAT), (VA_ARG))
int xine_private_asprintf(char **string, const char *format, ...) XINE_FORMAT_PRINTF(2, 3);
int xine_private_vasprintf(char **string, const char *format, va_list ap) XINE_FORMAT_PRINTF(2, 0);
#endif

/* replacement of strndup */
#ifndef HAVE_STRNDUP
#define HAVE_STRNDUP
#define strndup(S, N) xine_private_strndup((S), (N))
char *xine_private_strndup(const char *s, size_t n);
#endif

/* handle non-standard function names */
#if !defined(HAVE_SNPRINTF) && defined(HAVE__SNPRINTF)
#  define HAVE_SNPRINTF
#  define snprintf _snprintf
#endif
#if !defined(HAVE_VSNPRINTF) && defined(HAVE__VSNPRINTF)
#  define HAVE_VSNPRINTF
#  define vsnprintf _vsnprintf
#endif
#if !defined(HAVE_STRCASECMP) && defined(HAVE__STRICMP)
#  define HAVE_STRCASECMP
#  define strcasecmp _stricmp
#endif
#if !defined(HAVE_STRNCASECMP) && defined(HAVE__STRNICMP)
#  define HAVE_STRNCASECMP
#  define strncasecmp _strnicmp
#endif

#include <math.h>
#ifndef M_PI
#  define M_PI 3.14159265358979323846
#endif

#ifdef WIN32
/* this hack applied only on attic version of MinGW platform */
#  if !defined(va_copy) && !defined(HAVE_VA_COPY)
#    define va_copy(DEST, SRC) ((DEST) = (SRC))
#  endif

#  include <io.h>
#  ifdef _MSC_VER
#    include <direct.h>
#  endif
#  ifdef HAVE_SYS_STAT_H
#    include <sys/stat.h>
#  endif
#  define mkdir(A, B) _mkdir((A))

#  ifndef S_ISDIR
#    define S_ISDIR(m) ((m) & _S_IFDIR)
#  endif

#  ifndef S_ISREG
#    define S_ISREG(m) ((m) & _S_IFREG)
#  endif

#  ifndef S_ISBLK
#    define S_ISBLK(m) 0
#  endif

#  ifndef S_ISCHR
#    define S_ISCHR(m) 0
#  endif

#  ifndef S_ISLNK
#    define S_ISLNK(mode)  0
#  endif

#  ifndef S_ISSOCK
#    define S_ISSOCK(mode) 0
#  endif

#  ifndef S_ISFIFO
#    define S_ISFIFO(mode) 0
#  endif

#  ifndef S_IXUSR
#    define S_IXUSR S_IEXEC
#  endif

#  ifndef S_IXGRP
#    define S_IXGRP S_IEXEC
#  endif

#  ifndef S_IXOTH
#    define S_IXOTH S_IEXEC
#  endif

#  if !S_IXUGO
#    define S_IXUGO (S_IXUSR | S_IXGRP | S_IXOTH)
#  endif

/*
 * workaround compatibility code due to 'near' and 'far' keywords in windef.h
 * (do it only inside ffmpeg)
 */
#  ifdef HAVE_AV_CONFIG_H
#    include <windows.h>
#    ifdef near
#      undef near
#    endif
#    ifdef far
#      undef far
#    endif
#    ifdef frm1
#      undef frm1
#    endif
     /* it sucks everywhere :-) */
#    define near win32_sucks_near
#    define far win32_sucks_far
#  endif /* av_config */

#endif

#ifndef HAVE_READLINK
#  define readlink(PATH, BUF, BUFSIZE) 0
#endif

/* replacing lstat by stat */
#ifndef HAVE_LSTAT
#  define HAVE_LSTAT
#  define lstat(FILENAME, BUF) stat((FILENAME), (BUF))
#endif

/* replacements of dirent for MSVC platform */
#ifndef HAVE_OPENDIR
#define HAVE_OPENDIR
typedef struct DIR DIR;

struct dirent {
  unsigned short d_reclen;
  char *d_name;
};

DIR           *xine_private_opendir(const char *);
int           xine_private_closedir(DIR *);
struct dirent *xine_private_readdir(DIR *);
void          xine_private_rewinddir(DIR *);

#define opendir(DIRENT_NAME) xine_private_opendir((DIRENT_NAME))
#define closedir(DIRENT_DIR) xine_private_closedir((DIRENT_DIR))
#define readdir(DIRENT_DIR) xine_private_readdir((DIRENT_DIR))
#define rewinddir(DIRENT_DIR) xine_private_rewinddir((DIRENT_DIR))

#endif

#endif
