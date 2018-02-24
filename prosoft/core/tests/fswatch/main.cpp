// Copyright © 2017, Prosoft Engineering, Inc. (A.K.A "Prosoft")
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are met:
//     * Redistributions of source code must retain the above copyright
//       notice, this list of conditions and the following disclaimer.
//     * Redistributions in binary form must reproduce the above copyright
//       notice, this list of conditions and the following disclaimer in the
//       documentation and/or other materials provided with the distribution.
//     * Neither the name of Prosoft nor the names of its contributors may be
//       used to endorse or promote products derived from this software without
//       specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
// ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
// WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
// DISCLAIMED. IN NO EVENT SHALL PROSOFT ENGINEERING, INC. BE LIABLE FOR ANY
// DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
// (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
// LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
// ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
// SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

#include <thread>

#include <filesystem.hpp>

int main(int argc, char* argv[]) {
    #if PS_HAVE_FILESYSTEM_CHANGE_ITERATOR
        if (argc < 2) {
            return EINVAL;
        }
    
        using namespace prosoft::filesystem;
        std::vector<path> paths;
    
        std::transform(&argv[1], &argv[argc], std::back_inserter(paths), [](auto ap){
            return path{ap};
        });
    
    
        error_code ec;
        if (!is_directory(paths[0], ec)) {
            return EINVAL;
        }
    
        auto i = changed_directory_iterator{paths[0]};
        for (auto& e : i) {
            if (e.empty()) {
                std::this_thread::sleep_for(std::chrono::seconds{1});
                continue;
            }
            
            std::cout << e.path() << "\n";
        }
    
        return 0;
    #else
        return ENOTSUP;
    #endif
}
