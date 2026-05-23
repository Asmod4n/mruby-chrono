MRuby::Gem::Specification.new('mruby-chrono') do |spec|
  spec.license = 'Apache-2'
  spec.author  = 'Hendrik Beskow'
  spec.summary = 'Duration values for mruby ↔ C/C++ time-API interop'
  spec.add_dependency 'mruby-c-ext-helpers'

  def detect_cxx_std(spec)
    require 'tempfile'
    candidates = spec.for_windows? \
      ? %w[/std:c++latest /std:c++20 /std:c++17] \
      : %w[-std=c++26 -std=c++23 -std=c++20 -std=c++17]

    src = Tempfile.new(['probe', '.cpp'])
    src.write("int main(){}\n"); src.close
    obj = "#{src.path}.o"
    begin
      candidates.each do |flag|
        cmd = spec.for_windows? \
          ? "#{spec.cxx.command} #{flag} /c #{src.path} /Fo#{obj}" \
          : "#{spec.cxx.command} #{flag} -c #{src.path} -o #{obj}"
        return flag if system(cmd, out: File::NULL, err: File::NULL)
      end
      spec.for_windows? ? '/std:c++17' : '-std=c++17'
    ensure
      src.unlink
      File.unlink(obj) if File.exist?(obj)
    end
  end

  spec.cxx.flags << detect_cxx_std(spec)
end