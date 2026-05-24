/*
 * mruby-chrono — implementation
 *
 * Ruby surface:
 *   Numeric#ns #us #ms #s #min #h [#days #weeks on C++20]  — return Float seconds
 *   Chrono.steady  / Chrono::Steady.now                    — monotonic Float seconds
 *   Chrono.system  / Chrono::System.now                    — wall-clock Float seconds
 *   Chrono::Timer.new / #reset / #elapsed                  — steady-clock stopwatch
 *
 * C API (chrono.h):
 *   mrb_chrono_to_timespec / _to_timeval / _to_ms_int / _to_ms_long
 *
 * C++ API (chrono.hpp):
 *   mrb_chrono::as<T>(mrb, v)  — Float seconds → std::chrono::duration<T>
 *   mrb_chrono::from(mrb, dur) — std::chrono::duration  → Float seconds
 */

#include <mruby.h>
#include <mruby/class.h>
#include <mruby/numeric.h>
#include <mruby/cpp_helpers.hpp>
#include <mruby/chrono.hpp>
#include <mruby/chrono.h>

#include <chrono>
#include <cmath>
#include <ctime>

 /* ------------------------------------------------------------------ */
 /*  Numeric → Float seconds                                            */
 /* ------------------------------------------------------------------ */

namespace {

  /*
   * Convert self (any Numeric) to Float seconds by scaling:
   *   result = self * (period::num / period::den)
   *
   * mrb_as_float handles Integer, Float, and anything that responds to
   * to_f (Rational, user-defined Numeric subclasses) without a VM
   * funcall for the base types.
   */
  template <typename ChronoType>
  mrb_value
    numeric_to_seconds(mrb_state* mrb, mrb_value self)
  {
    using period = typename ChronoType::period;
    constexpr double scale =
      static_cast<double>(period::num) / static_cast<double>(period::den);

    double count = static_cast<double>(mrb_as_float(mrb, self));
    if (!std::isfinite(count))
      mrb_raise(mrb, E_RANGE_ERROR, "non-finite value");

    return mrb_float_value(mrb, static_cast<mrb_float>(count * scale));
  }

#define DEF_UNIT(name, ChronoType)                                     \
  mrb_value                                                            \
  numeric_##name(mrb_state* mrb, mrb_value self) {                     \
    return numeric_to_seconds<ChronoType>(mrb, self);                  \
  }

  DEF_UNIT(nanoseconds, std::chrono::nanoseconds)
    DEF_UNIT(microseconds, std::chrono::microseconds)
    DEF_UNIT(milliseconds, std::chrono::milliseconds)
    DEF_UNIT(seconds, std::chrono::seconds)
    DEF_UNIT(minutes, std::chrono::minutes)
    DEF_UNIT(hours, std::chrono::hours)
#if defined(__cpp_lib_chrono) && __cpp_lib_chrono >= 201907L
    DEF_UNIT(days, std::chrono::days)
    DEF_UNIT(weeks, std::chrono::weeks)
#endif
#undef DEF_UNIT

} /* anonymous namespace */


/* ------------------------------------------------------------------ */
/*  Clocks                                                             */
/* ------------------------------------------------------------------ */

namespace {

  mrb_value
    mrb_chrono_steady_now(mrb_state* mrb, mrb_value)
  {
    return mrb_chrono::from(mrb,
      std::chrono::steady_clock::now().time_since_epoch());
  }

  mrb_value
    mrb_chrono_system_now(mrb_state* mrb, mrb_value)
  {
    return mrb_chrono::from(mrb,
      std::chrono::system_clock::now().time_since_epoch());
  }

} /* anonymous namespace */


/* ------------------------------------------------------------------ */
/*  Timer                                                              */
/* ------------------------------------------------------------------ */

namespace {

  struct Timer {
    std::chrono::steady_clock::time_point start;

    Timer() : start(std::chrono::steady_clock::now()) {}

    void reset() { start = std::chrono::steady_clock::now(); }

    std::chrono::steady_clock::duration since_start() const {
      return std::chrono::steady_clock::now() - start;
    }
  };

} /* anonymous namespace */

/* MRB_CPP_DEFINE_TYPE(T, tag) from mruby-c-ext-helpers:
 * generates the mrb_data_type + free function for Timer. */
MRB_CPP_DEFINE_TYPE(Timer, timer)

namespace {

  mrb_value
    timer_init(mrb_state* mrb, mrb_value self)
  {
    mrb_cpp_new<Timer>(mrb, self);
    return self;
  }

  mrb_value
    timer_reset(mrb_state* mrb, mrb_value self)
  {
    mrb_cpp_get<Timer>(mrb, self)->reset();
    return self;
  }

  mrb_value
    timer_elapsed(mrb_state* mrb, mrb_value self)
  {
    return mrb_chrono::from(mrb,
      mrb_cpp_get<Timer>(mrb, self)->since_start());
  }

} /* anonymous namespace */


/* ------------------------------------------------------------------ */
/*  C API (chrono.h)                                                   */
/* ------------------------------------------------------------------ */

namespace {

  /* Seconds → ticks in the requested unit. */
  double
    dur_scale(double sec, mrb_chrono_dur_type dur)
  {
    switch (dur) {
    case MRB_CHRONO_DUR_NANOSECONDS:  return sec * 1e9;
    case MRB_CHRONO_DUR_MICROSECONDS: return sec * 1e6;
    case MRB_CHRONO_DUR_MILLISECONDS: return sec * 1e3;
    case MRB_CHRONO_DUR_SECONDS:      return sec;
    case MRB_CHRONO_DUR_MINUTES:      return sec / 60.0;
    case MRB_CHRONO_DUR_HOURS:        return sec / 3600.0;
    case MRB_CHRONO_DUR_DAYS:         return sec / 86400.0;
    case MRB_CHRONO_DUR_WEEKS:        return sec / 604800.0;
    }
    return sec; /* unreachable */
  }

  /* Apply rounding, staying in double so callers can range-check before
   * casting to an integer type. Separating this from the integer cast
   * avoids UB from casting Inf or out-of-range values directly. */
  double
    apply_rounding_d(double ticks, mrb_chrono_rounding r)
  {
    switch (r) {
    case MRB_CHRONO_TRUNC:   return std::trunc(ticks);
    case MRB_CHRONO_FLOOR:   return std::floor(ticks);
    case MRB_CHRONO_CEIL:    return std::ceil(ticks);
    case MRB_CHRONO_NEAREST: {
      /* std::chrono::round only works on integer-rep durations —
       * duration<double> fails its enable_if constraint. Implement
       * half-to-even (banker's rounding) directly on the double. */
      double t = std::floor(ticks);
      double diff = ticks - t;
      if (diff < 0.5) return t;
      if (diff > 0.5) return t + 1.0;
      /* exactly 0.5 — round to even */
      return (std::fmod(t, 2.0) == 0.0) ? t : t + 1.0;
    }
    }
    return std::trunc(ticks); /* unreachable */
  }

  /* POSIX requires tv_nsec/tv_usec in [0, period). C division truncates
   * toward zero so negative inputs produce a negative sub-second field —
   * borrow one second to normalise. */
  void normalise_timespec(struct timespec* ts) {
    if (ts->tv_nsec < 0) { ts->tv_nsec += 1000000000L; ts->tv_sec -= 1; }
  }
  void normalise_timeval(struct timeval* tv) {
    if (tv->tv_usec < 0) { tv->tv_usec += 1000000L;    tv->tv_sec -= 1; }
  }

  /* Portable signed 64-bit multiply with overflow detection.
   *
   * __builtin_mul_overflow on GCC/Clang; manual pre-check on MSVC (and
   * any other compiler without the builtin). The manual check mirrors
   * mruby's own numeric.h pattern, retargeted to int64_t rather than
   * mrb_int so it works correctly on 32-bit mrb_int builds too. */
  static inline bool
    i64_mul_overflow(int64_t a, int64_t b, int64_t* out)
  {
#if defined(__GNUC__) || defined(__clang__)
    return __builtin_mul_overflow(a, b, out);
#else
    if (a > 0) {
      if (b > 0 && a > INT64_MAX / b) return true;
      if (b < 0 && b < INT64_MIN / a) return true;
    }
    else if (a < 0) {
      if (b > 0 && a < INT64_MIN / b) return true;
      if (b < 0 && a < INT64_MAX / b) return true;
    }
    *out = a * b;
    return false;
#endif
  }

  /* Expected sizeof for each out_type — verified against the caller's
   * out_size before any write through the void*. */
  size_t
    expected_size(mrb_chrono_out_type t)
  {
    switch (t) {
    case MRB_CHRONO_OUT_INT8:     return sizeof(int8_t);
    case MRB_CHRONO_OUT_UINT8:    return sizeof(uint8_t);
    case MRB_CHRONO_OUT_INT16:    return sizeof(int16_t);
    case MRB_CHRONO_OUT_UINT16:   return sizeof(uint16_t);
    case MRB_CHRONO_OUT_INT32:    return sizeof(int32_t);
    case MRB_CHRONO_OUT_UINT32:   return sizeof(uint32_t);
    case MRB_CHRONO_OUT_INT64:    return sizeof(int64_t);
    case MRB_CHRONO_OUT_UINT64:   return sizeof(uint64_t);
    case MRB_CHRONO_OUT_INT:      return sizeof(int);
    case MRB_CHRONO_OUT_UINT:     return sizeof(unsigned int);
    case MRB_CHRONO_OUT_LONG:     return sizeof(long);
    case MRB_CHRONO_OUT_ULONG:    return sizeof(unsigned long);
    case MRB_CHRONO_OUT_LLONG:    return sizeof(long long);
    case MRB_CHRONO_OUT_ULLONG:   return sizeof(unsigned long long);
    case MRB_CHRONO_OUT_FLOAT:    return sizeof(float);
    case MRB_CHRONO_OUT_DOUBLE:   return sizeof(double);
    case MRB_CHRONO_OUT_TIMESPEC: return sizeof(struct timespec);
    case MRB_CHRONO_OUT_TIMEVAL:  return sizeof(struct timeval);
    }
    return 0; /* unreachable */
  }

  /* Write an integer type T through void*, with full overflow checking.
   *
   * Range check is done in double against std::numeric_limits<T> before
   * any integer cast — this avoids UB from casting an out-of-range or
   * non-finite double to an integer type.
   *
   * We use numeric_limits rather than mrb_int_mul_overflow /
   * mrb_int_add_overflow here because those functions operate at
   * mrb_int width and would fire prematurely on 32-bit builds for the
   * wider integer types (int64, long long) that callers may request. */
  template <typename T>
  void
    write_int(mrb_state* mrb, void* out, double ticks, mrb_chrono_rounding r)
  {
    double rounded = apply_rounding_d(ticks, r);
    if (!std::isfinite(rounded)
      || rounded < static_cast<double>(std::numeric_limits<T>::min())
      || rounded > static_cast<double>(std::numeric_limits<T>::max()))
      mrb_raise(mrb, E_RANGE_ERROR,
        "mrb_chrono_convert: value out of range for output type");
    *static_cast<T*>(out) = static_cast<T>(rounded);
  }

  /* Write a floating-point type T through void*.
   * Non-finite doubles are passed through (Inf/NaN are valid float values).
   * For float (32-bit) we guard against a finite double that exceeds
   * float's representable range, which would produce ±Inf silently. */
  template <typename T>
  void
    write_float(mrb_state* mrb, void* out, double ticks, mrb_chrono_rounding r)
  {
    double rounded = apply_rounding_d(ticks, r);
    if constexpr (std::is_same_v<T, float>) {
      constexpr double fmax = static_cast<double>(std::numeric_limits<float>::max());
      if (std::isfinite(rounded) && (rounded > fmax || rounded < -fmax))
        mrb_raise(mrb, E_RANGE_ERROR,
          "mrb_chrono_convert: value out of range for float output");
    }
    *static_cast<T*>(out) = static_cast<T>(rounded);
    (void)mrb;
  }

} /* anonymous namespace */

MRB_BEGIN_DECL

mrb_value
mrb_chrono_from(mrb_state* mrb, mrb_value v, mrb_chrono_dur_type dur_type)
{
  double ticks = static_cast<double>(mrb_as_float(mrb, v));
  if (!std::isfinite(ticks))
    mrb_raise(mrb, E_RANGE_ERROR, "mrb_chrono_from: non-finite input value");

  /* Inverse of dur_scale: unit ticks → seconds. */
  static const double to_sec[] = {
    1e-9,      /* nanoseconds  */
    1e-6,      /* microseconds */
    1e-3,      /* milliseconds */
    1.0,       /* seconds      */
    60.0,      /* minutes      */
    3600.0,    /* hours        */
    86400.0,   /* days         */
    604800.0,  /* weeks        */
  };
  double sec = ticks * to_sec[dur_type];
  if (!std::isfinite(sec))
    mrb_raise(mrb, E_RANGE_ERROR, "mrb_chrono_from: result out of range");

  return mrb_float_value(mrb, static_cast<mrb_float>(sec));
}

void
mrb_chrono_convert(mrb_state* mrb,
  mrb_value           v,
  mrb_chrono_out_type out_type,
  mrb_chrono_dur_type dur_type,
  mrb_chrono_rounding rounding,
  void* out,
  size_t              out_size)
{
  if (!out)
    mrb_raise(mrb, E_ARGUMENT_ERROR, "mrb_chrono_convert: out is NULL");

  size_t expected = expected_size(out_type);
  if (out_size != expected)
    mrb_raisef(mrb, E_ARGUMENT_ERROR,
      "mrb_chrono_convert: out_size %d != sizeof(out_type) %d",
      (int)out_size, (int)expected);

  double sec = static_cast<double>(mrb_as_float(mrb, v));
  if (!std::isfinite(sec))
    mrb_raise(mrb, E_RANGE_ERROR, "mrb_chrono_convert: non-finite input value");

  double ticks = dur_scale(sec, dur_type);

  switch (out_type) {
  case MRB_CHRONO_OUT_INT8:   write_int<int8_t>(mrb, out, ticks, rounding); break;
  case MRB_CHRONO_OUT_UINT8:  write_int<uint8_t>(mrb, out, ticks, rounding); break;
  case MRB_CHRONO_OUT_INT16:  write_int<int16_t>(mrb, out, ticks, rounding); break;
  case MRB_CHRONO_OUT_UINT16: write_int<uint16_t>(mrb, out, ticks, rounding); break;
  case MRB_CHRONO_OUT_INT32:  write_int<int32_t>(mrb, out, ticks, rounding); break;
  case MRB_CHRONO_OUT_UINT32: write_int<uint32_t>(mrb, out, ticks, rounding); break;
  case MRB_CHRONO_OUT_INT64:  write_int<int64_t>(mrb, out, ticks, rounding); break;
  case MRB_CHRONO_OUT_UINT64: write_int<uint64_t>(mrb, out, ticks, rounding); break;
  case MRB_CHRONO_OUT_INT:    write_int<int>(mrb, out, ticks, rounding); break;
  case MRB_CHRONO_OUT_UINT:   write_int<unsigned int>(mrb, out, ticks, rounding); break;
  case MRB_CHRONO_OUT_LONG:   write_int<long>(mrb, out, ticks, rounding); break;
  case MRB_CHRONO_OUT_ULONG:  write_int<unsigned long>(mrb, out, ticks, rounding); break;
  case MRB_CHRONO_OUT_LLONG:  write_int<long long>(mrb, out, ticks, rounding); break;
  case MRB_CHRONO_OUT_ULLONG: write_int<unsigned long long>(mrb, out, ticks, rounding); break;
  case MRB_CHRONO_OUT_FLOAT:  write_float<float>(mrb, out, ticks, rounding); break;
  case MRB_CHRONO_OUT_DOUBLE: write_float<double>(mrb, out, ticks, rounding); break;

  case MRB_CHRONO_OUT_TIMESPEC: {
    /* Convert to whole nanoseconds with overflow check.
     * __builtin_mul_overflow is used here (not mrb_int_mul_overflow)
     * because we need int64_t width — mrb_int may be 32-bit. */
    double rounded = apply_rounding_d(ticks, rounding);
    if (rounded < static_cast<double>(INT64_MIN) || rounded > static_cast<double>(INT64_MAX))
      mrb_raise(mrb, E_RANGE_ERROR, "mrb_chrono_convert: value out of range for timespec");
    int64_t total = static_cast<int64_t>(rounded);

    static const int64_t ns_per_tick[] = {
      /* ns   us      ms         s           min              h */
         1LL, 1000LL, 1000000LL, 1000000000LL, 60000000000LL, 3600000000000LL,
         /* day               week */
            86400000000000LL, 604800000000000LL
    };
    int64_t ns;
    if (i64_mul_overflow(total, ns_per_tick[dur_type], &ns))
      mrb_raise(mrb, E_RANGE_ERROR, "mrb_chrono_convert: value out of range for timespec");

    struct timespec* ts = static_cast<struct timespec*>(out);
    ts->tv_sec = static_cast<time_t>(ns / 1000000000LL);
    ts->tv_nsec = static_cast<long>  (ns % 1000000000LL);
    normalise_timespec(ts);
    break;
  }
  case MRB_CHRONO_OUT_TIMEVAL: {
    /* Same pattern in microseconds. Nanoseconds → microseconds is a
     * divide rather than a multiply, handled separately. */
    int64_t us;
    if (dur_type == MRB_CHRONO_DUR_NANOSECONDS) {
      double us_d = apply_rounding_d(ticks / 1000.0, rounding);
      if (us_d < static_cast<double>(INT64_MIN) || us_d > static_cast<double>(INT64_MAX))
        mrb_raise(mrb, E_RANGE_ERROR, "mrb_chrono_convert: value out of range for timeval");
      us = static_cast<int64_t>(us_d);
    }
    else {
      double rounded = apply_rounding_d(ticks, rounding);
      if (rounded < static_cast<double>(INT64_MIN) || rounded > static_cast<double>(INT64_MAX))
        mrb_raise(mrb, E_RANGE_ERROR, "mrb_chrono_convert: value out of range for timeval");
      int64_t total = static_cast<int64_t>(rounded);

      static const int64_t us_per_tick[] = {
        /* ns(handled above) us   ms      s        min          h */
           0LL,              1LL, 1000LL, 1000000LL, 60000000LL, 3600000000LL,
           /* day             week */
              86400000000LL, 604800000000LL
      };
      if (i64_mul_overflow(total, us_per_tick[dur_type], &us))
        mrb_raise(mrb, E_RANGE_ERROR, "mrb_chrono_convert: value out of range for timeval");
    }

    struct timeval* tv = static_cast<struct timeval*>(out);
    tv->tv_sec = static_cast<long>(us / 1000000LL);
    tv->tv_usec = static_cast<long>(us % 1000000LL);
    normalise_timeval(tv);
    break;
  }
  }
}

MRB_END_DECL


/* ------------------------------------------------------------------ */
/*  gem_init / gem_final                                               */
/* ------------------------------------------------------------------ */

MRB_BEGIN_DECL

void
mrb_mruby_chrono_gem_init(mrb_state* mrb)
{
  struct RClass* chrono_mod = mrb_define_module_id(mrb, MRB_SYM(Chrono));

  /* Chrono.steady / Chrono.system (module-function convenience aliases) */
  mrb_define_module_function_id(mrb, chrono_mod, MRB_SYM(steady),
    mrb_chrono_steady_now, MRB_ARGS_NONE());
  mrb_define_module_function_id(mrb, chrono_mod, MRB_SYM(system),
    mrb_chrono_system_now, MRB_ARGS_NONE());

  /* Chrono::Steady.now */
  struct RClass* steady_mod =
    mrb_define_module_under_id(mrb, chrono_mod, MRB_SYM(Steady));
  mrb_define_module_function_id(mrb, steady_mod, MRB_SYM(now),
    mrb_chrono_steady_now, MRB_ARGS_NONE());

  /* Chrono::System.now */
  struct RClass* system_mod =
    mrb_define_module_under_id(mrb, chrono_mod, MRB_SYM(System));
  mrb_define_module_function_id(mrb, system_mod, MRB_SYM(now),
    mrb_chrono_system_now, MRB_ARGS_NONE());

  /* Chrono::Timer */
  struct RClass* timer_cls = mrb_define_class_under_id(
    mrb, chrono_mod, MRB_SYM(Timer), mrb->object_class);
  MRB_SET_INSTANCE_TT(timer_cls, MRB_TT_DATA);
  mrb_define_method_id(mrb, timer_cls, MRB_SYM(initialize), timer_init, MRB_ARGS_NONE());
  mrb_define_method_id(mrb, timer_cls, MRB_SYM(reset), timer_reset, MRB_ARGS_NONE());
  mrb_define_method_id(mrb, timer_cls, MRB_SYM(elapsed), timer_elapsed, MRB_ARGS_NONE());

  /* Numeric extensions: 500.ms → 0.5 (Float seconds), etc. */
  struct RClass* numeric = mrb_class_get_id(mrb, MRB_SYM(Numeric));

  mrb_define_method_id(mrb, numeric, MRB_SYM(nanoseconds), numeric_nanoseconds, MRB_ARGS_NONE());
  mrb_define_method_id(mrb, numeric, MRB_SYM(ns), numeric_nanoseconds, MRB_ARGS_NONE());
  mrb_define_method_id(mrb, numeric, MRB_SYM(microseconds), numeric_microseconds, MRB_ARGS_NONE());
  mrb_define_method_id(mrb, numeric, MRB_SYM(us), numeric_microseconds, MRB_ARGS_NONE());
  mrb_define_method_id(mrb, numeric, MRB_SYM(milliseconds), numeric_milliseconds, MRB_ARGS_NONE());
  mrb_define_method_id(mrb, numeric, MRB_SYM(ms), numeric_milliseconds, MRB_ARGS_NONE());
  mrb_define_method_id(mrb, numeric, MRB_SYM(seconds), numeric_seconds, MRB_ARGS_NONE());
  mrb_define_method_id(mrb, numeric, MRB_SYM(s), numeric_seconds, MRB_ARGS_NONE());
  mrb_define_method_id(mrb, numeric, MRB_SYM(minutes), numeric_minutes, MRB_ARGS_NONE());
  mrb_define_method_id(mrb, numeric, MRB_SYM(min), numeric_minutes, MRB_ARGS_NONE());
  mrb_define_method_id(mrb, numeric, MRB_SYM(hours), numeric_hours, MRB_ARGS_NONE());
  mrb_define_method_id(mrb, numeric, MRB_SYM(h), numeric_hours, MRB_ARGS_NONE());
#if defined(__cpp_lib_chrono) && __cpp_lib_chrono >= 201907L
  mrb_define_method_id(mrb, numeric, MRB_SYM(days), numeric_days, MRB_ARGS_NONE());
  mrb_define_method_id(mrb, numeric, MRB_SYM(weeks), numeric_weeks, MRB_ARGS_NONE());
#endif
}

void
mrb_mruby_chrono_gem_final(mrb_state*) {}

MRB_END_DECL