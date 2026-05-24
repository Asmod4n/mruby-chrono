/*
 * test/chrono_ctest.c
 *
 * Registers ChronoCTest as a Ruby module whose methods exercise the C API
 * (mrb_chrono_from, mrb_chrono_convert). The actual assertions live in
 * test/chrono.rb which calls these methods and checks return values with
 * mrbtest's assert_equal / assert_float / assert_raise.
 */

#include <mruby.h>
#include <mruby/array.h>
#include <mruby/class.h>
#include <mruby/numeric.h>
#include <mruby/chrono.h>

 /* ------------------------------------------------------------------ */
 /*  mrb_chrono_from wrappers                                           */
 /* ------------------------------------------------------------------ */

 /* ChronoCTest.from(value, dur_type) -> Float seconds */
static mrb_value
ctest_from(mrb_state* mrb, mrb_value self)
{
  mrb_value v;
  mrb_int   dur;
  mrb_get_args(mrb, "oi", &v, &dur);
  return mrb_chrono_from(mrb, v, (mrb_chrono_dur_type)dur);
}

/* ------------------------------------------------------------------ */
/*  mrb_chrono_convert wrappers — one per output type                  */
/* ------------------------------------------------------------------ */

/* ChronoCTest.to_int32(value, dur_type, rounding) -> Integer */
static mrb_value
ctest_to_int32(mrb_state* mrb, mrb_value self)
{
  mrb_value v; mrb_int dur, rnd;
  mrb_get_args(mrb, "oii", &v, &dur, &rnd);
  int32_t out;
  mrb_chrono_convert(mrb, v,
    MRB_CHRONO_OUT_INT32, (mrb_chrono_dur_type)dur, (mrb_chrono_rounding)rnd,
    &out, sizeof out);
  return mrb_int_value(mrb, (mrb_int)out);
}

/* ChronoCTest.to_int64(value, dur_type, rounding) -> Integer */
static mrb_value
ctest_to_int64(mrb_state* mrb, mrb_value self)
{
  mrb_value v; mrb_int dur, rnd;
  mrb_get_args(mrb, "oii", &v, &dur, &rnd);
  int64_t out;
  mrb_chrono_convert(mrb, v,
    MRB_CHRONO_OUT_INT64, (mrb_chrono_dur_type)dur, (mrb_chrono_rounding)rnd,
    &out, sizeof out);
  return mrb_int_value(mrb, (mrb_int)out);
}

/* ChronoCTest.to_long(value, dur_type, rounding) -> Integer */
static mrb_value
ctest_to_long(mrb_state* mrb, mrb_value self)
{
  mrb_value v; mrb_int dur, rnd;
  mrb_get_args(mrb, "oii", &v, &dur, &rnd);
  long out;
  mrb_chrono_convert(mrb, v,
    MRB_CHRONO_OUT_LONG, (mrb_chrono_dur_type)dur, (mrb_chrono_rounding)rnd,
    &out, sizeof out);
  return mrb_int_value(mrb, (mrb_int)out);
}

/* ChronoCTest.to_double(value, dur_type, rounding) -> Float */
static mrb_value
ctest_to_double(mrb_state* mrb, mrb_value self)
{
  mrb_value v; mrb_int dur, rnd;
  mrb_get_args(mrb, "oii", &v, &dur, &rnd);
  double out;
  mrb_chrono_convert(mrb, v,
    MRB_CHRONO_OUT_DOUBLE, (mrb_chrono_dur_type)dur, (mrb_chrono_rounding)rnd,
    &out, sizeof out);
  return mrb_float_value(mrb, (mrb_float)out);
}

/* ChronoCTest.to_timespec(value, dur_type, rounding) -> [tv_sec, tv_nsec] */
static mrb_value
ctest_to_timespec(mrb_state* mrb, mrb_value self)
{
  mrb_value v; mrb_int dur, rnd;
  mrb_get_args(mrb, "oii", &v, &dur, &rnd);
  struct timespec ts;
  mrb_chrono_convert(mrb, v,
    MRB_CHRONO_OUT_TIMESPEC, (mrb_chrono_dur_type)dur, (mrb_chrono_rounding)rnd,
    &ts, sizeof ts);
  mrb_value pair[2];
  pair[0] = mrb_int_value(mrb, (mrb_int)ts.tv_sec);
  pair[1] = mrb_int_value(mrb, (mrb_int)ts.tv_nsec);
  return mrb_ary_new_from_values(mrb, 2, pair);
}

/* ChronoCTest.to_timeval(value, dur_type, rounding) -> [tv_sec, tv_usec] */
static mrb_value
ctest_to_timeval(mrb_state* mrb, mrb_value self)
{
  mrb_value v; mrb_int dur, rnd;
  mrb_get_args(mrb, "oii", &v, &dur, &rnd);
  struct timeval tv;
  mrb_chrono_convert(mrb, v,
    MRB_CHRONO_OUT_TIMEVAL, (mrb_chrono_dur_type)dur, (mrb_chrono_rounding)rnd,
    &tv, sizeof tv);
  mrb_value pair[2];
  pair[0] = mrb_int_value(mrb, (mrb_int)tv.tv_sec);
  pair[1] = mrb_int_value(mrb, (mrb_int)tv.tv_usec);
  return mrb_ary_new_from_values(mrb, 2, pair);
}

/* ------------------------------------------------------------------ */
/*  gem_test entry point                                               */
/* ------------------------------------------------------------------ */

void
mrb_mruby_chrono_gem_test(mrb_state* mrb)
{
  /* C++ API tests registered from chrono_cpptest.cpp */
  void mrb_chrono_register_cpp_tests(mrb_state*);
  mrb_chrono_register_cpp_tests(mrb);

  struct RClass* mod = mrb_define_module(mrb, "ChronoCTest");

  /* Enum constants so the Ruby test file can refer to them by name */
  mrb_define_const(mrb, mod, "DUR_NANOSECONDS", mrb_int_value(mrb, MRB_CHRONO_DUR_NANOSECONDS));
  mrb_define_const(mrb, mod, "DUR_MICROSECONDS", mrb_int_value(mrb, MRB_CHRONO_DUR_MICROSECONDS));
  mrb_define_const(mrb, mod, "DUR_MILLISECONDS", mrb_int_value(mrb, MRB_CHRONO_DUR_MILLISECONDS));
  mrb_define_const(mrb, mod, "DUR_SECONDS", mrb_int_value(mrb, MRB_CHRONO_DUR_SECONDS));
  mrb_define_const(mrb, mod, "DUR_MINUTES", mrb_int_value(mrb, MRB_CHRONO_DUR_MINUTES));
  mrb_define_const(mrb, mod, "DUR_HOURS", mrb_int_value(mrb, MRB_CHRONO_DUR_HOURS));

  mrb_define_const(mrb, mod, "TRUNC", mrb_int_value(mrb, MRB_CHRONO_TRUNC));
  mrb_define_const(mrb, mod, "FLOOR", mrb_int_value(mrb, MRB_CHRONO_FLOOR));
  mrb_define_const(mrb, mod, "CEIL", mrb_int_value(mrb, MRB_CHRONO_CEIL));
  mrb_define_const(mrb, mod, "NEAREST", mrb_int_value(mrb, MRB_CHRONO_NEAREST));

  mrb_define_module_function(mrb, mod, "from", ctest_from, MRB_ARGS_REQ(2));
  mrb_define_module_function(mrb, mod, "to_int32", ctest_to_int32, MRB_ARGS_REQ(3));
  mrb_define_module_function(mrb, mod, "to_int64", ctest_to_int64, MRB_ARGS_REQ(3));
  mrb_define_module_function(mrb, mod, "to_long", ctest_to_long, MRB_ARGS_REQ(3));
  mrb_define_module_function(mrb, mod, "to_double", ctest_to_double, MRB_ARGS_REQ(3));
  mrb_define_module_function(mrb, mod, "to_timespec", ctest_to_timespec, MRB_ARGS_REQ(3));
  mrb_define_module_function(mrb, mod, "to_timeval", ctest_to_timeval, MRB_ARGS_REQ(3));
}