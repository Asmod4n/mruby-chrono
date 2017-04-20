require 'rbconfig'
require 'mkmf'

MRuby::Gem::Specification.new('mruby-chrono') do |spec|
  spec.license = 'Apache-2'
  spec.author  = 'Hendrik Beskow'
  spec.summary = 'A Time library for mruby'

  if spec.cc.search_header_path 'time.h'
    spec.cc.defines << 'HAVE_TIME_H'
  end

  if build.kind_of?(MRuby::CrossBuild)
    unless (build.host_target.include?('darwin')||build.host_target.include?('mingw'))
      spec.linker.libraries << 'rt'
    end
  elsif !(RbConfig::CONFIG['target_os'].include?('darwin')||RbConfig::CONFIG['target_os'].include?('mingw'))
    unless have_library("c", "clock_gettime")
      spec.linker.libraries << 'rt'
    end
  end
end
