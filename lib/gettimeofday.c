/* replacement function of gettimeofday */

#include "config.h"

#ifdef HAVE_POSIX_TIMERS
#  include <time.h>
#else
#  include <sys/timeb.h>
#endif

#ifdef WIN32
#  include <winsock.h>
#else
#  include <sys/time.h>
#endif

int xine_private_gettimeofday(struct timeval *tv) {
#ifdef HAVE_POSIX_TIMERS
  struct timespec *ts;
  int r;
  r = clock_gettime (CLOCK_REALTIME, &ts);
  if (!r) {
    tv->tv_sec  = ts.tv_sec;
    tv->tv_usec = ts.tv_nsec / 1000;
  }
  return r;
}
#else
  struct timeb tp;

  ftime(&tp);
  tv->tv_sec = tp.time;
  tv->tv_usec = tp.millitm * 1000;

  return 0;
#endif
}
