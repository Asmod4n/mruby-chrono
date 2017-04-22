/*
 * Author:  David Robert Nadeau
 * Site:    http://NadeauSoftware.com/
 * License: Creative Commons Attribution 3.0 Unported License
 *          http://creativecommons.org/licenses/by/3.0/deed.en_US
 *
 * Changes by Hendrik Beskow: change tab so spaces, remove all wall clocks, fill timeConvert at library load on macOS
 */
#include <errno.h>
#ifdef _WIN32
#define VC_EXTRALEAN
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include <assert.h>
#elif defined(__unix__) || defined(__unix) || defined(unix) || (defined(__APPLE__) && defined(__MACH__))
#include <unistd.h> /* POSIX flags */
#include <time.h> /* clock_gettime(), time() */
#include <sys/time.h> /* gethrtime(), gettimeofday() */

#if defined(__MACH__) && defined(__APPLE__)
#include <mach/mach.h>
#include <mach/mach_time.h>
#include <assert.h>
static mrb_float timeConvert;
static void __attribute__((constructor)) sTimebaseInfo_init()
{
  mach_timebase_info_data_t timeBase;
  mach_timebase_info(&timeBase);
  assert(timeBase.denom != 0 && timeBase.numer != 0);
  timeConvert = (mrb_float)timeBase.numer /
    (mrb_float)timeBase.denom /
    (mrb_float)NSEC_PER_SEC;
}
#endif
#else
#error "Unable to define getRealTime( ) for an unknown OS."
#endif





/**
 * Returns the real time, in seconds, or -1.0 if an error occurred.
 *
 * Time is measured since an arbitrary and OS-dependent start time.
 * The returned real time is only useful for computing an elapsed time
 * between two calls to this function.
 */
mrb_float getRealTime()
{
#if defined(_WIN32)
  static mrb_float frequency = 0.0;

  if (frequency == 0.0) {
    LARGE_INTEGER freq;
    QueryPerformanceFrequency(&freq);
    assert (freq.QuadPart != 0);
    frequency = (mrb_float) freq.QuadPart;
  }
  LARGE_INTEGER count;
  QueryPerformanceCounter(&count);

  return (mrb_float)count.QuadPart / frequency;

#elif (defined(__hpux) || defined(hpux)) || ((defined(__sun__) || defined(__sun) || defined(sun)) && (defined(__SVR4) || defined(__svr4__)))
  /* HP-UX, Solaris. ------------------------------------------ */
  return (mrb_float)gethrtime( ) / (mrb_float)NSEC_PER_SEC;

#elif defined(__MACH__) && defined(__APPLE__)
  /* OSX. ----------------------------------------------------- */
  return (mrb_float)mach_absolute_time() * timeConvert;

#elif defined(_POSIX_VERSION) && defined(_POSIX_TIMERS) && (_POSIX_TIMERS > 0)
  /* POSIX. --------------------------------------------------- */
  struct timespec ts;
#if defined(CLOCK_MONOTONIC_PRECISE)
  /* BSD. --------------------------------------------- */
  const clockid_t id = CLOCK_MONOTONIC_PRECISE;
#elif defined(CLOCK_MONOTONIC_RAW)
  /* Linux. ------------------------------------------- */
  const clockid_t id = CLOCK_MONOTONIC_RAW;
#elif defined(CLOCK_HIGHRES)
  /* Solaris. ----------------------------------------- */
  const clockid_t id = CLOCK_HIGHRES;
#elif defined(CLOCK_MONOTONIC)
  /* AIX, BSD, Linux, POSIX, Solaris. ----------------- */
  const clockid_t id = CLOCK_MONOTONIC;
#else
  const clockid_t id = (clockid_t)-1; /* Unknown. */
#endif /* CLOCKS * */
  if ( id != (clockid_t)-1 && clock_gettime( id, &ts ) != -1 )
    return (mrb_float)ts.tv_sec +
      (mrb_float)ts.tv_nsec / (mrb_float)NSEC_PER_SEC;
#endif /* POSIX * */
  errno = EFAULT;
  return -1.0;    /* Failed. */
}

