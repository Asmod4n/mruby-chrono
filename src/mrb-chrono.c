#include <mruby.h>
#include <unistd.h>

#if _POSIX_TIMERS > 0 && defined(_POSIX_MONOTONIC_CLOCK)
#include <time.h>

MRB_INLINE mrb_float
mrb_chrono_monotonic_now()
{
  struct timespec ts;

  clock_gettime(CLOCK_MONOTONIC, &ts);

  return (mrb_float) ts.tv_sec + ((mrb_float) ts.tv_nsec / 1000000000.0));
}

#elif defined(__MACH__)
#include <mach/mach_time.h>

static mach_timebase_info_data_t sTimebaseInfo;
static void __attribute__((constructor)) sTimebaseInfo_init()
{
  mach_timebase_info(&sTimebaseInfo);
}

MRB_INLINE mrb_float
mrb_chrono_monotonic_now()
{
  return (mrb_float) mach_absolute_time() * (mrb_float) sTimebaseInfo.numer / (mrb_float) sTimebaseInfo.denom / 1000000000.0;
}

#elif defined(_MSC_VER)
#include <windows.h>
#include <assert.h>

MRB_INLINE mrb_float
mrb_chrono_monotonic_now()
{
  static mrb_float frequency = 0.0;

  if (frequency == 0.0) {
    LARGE_INTEGER freq;
    QueryPerformanceFrequency (&freq);
    assert (freq.QuadPart != 0);
    frequency = (mrb_float) freq.QuadPart / 1000.0;
  }

  LARGE_INTEGER count;
  QueryPerformanceCounter (&count);

  return (mrb_float) cound.QuadPart / frequency;
}

#else
#error "cannot find a monotonic time source"
#endif

static mrb_value
mrb_chrono_steady_now(mrb_state *mrb, mrb_value self)
{
  return mrb_float_value(mrb, mrb_chrono_monotonic_now());
}

void
mrb_mruby_chrono_gem_init(mrb_state* mrb)
{
  struct RClass *chrono_mod, *steady_mod;
  chrono_mod = mrb_define_module(mrb, "Chrono");
  steady_mod = mrb_define_module_under(mrb, chrono_mod, "Steady");

  mrb_define_module_function(mrb, steady_mod, "now", mrb_chrono_steady_now, MRB_ARGS_NONE());
}

void mrb_mruby_chrono_gem_final(mrb_state* mrb) {}
