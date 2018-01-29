// Copyright © 2014-2018, Prosoft Engineering, Inc. (A.K.A "Prosoft")
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

#include <thread>

#include <semaphore.hpp>

#include "catch.hpp"

using namespace prosoft;

TEST_CASE("semaphore internals") {
    using namespace prosoft::isemaphore;
    
    semaphore_value::value_type i;
    
    // Catch may take the address of values in its sub-expressions.
    auto sem_signal = semaphore_value::signal_value;
    auto event_signal = event_semaphore_value::signal_value;
    auto binary_signal = binary_semaphore_value::signal_value;
    
    SECTION("semaphore") {
        semaphore_value v{semaphore_value::default_value};
        i = --v;
        REQUIRE(i < sem_signal);
        i = v++;
        REQUIRE(i < sem_signal);
        REQUIRE(v >= sem_signal);
        
        CHECK_THROWS(semaphore_value{-1});
    }
    
    SECTION("semaphore signal-before-wait") {
        semaphore_value v{semaphore_value::default_value};
        i = v++;
        REQUIRE(i == sem_signal);
        REQUIRE(v >= sem_signal);
        i = --v;
        REQUIRE(i >= sem_signal);
    }
    
    SECTION("event") {
        event_semaphore_value ev{3};
        i = --ev;
        REQUIRE(i < event_signal);
        REQUIRE(ev < event_signal);
        i = ev++;
        REQUIRE(i >= event_signal);
        REQUIRE(ev < event_signal);
        i = ev++;
        REQUIRE(i >= event_signal);
        REQUIRE(ev < event_signal);
        i = ev++;
        REQUIRE(i < event_signal);
        REQUIRE(ev >= event_signal);
        
        i = --ev;
        REQUIRE(i >= event_signal);
        REQUIRE(ev >= event_signal);
        ev.reset(3);
        i = --ev;
        REQUIRE(i < event_signal);
        REQUIRE(ev < event_signal);
        
        CHECK_THROWS(ev.reset(-1));
        CHECK_THROWS(event_semaphore_value{-1});
    }
    
    SECTION("event pre-signal") {
        event_semaphore_value ev{3};
        i = ev++;
        REQUIRE(i > event_signal);
        REQUIRE(ev < event_signal);
        i = ev++;
        REQUIRE(i == event_signal);
        REQUIRE(ev < event_signal);
        i = --ev;
        REQUIRE(i < event_signal);
        REQUIRE(ev < event_signal);
        i = ev++;
        REQUIRE(i < event_signal);
        REQUIRE(ev == event_signal);
    }
    
    SECTION("binary") {
        binary_semaphore_value bv{binary_semaphore_value::default_value};
        i = --bv;
        REQUIRE(i < binary_signal);
        i = bv++;
        REQUIRE(i < binary_signal);
        REQUIRE(bv == binary_signal);
        i = --bv;
        REQUIRE(i == binary_signal); // no wait -- a reset could force the wait
        i = bv++;
        REQUIRE(i < binary_signal);
        REQUIRE(bv == binary_signal);
        
        // signal before wait
        i = bv++;
        REQUIRE(i == binary_signal);
        REQUIRE(bv == binary_signal);
        i = --bv;
        REQUIRE(i == binary_signal); // no wait
    }
}

TEST_CASE("semaphore") {
    semaphore s;
    REQUIRE(semaphore::status_type::timeout == s.wait_for(std::chrono::milliseconds(1)));
    s.signal(); // now at 0 again
    
    s.signal();
    REQUIRE(semaphore::status_type::no_timeout == s.wait_for(std::chrono::milliseconds(1)));
    
    PS_CONSTEXPR const int nwaiters = 3;
    std::atomic_int i{nwaiters};
    
    std::thread t{[&]() {
        s.wait();
        --i;
    }};
    t.detach();
    
    t = std::thread{[&]() {
        s.wait();
        --i;
    }};
    t.detach();
    
    t = std::thread{[&]() {
        s.wait();
        --i;
    }};
    t.detach();
    
    t = std::thread{[&]() {
        for (auto n = nwaiters; n > 0; --n) {
            s.signal();
        }
    }};
    t.detach();
    
    int sleeps = 0;
    while (i > 0 && sleeps < 1000) {
        using namespace std::chrono;
        std::this_thread::sleep_for(duration_cast<nanoseconds>(milliseconds{10}));
        ++sleeps;
    }

    CHECK(i == 0);
    
    REQUIRE(semaphore::status_type::timeout == s.wait_for(std::chrono::milliseconds(1)));
}

TEST_CASE("binary_semaphore") {
    binary_semaphore s;
    CHECK(s.count() == 0);
    REQUIRE(semaphore::status_type::timeout == s.wait_for(std::chrono::milliseconds(1)));
    CHECK(s.count() == 0);
    
    s.signal();
    CHECK(s.count() == 1);
    REQUIRE(semaphore::status_type::no_timeout == s.wait_for(std::chrono::milliseconds(1)));
    CHECK(s.count() == 0);
    
    std::atomic_int i{0};
    
    std::thread t{[&]() {
        s.wait();
        ++i;
    }};
    t.detach();
    
    using namespace std::chrono;
    std::this_thread::sleep_for(duration_cast<nanoseconds>(milliseconds{20})); // wait for thread to spawn and block on wait()
    REQUIRE(i == 0);
    s.signal(); // should occur after thread wait()
    std::this_thread::sleep_for(duration_cast<nanoseconds>(milliseconds{30})); // wait for thread to wakeup and increment value
    CHECK(i == 1);
    
    // Channel should be in signaled state, so a wait should be a nop.
    // However if the thread wait() happened AFTER the signal the channel will be in a wait state.
    const auto expected = s.count() == 1 ? semaphore::status_type::no_timeout : semaphore::status_type::timeout;
    REQUIRE(expected == s.wait_for(std::chrono::milliseconds(1)));
    CHECK(s.count() == 0);
    
    s.reset();
    CHECK(s.count() == 0);
    REQUIRE(semaphore::status_type::timeout == s.wait_for(std::chrono::milliseconds(1)));
}

TEST_CASE("event_semaphore") {
    event_semaphore s;
    REQUIRE(semaphore::status_type::timeout == s.wait_for(std::chrono::milliseconds(1)));
    
    s.signal();
    REQUIRE(semaphore::status_type::no_timeout == s.wait_for(std::chrono::milliseconds(1)));
    
    // channel is drained, so more waits are basically nops
    REQUIRE(semaphore::status_type::no_timeout == s.wait_for(std::chrono::milliseconds(1)));
    REQUIRE(semaphore::status_type::no_timeout == s.wait_for(std::chrono::milliseconds(1)));
    
    s.reset(1);
    REQUIRE(semaphore::status_type::timeout == s.wait_for(std::chrono::milliseconds(1)));
    
    s.reset(2);
    
    std::thread t{[&]() {
        s.signal();
    }};
    t.detach();
    
    t = std::thread{[&]() {
        s.signal();
    }};
    t.detach();
    
    REQUIRE(semaphore::status_type::no_timeout == s.wait_for(std::chrono::milliseconds(50)));
}
