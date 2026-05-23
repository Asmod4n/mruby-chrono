/*
 * mruby-chrono — implementation
 *
 * Public Ruby surface:
 *
 *   # Duration (new, primary purpose):
 *   Chrono::Duration.new(count, :unit)
 *   500.ms, 200.us, 5.s, 3.min, 2.h   (also .days/.weeks on C++20+)
 *   dur.as(:unit)
 *   dur.as(:unit, :rounding)
 *   dur.as_f(:unit)
 *   dur + other, dur - other, dur * scalar, dur / scalar
 *   dur == other, dur <=> other       (Comparable mixed in for <, <=, >, >=)
 *
 *   # Clocks (preserved from the original gem, 10+ years shipped):
 *   Chrono::Steady.now        # monotonic Float seconds, for benchmarking
 *   Chrono::Steady.duration   # same value as a Chrono::Duration
 *   Chrono::System.now        # wall-clock Float seconds since Unix epoch
 *   Chrono.steady             # alias for Chrono::Steady.now
 *   Chrono.system             # alias for Chrono::System.now
 *
 *   # Timer (preserved):
 *   t = Chrono::Timer.new
 *   t.elapsed     # Float seconds since .new or last #reset
 *   t.duration    # same span as a Chrono::Duration
 *   t.reset       # restart from now
 *
 * Duration storage: struct timespec in the mruby data slot, bound via
 * mruby-c-ext-helpers' MRB_CPP_DEFINE_TYPE(timespec, duration).
 * Math: std::chrono::duration_cast and friends.
 *
 * The C++ extension surface (mrb_chrono::from / mrb_chrono::as / the
 * Rounding enum / the typed duration_to_timespec accessor) is declared
 * in chrono.hpp; out-of-line definitions are at the end of this file.
 */

#include <mruby.h>
#include <mruby/class.h>
#include <mruby/data.h>
#include <mruby/presym.h>
#ifdef MRB_USE_BIGINT
MRB_BEGIN_DECL
#  include <mruby/internal.h>   /* mrb_bint_as_int64 */
MRB_END_DECL
#endif

#ifdef MRB_USE_FLOAT32
#  error "mruby-chrono: MRB_USE_FLOAT32 is too narrow for the clocks and Timer (Float seconds since 1970 needs >7 sig digits). Build without MRB_USE_FLOAT32."
#endif

#include <mruby/num_helpers.hpp>   /* mrb_convert_number<T>            */
#include <mruby/cpp_helpers.hpp>   /* MRB_CPP_DEFINE_TYPE / mrb_cpp_new */
#include <mruby/chrono.hpp>

#include <chrono>
#include <cmath>     /* std::isfinite */
#include <climits>   /* INT64_MAX, INT64_MIN */
#include <new>

using namespace mrb_chrono;


/* ----- int64_t multiply with overflow detection --------------------
 *
 * NOTE: mruby ships mrb_int_mul_overflow in <mruby/numeric.h>, but
 * that's mrb_int-typed and only protects at the build's chosen
 * mrb_int width. The values we multiply in Duration#* are int64_t
 * (std::chrono::nanoseconds::rep), wider than mrb_int on MRB_INT32 /
 * MRB_INT16 builds, so the mrb_int helpers don't cover our case.
 *
 * We need int64_t-level detection. GCC/Clang provide
 * __builtin_mul_overflow which mruby itself uses internally; MSVC
 * doesn't, so the fallback mirrors mruby's manual pattern from
 * numeric.h. Same logic, retargeted to int64_t.
 */
static inline bool
i64_mul_overflow(int64_t a, int64_t b, int64_t* out)
{
#if defined(__GNUC__) || defined(__clang__)
  return __builtin_mul_overflow(a, b, out);
#else
  /* Cross-compiler manual fallback (MSVC, etc.). */
  if (a > 0 && b > 0 && a > INT64_MAX / b) return true;
  if (a < 0 && b > 0 && a < INT64_MIN / b) return true;
  if (a > 0 && b < 0 && b < INT64_MIN / a) return true;
  if (a < 0 && b < 0) {
    if (a == INT64_MIN || b == INT64_MIN) return true;
    if (-a > INT64_MAX / -b) return true;
  }
  *out = a * b;
  return false;
#endif
}


/* ----- timespec free / alloc helpers --------------------------------
 *
 * struct timespec is trivially copyable; we just malloc/free a single
 * one per Duration. mrb_cpp_new<timespec>(mrb, self) from
 * mruby-c-ext-helpers allocates, runs timespec's (trivial) default
 * constructor in place, and registers the destructor with the GC.
 * MRB_CPP_DEFINE_TYPE generates the free function and the data_type;
 * the cpp_basename macro strips no prefix here because `timespec` is
 * already a plain type name in C++ (the `struct` keyword is optional
 * once <time.h> is in scope).
 */
MRB_CPP_DEFINE_TYPE(timespec, duration)

/* ----- Unit Symbol ↔ std::chrono mapping ---------------------------
 *
 * The accepted Symbols and which std::chrono type each maps to. On
 * C++20+ builds, :days and :weeks join the list because
 * std::chrono::days and ::weeks exist; on C++17 builds they're absent
 * everywhere — Symbol mapping, Numeric extension, the works.
 *
 * Each entry pairs the Symbol with the dispatch tag the rest of the
 * file uses. The tag exists only so we can write small switches in
 * the Ruby-side methods; nothing in the C++ extension API touches it.
 */
namespace {

enum class UnitTag : uint8_t {
  Nanoseconds,
  Microseconds,
  Milliseconds,
  Seconds,
  Minutes,
  Hours,
#if defined(__cpp_lib_chrono) && __cpp_lib_chrono >= 201907L
  Days,
  Weeks,
#endif
};

/* Symbol → UnitTag. Raises ArgumentError on unknown unit. */
UnitTag
sym_to_unit(mrb_state* mrb, mrb_sym s)
{
  if (s == MRB_SYM(nanoseconds)  || s == MRB_SYM(ns))  return UnitTag::Nanoseconds;
  if (s == MRB_SYM(microseconds) || s == MRB_SYM(us))  return UnitTag::Microseconds;
  if (s == MRB_SYM(milliseconds) || s == MRB_SYM(ms))  return UnitTag::Milliseconds;
  if (s == MRB_SYM(seconds)      || s == MRB_SYM(s))   return UnitTag::Seconds;
  if (s == MRB_SYM(minutes)      || s == MRB_SYM(min)) return UnitTag::Minutes;
  if (s == MRB_SYM(hours)        || s == MRB_SYM(h))   return UnitTag::Hours;
#if defined(__cpp_lib_chrono) && __cpp_lib_chrono >= 201907L
  if (s == MRB_SYM(days))   return UnitTag::Days;
  if (s == MRB_SYM(weeks))  return UnitTag::Weeks;
#endif
  mrb_raisef(mrb, E_ARGUMENT_ERROR, "unknown unit %!S",
             mrb_symbol_value(s));
  return UnitTag::Nanoseconds;  /* unreachable; mrb_raisef noreturn */
}

Rounding
sym_to_rounding(mrb_state* mrb, mrb_sym s)
{
  if (s == MRB_SYM(truncate)) return Rounding::Truncate;
  if (s == MRB_SYM(floor))    return Rounding::Floor;
  if (s == MRB_SYM(ceil))     return Rounding::Ceil;
  if (s == MRB_SYM(round))    return Rounding::Round;
  mrb_raisef(mrb, E_ARGUMENT_ERROR,
             "unknown rounding %!S; valid: :truncate :floor :ceil :round",
             mrb_symbol_value(s));
  return Rounding::Truncate;  /* unreachable */
}

} /* anonymous namespace */


/* ----- Numeric → int64 extractor ------------------------------------
 *
 * The Ruby surface accepts "any Integer" as a count or scalar — that
 * includes both immediate Integers (MRB_TT_INTEGER) and Bigints
 * (MRB_TT_BIGINT, when MRB_USE_BIGINT is enabled). The split between
 * the two is a build-matrix detail Ruby users shouldn't see.
 *
 * Returns int64_t. On Bigint values that don't fit int64_t,
 * mrb_bint_as_int64 raises RangeError honestly. On Float or other
 * types this returns 0 — callers should pre-check or branch
 * accordingly (the Numeric macro and Duration arithmetic do).
 */
namespace {
inline bool is_integerish(mrb_value v) {
  return mrb_integer_p(v) || mrb_bigint_p(v);
}

inline int64_t to_i64(mrb_state* mrb, mrb_value v) {
  if (mrb_integer_p(v)) return mrb_integer(v);
#ifdef MRB_USE_BIGINT
  if (mrb_bigint_p(v))  return mrb_bint_as_int64(mrb, v);
#endif
  return 0;
}
}  /* anonymous namespace */


/* ----- Construction dispatch ----------------------------------------
 *
 * Given a unit tag and an int64_t count, build a timespec and store.
 * The count's semantics depend on the unit — 5 milliseconds means
 * something different from 5 microseconds. We construct the
 * corresponding std::chrono value and convert through nanoseconds.
 */
namespace {

template <typename ChronoDuration>
struct timespec
build_ts(int64_t count)
{
  auto total_ns = std::chrono::duration_cast<std::chrono::nanoseconds>(
    ChronoDuration(count));
  return detail::ns_to_timespec(total_ns);
}

struct timespec
ts_from_count_unit(int64_t count, UnitTag unit)
{
  switch (unit) {
    case UnitTag::Nanoseconds:  return build_ts<std::chrono::nanoseconds>(count);
    case UnitTag::Microseconds: return build_ts<std::chrono::microseconds>(count);
    case UnitTag::Milliseconds: return build_ts<std::chrono::milliseconds>(count);
    case UnitTag::Seconds:      return build_ts<std::chrono::seconds>(count);
    case UnitTag::Minutes:      return build_ts<std::chrono::minutes>(count);
    case UnitTag::Hours:        return build_ts<std::chrono::hours>(count);
#if defined(__cpp_lib_chrono) && __cpp_lib_chrono >= 201907L
    case UnitTag::Days:         return build_ts<std::chrono::days>(count);
    case UnitTag::Weeks:        return build_ts<std::chrono::weeks>(count);
#endif
  }
  /* Unreachable. */
  return build_ts<std::chrono::nanoseconds>(0);
}

#ifndef MRB_NO_FLOAT
template <typename Rep, typename Period>
struct timespec
build_ts_float(double count)
{
  std::chrono::duration<Rep, Period> d(count);
  auto total_ns = std::chrono::duration_cast<std::chrono::nanoseconds>(d);
  return detail::ns_to_timespec(total_ns);
}

struct timespec
ts_from_float_count_unit(double count, UnitTag unit)
{
  switch (unit) {
    case UnitTag::Nanoseconds:  return build_ts_float<double, std::nano>(count);
    case UnitTag::Microseconds: return build_ts_float<double, std::micro>(count);
    case UnitTag::Milliseconds: return build_ts_float<double, std::milli>(count);
    case UnitTag::Seconds:      return build_ts_float<double, std::ratio<1>>(count);
    case UnitTag::Minutes:      return build_ts_float<double, std::ratio<60>>(count);
    case UnitTag::Hours:        return build_ts_float<double, std::ratio<3600>>(count);
#if defined(__cpp_lib_chrono) && __cpp_lib_chrono >= 201907L
    case UnitTag::Days:         return build_ts_float<double, std::ratio<86400>>(count);
    case UnitTag::Weeks:        return build_ts_float<double, std::ratio<604800>>(count);
#endif
  }
  return build_ts_float<double, std::nano>(0.0);
}
#endif

} /* anonymous namespace */


/* ----- Conversion dispatch (Duration → typed integer) --------------
 *
 * The Ruby-side `dur.as(:unit, :rounding)` and `dur.as_f(:unit)` go
 * through this. The chrono operation depends on the rounding mode; the
 * target type depends on the unit tag.
 */
namespace {

template <typename Target>
int64_t
do_cast(std::chrono::nanoseconds stored, Rounding r)
{
  switch (r) {
    case Rounding::Truncate: return std::chrono::duration_cast<Target>(stored).count();
    case Rounding::Floor:    return std::chrono::floor<Target>(stored).count();
    case Rounding::Ceil:     return std::chrono::ceil<Target>(stored).count();
    case Rounding::Round:    return std::chrono::round<Target>(stored).count();
  }
  return std::chrono::duration_cast<Target>(stored).count();  /* unreachable */
}

int64_t
cast_to_unit_int(std::chrono::nanoseconds stored, UnitTag unit, Rounding r)
{
  switch (unit) {
    case UnitTag::Nanoseconds:  return do_cast<std::chrono::nanoseconds>(stored, r);
    case UnitTag::Microseconds: return do_cast<std::chrono::microseconds>(stored, r);
    case UnitTag::Milliseconds: return do_cast<std::chrono::milliseconds>(stored, r);
    case UnitTag::Seconds:      return do_cast<std::chrono::seconds>(stored, r);
    case UnitTag::Minutes:      return do_cast<std::chrono::minutes>(stored, r);
    case UnitTag::Hours:        return do_cast<std::chrono::hours>(stored, r);
#if defined(__cpp_lib_chrono) && __cpp_lib_chrono >= 201907L
    case UnitTag::Days:         return do_cast<std::chrono::days>(stored, r);
    case UnitTag::Weeks:        return do_cast<std::chrono::weeks>(stored, r);
#endif
  }
  return 0;  /* unreachable */
}

#ifndef MRB_NO_FLOAT
template <typename Period>
double
cast_to_float(std::chrono::nanoseconds stored)
{
  /* For float output we always use duration_cast (truncation isn't
   * meaningful for float-to-float casts — chrono's cast is exact in
   * the math sense, only the IEEE 754 mantissa imposes precision). */
  return std::chrono::duration_cast<
    std::chrono::duration<double, Period>>(stored).count();
}

double
cast_to_unit_float(std::chrono::nanoseconds stored, UnitTag unit)
{
  switch (unit) {
    case UnitTag::Nanoseconds:  return cast_to_float<std::nano>(stored);
    case UnitTag::Microseconds: return cast_to_float<std::micro>(stored);
    case UnitTag::Milliseconds: return cast_to_float<std::milli>(stored);
    case UnitTag::Seconds:      return cast_to_float<std::ratio<1>>(stored);
    case UnitTag::Minutes:      return cast_to_float<std::ratio<60>>(stored);
    case UnitTag::Hours:        return cast_to_float<std::ratio<3600>>(stored);
#if defined(__cpp_lib_chrono) && __cpp_lib_chrono >= 201907L
    case UnitTag::Days:         return cast_to_float<std::ratio<86400>>(stored);
    case UnitTag::Weeks:        return cast_to_float<std::ratio<604800>>(stored);
#endif
  }
  return 0.0;  /* unreachable */
}
#endif

} /* anonymous namespace */


/* ----- Ruby method shims -------------------------------------------- */
namespace {

/* Helper: allocate a new Chrono::Duration mrb_value with the given ts.
 *
 * Goes through mrb_obj_alloc + mrb_cpp_new (not mrb_obj_new) so Ruby's
 * initialize is NOT called — initialize requires (count, :unit) args,
 * but the internal construction path already has a fully-formed
 * timespec to store. Callers from the Numeric extensions and from
 * arithmetic use this path; Ruby-side Chrono::Duration.new still goes
 * through initialize normally. */
mrb_value
new_duration(mrb_state* mrb, struct RClass* cls, struct timespec ts)
{
  mrb_value obj = mrb_obj_value(mrb_obj_alloc(mrb, MRB_TT_DATA, cls));
  *mrb_cpp_new<timespec>(mrb, obj) = ts;
  return obj;
}

struct RClass*
duration_class(mrb_state* mrb)
{
  struct RClass* chrono = mrb_module_get_id(mrb, MRB_SYM(Chrono));
  return mrb_class_get_under_id(mrb, chrono, MRB_SYM(Duration));
}

/* Chrono::Duration.new(count, :unit). count may be Integer or Float. */
mrb_value
duration_init(mrb_state* mrb, mrb_value self)
{
  mrb_value count_v;
  mrb_sym   unit_sym;
  mrb_get_args(mrb, "on", &count_v, &unit_sym);

  UnitTag unit = sym_to_unit(mrb, unit_sym);
  struct timespec ts;

  if (is_integerish(count_v)) {
    ts = ts_from_count_unit(to_i64(mrb, count_v), unit);
  }
#ifndef MRB_NO_FLOAT
  else if (mrb_float_p(count_v)) {
    ts = ts_from_float_count_unit(mrb_float(count_v), unit);
  }
#endif
  else {
    mrb_raise(mrb, E_TYPE_ERROR,
              "Chrono::Duration.new: count must be Integer or Float");
  }

  *mrb_cpp_new<timespec>(mrb, self) = ts;
  return self;
}

/* dur.as(:unit)  or  dur.as(:unit, :rounding) → Integer */
mrb_value
duration_as(mrb_state* mrb, mrb_value self)
{
  mrb_sym unit_sym;
  mrb_sym rounding_sym = MRB_SYM(truncate);
  mrb_get_args(mrb, "n|n", &unit_sym, &rounding_sym);

  UnitTag  unit = sym_to_unit(mrb, unit_sym);
  Rounding r    = sym_to_rounding(mrb, rounding_sym);

  struct timespec ts = duration_to_timespec(mrb, self);
  auto stored = detail::timespec_to_ns(ts);
  auto out    = cast_to_unit_int(stored, unit, r);

  /* Box back via the build's available Integer / BigInt as appropriate.
   * mrb_convert_number from mruby-c-ext-helpers picks Fixnum, mrb_int
   * Integer, or BigInt based on the build matrix; raises RangeError if
   * nothing fits. */
  return mrb_convert_number(mrb, out);
}

#ifndef MRB_NO_FLOAT
/* dur.as_f(:unit) → Float */
mrb_value
duration_as_f(mrb_state* mrb, mrb_value self)
{
  mrb_sym unit_sym;
  mrb_get_args(mrb, "n", &unit_sym);

  UnitTag unit = sym_to_unit(mrb, unit_sym);
  struct timespec ts = duration_to_timespec(mrb, self);
  auto stored = detail::timespec_to_ns(ts);
  double out = cast_to_unit_float(stored, unit);

  return mrb_float_value(mrb, out);
}
#endif

/* dur + other  /  dur - other */
mrb_value
duration_plus(mrb_state* mrb, mrb_value self)
{
  mrb_value other;
  mrb_get_args(mrb, "o", &other);
  if (!mrb_data_p(other) || DATA_TYPE(other) != &duration_type) {
    mrb_raise(mrb, E_TYPE_ERROR, "Duration#+: expected a Chrono::Duration");
  }
  struct timespec a = duration_to_timespec(mrb, self);
  struct timespec b = duration_to_timespec(mrb, other);
  auto sum = detail::timespec_to_ns(a) + detail::timespec_to_ns(b);
  return new_duration(mrb, duration_class(mrb), detail::ns_to_timespec(sum));
}

mrb_value
duration_minus(mrb_state* mrb, mrb_value self)
{
  mrb_value other;
  mrb_get_args(mrb, "o", &other);
  if (!mrb_data_p(other) || DATA_TYPE(other) != &duration_type) {
    mrb_raise(mrb, E_TYPE_ERROR, "Duration#-: expected a Chrono::Duration");
  }
  struct timespec a = duration_to_timespec(mrb, self);
  struct timespec b = duration_to_timespec(mrb, other);
  auto diff = detail::timespec_to_ns(a) - detail::timespec_to_ns(b);
  return new_duration(mrb, duration_class(mrb), detail::ns_to_timespec(diff));
}

/* dur * scalar */
mrb_value
duration_mul(mrb_state* mrb, mrb_value self)
{
  mrb_value scalar;
  mrb_get_args(mrb, "o", &scalar);
  struct timespec ts = duration_to_timespec(mrb, self);
  auto ns = detail::timespec_to_ns(ts).count();

  std::chrono::nanoseconds::rep product;
  if (is_integerish(scalar)) {
    if (i64_mul_overflow(ns, to_i64(mrb, scalar), &product)) {
      mrb_raise(mrb, E_RANGE_ERROR,
                "Duration#*: result out of int64 nanosecond range");
    }
  }
#ifndef MRB_NO_FLOAT
  else if (mrb_float_p(scalar)) {
    double d = static_cast<double>(ns) * mrb_float(scalar);
    if (!std::isfinite(d) ||
        d > static_cast<double>(INT64_MAX) ||
        d < static_cast<double>(INT64_MIN)) {
      mrb_raise(mrb, E_RANGE_ERROR,
                "Duration#*: result out of int64 nanosecond range "
                "(or non-finite)");
    }
    product = static_cast<decltype(ns)>(d);
  }
#endif
  else {
    mrb_raise(mrb, E_TYPE_ERROR, "Duration#*: expected Integer or Float scalar");
  }
  return new_duration(mrb, duration_class(mrb),
                      detail::ns_to_timespec(std::chrono::nanoseconds(product)));
}

/* dur / scalar */
mrb_value
duration_div(mrb_state* mrb, mrb_value self)
{
  mrb_value scalar;
  mrb_get_args(mrb, "o", &scalar);
  struct timespec ts = duration_to_timespec(mrb, self);
  auto ns = detail::timespec_to_ns(ts).count();

  std::chrono::nanoseconds::rep quotient;
  if (is_integerish(scalar)) {
    auto d = to_i64(mrb, scalar);
    if (d == 0) mrb_raise(mrb, E_ZERODIV_ERROR, "Duration#/: divided by 0");
    /* The only int64 division that overflows is INT64_MIN / -1
     * (mathematically INT64_MAX + 1, doesn't fit). */
    if (ns == INT64_MIN && d == -1) {
      mrb_raise(mrb, E_RANGE_ERROR,
                "Duration#/: INT64_MIN / -1 overflows int64 range");
    }
    quotient = ns / d;
  }
#ifndef MRB_NO_FLOAT
  else if (mrb_float_p(scalar)) {
    auto d = mrb_float(scalar);
    if (d == 0.0) mrb_raise(mrb, E_ZERODIV_ERROR, "Duration#/: divided by 0.0");
    double r = static_cast<double>(ns) / d;
    if (!std::isfinite(r) ||
        r > static_cast<double>(INT64_MAX) ||
        r < static_cast<double>(INT64_MIN)) {
      mrb_raise(mrb, E_RANGE_ERROR,
                "Duration#/: result out of int64 nanosecond range "
                "(or non-finite)");
    }
    quotient = static_cast<decltype(ns)>(r);
  }
#endif
  else {
    mrb_raise(mrb, E_TYPE_ERROR, "Duration#/: expected Integer or Float scalar");
  }
  return new_duration(mrb, duration_class(mrb),
                      detail::ns_to_timespec(std::chrono::nanoseconds(quotient)));
}

/* dur == other  (only true for another Duration with the same nanoseconds) */
mrb_value
duration_eq(mrb_state* mrb, mrb_value self)
{
  mrb_value other;
  mrb_get_args(mrb, "o", &other);
  if (!mrb_data_p(other) || DATA_TYPE(other) != &duration_type) {
    return mrb_false_value();
  }
  struct timespec a = duration_to_timespec(mrb, self);
  struct timespec b = duration_to_timespec(mrb, other);
  return mrb_bool_value(a.tv_sec == b.tv_sec && a.tv_nsec == b.tv_nsec);
}

/* dur <=> other → -1 / 0 / 1, or nil if other isn't a Duration */
mrb_value
duration_cmp(mrb_state* mrb, mrb_value self)
{
  mrb_value other;
  mrb_get_args(mrb, "o", &other);
  if (!mrb_data_p(other) || DATA_TYPE(other) != &duration_type) {
    return mrb_nil_value();
  }
  auto a = detail::timespec_to_ns(duration_to_timespec(mrb, self));
  auto b = detail::timespec_to_ns(duration_to_timespec(mrb, other));
  int cmp = (a < b) ? -1 : (a > b ? 1 : 0);
  return mrb_fixnum_value(cmp);
}

/* dur.inspect — "#<Chrono::Duration tv_sec=1 tv_nsec=500000000>" */
mrb_value
duration_inspect(mrb_state* mrb, mrb_value self)
{
  struct timespec ts = duration_to_timespec(mrb, self);
  /* %i in mrb_format reads mrb_int from varargs (vs %d which reads int);
   * see mrb_vformat in mruby/src/error.c. Cast tv_sec/tv_nsec to mrb_int
   * — on builds where time_t is wider than mrb_int the high bits get
   * truncated, which for inspect output is acceptable. */
  return mrb_format(mrb, "#<Chrono::Duration tv_sec=%i tv_nsec=%i>",
                    (mrb_int)ts.tv_sec, (mrb_int)ts.tv_nsec);
}


/* ----- Numeric extensions: 5.ms, 200.us, ... ----------------------- */

#define DEFINE_NUMERIC_TAG(name, ChronoType)                                   \
  mrb_value                                                                    \
  numeric_##name(mrb_state* mrb, mrb_value self) {                             \
    if (!is_integerish(self)) {                                                \
      mrb_raise(mrb, E_TYPE_ERROR,                                             \
                "Numeric#" #name ": only Integer supported");                  \
    }                                                                          \
    auto chrono_v = ChronoType(to_i64(mrb, self));                             \
    auto total_ns = std::chrono::duration_cast<std::chrono::nanoseconds>(      \
                      chrono_v);                                               \
    return new_duration(mrb, duration_class(mrb),                              \
                        detail::ns_to_timespec(total_ns));                     \
  }

DEFINE_NUMERIC_TAG(nanoseconds,  std::chrono::nanoseconds)
DEFINE_NUMERIC_TAG(microseconds, std::chrono::microseconds)
DEFINE_NUMERIC_TAG(milliseconds, std::chrono::milliseconds)
DEFINE_NUMERIC_TAG(seconds,      std::chrono::seconds)
DEFINE_NUMERIC_TAG(minutes,      std::chrono::minutes)
DEFINE_NUMERIC_TAG(hours,        std::chrono::hours)
#if defined(__cpp_lib_chrono) && __cpp_lib_chrono >= 201907L
DEFINE_NUMERIC_TAG(days,         std::chrono::days)
DEFINE_NUMERIC_TAG(weeks,        std::chrono::weeks)
#endif

#undef DEFINE_NUMERIC_TAG

} /* anonymous namespace */


/* ----- Out-of-line C++ extension API definitions ------------------- */

struct timespec
mrb_chrono::duration_to_timespec(mrb_state* mrb, mrb_value v)
{
  return *mrb_cpp_get<timespec>(mrb, v);
}

mrb_value
mrb_chrono::from_timespec(mrb_state* mrb, struct timespec ts)
{
  return new_duration(mrb, duration_class(mrb), ts);
}


/* ----- Original Steady / System clocks + Timer -----------------------
 *
 * Preserved from the original gem (10+ years shipped). Float seconds
 * since the appropriate epoch; Timer wraps a steady_clock::time_point
 * for elapsed-time measurement.
 *
 * These all return Float, so they're guarded on float being available.
 * MRB_USE_FLOAT32 is refused at the top of this file (too narrow for
 * "seconds since 1970" precision).
 */

#ifndef MRB_NO_FLOAT
namespace {

mrb_value
mrb_chrono_steady_now(mrb_state* mrb, mrb_value /*self*/)
{
  auto t = std::chrono::steady_clock::now().time_since_epoch();
  return mrb_float_value(mrb,
    std::chrono::duration<mrb_float>(t).count());
}

/* Same value as ::Steady.now but as a Chrono::Duration — for handing
 * the elapsed time to mrb_chrono::as<T> or composing with other
 * Durations without going through Float seconds. */
mrb_value
mrb_chrono_steady_duration(mrb_state* mrb, mrb_value /*self*/)
{
  auto t = std::chrono::steady_clock::now().time_since_epoch();
  return mrb_chrono::from(mrb, t);
}

mrb_value
mrb_chrono_system_now(mrb_state* mrb, mrb_value /*self*/)
{
  auto t = std::chrono::system_clock::now().time_since_epoch();
  return mrb_float_value(mrb,
    std::chrono::duration<mrb_float>(t).count());
}

/* No System.duration: the system clock gives a time point (a moment),
 * not a duration. Returning "seconds since 1970 as a Chrono::Duration"
 * would be a type-system lie about what the value is. Users wanting
 * the wall-clock time as a Numeric stay with Chrono.system / .now. */

struct Timer {
  std::chrono::steady_clock::time_point start;

  Timer() : start(std::chrono::steady_clock::now()) {}

  void reset() {
    start = std::chrono::steady_clock::now();
  }

  std::chrono::steady_clock::duration since_start() const {
    return std::chrono::steady_clock::now() - start;
  }

  mrb_float elapsed() const {
    return std::chrono::duration<mrb_float>(since_start()).count();
  }
};

} /* anonymous namespace */

/* Data-type binding from mruby-c-ext-helpers: defines timer_type and
 * timer_free, plus the mrb_data_type_traits<Timer> specialization so
 * mrb_cpp_new<Timer>(mrb, self) works. */
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
  return mrb_float_value(mrb,
    mrb_cpp_get<Timer>(mrb, self)->elapsed());
}

/* Duration counterpart of #elapsed — same value, as a Chrono::Duration. */
mrb_value
timer_duration(mrb_state* mrb, mrb_value self)
{
  return mrb_chrono::from(mrb,
    mrb_cpp_get<Timer>(mrb, self)->since_start());
}

} /* anonymous namespace */
#endif /* MRB_NO_FLOAT */


/* ----- gem_init / gem_final ----------------------------------------- */
MRB_BEGIN_DECL

void
mrb_mruby_chrono_gem_init(mrb_state* mrb)
{
  struct RClass* chrono_mod =
    mrb_define_module_id(mrb, MRB_SYM(Chrono));

  struct RClass* dur_cls = mrb_define_class_under_id(
    mrb, chrono_mod, MRB_SYM(Duration), mrb->object_class);
  MRB_SET_INSTANCE_TT(dur_cls, MRB_TT_DATA);

  /* Mix in Comparable so <, <=, >, >=, between?, clamp work off our
   * <=> implementation. */
  mrb_include_module(mrb, dur_cls,
                     mrb_module_get_id(mrb, MRB_SYM(Comparable)));

  mrb_define_method_id(mrb, dur_cls, MRB_SYM(initialize),
                       duration_init, MRB_ARGS_REQ(2));
  mrb_define_method_id(mrb, dur_cls, MRB_SYM(as),
                       duration_as, MRB_ARGS_ARG(1, 1));
#ifndef MRB_NO_FLOAT
  mrb_define_method_id(mrb, dur_cls, MRB_SYM(as_f),
                       duration_as_f, MRB_ARGS_REQ(1));
#endif
  mrb_define_method_id(mrb, dur_cls, MRB_OPSYM(add),
                       duration_plus,  MRB_ARGS_REQ(1));
  mrb_define_method_id(mrb, dur_cls, MRB_OPSYM(sub),
                       duration_minus, MRB_ARGS_REQ(1));
  mrb_define_method_id(mrb, dur_cls, MRB_OPSYM(mul),
                       duration_mul,   MRB_ARGS_REQ(1));
  mrb_define_method_id(mrb, dur_cls, MRB_OPSYM(div),
                       duration_div,   MRB_ARGS_REQ(1));
  mrb_define_method_id(mrb, dur_cls, MRB_OPSYM(eq),
                       duration_eq,    MRB_ARGS_REQ(1));
  mrb_define_method_id(mrb, dur_cls, MRB_OPSYM(cmp),
                       duration_cmp,   MRB_ARGS_REQ(1));
  mrb_define_method_id(mrb, dur_cls, MRB_SYM(inspect),
                       duration_inspect, MRB_ARGS_NONE());
  mrb_define_method_id(mrb, dur_cls, MRB_SYM(to_s),
                       duration_inspect, MRB_ARGS_NONE());

  /* Numeric extensions: 5.ms, 200.us, 2.h, etc. */
  struct RClass* integer = mrb->integer_class;
  mrb_define_method_id(mrb, integer, MRB_SYM(nanoseconds),
                       numeric_nanoseconds,  MRB_ARGS_NONE());
  mrb_define_method_id(mrb, integer, MRB_SYM(ns),
                       numeric_nanoseconds,  MRB_ARGS_NONE());
  mrb_define_method_id(mrb, integer, MRB_SYM(microseconds),
                       numeric_microseconds, MRB_ARGS_NONE());
  mrb_define_method_id(mrb, integer, MRB_SYM(us),
                       numeric_microseconds, MRB_ARGS_NONE());
  mrb_define_method_id(mrb, integer, MRB_SYM(milliseconds),
                       numeric_milliseconds, MRB_ARGS_NONE());
  mrb_define_method_id(mrb, integer, MRB_SYM(ms),
                       numeric_milliseconds, MRB_ARGS_NONE());
  mrb_define_method_id(mrb, integer, MRB_SYM(seconds),
                       numeric_seconds,      MRB_ARGS_NONE());
  mrb_define_method_id(mrb, integer, MRB_SYM(s),
                       numeric_seconds,      MRB_ARGS_NONE());
  mrb_define_method_id(mrb, integer, MRB_SYM(minutes),
                       numeric_minutes,      MRB_ARGS_NONE());
  mrb_define_method_id(mrb, integer, MRB_SYM(min),
                       numeric_minutes,      MRB_ARGS_NONE());
  mrb_define_method_id(mrb, integer, MRB_SYM(hours),
                       numeric_hours,        MRB_ARGS_NONE());
  mrb_define_method_id(mrb, integer, MRB_SYM(h),
                       numeric_hours,        MRB_ARGS_NONE());
#if defined(__cpp_lib_chrono) && __cpp_lib_chrono >= 201907L
  mrb_define_method_id(mrb, integer, MRB_SYM(days),
                       numeric_days,         MRB_ARGS_NONE());
  mrb_define_method_id(mrb, integer, MRB_SYM(weeks),
                       numeric_weeks,        MRB_ARGS_NONE());
#endif

  /* ----- Original Steady / System / Timer surface (Float-only) ----- */
#ifndef MRB_NO_FLOAT
  /* Chrono.steady, Chrono.system (module-function aliases) */
  mrb_define_module_function_id(mrb, chrono_mod, MRB_SYM(steady),
                                mrb_chrono_steady_now, MRB_ARGS_NONE());
  mrb_define_module_function_id(mrb, chrono_mod, MRB_SYM(system),
                                mrb_chrono_system_now, MRB_ARGS_NONE());

  /* Chrono::Steady.now / Chrono::Steady.duration */
  struct RClass* steady_mod =
    mrb_define_module_under_id(mrb, chrono_mod, MRB_SYM(Steady));
  mrb_define_module_function_id(mrb, steady_mod, MRB_SYM(now),
                                mrb_chrono_steady_now, MRB_ARGS_NONE());
  mrb_define_module_function_id(mrb, steady_mod, MRB_SYM(duration),
                                mrb_chrono_steady_duration, MRB_ARGS_NONE());

  /* Chrono::System.now — no .duration (System is a time point) */
  struct RClass* system_mod =
    mrb_define_module_under_id(mrb, chrono_mod, MRB_SYM(System));
  mrb_define_module_function_id(mrb, system_mod, MRB_SYM(now),
                                mrb_chrono_system_now, MRB_ARGS_NONE());

  /* Chrono::Timer.new / #reset / #elapsed / #duration */
  struct RClass* timer_cls = mrb_define_class_under_id(
    mrb, chrono_mod, MRB_SYM(Timer), mrb->object_class);
  MRB_SET_INSTANCE_TT(timer_cls, MRB_TT_DATA);
  mrb_define_method_id(mrb, timer_cls, MRB_SYM(initialize),
                       timer_init,     MRB_ARGS_NONE());
  mrb_define_method_id(mrb, timer_cls, MRB_SYM(reset),
                       timer_reset,    MRB_ARGS_NONE());
  mrb_define_method_id(mrb, timer_cls, MRB_SYM(elapsed),
                       timer_elapsed,  MRB_ARGS_NONE());
  mrb_define_method_id(mrb, timer_cls, MRB_SYM(duration),
                       timer_duration, MRB_ARGS_NONE());
#endif /* MRB_NO_FLOAT */
}

void
mrb_mruby_chrono_gem_final(mrb_state*) {}

MRB_END_DECL