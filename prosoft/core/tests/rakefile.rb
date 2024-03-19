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

require_relative '../rake/utils.rb'

task :default => [:all]

task :all => [
  :generate_debug,
  :generate_release,
  :build_debug,
  :build_release,
  :test_debug,
  :test_release,
]

task :debug => [
  :generate_debug,
  :build_debug,
  :test_debug,
]

task :release => [
  :generate_release,
  :build_release,
  :test_release,
]

task :generate_debug do
  CMAKE_CONFIGS.each do |cmakeConfig|
    builddir = build_dir(DEBUG_CONFIG, cmakeConfig)
    conan_args = ['install', '--install-folder', builddir,
                  '--profile:build=default',
                  '--settings', 'build_type=Debug'
    ]
    conan_args += cmakeConfig.conan_install_args
    conan_args += ['--build=missing', ROOT_DIR]
    sh 'conan', *conan_args
    Dir.chdir builddir do
        sh 'cmake', ROOT_DIR, *cmakeConfig.cmake_configure_args,
                    '-DCMAKE_TOOLCHAIN_FILE=' + builddir + '/conan_toolchain.cmake',
                    '-DCMAKE_BUILD_TYPE=' + DEBUG_CONFIG
    end
  end
end

task :generate_release do
  CMAKE_CONFIGS.each do |cmakeConfig|
    builddir = build_dir(RELEASE_CONFIG, cmakeConfig)
    conan_args = ['install', '--install-folder', builddir,
                  '--profile:build=default',
                  '--settings', 'build_type=Release',
                  '--settings', '&:build_type=RelWithDebInfo'   # consumer build_type (RELEASE_CONFIG)
    ]
    conan_args += cmakeConfig.conan_install_args
    conan_args += ['--build=missing', ROOT_DIR]
    sh 'conan', *conan_args
    Dir.chdir builddir do
        sh 'cmake', ROOT_DIR, *cmakeConfig.cmake_configure_args,
                    '-DCMAKE_TOOLCHAIN_FILE=' + builddir + '/conan_toolchain.cmake',
                    '-DCMAKE_BUILD_TYPE=' + RELEASE_CONFIG
    end
  end
end

task :build_debug do
  CMAKE_CONFIGS.each do |cmakeConfig|
    cmake_build build_dir(DEBUG_CONFIG, cmakeConfig), DEBUG_CONFIG
  end
end

task :build_release do
  CMAKE_CONFIGS.each do |cmakeConfig|
    cmake_build build_dir(RELEASE_CONFIG, cmakeConfig), RELEASE_CONFIG
  end
end

task :test_debug do
  CMAKE_CONFIGS.each do |cmakeConfig|
    ctest build_dir(DEBUG_CONFIG, cmakeConfig), DEBUG_CONFIG
  end
end

task :test_release do
  CMAKE_CONFIGS.each do |cmakeConfig|
    ctest build_dir(RELEASE_CONFIG, cmakeConfig), RELEASE_CONFIG
  end
end

task :ide do
  dir = build_dir DEBUG_CONFIG, CMAKE_CONFIGS[0]
  cmake_generate dir, DEBUG_CONFIG, ROOT_DIR, CMAKE_CONFIGS[0]
  open_ide dir
end
task :xcode => [:ide]
task :vs => [:ide]
