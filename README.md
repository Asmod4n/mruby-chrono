# mruby-chrono

Adds System and Steady clocks to mruby akin to std::chrono from c++11

Examples
========
```ruby
Chrono::Steady.now or Chrono.steady # returns a monotonic increasing timestamp, usefull for Benchmarking
```

```ruby
Chrono::System.now or Chrono.system # returns the current System Time
```

The return values are Ruby Floats.

You need a c++11 compatible c++ compiler.
