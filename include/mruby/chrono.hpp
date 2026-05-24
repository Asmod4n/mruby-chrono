/*
 * mruby-chrono — C++ extension API
 *
 * Duration values are plain Ruby Floats representing seconds.
 * These two templates convert between that Float and any
 * std::chrono::duration type.
 *
 *   #include <mruby/chrono.hpp>
 *
 *   // Float seconds mrb_value → any std::chrono::duration:
 *   auto ms  = mrb_chrono::as<std::chrono::milliseconds>(mrb, v);
 *   int  tms = (int)ms.count();
 *
 *   // any std::chrono::duration → Float seconds mrb_value:
 *   mrb_value v = mrb_chrono::from(mrb, std::chrono::milliseconds(500));
 *   // v == 0.5 (Float)
 */

#pragma once

#include <mruby.h>
#include <chrono>
#include <type_traits>

#ifdef MRB_NO_FLOAT
#  error "mruby-chrono requires floating-point support (MRB_NO_FLOAT is set)"
#endif
#ifdef MRB_USE_FLOAT32
#  error "mruby-chrono requires double precision (MRB_USE_FLOAT32 is set — clocks need it)"
#endif

namespace mrb_chrono {

  namespace detail {
    template <typename T>             struct is_duration : std::false_type {};
    template <typename R, typename P> struct is_duration<std::chrono::duration<R, P>> : std::true_type {};
    template <typename T> inline constexpr bool is_duration_v = is_duration<T>::value;
  } /* namespace detail */


  /*
   * mrb_value (Float, seconds) → Target std::chrono::duration.
   *
   * Raises TypeError (via mrb_to_flo) if v is not numeric.
   * Truncates toward zero when the float doesn't divide evenly.
   */
  template <typename Target,
    typename = std::enable_if_t<detail::is_duration_v<Target>>>
  Target
    as(mrb_state* mrb, mrb_value v)
  {
    using period = typename Target::period;
    using rep = typename Target::rep;
    /* seconds * (target ticks per second) */
    double ticks = static_cast<double>(mrb_as_float(mrb, v))
      * (static_cast<double>(period::den) / static_cast<double>(period::num));
    return Target(static_cast<rep>(ticks));
  }


  /*
   * Any std::chrono::duration → mrb_value (Float, seconds).
   */
  template <typename Source,
    typename = std::enable_if_t<detail::is_duration_v<Source>>>
  mrb_value
    from(mrb_state* mrb, Source dur)
  {
    using period = typename Source::period;
    double sec = static_cast<double>(dur.count())
      * (static_cast<double>(period::num) / static_cast<double>(period::den));
    return mrb_float_value(mrb, static_cast<mrb_float>(sec));
  }

} /* namespace mrb_chrono */