require 'rbconfig'
require 'mkmf'

MRuby::Gem::Specification.new('mruby-chrono') do |spec|
  spec.license = 'Apache-2'
  spec.author  = 'Hendrik Beskow'
  spec.summary = 'A Time library for mruby'

  if RbConfig::CONFIG['target_os'] == 'linux'
    #unless have_library("c", "clock_gettime")
      spec.linker.libraries << 'rt'
    #end
  end
end
