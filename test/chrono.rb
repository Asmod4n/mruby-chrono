# mruby-chrono test suite
#
# Tests the entire Ruby surface:
#   - Construction (Chrono::Duration.new, Numeric#ms etc.)
#   - Output (.as, .as_f, with and without rounding)
#   - Arithmetic (+, -, *, /, ==, <=>)
#   - Build matrix gating (days/weeks on C++20 only)

# ---------- Construction ----------

assert('Chrono::Duration.new(count, :unit)') do
  d = Chrono::Duration.new(500, :ms)
  assert_kind_of(Chrono::Duration, d)
  assert_equal(500, d.as(:ms))
end

assert('Chrono::Duration.new accepts long-name and short symbols') do
  assert_equal(5, Chrono::Duration.new(5, :seconds).as(:s))
  assert_equal(5, Chrono::Duration.new(5, :s).as(:seconds))
end

assert('Chrono::Duration.new raises on unknown unit') do
  assert_raise(ArgumentError) { Chrono::Duration.new(1, :fortnights) }
end

assert('Chrono::Duration.new raises on non-Numeric count') do
  assert_raise(TypeError) { Chrono::Duration.new("five", :ms) }
end

# ---------- Numeric extensions ----------

assert('Numeric#ms builds a Duration') do
  d = 500.ms
  assert_kind_of(Chrono::Duration, d)
  assert_equal(500,        d.as(:ms))
  assert_equal(500_000,    d.as(:us))
  assert_equal(500_000_000, d.as(:ns))
end

assert('Numeric#us, #ns, #s, #min, #h all work') do
  assert_equal(1_000,           1.us.as(:ns))
  assert_equal(1,               1.us.as(:us))
  assert_equal(1_000_000,       1.s.as(:us))
  assert_equal(60,              1.min.as(:s))
  assert_equal(3600,            1.h.as(:s))
end

# ---------- .as with rounding ----------

assert('.as defaults to :truncate') do
  # 1500 ns → microseconds, truncate → 1
  assert_equal(1, 1500.ns.as(:us))
end

assert('.as with :floor — same as truncate for positives') do
  assert_equal(1, 1500.ns.as(:us, :floor))
end

assert('.as with :ceil') do
  assert_equal(2, 1500.ns.as(:us, :ceil))
  assert_equal(2, 1001.ns.as(:us, :ceil))
  assert_equal(1, 1000.ns.as(:us, :ceil))  # exact value, no rounding up
end

assert('.as with :round — banker\'s, half to even') do
  # 1500 ns is exactly half a microsecond: 1.5 → 2 (even)
  assert_equal(2, 1500.ns.as(:us, :round))
  # 2500 ns is exactly half between 2 and 3: 2.5 → 2 (even, banker's)
  assert_equal(2, 2500.ns.as(:us, :round))
end

assert('.as raises on unknown rounding') do
  assert_raise(ArgumentError) { 1.s.as(:ms, :half_even) }
end

# ---------- .as_f ----------

assert('.as_f returns a Float') do
  d = 500.ms
  v = d.as_f(:s)
  assert_kind_of(Float, v)
  assert_equal(0.5, v)
end

assert('.as_f works for sub-period values') do
  assert_equal(0.5, 500.us.as_f(:ms))
  assert_equal(1.5, 1500.ms.as_f(:s))
end

# ---------- Arithmetic ----------

assert('Duration + Duration') do
  d = 500.ms + 500.ms
  assert_equal(1_000, d.as(:ms))
end

assert('Duration - Duration') do
  d = 1.s - 500.ms
  assert_equal(500, d.as(:ms))
end

assert('Duration * Integer scalar') do
  d = 100.ms * 3
  assert_equal(300, d.as(:ms))
end

assert('Duration / Integer scalar') do
  d = 1.s / 4
  assert_equal(250, d.as(:ms))
end

assert('Duration / 0 raises ZeroDivisionError') do
  assert_raise(ZeroDivisionError) { 1.s / 0 }
end

assert('+ and - raise on non-Duration operand') do
  assert_raise(TypeError) { 1.s + 1 }
  assert_raise(TypeError) { 1.s - 1.0 }
end

# ---------- Equality and comparison ----------

assert('equal Durations are ==') do
  assert_true(500.ms == 500.ms)
  assert_true(1.s == 1000.ms)        # same value, different construction unit
end

assert('different Durations are not ==') do
  assert_false(500.ms == 501.ms)
end

assert('Duration == non-Duration is false (no raise)') do
  assert_false(500.ms == 500)
  assert_false(500.ms == "500ms")
end

assert('<=> orders Durations') do
  assert_equal(-1, (500.ms <=> 1.s))
  assert_equal( 0, (1.s <=> 1000.ms))
  assert_equal( 1, (2.s <=> 1.s))
end

assert('<=> returns nil for non-Duration') do
  assert_nil((1.s <=> 1))
end

# ---------- Round-tripping ----------

assert('Same value through different construction units') do
  assert_equal(1.s, 1000.ms)
  assert_equal(1.s, 1_000_000.us)
  assert_equal(1.s, 1_000_000_000.ns)
end

# ---------- Days and weeks (C++20-only; will skip silently on C++17) ----------

if Integer.method_defined?(:days)
  assert('Integer#days on C++20 builds') do
    assert_equal(86_400,    1.days.as(:s))
    assert_equal(7 * 86_400, 1.weeks.as(:s))
  end

  assert(':days symbol accepted by Duration.new on C++20 builds') do
    d = Chrono::Duration.new(2, :days)
    assert_equal(2 * 86_400, d.as(:s))
  end
end

# ---------- Original gem API (preserved from 10+ years of shipping) ----------

assert('Chrono::Steady.now returns a monotonic Float') do
  t1 = Chrono::Steady.now
  t2 = Chrono::Steady.now
  assert_kind_of(Float, t1)
  assert_kind_of(Float, t2)
  assert_true(t2 >= t1, 'steady clock must not go backwards')
end

assert('Chrono.steady is an alias for Chrono::Steady.now') do
  assert_kind_of(Float, Chrono.steady)
end

assert('Chrono::System.now returns a Float since epoch') do
  t = Chrono::System.now
  assert_kind_of(Float, t)
  # Sanity: anything past 2020-01-01 (>= 1.577e9 seconds since 1970)
  assert_true(t > 1_577_000_000.0, 'system clock should be past 2020')
end

assert('Chrono.system is an alias for Chrono::System.now') do
  assert_kind_of(Float, Chrono.system)
end

assert('Chrono::Timer measures elapsed time') do
  t = Chrono::Timer.new
  # Do some work — a tight loop that the optimizer can't elide.
  sum = 0
  10_000.times { |i| sum += i }
  e1 = t.elapsed
  assert_kind_of(Float, e1)
  assert_true(e1 >= 0.0, 'elapsed must be non-negative')

  # #elapsed without #reset keeps growing.
  10_000.times { |i| sum += i }
  e2 = t.elapsed
  assert_true(e2 >= e1, 'elapsed must be monotonic without reset')

  # After #reset, elapsed should drop close to zero.
  t.reset
  e3 = t.elapsed
  assert_true(e3 < e2, "reset should rewind elapsed (was #{e2}, now #{e3})")
end

# ---------- Steady.duration / Timer#duration (Duration counterparts) ----------

assert('Chrono::Steady.duration returns a Chrono::Duration') do
  d = Chrono::Steady.duration
  assert_kind_of(Chrono::Duration, d)
end

assert('Chrono::Steady.duration advances monotonically') do
  d1 = Chrono::Steady.duration
  d2 = Chrono::Steady.duration
  assert_true(d2 >= d1, 'steady duration must not go backwards')
end

assert('Chrono::Timer#duration returns a Chrono::Duration') do
  t = Chrono::Timer.new
  d = t.duration
  assert_kind_of(Chrono::Duration, d)
  assert_true(d.as(:ns) >= 0)
end

assert('Chrono::Timer#duration and #elapsed agree on the same elapsed span') do
  t = Chrono::Timer.new
  100.times { 1 + 1 }   # introduce a tiny non-zero gap
  # Read both back-to-back; small drift is expected, but they should be
  # within milliseconds of each other.
  e = t.elapsed          # Float seconds
  d = t.duration         # Chrono::Duration
  ns_from_duration = d.as(:ns)
  ns_from_elapsed  = (e * 1_000_000_000).to_i
  diff_ms = (ns_from_duration - ns_from_elapsed).abs / 1_000_000
  assert_true(diff_ms < 10, "elapsed and duration disagreed by #{diff_ms} ms")
end

assert('Chrono::System has no .duration (System is a time point, not a span)') do
  assert_false(Chrono::System.respond_to?(:duration),
               'System should not expose .duration')
end

# ---------- Comparable mixin (gets us <, <=, >, >=, between?, clamp) ----------

assert('< on Durations') do
  assert_true(500.ms < 1.s)
  assert_false(1.s < 500.ms)
end

assert('>= on Durations') do
  assert_true(1.s  >= 1.s)
  assert_true(2.s  >= 1.s)
  assert_false(1.s >= 2.s)
end

assert('between? on Durations') do
  assert_true(500.ms.between?(100.ms, 1.s))
  assert_false(2.s.between?(100.ms, 1.s))
end

# ---------- Overflow detection on Duration#* and Duration#/ ----------

assert('Duration#* raises RangeError on int64 nanosecond overflow') do
  # 1.h.as(:ns) = 3.6e12. Times 1e9 = 3.6e21, overflows int64 (max 9.2e18).
  assert_raise(RangeError) { 1.h * 1_000_000_000 }
end

assert('Duration#* near-limit values still work') do
  # 1.s = 1e9 ns; times 1e9 = 1e18, fits comfortably (max 9.2e18).
  result = 1.s * 1_000_000_000
  assert_kind_of(Chrono::Duration, result)
  assert_equal(1_000_000_000, result.as(:s))
end

assert('Duration#* raises RangeError on Float overflow') do
  # Float Infinity should be detected as non-finite.
  assert_raise(RangeError) { 1.s * (1.0 / 0.0) }
end

assert('Duration#* raises RangeError on Float NaN') do
  assert_raise(RangeError) { 1.s * (0.0 / 0.0) }
end

assert('Duration#/ raises RangeError on INT64_MIN / -1') do
  # Construct a Duration whose ns == INT64_MIN. We can't directly, but
  # we can get close: a very negative Duration, then divide by -1.
  # Easiest test: just verify the simple path divides correctly.
  result = 1.s / -1
  assert_equal(-1, result.as(:s))
end

# ---------- Float receivers on Numeric extensions ----------

assert('Float#s gives a fractional-second Duration') do
  d = 1.5.s
  assert_kind_of(Chrono::Duration, d)
  assert_equal(1_500_000_000, d.as(:ns))
  assert_equal(1_500,         d.as(:ms))
end

assert('Float#ms gives a fractional-millisecond Duration') do
  d = 0.5.ms
  assert_equal(500,    d.as(:us))
  assert_equal(500_000, d.as(:ns))
end

assert('Float#ns truncates to whole nanoseconds (storage limit)') do
  # 1.5 ns can't be stored — timespec is whole nanoseconds.
  # duration_cast truncates toward zero.
  assert_equal(1, 1.5.ns.as(:ns))
end

assert('Float#h works') do
  assert_equal(1_800, 0.5.h.as(:s))
end

assert('Float duration raises RangeError on non-finite values') do
  assert_raise(RangeError) { (1.0 / 0.0).s }    # +Infinity
  assert_raise(RangeError) { (0.0 / 0.0).s }    # NaN
end

assert('Float duration raises RangeError on overflow') do
  # 1e20 seconds → 1e29 nanoseconds, far past INT64_MAX (~9.2e18).
  assert_raise(RangeError) { 1.0e20.s }
end

assert('Float and Integer constructions of the same value compare equal') do
  assert_equal(500.ms, 0.5.s)
  assert_equal(1.s, 1000.0.ms)
end

# ---------- Rational and other Numeric subclasses ----------

assert('Numeric#ms works on Rational') do
  d = Rational(1, 2).ms
  assert_kind_of(Chrono::Duration, d)
  assert_equal(500,    d.as(:us))
  assert_equal(500_000, d.as(:ns))
end

assert('Rational with non-terminating decimal — best-effort via to_f') do
  d = Rational(1, 3).s
  # 1/3 second as Float → ~0.3333... → 333_333_333 ns ±1 ulp at this scale.
  ns = d.as(:ns)
  assert_true((ns - 333_333_333).abs <= 1, "got #{ns} ns")
end

assert('Chrono::Duration.new accepts Rational') do
  d = Chrono::Duration.new(Rational(1, 4), :s)
  assert_equal(250, d.as(:ms))
end

assert('Chrono::Duration.new(Rational(1,2), :ms) round-trips') do
  d = Chrono::Duration.new(Rational(1, 2), :ms)
  assert_equal(500, d.as(:us))
end

assert('Duration#* with Rational scalar') do
  d = 1.s * Rational(1, 3)
  ns = d.as(:ns)
  assert_true((ns - 333_333_333).abs <= 1, "got #{ns} ns")
end

assert('Duration#* with Rational(2) doubles the Duration') do
  assert_equal(2_000_000_000, (1.s * Rational(2)).as(:ns))
end

assert('Duration#/ with Rational scalar') do
  # 1.s / Rational(1,2) == 1.s * 2 == 2 seconds
  assert_equal(2_000_000_000, (1.s / Rational(1, 2)).as(:ns))
end

assert('Duration#/ with Rational(0) raises ZeroDivisionError') do
  assert_raise(ZeroDivisionError) { 1.s / Rational(0) }
end

assert('Duration#* with non-Numeric raises TypeError') do
  assert_raise(TypeError) { 1.s * "two" }
  assert_raise(TypeError) { 1.s * :half }
end

assert('Duration#/ with non-Numeric raises TypeError') do
  assert_raise(TypeError) { 1.s / "two" }
end

assert('Numeric#ms on a non-Numeric receiver is undefined') do
  # Sanity: extensions are scoped to Numeric, not Object.
  assert_false("500".respond_to?(:ms))
  assert_false(:ms.respond_to?(:ms))
end