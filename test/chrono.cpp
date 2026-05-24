/*
 * test/chrono_cpptest.cpp
 *
 * Registers ChronoCppTest module methods that exercise the C++ API
 * (mrb_chrono::from, mrb_chrono::as). Called from mrb_mruby_chrono_gem_test
 * in chrono_ctest.c. Assertions live in test/chrono.rb.
 */

#include <mruby.h>
#include <mruby/class.h>
#include <mruby/numeric.h>
#include <mruby/chrono.hpp>
#include <chrono>

 /* ------------------------------------------------------------------ */
 /*  mrb_chrono::from — std::chrono::duration -> Float seconds          */
 /* ------------------------------------------------------------------ */

 /* ChronoCppTest.from_ms(n) -> Float */
static mrb_value
cpptest_from_ms(mrb_state* mrb, mrb_value)
{
  mrb_int n; mrb_get_args(mrb, "i", &n);
  return mrb_chrono::from(mrb, std::chrono::milliseconds(n));
}

/* ChronoCppTest.from_us(n) -> Float */
static mrb_value
cpptest_from_us(mrb_state* mrb, mrb_value)
{
  mrb_int n; mrb_get_args(mrb, "i", &n);
  return mrb_chrono::from(mrb, std::chrono::microseconds(n));
}

/* ChronoCppTest.from_ns(n) -> Float */
static mrb_value
cpptest_from_ns(mrb_state* mrb, mrb_value)
{
  mrb_int n; mrb_get_args(mrb, "i", &n);
  return mrb_chrono::from(mrb, std::chrono::nanoseconds(n));
}

/* ChronoCppTest.from_s(n) -> Float */
static mrb_value
cpptest_from_s(mrb_state* mrb, mrb_value)
{
  mrb_int n; mrb_get_args(mrb, "i", &n);
  return mrb_chrono::from(mrb, std::chrono::seconds(n));
}

/* ChronoCppTest.from_h(n) -> Float */
static mrb_value
cpptest_from_h(mrb_state* mrb, mrb_value)
{
  mrb_int n; mrb_get_args(mrb, "i", &n);
  return mrb_chrono::from(mrb, std::chrono::hours(n));
}

/* ChronoCppTest.steady_now -> Float (just checks it's positive) */
static mrb_value
cpptest_steady_now(mrb_state* mrb, mrb_value)
{
  return mrb_chrono::from(mrb,
    std::chrono::steady_clock::now().time_since_epoch());
}

/* ------------------------------------------------------------------ */
/*  mrb_chrono::as — Float seconds -> duration count as Integer        */
/* ------------------------------------------------------------------ */

/* ChronoCppTest.as_ms(v) -> Integer */
static mrb_value
cpptest_as_ms(mrb_state* mrb, mrb_value)
{
  mrb_value v; mrb_get_args(mrb, "o", &v);
  return mrb_int_value(mrb, (mrb_int)
    mrb_chrono::as<std::chrono::milliseconds>(mrb, v).count());
}

/* ChronoCppTest.as_us(v) -> Integer */
static mrb_value
cpptest_as_us(mrb_state* mrb, mrb_value)
{
  mrb_value v; mrb_get_args(mrb, "o", &v);
  return mrb_int_value(mrb, (mrb_int)
    mrb_chrono::as<std::chrono::microseconds>(mrb, v).count());
}

/* ChronoCppTest.as_ns(v) -> Integer */
static mrb_value
cpptest_as_ns(mrb_state* mrb, mrb_value)
{
  mrb_value v; mrb_get_args(mrb, "o", &v);
  return mrb_int_value(mrb, (mrb_int)
    mrb_chrono::as<std::chrono::nanoseconds>(mrb, v).count());
}

/* ChronoCppTest.as_h(v) -> Integer */
static mrb_value
cpptest_as_h(mrb_state* mrb, mrb_value)
{
  mrb_value v; mrb_get_args(mrb, "o", &v);
  return mrb_int_value(mrb, (mrb_int)
    mrb_chrono::as<std::chrono::hours>(mrb, v).count());
}

/* ------------------------------------------------------------------ */
/*  Registration — called from mrb_mruby_chrono_gem_test in .c file   */
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
}

MRB_END_DECL