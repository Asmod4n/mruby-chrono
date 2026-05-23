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
 * Storage internally is std::chrono::nanoseconds — the same type that
 * std::chrono::system_clock::duration and ::steady_clock::duration
 * already are on libstdc++ and libc++. Representable range is ±2^63 ns
 * ≈ ±292 years. The internal type is exposed as mrb_chrono::Duration
 * for callers that want to construct or read it directly.
 */

#pragma once

#include <mruby.h>
#include <mruby/data.h>
#include <chrono>
#include <cstdint>
#include <ctime>      /* struct timespec for to_timespec */
#include <type_traits>

namespace mrb_chrono {

/* The std::chrono type used as internal storage for Chrono::Duration.
 * Exposed for extension code that wants to construct or extract
 * directly via from_nanoseconds / stored_nanoseconds without going
 * through the templated from<T> / as<T> dispatch. */
using Duration = std::chrono::nanoseconds;


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


/* ----- Direct access to internal storage ----------------------------
 *
 * Used by the templated from<T> / as<T> below, and exposed for any
 * extension code that wants to work with the canonical
 * mrb_chrono::Duration type directly.
 *
 *   mrb_value dur = mrb_chrono::from_nanoseconds(mrb,
 *                     mrb_chrono::Duration(500'000'000));
 *
 *   mrb_chrono::Duration d = mrb_chrono::stored_nanoseconds(mrb, dur);
 *
 * These exist mainly as the substrate the templates dispatch through;
 * most callers should prefer mrb_chrono::from<T> / mrb_chrono::as<T>
 * because they don't tie the call site to a particular period.
 */
mrb_value from_nanoseconds(mrb_state* mrb, Duration ns);
Duration  stored_nanoseconds(mrb_state* mrb, mrb_value v);


/* ----- POSIX timespec interop ---------------------------------------
 *
 * Returns the Duration's value as a `struct timespec`, normalized so
 * tv_nsec is in [0, 1'000'000'000) — what POSIX syscalls expect.
 *
 *   struct timespec ts = mrb_chrono::to_timespec(mrb, dur);
 *   clock_nanosleep(CLOCK_MONOTONIC, 0, &ts, nullptr);
 *
 * On Linux, struct __kernel_timespec (used by io_uring submission
 * queue entries for timeouts) has the same layout as struct timespec
 * on 64-bit time_t platforms, so a reinterpret_cast or memcpy is the
 * usual bridge:
 *
 *   struct timespec ts = mrb_chrono::to_timespec(mrb, dur);
 *   struct __kernel_timespec kts;
 *   std::memcpy(&kts, &ts, sizeof(kts));   // or reinterpret_cast
 *   io_uring_prep_timeout(sqe, &kts, 0, 0);
 *
 * The reverse direction (timespec → Duration) intentionally has no
 * dedicated helper. The standard-library one-liner is plenty:
 *
 *   mrb_value dur = mrb_chrono::from(mrb,
 *                     std::chrono::seconds(ts.tv_sec) +
 *                     std::chrono::nanoseconds(ts.tv_nsec));
 */
struct timespec to_timespec(mrb_state* mrb, mrb_value v);


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
 * Internally: duration_cast to mrb_chrono::Duration (= nanoseconds),
 * store. Out-of-range inputs follow std::chrono::duration_cast's
 * behavior; the working range is ±292 years.
 */
template <typename ChronoDuration>
mrb_value from(mrb_state* mrb, ChronoDuration d) {
  static_assert(detail::is_std_chrono_duration_v<ChronoDuration>,
                "mrb_chrono::from: argument must be a std::chrono::duration");
  return from_nanoseconds(mrb,
    std::chrono::duration_cast<Duration>(d));
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

  Duration stored = stored_nanoseconds(mrb, v);

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