MRuby::Gem::Specification.new('mruby-chrono') do |spec|
  spec.license = 'Apache-2'
  spec.author  = 'Hendrik Beskow'
  spec.summary = 'Duration values for mruby ↔ C/C++ time-API interop'
  spec.add_dependency 'mruby-c-ext-helpers'
  spec.add_test_dependency 'mruby-rational'

  def detect_cxx_std(spec)
    require 'tempfile'
    # Which spelling, decided by the compiler and not by the platform.
    # spec.for_windows? is true for a MinGW cross build as well, and
    # MinGW is gcc: it wants the -std= form and reads /std:c++17 as a
    # file name. Asking the wrong question here is worse than a wrong
    # flag, because then every probe below fails and the fallback is
    # wrong as well.
    is_msvc = spec.build.toolchains.include?('visualcpp') ||
              spec.cxx.command.to_s =~ %r{(^|[\\/])cl(\.exe)?$}i
    candidates = is_msvc \
      ? %w[/std:c++latest /std:c++20 /std:c++17] \
      : %w[-std=c++26 -std=c++23 -std=c++20 -std=c++17]

    src = Tempfile.new(['probe', '.cpp'])
    src.write("int main(){}\n"); src.close
    obj = "#{src.path}.o"
    begin
      candidates.each do |flag|
        cmd = is_msvc \
          ? "#{spec.cxx.command} #{flag} /c #{src.path} /Fo#{obj}" \
          : "#{spec.cxx.command} #{flag} -c #{src.path} -o #{obj}"
        return flag if system(cmd, out: File::NULL, err: File::NULL)
      end
      is_msvc ? '/std:c++17' : '-std=c++17'
    ensure
      src.unlink
      File.unlink(obj) if File.exist?(obj)
    end
  end

  spec.cxx.flags << detect_cxx_std(spec)
end