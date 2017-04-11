// Copyright © 2013-2017, Prosoft Engineering, Inc. (A.K.A "Prosoft")
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

#ifndef PS_CORE_CONFIG_CPP_EXCEPT_H
#define PS_CORE_CONFIG_CPP_EXCEPT_H

#ifndef PS_CORE_CONFIG_CPP_H
#error "missing config_cpp"
#endif

#define PS_STRINGIFY__(L) #L
#define PS_STRINGIFY_(L) PS_STRINGIFY__(L)

#define PS_LINE_STR PS_STRINGIFY_(__LINE__)

#if defined(__cplusplus)

#include <iostream>
#include <stdexcept>

// Function/File names are not included in exceptions as they can bloat the binary and expose private info.

namespace prosoft {
inline void log_exception(int line) {
    std::cout << "Unknown exception @ " << line << std::endl;
}
inline void log_exception(const std::exception& ex, int line) {
    std::cout << "Exception @ " << line << ": " << ex.what() << std::endl;
}
} // prosoft
#define PSLogCppException(EX) prosoft::log_exception(EX, __LINE__)

#define PSIgnoreCppException(code)               \
    do {                                         \
        try {                                    \
            code;                                \
        } catch (std::exception & e) {           \
            prosoft::log_exception(e, __LINE__); \
        } catch (...) {                          \
            prosoft::log_exception(__LINE__);    \
        }                                        \
    } while (0)

#define PSSilenceCppException(code) \
    do {                            \
        try {                       \
            code;                   \
        } catch (...) {             \
        }                           \
    } while (0)

#define PS_THROW_IF(cond__, ex__)      \
    do {                               \
        if (PS_UNEXPECTED((cond__))) { \
            throw ex__;                \
        }                              \
    } while (0)

#define PS_THROW_IF_NULLPTR(p_) PS_THROW_IF(nullptr == p_, std::runtime_error("NULL @ " PS_LINE_STR "!"))

#endif // __cplusplus

#endif // PS_CORE_CONFIG_CPP_EXCEPT_H
