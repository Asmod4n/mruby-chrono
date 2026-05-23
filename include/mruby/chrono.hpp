/*
 * mruby-chrono — the entire public C++ extension API
 *
 * Two function templates and one rounding enum. That's the surface.
 *
 *     #include <mruby/chrono.hpp>
 *
 *     // Wrap a std::chrono value into a Chrono::Duration mrb_value:
 *     mrb_value dur = mrb_chrono::from(mrb, std::chrono::milliseconds(500));
 *
 *     // Extract a std::chrono value (of any target type) back out:
 *     auto ms = mrb_chrono::as<std::chrono::milliseconds>(mrb, dur);
 *     int timeout_ms = (int)ms.count();
 *
 *     auto us = mrb_chrono::as<std::chrono::microseconds>(
 *                 mrb, dur, mrb_chrono::Rounding::Floor);
 *
 * On C++17, target/source types are nanoseconds through hours. On
 * C++20+ (__cpp_lib_chrono >= 201907L), std::chrono::days and
 * std::chrono::weeks are accepted too — std::chrono provides them, the
 * gem just routes through. The gem doesn't synthesize types the
 * standard doesn't have.
 *
 * Storage internally is `struct timespec` in the mruby data slot.
 * That's an implementation detail; the C++ API is in terms of
 * std::chrono types.
 */

#pragma once

#include <mruby.h>
#include <mruby/data.h>
#include <chrono>
#include <cstdint>
#include <ctime>
#include <type_traits>

namespace mrb_chrono {

/* ----- Rounding policies --------------------------------------------
 *
 * Four modes, each a thin wrapper around the std::chrono operation of
 * the same intent. The gem doesn't add modes the standard library
 * doesn't already provide.
 */
enum class Rounding : uint8_t {
  Truncate = 0,  /* toward zero  — std::chrono::duration_cast */
  Floor    = 1,  /* toward -∞    — std::chrono::floor         */
  Ceil     = 2,  /* toward +∞    — std::chrono::ceil          */
  Round    = 3,  /* nearest, banker's tie-break — std::chrono::round */
};


/* ----- is_std_chrono_duration trait --------------------------------- */
namespace detail {
  template <typename T>
  struct is_std_chrono_duration : std::false_type {};

  template <typename Rep, typename Period>
  struct is_std_chrono_duration<std::chrono::duration<Rep, Period>>
    : std::true_type {};

  template <typename T>
  inline constexpr bool is_std_chrono_duration_v =
    is_std_chrono_duration<T>::value;
}


/* ----- Duration data type and accessor ------------------------------
 *
 * The Chrono::Duration mruby object holds a `struct timespec` in its
 * data slot. C++ extension code unwraps it via duration_to_timespec
 * below — the typed accessor type-checks and returns the timespec by
 * value. The mrb_data_type itself is TU-local in mrb-chrono.cpp;
 * external code reaches the timespec only through this function.
 */
struct timespec duration_to_timespec(mrb_state* mrb, mrb_value v);


/* ----- Internal: timespec ↔ std::chrono::nanoseconds ----------------
 *
 * The bridge that everything else goes through. timespec is POSIX
 * storage; nanoseconds is the working type for std::chrono operations.
 * The conversion is well-defined as long as (tv_sec * 1e9 + tv_nsec)
 * fits int64_t, which covers ±292 years — practical infinity.
 */
namespace detail {
  inline std::chrono::nanoseconds
  timespec_to_ns(struct timespec ts) {
    return std::chrono::seconds(ts.tv_sec) +
           std::chrono::nanoseconds(ts.tv_nsec);
  }

  inline struct timespec
  ns_to_timespec(std::chrono::nanoseconds total) {
    constexpr int64_t NS_PER_SEC = 1'000'000'000;
    int64_t n = total.count();
    struct timespec ts;
    ts.tv_sec  = static_cast<time_t>(n / NS_PER_SEC);
    ts.tv_nsec = static_cast<long>(n % NS_PER_SEC);
    /* Normalize: tv_nsec must be in [0, NS_PER_SEC). For negative
     * total this requires borrowing from tv_sec. */
    if (ts.tv_nsec < 0) {
      ts.tv_nsec += NS_PER_SEC;
      ts.tv_sec  -= 1;
    }
    return ts;
  }
}


/* ----- mrb_chrono::from -----------------------------------------------
 *
 * Wrap a std::chrono::duration of any unit into a Chrono::Duration
 * mrb_value. The source's period is what gives the Duration its
 * meaning; the source's Rep is just how the count was carried.
 *
 *     mrb_value d1 = mrb_chrono::from(mrb, std::chrono::milliseconds(500));
 *     mrb_value d2 = mrb_chrono::from(mrb, std::chrono::microseconds(uint64_t{1'000'000}));
 *     mrb_value d3 = mrb_chrono::from(mrb, std::chrono::duration<double>(0.5));  // half a second
 *
 * Internally: cast to std::chrono::nanoseconds, store as timespec.
 * Raises RangeError if the input's nanosecond representation would
 * overflow int64_t (basically unreachable for sane time values).
 */
mrb_value from_timespec(mrb_state* mrb, struct timespec ts);

template <typename ChronoDuration>
mrb_value from(mrb_state* mrb, ChronoDuration d) {
  static_assert(detail::is_std_chrono_duration_v<ChronoDuration>,
                "mrb_chrono::from: argument must be a std::chrono::duration");
  auto total_ns = std::chrono::duration_cast<std::chrono::nanoseconds>(d);
  return from_timespec(mrb, detail::ns_to_timespec(total_ns));
}


/* ----- mrb_chrono::as -------------------------------------------------
 *
 * Extract the Duration's value as a std::chrono::duration of the
 * caller's choice. The caller picks the period (and Rep, via the
 * full ChronoDuration type); the gem does the cast.
 *
 *     auto ms = mrb_chrono::as<std::chrono::milliseconds>(mrb, dur);
 *     int timeout_ms = (int)ms.count();
 *
 *     auto us_floored = mrb_chrono::as<std::chrono::microseconds>(
 *                         mrb, dur, mrb_chrono::Rounding::Floor);
 *
 * Default rounding is Truncate (matches std::chrono::duration_cast).
 * Floor / Ceil / Round delegate to the corresponding std::chrono free
 * function. Out-of-range conversions (where the target type's Rep is
 * narrow and the stored nanosecond count doesn't fit) follow the
 * std::chrono cast's behavior — typically silent truncation; the user
 * should pick a Rep wide enough for their range.
 */
template <typename ChronoDuration>
ChronoDuration as(mrb_state* mrb, mrb_value v,
                  Rounding r = Rounding::Truncate)
{
  static_assert(detail::is_std_chrono_duration_v<ChronoDuration>,
                "mrb_chrono::as: target must be a std::chrono::duration type");

  struct timespec ts = duration_to_timespec(mrb, v);
  auto stored = detail::timespec_to_ns(ts);

  switch (r) {
    case Rounding::Truncate: return std::chrono::duration_cast<ChronoDuration>(stored);
    case Rounding::Floor:    return std::chrono::floor<ChronoDuration>(stored);
    case Rounding::Ceil:     return std::chrono::ceil<ChronoDuration>(stored);
    case Rounding::Round:    return std::chrono::round<ChronoDuration>(stored);
  }
  /* Unreachable; switch is exhaustive over the enum's values. */
  return std::chrono::duration_cast<ChronoDuration>(stored);
}

} /* namespace mrb_chrono */
