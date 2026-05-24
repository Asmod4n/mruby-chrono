# mruby-chrono test suite
#
# Duration values are plain Ruby Floats (seconds).
# Numeric unit methods are sugar for Float arithmetic.

# ------------------------------------------------------------------ #
#  Numeric unit methods                                               #
# ------------------------------------------------------------------ #

assert('500.ms == 0.5') do
  assert_equal(0.5, 500.ms)
  assert_kind_of(Float, 500.ms)
end

assert('1.s == 1.0') do
  assert_equal(1.0, 1.s)
end

assert('1.us == 1e-6') do
  assert_float(1e-6, 1.us)
end

assert('1.ns == 1e-9') do
  assert_float(1e-9, 1.ns)
end

assert('1.min == 60.0') do
  assert_equal(60.0, 1.min)
end

assert('1.h == 3600.0') do
  assert_equal(3600.0, 1.h)
end

assert('long-name aliases work') do
  assert_equal(1.ms,  1.milliseconds)
  assert_equal(1.us,  1.microseconds)
  assert_equal(1.ns,  1.nanoseconds)
  assert_equal(1.s,   1.seconds)
  assert_equal(1.min, 1.minutes)
  assert_equal(1.h,   1.hours)
end

assert('Float receiver works: 1.5.s == 1.5') do
  assert_float(1.5, 1.5.s)
  assert_float(0.5e-3, 0.5.ms)
end

assert('Rational receiver: Rational(1,2).s == 0.5') do
  assert_float(0.5, Rational(1, 2).s)
end

assert('non-finite raises RangeError') do
  assert_raise(RangeError) { (1.0/0.0).s  }
  assert_raise(RangeError) { (0.0/0.0).ms }
end

assert('extensions are on Numeric only') do
  assert_false("500".respond_to?(:ms))
end

# ------------------------------------------------------------------ #
#  Units are just Float arithmetic — compose freely                   #
# ------------------------------------------------------------------ #

assert('500.ms + 500.ms == 1.0') do
  assert_float(1.0, 500.ms + 500.ms)
end

assert('1.min - 30.s == 30.0') do
  assert_float(30.0, 1.min - 30.s)
end

assert('0.5.h * 2 == 3600.0') do
  assert_equal(3600.0, 0.5.h * 2)
end

assert('1.h / 2 == 1800.0') do
  assert_equal(1800.0, 1.h / 2)
end

# ------------------------------------------------------------------ #
#  Days and weeks (C++20 only)                                        #
# ------------------------------------------------------------------ #

if Integer.method_defined?(:days)
  assert('1.days == 86400.0') do
    assert_equal(86400.0, 1.days)
  end

  assert('1.weeks == 604800.0') do
    assert_equal(604800.0, 1.weeks)
  end
end

# ------------------------------------------------------------------ #
#  Clocks                                                             #
# ------------------------------------------------------------------ #

assert('Chrono::Steady.now returns a monotonic Float') do
  t1 = Chrono::Steady.now
  t2 = Chrono::Steady.now
  assert_kind_of(Float, t1)
  assert_true(t2 >= t1, 'steady clock must not go backwards')
end

assert('Chrono.steady is an alias for Chrono::Steady.now') do
  assert_kind_of(Float, Chrono.steady)
end

assert('Chrono::System.now returns a Float past 2020') do
  t = Chrono::System.now
  assert_kind_of(Float, t)
  assert_true(t > 1_577_000_000.0, "system clock should be past 2020 (got #{t})")
end

assert('Chrono.system is an alias for Chrono::System.now') do
  assert_kind_of(Float, Chrono.system)
end

# ------------------------------------------------------------------ #
#  Timer                                                              #
# ------------------------------------------------------------------ #

assert('Chrono::Timer#elapsed returns a non-negative Float') do
  t = Chrono::Timer.new
  sum = 0
  10_000.times { |i| sum += i }
  e = t.elapsed
  assert_kind_of(Float, e)
  assert_true(e >= 0.0, "elapsed should be non-negative (got #{e})")
end

assert('Chrono::Timer#elapsed grows monotonically') do
  t = Chrono::Timer.new
  sum = 0
  10_000.times { |i| sum += i }
  e1 = t.elapsed
  10_000.times { |i| sum += i }
  e2 = t.elapsed
  assert_true(e2 >= e1, "elapsed should grow (#{e1} -> #{e2})")
end

assert('Chrono::Timer#reset restarts the clock') do
  t = Chrono::Timer.new
  sum = 0
  10_000.times { |i| sum += i }
  e1 = t.elapsed
  t.reset
  e2 = t.elapsed
  assert_true(e2 < e1, "after reset elapsed should drop (was #{e1}, now #{e2})")
end

assert('Chrono::Timer has no #duration method') do
  assert_false(Chrono::Timer.new.respond_to?(:duration))
end

# ------------------------------------------------------------------ #
#  No Chrono::Duration class                                          #
# ------------------------------------------------------------------ #

assert('Chrono::Duration does not exist') do
  assert_false(Chrono.const_defined?(:Duration))
end

assert('Chrono::Steady has no .duration') do
  assert_false(Chrono::Steady.respond_to?(:duration))
end

# ------------------------------------------------------------------ #
#  C API — mrb_chrono_from                                            #
# ------------------------------------------------------------------ #

assert('ChronoCTest.from: milliseconds -> seconds') do
  v = ChronoCTest.from(500, ChronoCTest::DUR_MILLISECONDS)
  assert_float(0.5, v)
  assert_kind_of(Float, v)
end

assert('ChronoCTest.from: microseconds -> seconds') do
  assert_float(1e-6, ChronoCTest.from(1, ChronoCTest::DUR_MICROSECONDS))
end

assert('ChronoCTest.from: nanoseconds -> seconds') do
  assert_float(1e-9, ChronoCTest.from(1, ChronoCTest::DUR_NANOSECONDS))
end

assert('ChronoCTest.from: seconds -> seconds') do
  assert_equal(1.0, ChronoCTest.from(1, ChronoCTest::DUR_SECONDS))
end

assert('ChronoCTest.from: minutes -> seconds') do
  assert_equal(60.0, ChronoCTest.from(1, ChronoCTest::DUR_MINUTES))
end

assert('ChronoCTest.from: hours -> seconds') do
  assert_equal(3600.0, ChronoCTest.from(1, ChronoCTest::DUR_HOURS))
end

assert('ChronoCTest.from: Float input') do
  assert_float(0.5, ChronoCTest.from(0.5, ChronoCTest::DUR_SECONDS))
end

assert('ChronoCTest.from: non-finite raises RangeError') do
  assert_raise(RangeError) { ChronoCTest.from(Float::INFINITY, ChronoCTest::DUR_SECONDS) }
end

# ------------------------------------------------------------------ #
#  C API — mrb_chrono_convert                                         #
# ------------------------------------------------------------------ #

assert('ChronoCTest.to_int32: 500ms -> 500 milliseconds') do
  assert_equal(500, ChronoCTest.to_int32(500.ms, ChronoCTest::DUR_MILLISECONDS, ChronoCTest::TRUNC))
end

assert('ChronoCTest.to_int64: 1s -> 1_000_000_000 nanoseconds') do
  assert_equal(1_000_000_000, ChronoCTest.to_int64(1.s, ChronoCTest::DUR_NANOSECONDS, ChronoCTest::TRUNC))
end

assert('ChronoCTest.to_long: 500ms -> 500 milliseconds (c-ares pattern)') do
  assert_equal(500, ChronoCTest.to_long(500.ms, ChronoCTest::DUR_MILLISECONDS, ChronoCTest::TRUNC))
end

assert('ChronoCTest.to_double: 1.s -> 1.0 seconds') do
  assert_float(1.0, ChronoCTest.to_double(1.s, ChronoCTest::DUR_SECONDS, ChronoCTest::TRUNC))
end

assert('ChronoCTest.to_double: 1500ms -> 1500.0 milliseconds') do
  assert_float(1500.0, ChronoCTest.to_double(1500.ms, ChronoCTest::DUR_MILLISECONDS, ChronoCTest::TRUNC))
end

assert('ChronoCTest.to_double: TRUNC truncates toward zero') do
  # 1500ms = 1.5 seconds, truncated to integer seconds = 1.0
  assert_float(1.0, ChronoCTest.to_double(1500.ms, ChronoCTest::DUR_SECONDS, ChronoCTest::TRUNC))
end

assert('ChronoCTest.to_int32: rounding CEIL') do
  # 1500ns -> 1.5us -> ceil -> 2
  assert_equal(2, ChronoCTest.to_int32(1500.ns, ChronoCTest::DUR_MICROSECONDS, ChronoCTest::CEIL))
end

assert('ChronoCTest.to_int32: rounding FLOOR') do
  assert_equal(1, ChronoCTest.to_int32(1500.ns, ChronoCTest::DUR_MICROSECONDS, ChronoCTest::FLOOR))
end

assert('ChronoCTest.to_int32: rounding NEAREST banker\'s — 2500ns -> 2us (half-to-even)') do
  assert_equal(2, ChronoCTest.to_int32(2500.ns, ChronoCTest::DUR_MICROSECONDS, ChronoCTest::NEAREST))
end

assert('ChronoCTest.to_timespec: 1500ms -> [1, 500_000_000]') do
  ts = ChronoCTest.to_timespec(1500.ms, ChronoCTest::DUR_NANOSECONDS, ChronoCTest::TRUNC)
  assert_equal(1,           ts[0])   # tv_sec
  assert_equal(500_000_000, ts[1])   # tv_nsec
end

assert('ChronoCTest.to_timeval: 1500ms -> [1, 500_000]') do
  tv = ChronoCTest.to_timeval(1500.ms, ChronoCTest::DUR_MICROSECONDS, ChronoCTest::TRUNC)
  assert_equal(1,       tv[0])   # tv_sec
  assert_equal(500_000, tv[1])   # tv_usec
end

assert('ChronoCTest.to_int32: out of range raises RangeError') do
  assert_raise(RangeError) { ChronoCTest.to_int32(1.0e20.s, ChronoCTest::DUR_SECONDS, ChronoCTest::TRUNC) }
end

# ------------------------------------------------------------------ #
#  C++ API — mrb_chrono::from                                         #
# ------------------------------------------------------------------ #

assert('ChronoCppTest.from_ms: 500 -> 0.5') do
  assert_float(0.5, ChronoCppTest.from_ms(500))
  assert_kind_of(Float, ChronoCppTest.from_ms(500))
end

assert('ChronoCppTest.from_us: 1 -> 1e-6') do
  assert_float(1e-6, ChronoCppTest.from_us(1))
end

assert('ChronoCppTest.from_ns: 1 -> 1e-9') do
  assert_float(1e-9, ChronoCppTest.from_ns(1))
end

assert('ChronoCppTest.from_s: 1 -> 1.0') do
  assert_equal(1.0, ChronoCppTest.from_s(1))
end

assert('ChronoCppTest.from_h: 1 -> 3600.0') do
  assert_equal(3600.0, ChronoCppTest.from_h(1))
end

assert('ChronoCppTest.from_ms: negative value') do
  assert_float(-0.25, ChronoCppTest.from_ms(-250))
end

assert('ChronoCppTest.from_ms: zero') do
  assert_equal(0.0, ChronoCppTest.from_ms(0))
end

assert('ChronoCppTest.steady_now returns positive Float') do
  t = ChronoCppTest.steady_now
  assert_kind_of(Float, t)
  assert_true(t > 0.0)
end

# ------------------------------------------------------------------ #
#  C++ API — mrb_chrono::as                                           #
# ------------------------------------------------------------------ #

assert('ChronoCppTest.as_ms: 0.5 -> 500') do
  assert_equal(500, ChronoCppTest.as_ms(0.5))
end

assert('ChronoCppTest.as_us: 1.0 -> 1_000_000') do
  assert_equal(1_000_000, ChronoCppTest.as_us(1.0))
end

assert('ChronoCppTest.as_ns: 1.0 -> 1_000_000_000') do
  assert_equal(1_000_000_000, ChronoCppTest.as_ns(1.0))
end

assert('ChronoCppTest.as_h: 3600.0 -> 1') do
  assert_equal(1, ChronoCppTest.as_h(3600.0))
end

assert('ChronoCppTest round-trip: from_ms -> as_ms') do
  assert_equal(250, ChronoCppTest.as_ms(ChronoCppTest.from_ms(250)))
end

assert('ChronoCppTest round-trip: from_us -> as_us') do
  assert_equal(1, ChronoCppTest.as_us(ChronoCppTest.from_us(1)))
end

assert('ChronoCppTest: Ruby unit methods feed directly into as_ms') do
  assert_equal(500, ChronoCppTest.as_ms(500.ms))
  assert_equal(1000, ChronoCppTest.as_ms(1.s))
end