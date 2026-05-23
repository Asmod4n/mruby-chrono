MRuby::Build.new do |conf|
  conf.toolchain
  conf.enable_debug
  conf.enable_test
  conf.gembox 'full-core'
  conf.gem File.expand_path(File.dirname(__FILE__))
end
