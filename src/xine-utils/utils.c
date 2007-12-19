/*
 * Copyright (C) 2000-2005 the xine project
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
#define	_POSIX_PTHREAD_SEMANTICS 1	/* for 5-arg getpwuid_r on solaris */

/*
#define LOG
*/

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <xine/xineutils.h>
#include <xine/xineintl.h>
#ifdef _MSC_VER
#include <xine/xine_internal.h>
#endif

#include <errno.h>
#include <pwd.h>
#include <sys/time.h>
#include <time.h>
#include <unistd.h>

#if HAVE_EXECINFO_H
#include <execinfo.h>
#endif

#if HAVE_UCONTEXT_H
#include <ucontext.h>
#endif

#ifdef HAVE_NL_LANGINFO
#include <langinfo.h>
#endif

#if defined(__CYGWIN__) || defined(WIN32)
#include <ctype.h>
#include <windows.h>
#endif

typedef struct {
  char                    *language;     /* name of the locale */
  char                    *encoding;     /* typical encoding */
  char                    *spu_encoding; /* default spu encoding */
  char                    *modifier;
} lang_locale_t;


/*
 * information about locales used in xine
 */
static const lang_locale_t lang_locales[] = {
  { "af_ZA",    "iso-8859-1",  "iso-8859-1",  NULL       },
  { "ar_AE",    "iso-8859-6",  "iso-8859-6",  NULL       },
  { "ar_BH",    "iso-8859-6",  "iso-8859-6",  NULL       },
  { "ar_DZ",    "iso-8859-6",  "iso-8859-6",  NULL       },
  { "ar_EG",    "iso-8859-6",  "iso-8859-6",  NULL       },
  { "ar_IN",    "utf-8",       "utf-8",       NULL       },
  { "ar_IQ",    "iso-8859-6",  "iso-8859-6",  NULL       },
  { "ar_JO",    "iso-8859-6",  "iso-8859-6",  NULL       },
  { "ar_KW",    "iso-8859-6",  "iso-8859-6",  NULL       },
  { "ar_LB",    "iso-8859-6",  "iso-8859-6",  NULL       },
  { "ar_LY",    "iso-8859-6",  "iso-8859-6",  NULL       },
  { "ar_MA",    "iso-8859-6",  "iso-8859-6",  NULL       },
  { "ar_OM",    "iso-8859-6",  "iso-8859-6",  NULL       },
  { "ar_QA",    "iso-8859-6",  "iso-8859-6",  NULL       },
  { "ar_SA",    "iso-8859-6",  "iso-8859-6",  NULL       },
  { "ar_SD",    "iso-8859-6",  "iso-8859-6",  NULL       },
  { "ar_SY",    "iso-8859-6",  "iso-8859-6",  NULL       },
  { "ar_TN",    "iso-8859-6",  "iso-8859-6",  NULL       },
  { "ar_YE",    "iso-8859-6",  "iso-8859-6",  NULL       },
  { "be_BY",    "cp1251",      "cp1251",      NULL       },
  { "bg_BG",    "cp1251",      "cp1251",      NULL       },
  { "br_FR",    "iso-8859-1",  "iso-88591",   NULL       },
  { "bs_BA",    "iso-8859-2",  "cp1250",      NULL       },
  { "ca_ES",    "iso-8859-1",  "iso-88591",   NULL       },
  { "ca_ES",    "iso-8859-15", "iso-8859-15", "euro"     },
  { "cs_CZ",    "iso-8859-2",  "cp1250",      NULL       },
  { "cy_GB",    "iso-8859-14", "iso-8859-14", NULL       },
  { "da_DK",    "iso-8859-1",  "iso-8859-1",  NULL       },
  { "de_AT",    "iso-8859-1",  "iso-8859-1",  NULL       },
  { "de_AT",    "iso-8859-15", "iso-8859-15", "euro"     },
  { "de_BE",    "iso-8859-1",  "iso-8859-1",  NULL       },
  { "de_BE",    "iso-8859-15", "iso-8859-15", "euro"     },
  { "de_CH",    "iso-8859-1",  "iso-8859-1",  NULL       },
  { "de_DE",    "iso-8859-1",  "iso-8859-1",  NULL       },
  { "de_DE",    "iso-8859-15", "iso-8859-15", "euro"     },
  { "de_LU",    "iso-8859-1",  "iso-8859-1",  NULL       },
  { "de_LU",    "iso-8859-15", "iso-8859-15", "euro"     },
  { "el_GR",    "iso-8859-7",  "iso-8859-7",  NULL       },
  { "en_AU",    "iso-8859-1",  "iso-8859-1",  NULL       },
  { "en_BW",    "iso-8859-1",  "iso-8859-1",  NULL       },
  { "en_CA",    "iso-8859-1",  "iso-8859-1",  NULL       },
  { "en_DK",    "iso-8859-1",  "iso-8859-1",  NULL       },
  { "en_GB",    "iso-8859-1",  "iso-8859-1",  NULL       },
  { "en_HK",    "iso-8859-1",  "iso-8859-1",  NULL       },
  { "en_IE",    "iso-8859-1",  "iso-8859-1",  NULL       },
  { "en_IE",    "iso-8859-15", "iso-8859-15", "euro"     },
  { "en_IN",    "utf-8",       "utf-8",       NULL       },
  { "en_NZ",    "iso-8859-1",  "iso-8859-1",  NULL       },
  { "en_PH",    "iso-8859-1",  "iso-8859-1",  NULL       },
  { "en_SG",    "iso-8859-1",  "iso-8859-1",  NULL       },
  { "en_US",    "iso-8859-1",  "iso-8859-1",  NULL       },
  { "en_ZA",    "iso-8859-1",  "iso-8859-1",  NULL       },
  { "en_ZW",    "iso-8859-1",  "iso-8859-1",  NULL       },
  { "es_AR",    "iso-8859-1",  "iso-8859-1",  NULL       },
  { "es_BO",    "iso-8859-1",  "iso-8859-1",  NULL       },
  { "es_CL",    "iso-8859-1",  "iso-8859-1",  NULL       },
  { "es_CO",    "iso-8859-1",  "iso-8859-1",  NULL       },
  { "es_CR",    "iso-8859-1",  "iso-8859-1",  NULL       },
  { "es_DO",    "iso-8859-1",  "iso-8859-1",  NULL       },
  { "es_EC",    "iso-8859-1",  "iso-8859-1",  NULL       },
  { "es_ES",    "iso-8859-1",  "iso-8859-1",  NULL       },
  { "es_ES",    "iso-8859-15", "iso-8859-15", "euro"     },
  { "es_GT",    "iso-8859-1",  "iso-8859-1",  NULL       },
  { "es_HN",    "iso-8859-1",  "iso-8859-1",  NULL       },
  { "es_MX",    "iso-8859-1",  "iso-8859-1",  NULL       },
  { "es_NI",    "iso-8859-1",  "iso-8859-1",  NULL       },
  { "es_PA",    "iso-8859-1",  "iso-8859-1",  NULL       },
  { "es_PE",    "iso-8859-1",  "iso-8859-1",  NULL       },
  { "es_PR",    "iso-8859-1",  "iso-8859-1",  NULL       },
  { "es_PY",    "iso-8859-1",  "iso-8859-1",  NULL       },
  { "es_SV",    "iso-8859-1",  "iso-8859-1",  NULL       },
  { "es_US",    "iso-8859-1",  "iso-8859-1",  NULL       },
  { "es_UY",    "iso-8859-1",  "iso-8859-1",  NULL       },
  { "es_VE",    "iso-8859-1",  "iso-8859-1",  NULL       },
  { "et_EE",    "iso-8859-1",  "iso-8859-1",  NULL       },
  { "eu_ES",    "iso-8859-1",  "iso-8859-1",  NULL       },
  { "eu_ES",    "iso-8859-15", "iso-8859-15", "euro"     },
  { "fa_IR",    "utf-8",       "utf-8",       NULL       },
  { "fi_FI",    "iso-8859-1",  "iso-8859-1",  NULL       },
  { "fi_FI",    "iso-8859-15", "iso-8859-15", "euro"     },
  { "fo_FO",    "iso-8859-1",  "iso-8859-1",  NULL       },
  { "fr_BE",    "iso-8859-1",  "iso-8859-1",  NULL       },
  { "fr_BE",    "iso-8859-15", "iso-8859-15", "euro"     },
  { "fr_CA",    "iso-8859-1",  "iso-8859-1",  NULL       },
  { "fr_CH",    "iso-8859-1",  "iso-8859-1",  NULL       },
  { "fr_FR",    "iso-8859-1",  "iso-8859-1",  NULL       },
  { "fr_FR",    "iso-8859-15", "iso-8859-15", "euro"     },
  { "fr_LU",    "iso-8859-1",  "iso-8859-1",  NULL       },
  { "fr_LU",    "iso-8859-15", "iso-8859-15", "euro"     },
  { "ga_IE",    "iso-8859-1",  "iso-8859-1",  NULL       },
  { "ga_IE",    "iso-8859-15", "iso-8859-15", "euro"     },
  { "gl_ES",    "iso-8859-1",  "iso-8859-1",  NULL       },
  { "gl_ES",    "iso-8859-15", "iso-8859-15", "euro"     },
  { "gv_GB",    "iso-8859-1",  "iso-8859-1",  NULL       },
  { "he_IL",    "iso-8859-8",  "iso-8859-8",  NULL       },
  { "hi_IN",    "utf-8",       "utf-8",       NULL       },
  { "hr_HR",    "iso-8859-2",  "cp1250",      NULL       },
  { "hu_HU",    "iso-8859-2",  "cp1250",      NULL       },
  { "id_ID",    "iso-8859-1",  "iso-8859-1",  NULL       },
  { "is_IS",    "iso-8859-1",  "iso-8859-1",  NULL       },
  { "it_CH",    "iso-8859-1",  "iso-8859-1",  NULL       },
  { "it_IT",    "iso-8859-1",  "iso-8859-1",  NULL       },
  { "it_IT",    "iso-8859-15", "iso-8859-15", "euro"     },
  { "iw_IL",    "iso-8859-8",  "iso-8859-8",  NULL       },
  { "ja_JP",    "euc-jp",      "euc-jp",      NULL       },
  { "ja_JP",    "ujis",        "ujis",        NULL       },
  { "japanese", "euc",         "euc",         NULL       },
  { "ka_GE",    "georgian-ps", "georgian-ps", NULL       },
  { "kl_GL",    "iso-8859-1",  "iso-8859-1",  NULL       },
  { "ko_KR",    "euc-kr",      "euc-kr",      NULL       },
  { "ko_KR",    "utf-8",       "utf-8",       NULL       },
  { "korean",   "euc",         "euc",         NULL       },
  { "kw_GB",    "iso-8859-1",  "iso-8859-1",  NULL       },
  { "lt_LT",    "iso-8859-13", "iso-8859-13", NULL       },
  { "lv_LV",    "iso-8859-13", "iso-8859-13", NULL       },
  { "mi_NZ",    "iso-8859-13", "iso-8859-13", NULL       },
  { "mk_MK",    "iso-8859-5",  "cp1251",      NULL       },
  { "mr_IN",    "utf-8",       "utf-8",       NULL       },
  { "ms_MY",    "iso-8859-1",  "iso-8859-1",  NULL       },
  { "mt_MT",    "iso-8859-3",  "iso-8859-3",  NULL       },
  { "nb_NO",    "ISO-8859-1",  "ISO-8859-1",  NULL       },
  { "nl_BE",    "iso-8859-1",  "iso-8859-1",  NULL       },
  { "nl_BE",    "iso-8859-15", "iso-8859-15", "euro"     },
  { "nl_NL",    "iso-8859-1",  "iso-8859-1",  NULL       },
  { "nl_NL",    "iso-8859-15", "iso-8859-15", "euro"     },
  { "nn_NO",    "iso-8859-1",  "iso-8859-1",  NULL       },
  { "no_NO",    "iso-8859-1",  "iso-8859-1",  NULL       },
  { "oc_FR",    "iso-8859-1",  "iso-8859-1",  NULL       },
  { "pl_PL",    "iso-8859-2",  "cp1250",      NULL       },
  { "pt_BR",    "iso-8859-1",  "iso-8859-1",  NULL       },
  { "pt_PT",    "iso-8859-1",  "iso-8859-1",  NULL       },
  { "pt_PT",    "iso-8859-15", "iso-8859-15", "euro"     },
  { "ro_RO",    "iso-8859-2",  "cp1250",      NULL       },
  { "ru_RU",    "iso-8859-5",  "cp1251",      NULL       },
  { "ru_RU",    "koi8-r",      "cp1251",      NULL       },
  { "ru_UA",    "koi8-u",      "cp1251",      NULL       },
  { "se_NO",    "utf-8",       "utf-8",       NULL       },
  { "sk_SK",    "iso-8859-2",  "cp1250",      NULL       },
  { "sl_SI",    "iso-8859-2",  "cp1250",      NULL       },
  { "sq_AL",    "iso-8859-1",  "iso-8859-1",  NULL       },
  { "sr_YU",    "iso-8859-2",  "cp1250",      NULL       },
  { "sr_YU",    "iso-8859-5",  "cp1251",      "cyrillic" },
  { "sv_FI",    "iso-8859-1",  "iso-8859-1",  NULL       },
  { "sv_FI",    "iso-8859-15", "iso-8859-15", "euro"     },
  { "sv_SE",    "iso-8859-1",  "iso-8859-1",  NULL       },
  { "ta_IN",    "utf-8",       "utf-8",       NULL       },
  { "te_IN",    "utf-8",       "utf-8",       NULL       },
  { "tg_TJ",    "koi8-t",      "cp1251",      NULL       },
  { "th_TH",    "tis-620",     "tis-620",     NULL       },
  { "tl_PH",    "iso-8859-1",  "iso-8859-1",  NULL       },
  { "tr_TR",    "iso-8859-9",  "iso-8859-9",  NULL       },
  { "uk_UA",    "koi8-u",      "cp1251",      NULL       },
  { "ur_PK",    "utf-8",       "utf-8",       NULL       },
  { "uz_UZ",    "iso-8859-1",  "iso-8859-1",  NULL       },
  { "vi_VN",    "tcvn",        "tcvn",        NULL       },
  { "vi_VN",    "utf-8",       "utf-8",       NULL       },
  { "wa_BE",    "iso-8859-1",  "iso-8859-1",  NULL       },
  { "wa_BE",    "iso-8859-15", "iso-8859-15", "euro"     },
  { "yi_US",    "cp1255",      "cp1255",      NULL       },
  { "zh_CN",    "gb18030",     "gb18030",     NULL       },
  { "zh_CN",    "gb2312",      "gb2312",      NULL       },
  { "zh_CN",    "gbk",         "gbk",         NULL       },
  { "zh_HK",    "big5-hkscs",  "big5-hkscs",  NULL       },
  { "zh_TW",    "big-5",       "big-5",       NULL       },
  { "zh_TW",    "euc-tw",      "euc-tw",      NULL       },
  { NULL,       NULL,          NULL,          NULL       }
};


void *xine_xmalloc(size_t size) {
  void *ptr;

  /* prevent xine_xmalloc(0) of possibly returning NULL */
  if( !size )
    size++;

  if((ptr = calloc(1, size)) == NULL) {
    fprintf(stderr, "%s: malloc() failed: %s.\n",
	    __XINE_FUNCTION__, strerror(errno));
    return NULL;
  }

  return ptr;
}

/**
 * @brief Wrapper around calloc() function.
 * @param nmemb Number of elements to allocate
 * @param size Size of each element to allocate
 *
 * This is a simple wrapper around calloc(), the only thing
 * it does more than calloc() is outputting an error if
 * the calloc fails (returning NULL).
 */
void *xine_xcalloc(size_t nmemb, size_t size) {
  void *ptr;

  if((ptr = calloc(nmemb, size)) == NULL) {
    fprintf(stderr, "%s: calloc() failed: %s.\n",
	    __XINE_FUNCTION__, strerror(errno));
    return NULL;
  }

  return ptr;
}

void *xine_xmalloc_aligned(size_t alignment, size_t size, void **base) {

  char *ptr;
  
  *base = ptr = xine_xmalloc (size+alignment);
  
  while ((size_t) ptr % alignment)
    ptr++;
    
  return ptr;
}

#ifdef WIN32
/*
 * Parse command line with Windows XP syntax and copy the command (argv[0]).
 */
static size_t xine_strcpy_command(const char *cmdline, char *cmd, size_t maxlen) {
  size_t i, j;

  i = 0;
  j = 0;
  while (cmdline[i] && isspace(cmdline[i])) i++;

  while (cmdline[i] && !isspace(cmdline[i]) && j + 2 < maxlen) {
    switch (cmdline[i]) {
    case '\"':
      i++;
      while (cmdline[i] && cmdline[i] != '\"') {
        if (cmdline[i] == '\\') {
          i++;
          if (cmdline[i] == '\"') cmd[j++] = '\"';
          else {
            cmd[j++] = '\\';
            cmd[j++] = cmdline[i];
          }
        } else cmd[j++] = cmdline[i];
        if (cmdline[i]) i++;
      }
      break;

    case '\\':
      i++;
      if (cmdline[i] == '\"') cmd[j++] = '\"';
      else {
        cmd[j++] = '\\';
        i--;
      }
      break;

    default:
      cmd[j++] = cmdline[i];
    }

    i++;
  }
  cmd[j] = '\0';

  return j;
}
#endif

#ifndef BUFSIZ
#define BUFSIZ 256
#endif

const char *xine_get_homedir(void) {
#ifdef WIN32
  static char homedir[1024] = {0, };
  char *s;
  int len;

  if (!homedir[0]) {
    len = xine_strcpy_command(GetCommandLine(), homedir, sizeof(homedir));
    s = strdup(homedir);
    GetFullPathName(s, sizeof(homedir), homedir, NULL);
    free(s);
    if ((s = strrchr(homedir, '\\'))) *s = '\0';
  }

  return homedir;
#else
  struct passwd pwd, *pw = NULL;
  static char homedir[BUFSIZ] = {0,};

  if(homedir[0])
    return homedir;

#ifdef HAVE_GETPWUID_R
  if(getpwuid_r(getuid(), &pwd, homedir, sizeof(homedir), &pw) != 0 || pw == NULL) {
#else
  if((pw = getpwuid(getuid())) == NULL) {
#endif
    char *tmp = getenv("HOME");
    if(tmp) {
      strncpy(homedir, tmp, sizeof(homedir));
      homedir[sizeof(homedir) - 1] = '\0';
    }
  } else {
    char *s = strdup(pw->pw_dir);
    strncpy(homedir, s, sizeof(homedir));
    homedir[sizeof(homedir) - 1] = '\0';
    free(s);
  }

  if(!homedir[0]) {
    printf("xine_get_homedir: Unable to get home directory, set it to /tmp.\n");
    strcpy(homedir, "/tmp");
  }

  return homedir;
#endif /* WIN32 */
}

#if defined(WIN32) || defined(__CYGWIN__)
static void xine_get_rootdir(char *rootdir, size_t maxlen) {
  char *s;

  strncpy(rootdir, xine_get_homedir(), maxlen - 1);
  rootdir[maxlen - 1] = '\0';
  if ((s = strrchr(rootdir, XINE_DIRECTORY_SEPARATOR_CHAR))) *s = '\0';
}

const char *xine_get_plugindir(void) {
  static char plugindir[1024] = {0, };

  if (!plugindir[0]) {
    xine_get_rootdir(plugindir, sizeof(plugindir) - strlen(XINE_REL_PLUGINDIR) - 1);
    strcat(plugindir, XINE_DIRECTORY_SEPARATOR_STRING XINE_REL_PLUGINDIR);
  }

  return plugindir;
}

const char *xine_get_fontdir(void) {
  static char fontdir[1024] = {0, };

  if (!fontdir[0]) {
    xine_get_rootdir(fontdir, sizeof(fontdir) - strlen(XINE_REL_FONTDIR) - 1);
    strcat(fontdir, XINE_DIRECTORY_SEPARATOR_STRING XINE_REL_FONTDIR);
  }

  return fontdir;
}

const char *xine_get_localedir(void) {
  static char localedir[1024] = {0, };

  if (!localedir[0]) {
    xine_get_rootdir(localedir, sizeof(localedir) - strlen(XINE_REL_LOCALEDIR) - 1);
    strcat(localedir, XINE_DIRECTORY_SEPARATOR_STRING XINE_REL_LOCALEDIR);
  }

  return localedir;
}
#endif

char *xine_chomp(char *str) {
  char *pbuf;

  pbuf = str;

  while(*pbuf != '\0') pbuf++;

  while(pbuf > str) {
    if(*pbuf == '\r' || *pbuf == '\n' || *pbuf == '"') *pbuf = '\0';
    pbuf--;
  }

  while(*pbuf == '=') pbuf++;

  return pbuf;
}


/*
 * a thread-safe usecond sleep
 */
void xine_usec_sleep(unsigned usec) {
#if 0
#if HAVE_NANOSLEEP
  /* nanosleep is prefered on solaris, because it's mt-safe */
  struct timespec ts, remaining;

  ts.tv_sec =   usec / 1000000;
  ts.tv_nsec = (usec % 1000000) * 1000;
  while (nanosleep (&ts, &remaining) == -1 && errno == EINTR)
    ts = remaining;
#else
#  if WIN32
  Sleep(usec / 1000);
#  else
  usleep(usec);
#  endif
#endif
#else
  if (usec < 10000) {
      usec = 10000;
  }
  struct timeval tm;
  tm.tv_sec  = usec / 1000000;
  tm.tv_usec = usec % 1000000;
  select(0, 0, 0, 0, &tm);
#endif
}


/* print a hexdump of length bytes from the data given in buf */
void xine_hexdump (const void *buf_gen, int length) {
  static const char separator[70] = "---------------------------------------------------------------------";

  const uint8_t *const buf = (const uint8_t*)buf;
  int j = 0;

  /* printf ("Hexdump: %i Bytes\n", length);*/
  puts(separator);

  while(j<length) {
    int i;
    const int imax = (j+16 < length) ? (j+16) : length;

    printf ("%04X ",j);
    for (i=j; i<j+16; i++) {
      if( i<length )
        printf ("%02X ", buf[i]);
      else
        printf("   ");
    }

    for (i=j; i < imax; i++) {
      uint8_t c = buf[i];
      if ((c>=32) && (c<127))
	c = '.';

      fputc(c, stdout);
    }
    j=i;

    fputc('\n', stdout);
  }

  puts(separator);
}


static const lang_locale_t *_get_first_lang_locale(const char *lcal) {
  const lang_locale_t *llocale;
  int lang_len;
  char *mod;

  if(lcal && *lcal) {
    llocale = &*lang_locales;

    if ((mod = strchr(lcal, '@')))
      lang_len = mod++ - lcal;
    else
      lang_len = strlen(lcal);
    
    while(llocale->language) {
      if(!strncmp(lcal, llocale->language, lang_len)) {
        if ((!mod && !llocale->modifier) || (mod && llocale->modifier && !strcmp(mod, llocale->modifier)))
	  return llocale;
      }
      
      llocale++;
    }
  }
  return NULL;
}


static char *_get_lang(void) {
    char *lang;
    
    if(!(lang = getenv("LC_ALL")))
      if(!(lang = getenv("LC_MESSAGES")))
        lang = getenv("LANG");

  return lang;
}


/*
 * get encoding of current locale
 */
char *xine_get_system_encoding(void) {
  char *codeset = NULL;
  
#ifdef HAVE_NL_LANGINFO
  setlocale(LC_ALL, "");
  codeset = nl_langinfo(CODESET);
#endif
  /*
   * guess locale codeset according to shell variables
   * when nl_langinfo(CODESET) isn't available or workig
   */
  if (!codeset || strstr(codeset, "ANSI") != 0) {
    char *lang = _get_lang();

    codeset = NULL;

    if(lang) {
      char *lg, *enc, *mod;

      lg = strdup(lang);

      if((enc = strchr(lg, '.')) && (strlen(enc) > 1)) {
        enc++;

        if((mod = strchr(enc, '@')))
          *mod = '\0';

        codeset = strdup(enc);
      }
      else {
        const lang_locale_t *llocale = _get_first_lang_locale(lg);

        if(llocale && llocale->encoding)
          codeset = strdup(llocale->encoding);
      }

      free(lg);
    }
  } else
    codeset = strdup(codeset);

  return codeset;
}


/*
 * guess default encoding of subtitles
 */
const char *xine_guess_spu_encoding(void) {
  char *lang = _get_lang();

  if (lang) {
    const lang_locale_t *llocale;
    char *lg, *enc;

    lg = strdup(lang);

    if ((enc = strchr(lg, '.'))) *enc = '\0';
    llocale = _get_first_lang_locale(lg);
    free(lg);
    if (llocale && llocale->spu_encoding) return llocale->spu_encoding;
  }

  return "iso-8859-1";
}


#ifdef _MSC_VER
void xine_xprintf(xine_t *xine, int verbose, const char *fmt, ...) {
  char message[256];
  va_list ap;

  if (xine && xine->verbosity >= verbose) {
    va_start(ap, fmt);
    vsnprintf(message, sizeof(message), fmt, ap);
    va_end(ap);
    xine_log(xine, XINE_LOG_TRACE, "%s", message);
  }
}
#endif

int xine_monotonic_clock(struct timeval *tv, struct timezone *tz)
{
#if _POSIX_TIMERS > 0 && defined(_POSIX_MONOTONIC_CLOCK) && defined(HAVE_POSIX_TIMERS)
  static int initialized = 0;
  static int use_clock_monotonic = 0;
     
  struct timespec tp;
  
  if( !initialized ) { 
    struct timespec res;
    int ret;
  
    ret = clock_getres(CLOCK_MONOTONIC, &res);
    
    if( ret != 0 ) {
      lprintf("get resolution of monotonic clock failed\n");
    } else {
      /* require at least milisecond resolution */
      if( res.tv_sec > 0 ||
          res.tv_nsec > 1000000 ) {
        lprintf("monotonic clock resolution (%d:%d) too bad\n",
                 (int)res.tv_sec, (int)res.tv_nsec);
      } else {
        if( clock_gettime(CLOCK_MONOTONIC, &tp) != 0 ) {
          lprintf("get monotonic clock failed\n");
        } else {
          lprintf("using monotonic clock\n");
          use_clock_monotonic = 1;
        }
      }
    }
    initialized = 1;
  }  
  
  if(use_clock_monotonic && !clock_gettime(CLOCK_MONOTONIC, &tp)) {
    tv->tv_sec = tp.tv_sec;
    tv->tv_usec = tp.tv_nsec / 1000;
    return 0;
  } else {
    return gettimeofday(tv, tz);
  }

#else
  
  return gettimeofday(tv, tz);

#endif
}
