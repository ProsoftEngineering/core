// Copyright © 2014-2015, Prosoft Engineering, Inc. (A.K.A "Prosoft")
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

#ifndef PS_CORE_SEMAPHORE_HPP
#define PS_CORE_SEMAPHORE_HPP

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <mutex>
#include <stdexcept>
#include <type_traits>

#include <prosoft/core/config/config.h>

namespace prosoft {

namespace isemaphore {

struct semaphore_value {
    typedef s_size_t value_type;
    static PS_CONSTEXPR const value_type signal_value = 0;
    static PS_CONSTEXPR const value_type default_value = 0;

    explicit semaphore_value(value_type v)
        : val(v) {
        if (v < signal_value) {
            throw std::runtime_error("invalid semaphore value");
        }
    }
    
    PS_DISABLE_DEFAULT_CONSTRUCTOR(semaphore_value);
    PS_DISABLE_COPY(semaphore_value);

    value_type operator--() {
        return --val;
    }

    value_type operator++(int) {
        value_type v{val};
        ++val;
        return v;
    }

    operator value_type() {
        return val;
    }

private:
    value_type val;
};

struct event_semaphore_value {
    typedef semaphore_value::value_type value_type;
    static PS_CONSTEXPR const value_type signal_value = 1;
    static PS_CONSTEXPR const value_type default_value = 1;

    explicit event_semaphore_value(value_type count)
        : val(count) {
        if (PS_UNEXPECTED(count < signal_value)) {
            throw std::runtime_error("invalid semaphore value");
        }
    }
    
    PS_DISABLE_DEFAULT_CONSTRUCTOR(event_semaphore_value);
    PS_DISABLE_COPY(event_semaphore_value);

    value_type operator--() {
        return operator value_type();
    }

    value_type operator++(int) {
        return --val;
    }

    operator value_type() {
        value_type v = val;
        // In the latter case the events have already completed before the wait.
        return v >= signal_value ? -v : signal_value;
    }

    void reset(value_type count) {
        if (PS_UNEXPECTED(count < signal_value)) {
            throw std::runtime_error("invalid semaphore value");
        }
        val = count;
    }

private:
    value_type val;
};

struct binary_semaphore_value {
    typedef int value_type;
    static PS_CONSTEXPR const value_type signal_value = 1;
    static PS_CONSTEXPR const value_type default_value = 0; // default to wait state

    explicit binary_semaphore_value(value_type v)
        : val(!!v) {} // just convert all values to 1/0
    
    PS_DISABLE_DEFAULT_CONSTRUCTOR(binary_semaphore_value);
    PS_DISABLE_COPY(binary_semaphore_value);

    value_type operator--() { // XXX: unlike other semaphores we only wait if the previous (not new) value was < signal_value.
        using std::swap;
        value_type v{0};
        swap(val, v);
        return v;
    }
    value_type operator++(int) {
        using std::swap;
        value_type v{1};
        swap(val, v);
        return v;
    }
    operator value_type() {
        return val;
    }

    void reset() {
        val = 0;
    }

private:
    value_type val;
};

} // isemaphore

template <class Value>
class basic_semaphore {
    typedef typename Value::value_type integral_type;
    typedef Value value_type;

public:
    typedef std::cv_status status_type;

    explicit basic_semaphore(integral_type value = value_type::default_value)
        : m_val(value) {}
    
    PS_DISABLE_COPY(basic_semaphore);
    
    void wait();

    template <typename Duration>
    status_type wait_for(const Duration&);

    void signal() {
        std::lock_guard<std::mutex> l(m_lck);
        if (m_val++ < value_type::signal_value) {
            m_condition.notify_all();
        }
    }

    template <typename = typename std::enable_if<std::is_same<Value, isemaphore::binary_semaphore_value>::value>>
    void reset() {
        std::lock_guard<std::mutex> l(m_lck);
        m_val.reset();
    }

    template <typename = typename std::enable_if<std::is_same<Value, isemaphore::event_semaphore_value>::value>>
    void reset(integral_type count) {
        std::lock_guard<std::mutex> l(m_lck);
        m_val.reset(count);
    }

private:
    value_type m_val;
    std::mutex m_lck;
    std::condition_variable m_condition;
};

template <class Value>
void basic_semaphore<Value>::wait() {
    std::unique_lock<std::mutex> l(m_lck);
    if (--m_val < value_type::signal_value) {
        m_condition.wait(l, [this]() { return m_val >= value_type::signal_value; });
    }
}

template <class Value>
template <typename Duration>
typename basic_semaphore<Value>::status_type basic_semaphore<Value>::wait_for(const Duration& d) {
    status_type st(status_type::no_timeout);
    std::unique_lock<std::mutex> l(m_lck);
    if (--m_val < value_type::signal_value) {
        st = m_condition.wait_for(l, d, [this]() { return m_val >= value_type::signal_value; }) ? status_type::no_timeout : status_type::timeout;
    }
    return st;
}

// classic behavior -- negative values indicate the number of waiters
using semaphore = basic_semaphore<isemaphore::semaphore_value>;
// single waiter for N predetermined events to complete
using event_semaphore = basic_semaphore<isemaphore::event_semaphore_value>;
// producer-consumer scenerios
using binary_semaphore = basic_semaphore<isemaphore::binary_semaphore_value>;

} // prosoft

#endif // PS_CORE_SEMAPHORE_HPP
