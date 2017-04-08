#include <mruby.h>
#ifndef _MSC_VER
#include <unistd.h>
#endif

#if _POSIX_TIMERS > 0 && defined(_POSIX_MONOTONIC_CLOCK)
#include <time.h>

static mrb_value
mrb_chrono_steady_now(mrb_state *mrb, mrb_value self)
{
  struct timespec ts;

  clock_gettime(CLOCK_MONOTONIC, &ts);

  return mrb_float_value(mrb, ts.tv_sec + (ts.tv_nsec / 1000000000.0));
}

#elif defined(__MACH__)
#include <mach/mach_time.h>

static mach_timebase_info_data_t sTimebaseInfo;
static void __attribute__((constructor)) sTimebaseInfo_init()
{
  mach_timebase_info(&sTimebaseInfo);
}

static mrb_value
mrb_chrono_steady_now(mrb_state *mrb, mrb_value self)
{
  return mrb_float_value(mrb, (mach_absolute_time() * sTimebaseInfo.numer / sTimebaseInfo.denom) / 1000000000.0);
}

#elif defined(_MSC_VER)
#define VC_EXTRALEAN
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <assert.h>

static mrb_value
mrb_chrono_steady_now(mrb_state *mrb, mrb_value self)
{
  static mrb_float frequency = 0.0;

  if (frequency == 0.0) {
    LARGE_INTEGER freq;
    QueryPerformanceFrequency(&freq);
    assert (freq.QuadPart != 0);
    frequency = (mrb_float) freq.QuadPart;
  }

  LARGE_INTEGER count;
  QueryPerformanceCounter(&count);

  return mrb_float_value(mrb, count.QuadPart / frequency);
}

#else
#error "cannot find a monotonic time clock"
#endif

#ifndef _MSC_VER
#include <sys/time.h>
#endif

static mrb_value
mrb_chrono_system_now(mrb_state *mrb, mrb_value self)
{
#ifdef _MSC_VER
  FILETIME ft;
  GetSystemTimeAsFileTime(&ft);

  ULARGE_INTEGER dateTime;
  memcpy(&dateTime, &ft, sizeof(dateTime));

  return mrb_float_value(mrb, dateTime.QuadPart / 10000000.0);
#elif _POSIX_TIMERS > 0
  struct timespec ts;

  clock_gettime(CLOCK_REALTIME, &ts);

  return mrb_float_value(mrb, ts.tv_sec + (ts.tv_nsec / 1000000000.0));
#else
  struct timeval tv;
  gettimeofday(&tv, NULL);

  return mrb_float_value(mrb, tv.tv_sec + (tv.tv_usec / 1000000.0));
#endif
}

void
mrb_mruby_chrono_gem_init(mrb_state* mrb)
{
  struct RClass *chrono_mod, *steady_mod, *system_mod;
  chrono_mod = mrb_define_module(mrb, "Chrono");

  steady_mod = mrb_define_module_under(mrb, chrono_mod, "Steady");
  mrb_define_module_function(mrb, steady_mod, "now", mrb_chrono_steady_now, MRB_ARGS_NONE());

  system_mod = mrb_define_module_under(mrb, chrono_mod, "System");
  mrb_define_module_function(mrb, system_mod, "now", mrb_chrono_system_now, MRB_ARGS_NONE());
}

void mrb_mruby_chrono_gem_final(mrb_state* mrb) {}
