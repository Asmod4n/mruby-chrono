#include <mruby.h>
#ifdef MRB_USE_FLOAT
#error "MRB_USE_FLOAT is too small for mruby-chrono"
#endif
#include <mruby/error.h>
#include "getRealTime.h"

static mrb_value
mrb_chrono_steady_now(mrb_state *mrb, mrb_value self)
{
  mrb_float realtime = getRealTime();
  if (realtime == -1.0) {
    mrb_sys_fail(mrb, "getRealTime");
  }
  return mrb_float_value(mrb, realtime);
}

static mrb_value
mrb_chrono_system_now(mrb_state *mrb, mrb_value self)
{
#ifdef _MSC_VER
  FILETIME ft;
  GetSystemTimeAsFileTime(&ft);

  ULARGE_INTEGER dateTime;
  memcpy(&dateTime, &ft, sizeof(dateTime));

  return mrb_float_value(mrb, dateTime.QuadPart / 10000000.0);
#elif defined(CLOCK_REALTIME)
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
