# Copyright © 2015-2021, Prosoft Engineering, Inc. (A.K.A "Prosoft")
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

# VS2017 breaks with previous install orginization leading to distinct install paths for each edition (Enterprise, Pro, etc).
HAVE_VS2022 = File.exist? 'C:/Program Files/Microsoft Visual Studio/2022'
HAVE_VS2019 = File.exist? 'C:/Program Files (x86)/Microsoft Visual Studio/2019'
HAVE_VS2017 = File.exist? 'C:/Program Files (x86)/Microsoft Visual Studio/2017' # /Professional/VC/Auxiliary/Build/vcvarsall.bat
HAVE_VS2015 = File.exist? 'C:/Program Files (x86)/Microsoft Visual Studio 14.0/VC/vcvarsall.bat'
if HAVE_VS2022 or HAVE_VS2019 or HAVE_VS2017 or HAVE_VS2015
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

class CMakeConfig
  attr_reader :id, :cmake_configure_args, :conan_install_args

  def initialize(id: '', cmake_configure_args: [], conan_install_args: [])
    @id = id                                        # "VS2019x86"
    @cmake_configure_args = cmake_configure_args    # '-G',  'Visual Studio 16 2019', '-A', 'Win32'
    @conan_install_args = conan_install_args        # '-s', 'arch=x86'
  end
end

CMAKE_CONFIGS = []
 
if which('xcrun')
  CMAKE_CONFIGS.push(CMakeConfig.new(:id => 'Xcode',
                                     :cmake_configure_args => ['-G', 'Xcode']))
elsif HAVE_VS2022
  CMAKE_CONFIGS.push(CMakeConfig.new(:id => 'VS2022',
                                     :cmake_configure_args => ['-G', 'Visual Studio 17 2022']))
  CMAKE_CONFIGS.push(CMakeConfig.new(:id => 'VS2022x86',
                                     :cmake_configure_args => ['-G', 'Visual Studio 17 2022', '-A', 'Win32'],
                                     :conan_install_args => ['-s', 'arch=x86']))
elsif HAVE_VS2019
  CMAKE_CONFIGS.push(CMakeConfig.new(:id => 'VS2019',
                                     :cmake_configure_args => ['-G', 'Visual Studio 16 2019']))
  CMAKE_CONFIGS.push(CMakeConfig.new(:id => 'VS2019x86',
                                     :cmake_configure_args => ['-G', 'Visual Studio 16 2019', '-A', 'Win32'],
                                     :conan_install_args => ['-s', 'arch=x86']))
elsif HAVE_VS2017
  CMAKE_CONFIGS.push(CMakeConfig.new(:id => 'VS2017',
                                     :cmake_configure_args => ['-G', 'Visual Studio 15 2017 Win64']))
  CMAKE_CONFIGS.push(CMakeConfig.new(:id => 'VS2017x86',
                                     :cmake_configure_args => ['-G', 'Visual Studio 15 2017'],
                                     :conan_install_args => ['-s', 'arch=x86']))
elsif HAVE_VS2015
  CMAKE_CONFIGS.push(CMakeConfig.new(:id => 'VS2015',
                                     :cmake_configure_args => ['-G', 'Visual Studio 14 2015 Win64']))
  CMAKE_CONFIGS.push(CMakeConfig.new(:id => 'VS2015x86',
                                     :cmake_configure_args => ['-G', 'Visual Studio 14 2015'],
                                     :conan_install_args => ['-s', 'arch=x86']))
elsif which('ninja')
  CMAKE_CONFIGS.push(CMakeConfig.new(:id => 'Ninja',
                                     :cmake_configure_args => ['-G', 'Ninja']))
else
  CMAKE_CONFIGS.push(CMakeConfig.new(:id => 'Makefiles',
                                     :cmake_configure_args => ['-G', 'Unix Makefiles']))
end

def build_dir(config, cmakeConfig)
  return File.join(ROOT_DIR, "build_#{cmakeConfig.id}_#{config}")
end

def cmake_generate(builddir, config, srcdir, cmakeConfig=CMAKE_CONFIGS[0])
  FileUtils.mkdir_p builddir
  Dir.chdir builddir do
    sh 'cmake', *cmakeConfig.cmake_configure_args, '-DCMAKE_BUILD_TYPE=' + config, srcdir
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
