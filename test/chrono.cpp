/*
 * test/chrono_cpptest.cpp
 *
 * Registers ChronoCppTest module methods exercising the C++ API.
 * Called from mrb_mruby_chrono_gem_test in chrono_ctest.c.
 * Assertions live in test/chrono.rb.
 */

#include <mruby.h>
#include <mruby/class.h>
#include <mruby/numeric.h>
#include <mruby/chrono.hpp>
#include <chrono>

 /* ------------------------------------------------------------------ */
 /*  mrb_chrono::from                                                   */
 /* ------------------------------------------------------------------ */

#define DEF_FROM(name, ChronoType) \
  static mrb_value cpptest_from_##name(mrb_state* mrb, mrb_value) { \
    mrb_int n; mrb_get_args(mrb, "i", &n);                           \
    return mrb_chrono::from(mrb, ChronoType(n));                     \
  }

DEF_FROM(ms, std::chrono::milliseconds)
DEF_FROM(us, std::chrono::microseconds)
DEF_FROM(ns, std::chrono::nanoseconds)
DEF_FROM(s, std::chrono::seconds)
DEF_FROM(h, std::chrono::hours)
#undef DEF_FROM

static mrb_value
cpptest_steady_now(mrb_state* mrb, mrb_value)
{
  return mrb_chrono::from(mrb,
    std::chrono::steady_clock::now().time_since_epoch());
}

/* ------------------------------------------------------------------ */
/*  mrb_chrono::as / floor / ceil / round                              */
/* ------------------------------------------------------------------ */

#define DEF_AS(suffix, fn, ChronoType) \
  static mrb_value cpptest_as_##suffix(mrb_state* mrb, mrb_value) { \
    mrb_value v; mrb_get_args(mrb, "o", &v);                         \
    return mrb_int_value(mrb, (mrb_int)                              \
      mrb_chrono::fn<ChronoType>(mrb, v).count());                   \
  }

DEF_AS(ms, as, std::chrono::milliseconds)
DEF_AS(us, as, std::chrono::microseconds)
DEF_AS(ns, as, std::chrono::nanoseconds)
DEF_AS(h, as, std::chrono::hours)
DEF_AS(ms_floor, floor, std::chrono::milliseconds)
DEF_AS(us_floor, floor, std::chrono::microseconds)
DEF_AS(ms_ceil, ceil, std::chrono::milliseconds)
DEF_AS(us_ceil, ceil, std::chrono::microseconds)
DEF_AS(us_round, round, std::chrono::microseconds)
#undef DEF_AS

/* ------------------------------------------------------------------ */
/*  Registration                                                       */
/* ------------------------------------------------------------------ */

MRB_BEGIN_DECL

void
mrb_chrono_register_cpp_tests(mrb_state* mrb)
{
  struct RClass* mod = mrb_define_module(mrb, "ChronoCppTest");

  mrb_define_module_function(mrb, mod, "from_ms", cpptest_from_ms, MRB_ARGS_REQ(1));
  mrb_define_module_function(mrb, mod, "from_us", cpptest_from_us, MRB_ARGS_REQ(1));
  mrb_define_module_function(mrb, mod, "from_ns", cpptest_from_ns, MRB_ARGS_REQ(1));
  mrb_define_module_function(mrb, mod, "from_s", cpptest_from_s, MRB_ARGS_REQ(1));
  mrb_define_module_function(mrb, mod, "from_h", cpptest_from_h, MRB_ARGS_REQ(1));
  mrb_define_module_function(mrb, mod, "steady_now", cpptest_steady_now, MRB_ARGS_NONE());
  mrb_define_module_function(mrb, mod, "as_ms", cpptest_as_ms, MRB_ARGS_REQ(1));
  mrb_define_module_function(mrb, mod, "as_us", cpptest_as_us, MRB_ARGS_REQ(1));
  mrb_define_module_function(mrb, mod, "as_ns", cpptest_as_ns, MRB_ARGS_REQ(1));
  mrb_define_module_function(mrb, mod, "as_h", cpptest_as_h, MRB_ARGS_REQ(1));
  mrb_define_module_function(mrb, mod, "as_ms_floor", cpptest_as_ms_floor, MRB_ARGS_REQ(1));
  mrb_define_module_function(mrb, mod, "as_us_floor", cpptest_as_us_floor, MRB_ARGS_REQ(1));
  mrb_define_module_function(mrb, mod, "as_ms_ceil", cpptest_as_ms_ceil, MRB_ARGS_REQ(1));
  mrb_define_module_function(mrb, mod, "as_us_ceil", cpptest_as_us_ceil, MRB_ARGS_REQ(1));
  mrb_define_module_function(mrb, mod, "as_us_round", cpptest_as_us_round, MRB_ARGS_REQ(1));
}

MRB_END_DECL