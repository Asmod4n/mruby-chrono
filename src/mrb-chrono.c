#include <mruby.h>
#ifdef MRB_USE_FLOAT
#error "MRB_USE_FLOAT is too small for mruby-chrono"
#endif
#include <mruby/error.h>

#if (__GNUC__ >= 3) || (__INTEL_COMPILER >= 800) || defined(__clang__)
# define likely(x) __builtin_expect(!!(x), 1)
# define unlikely(x) __builtin_expect(!!(x), 0)
#else
# define likely(x) (x)
# define unlikely(x) (x)
#endif

#include "getRealTime.h"

#ifndef USEC_PER_SEC
#define USEC_PER_SEC 1000000UL
#endif

static mrb_value
mrb_chrono_steady_now(mrb_state *mrb, mrb_value self)
{
  mrb_float realtime = getRealTime();
  if (unlikely(realtime == -1.0)) {
    mrb_sys_fail(mrb, "getRealTime");
  }
  return mrb_float_value(mrb, realtime);
}

static mrb_value
mrb_chrono_system_now(mrb_state *mrb, mrb_value self)
{
#ifdef _WIN32
  FILETIME ft;
  ULONGLONG dateTime;
#if defined(NTDDI_WIN8) && NTDDI_VERSION >= NTDDI_WIN8
  GetSystemTimePreciseAsFileTime(&ft);
#else
  GetSystemTimeAsFileTime(&ft);
#endif
  dateTime = ((ULONGLONG) ft.dwHighDateTime << 32)|(ULONGLONG) ft.dwLowDateTime;

  return mrb_float_value(mrb, (mrb_float) dateTime / 10000000.0);
#elif defined(CLOCK_REALTIME)
  struct timespec ts;
  clock_gettime(CLOCK_REALTIME, &ts);

  return (mrb_float) ts.tv_sec + (mrb_float) ((mrb_float) ts.tv_nsec / (mrb_float)NSEC_PER_SEC));
#else
  struct timeval tv;
  gettimeofday(&tv, NULL);

  return mrb_float_value(mrb, (mrb_float) tv.tv_sec + (mrb_float) ((mrb_float) tv.tv_usec / (mrb_float)USEC_PER_SEC));
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
