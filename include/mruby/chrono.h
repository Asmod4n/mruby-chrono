/*
 * mruby-chrono — C extension API
 *
 * One function, three enums, a void* output parameter, and a size_t
 * for the caller to prove the pointer matches the requested type.
 *
 *   #include <mruby/chrono.h>
 *
 *   // 500.ms → int32_t milliseconds, truncated
 *   int32_t ms;
 *   mrb_chrono_convert(mrb, v,
 *     MRB_CHRONO_OUT_INT32, MRB_CHRONO_DUR_MILLISECONDS, MRB_CHRONO_TRUNC,
 *     &ms, sizeof ms);
 *
 *   // 500.ms → struct timespec, ceiling
 *   struct timespec ts;
 *   mrb_chrono_convert(mrb, v,
 *     MRB_CHRONO_OUT_TIMESPEC, MRB_CHRONO_DUR_NANOSECONDS, MRB_CHRONO_CEIL,
 *     &ts, sizeof ts);
 *
 *   // 500.ms → long milliseconds (c-ares), nearest
 *   long ms;
 *   mrb_chrono_convert(mrb, v,
 *     MRB_CHRONO_OUT_LONG, MRB_CHRONO_DUR_MILLISECONDS, MRB_CHRONO_NEAREST,
 *     &ms, sizeof ms);
 *
 * Raises TypeError  if v is not numeric.
 * Raises ArgumentError if out is NULL or out_size != sizeof(out_type).
 * Raises RangeError  if the value does not fit in the requested type.
 */

#pragma once

#include <mruby.h>
#include <stdint.h>
#include <stddef.h>
#include <time.h>   /* struct timespec — C11, POSIX, VS2015+ */

#ifdef _WIN32
#  if !defined(_TIMEVAL_DEFINED) && !defined(_WINSOCKAPI_) && !defined(_WINSOCK2API_)
struct timeval { long tv_sec; long tv_usec; };
#    define _TIMEVAL_DEFINED
#  endif
#else
#  include <sys/time.h>
#endif

 /* ----- Output type ------------------------------------------------- */

typedef enum mrb_chrono_out_type {
  /* Fixed-width integers */
  MRB_CHRONO_OUT_INT8 = 0,
  MRB_CHRONO_OUT_UINT8 = 1,
  MRB_CHRONO_OUT_INT16 = 2,
  MRB_CHRONO_OUT_UINT16 = 3,
  MRB_CHRONO_OUT_INT32 = 4,
  MRB_CHRONO_OUT_UINT32 = 5,
  MRB_CHRONO_OUT_INT64 = 6,
  MRB_CHRONO_OUT_UINT64 = 7,
  /* Platform-width integers */
  MRB_CHRONO_OUT_INT = 8,
  MRB_CHRONO_OUT_UINT = 9,
  MRB_CHRONO_OUT_LONG = 10,
  MRB_CHRONO_OUT_ULONG = 11,
  MRB_CHRONO_OUT_LLONG = 12,
  MRB_CHRONO_OUT_ULLONG = 13,
  /* Floating-point */
  MRB_CHRONO_OUT_FLOAT = 14,
  MRB_CHRONO_OUT_DOUBLE = 15,
  /* Struct types */
  MRB_CHRONO_OUT_TIMESPEC = 16,  /* struct timespec — nanosecond resolution */
  MRB_CHRONO_OUT_TIMEVAL = 17,  /* struct timeval  — microsecond resolution */
	MRB_CHRONO_OUT_TIME_T = 18,  /* time_t (seconds, for legacy APIs) */
} mrb_chrono_out_type;

/* ----- Duration (unit) type ---------------------------------------- */

typedef enum mrb_chrono_dur_type {
  MRB_CHRONO_DUR_NANOSECONDS = 0,
  MRB_CHRONO_DUR_MICROSECONDS = 1,
  MRB_CHRONO_DUR_MILLISECONDS = 2,
  MRB_CHRONO_DUR_SECONDS = 3,
  MRB_CHRONO_DUR_MINUTES = 4,
  MRB_CHRONO_DUR_HOURS = 5,
  MRB_CHRONO_DUR_DAYS = 6,
  MRB_CHRONO_DUR_WEEKS = 7,
} mrb_chrono_dur_type;

/* ----- Rounding mode ----------------------------------------------- */

typedef enum mrb_chrono_rounding {
  MRB_CHRONO_TRUNC = 0,  /* toward zero (default C cast) */
  MRB_CHRONO_FLOOR = 1,  /* toward -∞                    */
  MRB_CHRONO_CEIL = 2,  /* toward +∞                    */
  MRB_CHRONO_NEAREST = 3,  /* half-away-from-zero          */
} mrb_chrono_rounding;

/* ----- The one function -------------------------------------------- */

MRB_BEGIN_DECL

/*
 * Any numeric mrb_value interpreted as dur_type units → Float seconds.
 *
 *   mrb_value v = mrb_chrono_from(mrb,
 *                   mrb_fixnum_value(timeout_ms),
 *                   MRB_CHRONO_DUR_MILLISECONDS);
 *   // v == 0.5 (Float) for timeout_ms = 500
 *
 * Wrapping C/C++ numeric types into mrb_value before calling:
 *
 *   mrb_int_value(mrb, n)        — mrb_int (may be 32-bit on embedded builds)
 *   mrb_float_value(mrb, f)      — double / mrb_float
 *   mrb_convert_int64(mrb, n)    — int64_t, promotes to Bigint if > mrb_int max
 *   mrb_convert_uint64(mrb, n)   — uint64_t, promotes to Bigint if > mrb_int max
 *
 * Prefer mrb_convert_int64 / mrb_convert_uint64 for wide integer types
 * (nanosecond counts, large microsecond timestamps) so values that
 * exceed mrb_int range on narrow builds are not silently truncated.
 * mrb_as_float handles Bigint correctly via to_f on the receiving end.
 *
 * Raises TypeError  if v is not numeric.
 * Raises RangeError if v is non-finite.
 */
  MRB_API mrb_value mrb_chrono_from(mrb_state* mrb,
    mrb_value           v,
    mrb_chrono_dur_type dur_type);

MRB_API void mrb_chrono_convert(mrb_state* mrb,
  mrb_value            v,
  mrb_chrono_out_type  out_type,
  mrb_chrono_dur_type  dur_type,
  mrb_chrono_rounding  rounding,
  void* out,
  size_t               out_size);

MRB_END_DECL
