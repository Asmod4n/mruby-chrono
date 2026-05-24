/*
 * mruby-chrono — C++ extension API
 *
 * Mirrors std::chrono's naming convention — the rounding function is
 * the outermost call, just like the standard library:
 *
 *   auto ms = mrb_chrono::as<std::chrono::milliseconds>(mrb, v);    // truncate
 *   auto ms = mrb_chrono::floor<std::chrono::milliseconds>(mrb, v); // toward -∞
 *   auto ms = mrb_chrono::ceil<std::chrono::milliseconds>(mrb, v);  // toward +∞
 *   auto ms = mrb_chrono::round<std::chrono::milliseconds>(mrb, v); // half-to-even
 *
 *   mrb_value v = mrb_chrono::from(mrb, std::chrono::milliseconds(500)); // -> 0.5
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

    /* Convert Float seconds mrb_value to duration<double, Target::period>.
     * All four as/floor/ceil/round functions go through this. */
    template <typename Target>
    std::chrono::duration<double, typename Target::period>
      to_float_dur(mrb_state* mrb, mrb_value v)
    {
      using period = typename Target::period;
      double ticks = static_cast<double>(mrb_as_float(mrb, v))
        * (static_cast<double>(period::den) / static_cast<double>(period::num));
      return std::chrono::duration<double, period>(ticks);
    }
  } /* namespace detail */


  /* Truncate toward zero — equivalent to std::chrono::duration_cast */
  template <typename Target,
    typename = std::enable_if_t<detail::is_duration_v<Target>>>
  Target
    as(mrb_state* mrb, mrb_value v)
  {
    return std::chrono::duration_cast<Target>(detail::to_float_dur<Target>(mrb, v));
  }

  /* Toward -∞ — equivalent to std::chrono::floor */
  template <typename Target,
    typename = std::enable_if_t<detail::is_duration_v<Target>>>
  Target
    floor(mrb_state* mrb, mrb_value v)
  {
    return std::chrono::floor<Target>(detail::to_float_dur<Target>(mrb, v));
  }

  /* Toward +∞ — equivalent to std::chrono::ceil */
  template <typename Target,
    typename = std::enable_if_t<detail::is_duration_v<Target>>>
  Target
    ceil(mrb_state* mrb, mrb_value v)
  {
    return std::chrono::ceil<Target>(detail::to_float_dur<Target>(mrb, v));
  }

  /* Half-to-even (banker's) — equivalent to std::chrono::round */
  template <typename Target,
    typename = std::enable_if_t<detail::is_duration_v<Target>>>
  Target
    round(mrb_state* mrb, mrb_value v)
  {
    return std::chrono::round<Target>(detail::to_float_dur<Target>(mrb, v));
  }


  /* Any std::chrono::duration → mrb_value (Float, seconds) */
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