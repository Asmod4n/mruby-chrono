# mruby-chrono

Numeric unit methods and C/C++ time-API interop for mruby.

```ruby
500.ms        # => 0.5   (Float seconds)
1.h           # => 3600.0
200.us        # => 0.0002
Rational(1,3).s  # => 0.3333...
```

All unit methods — `.ns` `.us` `.ms` `.s` `.min` `.h` (and `.days`/`.weeks` on C++20+) —
return a plain Float representing seconds. On 64-bit mruby these are NaN-boxed and
never heap-allocated. Standard Float arithmetic applies directly.

## C++ API

```cpp
#include <mruby/chrono.hpp>

// std::chrono duration -> Float seconds mrb_value
mrb_value v = mrb_chrono::from(mrb, std::chrono::milliseconds(500)); // 0.5

// Float seconds mrb_value -> any std::chrono duration
auto ms = mrb_chrono::as<std::chrono::milliseconds>(mrb, v); // std::chrono::milliseconds(500)
auto us = mrb_chrono::as<std::chrono::microseconds>(mrb, v); // std::chrono::microseconds(500000)

// Use .count() to get the raw integer for C APIs
int timeout_ms = (int)ms.count(); // 500
```

## C API

Wrap a C numeric into a Float seconds mrb_value using `mrb_chrono_from`.
Use `mrb_convert_int64` / `mrb_convert_uint64` (from mruby-c-ext-helpers) for wide
integer types — they promote to Bigint automatically when the value exceeds `mrb_int` range.

```c
#include <mruby/chrono.h>
#include <mruby/num_helpers.h>

// C numeric in known units -> Float seconds
mrb_value v = mrb_chrono_from(mrb, mrb_convert_int64(mrb, timeout_ns),
                               MRB_CHRONO_DUR_NANOSECONDS);

// Float seconds -> whatever C type your library needs
int32_t  ms; mrb_chrono_convert(mrb, v, MRB_CHRONO_OUT_INT32,
                MRB_CHRONO_DUR_MILLISECONDS, MRB_CHRONO_TRUNC, &ms, sizeof ms);

long     ml; mrb_chrono_convert(mrb, v, MRB_CHRONO_OUT_LONG,
                MRB_CHRONO_DUR_MILLISECONDS, MRB_CHRONO_TRUNC, &ml, sizeof ml);

struct timespec ts; mrb_chrono_convert(mrb, v, MRB_CHRONO_OUT_TIMESPEC,
                MRB_CHRONO_DUR_NANOSECONDS, MRB_CHRONO_CEIL, &ts, sizeof ts);

struct timeval  tv; mrb_chrono_convert(mrb, v, MRB_CHRONO_OUT_TIMEVAL,
                MRB_CHRONO_DUR_MICROSECONDS, MRB_CHRONO_FLOOR, &tv, sizeof tv);
```

**Output types** (`mrb_chrono_out_type`): `INT8`…`UINT64`, `INT`/`UINT`, `LONG`/`ULONG`,
`LLONG`/`ULLONG`, `FLOAT`, `DOUBLE`, `TIMESPEC`, `TIMEVAL`.

**Duration types** (`mrb_chrono_dur_type`): `NANOSECONDS` through `WEEKS`.

**Rounding** (`mrb_chrono_rounding`): `TRUNC` (toward zero), `FLOOR` (toward −∞),
`CEIL` (toward +∞), `NEAREST` (banker's / half-to-even).

The size parameter is checked against `sizeof(output_type)` before writing —
a mismatch raises `ArgumentError`.

## Clocks and Timer

```ruby
Chrono::Steady.now   # monotonic Float seconds
Chrono::System.now   # wall-clock Float seconds since Unix epoch
Chrono.steady        # alias for Chrono::Steady.now
Chrono.system        # alias for Chrono::System.now

t = Chrono::Timer.new
do_work
t.elapsed            # Float seconds since .new or last #reset
t.reset              # restart
```

## Requirements

C++17 or later. `MRB_USE_FLOAT32` and `MRB_NO_FLOAT` are not supported.
Depends on [mruby-c-ext-helpers](https://github.com/Hendrik-2/mruby-c-ext-helpers).

## License

Apache-2.0