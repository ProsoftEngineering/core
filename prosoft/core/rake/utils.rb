# Copyright © 2015, Prosoft Engineering, Inc. (A.K.A "Prosoft")
# All rights reserved.
# 
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions are met:
#     * Redistributions of source code must retain the above copyright
#       notice, this list of conditions and the following disclaimer.
#     * Redistributions in binary form must reproduce the above copyright
#       notice, this list of conditions and the following disclaimer in the
#       documentation and/or other materials provided with the distribution.
#     * Neither the name of Prosoft nor the names of its contributors may be
#       used to endorse or promote products derived from this software without
#       specific prior written permission.
# 
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
# ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
# WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
# DISCLAIMED. IN NO EVENT SHALL PROSOFT ENGINEERING, INC. BE LIABLE FOR ANY
# DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
# (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
# LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
# ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
# SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

# Cross-platform way of finding an executable in the $PATH.
# which('ruby') #=> /usr/bin/ruby
# http://stackoverflow.com/a/5471032/412179
def which(cmd)
  exts = ENV['PATHEXT'] ? ENV['PATHEXT'].split(';') : ['']
  ENV['PATH'].split(File::PATH_SEPARATOR).each do |path|
    exts.each do |ext|
      exe = File.join(path, "#{cmd}#{ext}")
      return exe if File.executable?(exe) && !File.directory?(exe)
    end
  end
  return nil
end

HAVE_VS2015 = File.exist? 'C:/Program Files (x86)/Microsoft Visual Studio 14.0/VC/vcvarsall.bat'
if HAVE_VS2015
  UNAME = 'MSVC'
elsif which('uname')
  UNAME = `uname -s`.strip
else
  abort "Unknown platform!"
end

if which('gmake')
  MAKE = 'gmake'
elsif which('make')
  MAKE = 'make'
else
  MAKE = nil
end

ROOT_DIR = Dir.getwd
DEBUG_CONFIG = 'Debug'
RELEASE_CONFIG = 'RelWithDebInfo'

if which('xcrun')
  CMAKE_DEFAULT_GENERATOR = 'Xcode'
elsif which('ninja')
  CMAKE_DEFAULT_GENERATOR = 'Ninja'
elsif HAVE_VS2015
  CMAKE_DEFAULT_GENERATOR = 'Visual Studio 14 Win64'
else
  CMAKE_DEFAULT_GENERATOR = 'Unix Makefiles'
end

CMAKE_GENERATORS = [CMAKE_DEFAULT_GENERATOR]
if HAVE_VS2015
  CMAKE_GENERATORS << 'Visual Studio 14 Win64'
  CMAKE_GENERATORS << 'Visual Studio 14'
end
CMAKE_GENERATORS.uniq!

def build_dir(config, generator)
  generator = generator.tr(' ', '')
  return File.join(ROOT_DIR, "build_#{UNAME}_#{generator}_#{config}")
end

def cmake_generate(builddir, config, srcdir, generator=CMAKE_DEFAULT_GENERATOR)
  FileUtils.mkdir_p builddir
  Dir.chdir builddir do
    sh 'cmake', '-G', generator, '-DCMAKE_BUILD_TYPE=' + config, srcdir
  end
end

def cmake_build(builddir, config, target=nil)
  if target
    sh 'cmake', '--build', builddir, '--config', config, '--target', target
  else
    sh 'cmake', '--build', builddir, '--config', config
  end
end

def ctest(builddir, config)
  Dir.chdir builddir do
    sh 'ctest', '--build-config', config, '--output-on-failure'
  end
end

def make(dir, target)
  sh MAKE, '-C', dir, target
end

def open_ide(builddir)
  xcodeproj = Dir.glob(File.join(builddir, '*.xcodeproj')).last
  sln = Dir.glob(File.join(builddir, '*.sln')).last
  if xcodeproj
    sh 'open', xcodeproj
  elsif sln
    sh 'start', sln
  else
    abort "No known project file found!"
  end
end
