[![Build Status](https://travis-ci.org/Asmod4n/mruby-chrono.svg?branch=master)](https://travis-ci.org/Asmod4n/mruby-chrono)
# mruby-chrono

Adds System and Steady clocks to mruby akin to std::chrono from c++11

Examples
========
```ruby
Chrono::Steady.now # returns a monotonic increasing timestamp, usefull for Benchmarking
```

```ruby
Chrono::System.now # returns the current System Time
```
