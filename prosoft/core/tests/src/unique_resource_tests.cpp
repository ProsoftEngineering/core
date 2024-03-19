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

#include <prosoft/core/config/config_platform.h>

#include <string>

#include <unique_resource.hpp>

#include <catch2/catch_test_macros.hpp>

using namespace prosoft;

TEST_CASE("unique_resource") {
    SECTION("malloc") {
        auto mp = make_malloc<unsigned char>(1024);
        CHECK(mp);
    
        std::memset(mp.get(), 0xff, 1024);
    
        make_malloc(mp, 2048);
        CHECK(mp);
        auto p = mp.get();
        CHECK(p[0] == 0xff);
        CHECK(p[1023] == 0xff);
    
        mp = make_malloc<unsigned char>(1024, 1);
        CHECK(mp);
        p = mp.get();
        CHECK(p[0] == 0);
        CHECK(p[1023] == 0);
    
        mp = init_malloc<unsigned char>(3);
        CHECK(mp);
        p = mp.get();
        CHECK(p[0] == 0);
        CHECK(p[1] == 0);
        CHECK(p[2] == 0);
    
        mp = init_malloc<unsigned char>(3, 0x55);
        CHECK(mp);
        p = mp.get();
        CHECK(p[0] == 0x55);
        CHECK(p[1] == 0x55);
        CHECK(p[2] == 0x55);
    
        auto mpi = make_malloc<int>(count_t{10});
        CHECK(mpi);
        // Just make sure the correct amount of mem is allocated.
        (void)mpi.get()[0];
        (void)mpi.get()[9];
    }

#if __APPLE__
    WHEN("Retaining a unique CF pointer") {
        auto p = ::CFStringCreateWithCString(kCFAllocatorDefault, "this is a long enough test string to bypass tagged pointers", kCFStringEncodingUTF8);
        REQUIRE(::CFGetRetainCount(p) == 1);
        {
            ::CFRetain(p);
            REQUIRE(::CFGetRetainCount(p) == 2);
            unique_cftype<CFStringRef>{ p };
        }
        THEN("the retain count is reduced when the unique pointer is destroyed") {
            REQUIRE(::CFGetRetainCount(p) == 1);
            ::CFRelease(p);
        }
    }
#endif

    WHEN("Creating a unique pointer via a handle") {
        auto create = [](std::string** s) {
            REQUIRE(s != nullptr);
            *s = new std::string{"test"};
        };
        
        std::unique_ptr<std::string> p;
        create(handle(p));
        THEN("the pointer is not null and is valid") {
            REQUIRE(p.get() != nullptr);
            CHECK(*p == "test");
        }
    }
    
#if _WIN32
    WHEN("Creating a unique global pointer") {
        windows::unique_global<void> ug{ ::GlobalAlloc(0, 1) };
        THEN("the pointer is valid") {
            CHECK(ug.get() != nullptr);
        }
    }

    WHEN("Creating a unique local pointer") {
        windows::unique_local<void> ul{ ::LocalAlloc(0, 1) };
        THEN("the pointer is valid") {
            CHECK(ul.get() != nullptr);
        }
    }

    WHEN("Creating a Handle") {
        windows::Handle h{ ::CreateEvent(nullptr, false, true, nullptr) };
        THEN("the handle is valid") {
            CHECK(h);
        } AND_WHEN("resetting the handle, it is invalid") {
            h.reset();
            CHECK(!h);
        }
    }
    
    WHEN("Swapping two Handles") {
        windows::Handle h{ INVALID_HANDLE_VALUE };
        windows::Handle h2;
        h.swap(h2);
        THEN("the handle values are swapped") {
            CHECK(h.get() == nullptr);
            CHECK(h2.get() == INVALID_HANDLE_VALUE);
        }
    }

    WHEN("Creating a Handle with an invalid value") {
        windows::Handle h{ INVALID_HANDLE_VALUE };
        THEN("the handle is invalid") {
            CHECK(INVALID_HANDLE_VALUE == h.get());
            CHECK(!h);
        } AND_WHEN("assigning a new handle, the old invalid handle value is ignored") {
            h = nullptr;
            CHECK(!h);
        }
    }

    WHEN("Creating a windows::Handle via handle") {
        auto create = [](HANDLE* h) {
            *h = INVALID_HANDLE_VALUE;
        };

        windows::Handle h;
        CHECK(h.get() == nullptr);
        create(handle(h));
        THEN("the handle is the expected value") {
            CHECK(INVALID_HANDLE_VALUE == h.get());
        }
    }
#endif // _WIN32
}
