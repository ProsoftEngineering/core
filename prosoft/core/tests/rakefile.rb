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

require_relative 'rakefile_utils.rb'

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
  CMAKE_GENERATORS.each do |gen|
    cmake_generate build_dir(DEBUG_CONFIG, gen), DEBUG_CONFIG, ROOT_DIR, gen
  end
end

task :generate_release do
  CMAKE_GENERATORS.each do |gen|
    cmake_generate build_dir(RELEASE_CONFIG, gen), RELEASE_CONFIG, ROOT_DIR, gen
  end
end

task :build_debug do
  CMAKE_GENERATORS.each do |gen|
    cmake_build build_dir(DEBUG_CONFIG, gen), DEBUG_CONFIG
  end
end

task :build_release do
  CMAKE_GENERATORS.each do |gen|
    cmake_build build_dir(RELEASE_CONFIG, gen), RELEASE_CONFIG
  end
end

task :test_debug do
  CMAKE_GENERATORS.each do |gen|
    ctest build_dir(DEBUG_CONFIG, gen), DEBUG_CONFIG
  end
end

task :test_release do
  CMAKE_GENERATORS.each do |gen|
    ctest build_dir(RELEASE_CONFIG, gen), RELEASE_CONFIG
  end
end

task :ide do
  dir = build_dir DEBUG_CONFIG, CMAKE_DEFAULT_GENERATOR
  cmake_generate dir, DEBUG_CONFIG, ROOT_DIR, CMAKE_DEFAULT_GENERATOR
  open_ide dir
end
task :xcode => [:ide]
task :vs => [:ide]
