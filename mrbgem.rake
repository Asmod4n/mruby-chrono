require 'rbconfig'
require 'mkmf'

MRuby::Gem::Specification.new('mruby-chrono') do |spec|
  spec.license = 'Apache-2'
  spec.author  = 'Hendrik Beskow'
  spec.summary = 'A Time library for mruby'

  if spec.cc.search_header_path 'time.h'
    spec.cc.defines << 'HAVE_TIME_H'
  end

  if build.kind_of?(MRuby::CrossBuild) && %w(x86_64-pc-linux-gnu i686-pc-linux-gnu).include?(build.host_target)
    spec.linker.libraries << 'rt'
  elsif RbConfig::CONFIG['target_os'] == 'linux'
    unless have_library("c", "clock_gettime")
      spec.linker.libraries << 'rt'
    end
  end
end
