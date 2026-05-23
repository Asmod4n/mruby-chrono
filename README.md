# mruby-chrono

Adds time facilities to mruby modelled on C++'s `<chrono>`. Three
pieces:

- **Steady and System clocks**, returning `Float` seconds since the
  appropriate epoch — the original gem's surface, preserved.
- **`Chrono::Timer`** for measuring elapsed time using the steady
  clock — also preserved.
- **`Chrono::Duration`**, a typed duration value with C and C++
  extension interop, for handing time values to libraries that want
  `int timeout_ms`, `struct timespec`, `std::chrono::microseconds`, or
  fractional seconds.

Requires a C++17-compatible compiler. Days and weeks become available
on C++20+ builds where `std::chrono::days` and `std::chrono::weeks`
exist in the standard library. `MRB_USE_FLOAT32` is refused — the
clocks return Float seconds since 1970, which needs more precision
than float32 has.

## Clocks

Two clocks. `Steady` is monotonic — useful for measuring intervals.
`System` is the wall clock and can jump (NTP corrections, manual time
set).

```ruby
Chrono::Steady.now        # Float seconds, monotonic
Chrono::Steady.duration   # Chrono::Duration, monotonic
Chrono.steady             # alias for Chrono::Steady.now

Chrono::System.now        # Float seconds since Unix epoch (wall clock)
Chrono.system             # alias for Chrono::System.now
```

`Steady` exposes both `.now` (Float seconds) and `.duration`
(Chrono::Duration). Use `.duration` when you want to compose with
Duration arithmetic or hand the value to a C/C++ extension via
`mrb_chrono::as<T>` without round-tripping through Float seconds.

`System` only exposes `.now`. There is intentionally no
`System.duration` — the system clock represents a point in time, not a
span, and wrapping "seconds since 1970" in a `Chrono::Duration` would
misrepresent what the value is. Users wanting the wall clock as a
Numeric stay with `Chrono.system` / `Chrono::System.now`.

## Timer

A start/stop timer using the steady clock:

```ruby
t = Chrono::Timer.new
do_work
t.elapsed    # => Float seconds since .new or last #reset
t.duration   # => Chrono::Duration since .new or last #reset
t.reset      # restart from now
```

Both `#elapsed` and `#duration` read the same `now() - start` value;
they differ only in shape. `#elapsed` is the original Float-seconds
form; `#duration` is the new Duration form.

## Durations

`Chrono::Duration` is a unit-tagged time value. The intent is one
thing: hand the same "500 milliseconds" to whichever C or C++ library
needs it, in whichever shape that library expects.

### Ruby surface

```ruby
# Construct — any Numeric works (Integer, Float, Rational, Bigint, ...)
500.ms                              # Integer; also .us, .ns, .s, .min, .h
                                    # .days and .weeks on C++20+ builds
0.5.s                               # Float — fractional seconds
Rational(1, 3).s                    # Rational — coerced via to_f
Chrono::Duration.new(500, :ms)      # explicit form
Chrono::Duration.new(Rational(1, 2), :ms)

# Convert out
dur.as(:microseconds)               # Integer; truncates by default
dur.as(:microseconds, :round)       # Integer with a rounding policy
dur.as_f(:seconds)                  # Float — for IO.select et al.

# Arithmetic — scalars may be any Numeric
dur + other                         # Duration + Duration
dur - other                         # Duration - Duration
dur * 2                             # Duration * Numeric  (Integer/Float/Rational/...)
dur / Rational(1, 4)                # Duration / Numeric
```

Accepted unit Symbols, long and short:

| Long           | Short |
| -------------- | ----- |
| `:nanoseconds` | `:ns` |
| `:microseconds`| `:us` |
| `:milliseconds`| `:ms` |
| `:seconds`     | `:s`  |
| `:minutes`     | `:min`|
| `:hours`       | `:h`  |
| `:days`        | —     | *(C++20+)*
| `:weeks`       | —     | *(C++20+)*

Rounding policies — each one a `std::chrono` operation under a Ruby
Symbol name:

| Symbol       | std::chrono operation                  |
| ------------ | -------------------------------------- |
| `:truncate`  | `std::chrono::duration_cast<Target>`   |
| `:floor`     | `std::chrono::floor<Target>`           |
| `:ceil`      | `std::chrono::ceil<Target>`            |
| `:round`     | `std::chrono::round<Target>` (banker's)|

### Numeric coercion

Construction (`Chrono::Duration.new`, `Numeric#ms` etc.) and scalar
arithmetic (`*`, `/`) accept any `Numeric`. Integer and Bigint go
through an int64 fast path with overflow checking. Everything else —
Float, Rational, user-defined Numeric subclasses — is coerced via
`to_f`, so the result is a nearest-IEEE-754-double approximation
truncated to int64 nanoseconds. For terminating fractions this is
exact (`Rational(1, 2).ms == 500.us`); for repeating ones it's within
one nanosecond of the mathematical value (`Rational(1, 3).s` →
333,333,333 ns).

### C++ extension surface

```cpp
#include <mruby/chrono.hpp>

// Wrap any std::chrono::duration as a Chrono::Duration mrb_value:
mrb_value d = mrb_chrono::from(mrb, std::chrono::milliseconds(500));

// Extract as any std::chrono::duration target type:
auto ms = mrb_chrono::as<std::chrono::milliseconds>(mrb, d);
int timeout_ms = (int)ms.count();

// With a rounding policy:
auto us = mrb_chrono::as<std::chrono::microseconds>(
            mrb, d, mrb_chrono::Rounding::Floor);
```

The whole C++ surface: `from`, `as`, and the `Rounding` enum. Both
functions are templated on `std::chrono::duration` types — whatever
the user's C++ standard provides.

Add the gem as a dependency in your `mrbgem.rake`:

```ruby
spec.add_dependency 'mruby-chrono'
```

then `#include <mruby/chrono.hpp>` in your `.cpp` and call.

### Interop examples

**`IO.select` (mruby stdlib — takes fractional seconds):**

```ruby
timeout = 500.ms
IO.select([sock], nil, nil, timeout.as_f(:seconds))
```

**A C library wanting `int timeout_ms`:**

```cpp
extern "C" mrb_value
my_wait(mrb_state* mrb, mrb_value self) {
  mrb_value dur;
  mrb_get_args(mrb, "o", &dur);
  auto ms = mrb_chrono::as<std::chrono::milliseconds>(mrb, dur);
  int rc = lib_wait((int)ms.count());
  return mrb_fixnum_value(rc);
}
```

**Bridging between two C libraries with different unit conventions:**

```cpp
// lib_a hands out uint64_t nanoseconds, lib_b wants microseconds.
uint64_t ns = lib_a_timestamp();
mrb_value dur = mrb_chrono::from(mrb, std::chrono::nanoseconds(ns));
// ... dur can travel through Ruby code, be stored, passed around ...
auto us = mrb_chrono::as<std::chrono::microseconds>(mrb, dur);
lib_b_set_time(us.count());
```

**Constructing from POSIX `struct timespec`:**

```cpp
struct timespec ts;
clock_gettime(CLOCK_MONOTONIC, &ts);
mrb_value dur = mrb_chrono::from(mrb,
                  std::chrono::seconds(ts.tv_sec) +
                  std::chrono::nanoseconds(ts.tv_nsec));
```

**Benchmarking with Duration arithmetic instead of Float seconds:**

```ruby
t0 = Chrono::Steady.duration
do_work
t1 = Chrono::Steady.duration
(t1 - t0).as(:microseconds)   # elapsed time in µs, no Float-rounding intermediate
```

## Backward compatibility

The clocks (`Chrono::Steady.now`, `Chrono::System.now`, `Chrono.steady`,
`Chrono.system`) and `Chrono::Timer#elapsed` / `#reset` have been in
this gem for 10+ years and are preserved verbatim. The `.duration`
methods on Steady and Timer, the `Chrono::Duration` class, the
`Comparable` mixin on Duration, and the `mrb_chrono::from` /
`mrb_chrono::as` C++ extension surface are additive — nothing existing
breaks.

## Dependencies

- [mruby-c-ext-helpers](https://github.com/Hendrik-2/mruby-c-ext-helpers)
  — for `mrb_convert_number<T>` (Numeric boxing on `dur.as`) and
  `MRB_CPP_DEFINE_TYPE` (the data-type bindings for Duration and
  Timer).

## License

Apache-2.0.
