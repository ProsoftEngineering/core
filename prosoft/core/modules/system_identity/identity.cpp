// Copyright © 2015-2016, Prosoft Engineering, Inc. (A.K.A "Prosoft")
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

#if __APPLE__
#include <CoreServices/CoreServices.h>
#include <SystemConfiguration/SystemConfiguration.h>
#include <membership.h>
#elif !_WIN32
#include <grp.h>
#include <pwd.h>
#include <stdlib.h>
#include <stdlib.h>
#include <sys/types.h>
#include <vector>
#else
#include <prosoft/core/config/config_windows.h>
#include <windows.h>
#include <wtsapi32.h>
#include <sddl.h>
#endif

#include <string/platform_convert.hpp>
#include <string/unicode_convert.hpp>
#include <system_error.hpp>
#include <system_utils.hpp>

#include "identity.hpp"

namespace {
using namespace prosoft::system;

inline bool is_unknown(const identity& lhs) noexcept {
    return lhs.type() == identity_type::unknown;
}

#if _WIN32
struct to_identity_type {
    identity_type operator()(::SID_NAME_USE t) {
        switch (t) {
            case SidTypeUser: return identity_type::user;
            case SidTypeGroup:
            case SidTypeWellKnownGroup: return identity_type::group;
            case SidTypeDeletedAccount:
            case SidTypeUnknown:
                return identity_type::unknown;
            //XXX: TokenLogonSid has an unlisted type of 11
            default: return identity_type::other;
        }
    }

    identity_type operator()(::PSID t) {
        constexpr const DWORD maxNameLength = 255;
        ::SID_NAME_USE nameType;
        DWORD nCount = maxNameLength, dnCount = maxNameLength;
        auto dummy = prosoft::make_malloc<wchar_t>(prosoft::count_t(nCount + 1 /* null term*/));
        if (!::LookupAccountSidW(nullptr, t, dummy.get(), &nCount, dummy.get(), &dnCount, &nameType)) {
            auto err = system_error("failed to lookup identity");
            if (ERROR_NONE_MAPPED == err.code().value()) {
                nameType = SidTypeUnknown;
            } else {
                throw err;
            }
        }
        return operator()(nameType);
    }
};

using token_user_ptr = prosoft::unique_malloc<::TOKEN_USER>;
inline token_user_ptr token_user(const prosoft::windows::Handle& token, DWORD& err) {
    return token_info<::TOKEN_USER>(TOKEN_INFORMATION_CLASS::TokenUser, token.get(), err);
}

struct SidDelete {
    void operator()(void* p) {
        if (p) { ::FreeSid(p); }
    }
};
using sid_ptr = std::unique_ptr<typename std::remove_pointer<PSID>::type, SidDelete>;

// Search MSDN for "well-known sids"
inline sid_ptr make_admin_group_sid() {
    SID_IDENTIFIER_AUTHORITY auth = SECURITY_NT_AUTHORITY;
    sid_ptr s;
    (void)::AllocateAndInitializeSid(&auth, 2, SECURITY_BUILTIN_DOMAIN_RID, DOMAIN_ALIAS_RID_ADMINS, 0, 0, 0, 0, 0, 0, prosoft::handle(s));
    return s;
}

inline sid_ptr make_everyone_group_sid() {
    SID_IDENTIFIER_AUTHORITY auth = SECURITY_WORLD_SID_AUTHORITY;
    sid_ptr s;
    (void)::AllocateAndInitializeSid(&auth, 1, SECURITY_WORLD_RID, 0, 0, 0, 0, 0, 0, 0, prosoft::handle(s));
    return s;
}

class SIDProperties {
    prosoft::native_string_type m_name;
    prosoft::native_string_type m_domain;

public:
    SIDProperties(const identity& i) {
        DWORD nlen = 0;
        DWORD dnlen = 0;
        SID_NAME_USE ntype;
        if (!::LookupAccountSidW(nullptr, i.system_identity(), nullptr, &nlen, nullptr, &dnlen, &ntype) && nlen && dnlen) {
            m_name.reserve(nlen); // counts include the null term, we need to reserve buffer space for it
            m_name.reserve(dnlen);
            m_name.resize(nlen - 1); // but the size should not reflect the null term
            m_domain.resize(dnlen - 1);
            (void)::LookupAccountSidW(nullptr, i.system_identity(), &m_name[0], &nlen, &m_domain[0], &dnlen, &ntype);
        }
    }
    PS_DEFAULT_DESTRUCTOR(SIDProperties);
    PS_DEFAULT_COPY(SIDProperties);
    PS_DEFAULT_MOVE(SIDProperties);

    explicit operator bool() const {
        return !m_name.empty(); // It's valid for domain to be empty (e.g. Everyone)
    }

    prosoft::native_string_type name() const & { // may be empty
        return m_name; // XXX: need to research how to get actual full name from a SID
    }
    
    prosoft::native_string_type name() && { // may be empty
        return std::move(m_name);
    }

    prosoft::native_string_type account_name() const {
        if (!m_domain.empty()) {
            return m_domain + prosoft::native_string_type{L"\\"} + m_name; // SAM format, UPN format woud be name@DOMAIN
        } else {
            return m_name;
        }
    }
};
#elif __APPLE__
class SIDProperties {
    using CSIdentityQuery_t = prosoft::unique_cftype<CSIdentityQueryRef>;
    prosoft::CF::unique_array m_results;

public:
    SIDProperties(const identity& i) {
        const auto csclass = is_user(i) ? kCSIdentityClassUser : kCSIdentityClassGroup;
        CSIdentityQuery_t query{::CSIdentityQueryCreateForPosixID(kCFAllocatorDefault, i.system_identity(), csclass, ::CSGetDefaultIdentityAuthority())};
        if (query) {
            if (CSIdentityQueryExecute(query.get(), kCSIdentityQueryIncludeHiddenIdentities, nullptr)) {
                m_results.reset(::CSIdentityQueryCopyResults(query.get()));
            }
        }
    }
    PS_DEFAULT_DESTRUCTOR(SIDProperties);
    PS_DISABLE_COPY(SIDProperties);
    PS_DEFAULT_MOVE(SIDProperties);

    explicit operator bool() const {
        return (m_results && ::CFArrayGetCount(m_results.get()) >= 1);
    }

    CSIdentityRef operator[](size_t i) const {
        return (CSIdentityRef)::CFArrayGetValueAtIndex(m_results.get(), i);
    }

    operator CSIdentityRef() const {
        return operator[](0);
    }

    CFStringRef __nullable Name() const {
        return ::CSIdentityGetFullName(*this);
    }

    CFStringRef AccountName() const {
        return ::CSIdentityGetPosixName(*this);
    }

    prosoft::native_string_type name() const {
        if (const auto fullname = Name()) {
            return prosoft::from_CFString<prosoft::native_string_type>{}(fullname);
        } else {
            return {};
        }
    }

    prosoft::native_string_type account_name() const {
        return prosoft::from_CFString<prosoft::native_string_type>{}(AccountName());
    }
};
#else
#define PS_USING_PASSWD_API

template <class Entry>
class system_entry {
    typedef Entry entry_t;
    entry_t m_entry;
    std::vector<char> m_buf;
    static constexpr const size_t k_buf_size = 1024;

    void zero_entry() {
        *(reinterpret_cast<uintptr_t*>(&m_entry)) = 0; // XXX: zero user/group name
    }

    void reserve() {
        m_buf.reserve(k_buf_size);
    }

    void clear() {
        m_buf = decltype(m_buf){};
    }

protected:
    template <class Syscall>
    void init(const char* uname, Syscall scall) {
        zero_entry();
        if (uname) {
            entry_t* result;
            reserve();
            if (0 != scall(uname, &m_entry, m_buf.data(), k_buf_size, &result) || !result) {
                clear();
                zero_entry();
            }
        }
    }

    template <class Syscall>
    void init(uid_t uid, Syscall scall) {
        zero_entry();
        reserve();
        entry_t* result;
        if (0 != scall(uid, &m_entry, m_buf.data(), k_buf_size, &result) || !result) {
            clear();
            zero_entry();
        }
    }

public:
    system_entry() {}
    virtual ~system_entry() {}
    PS_DISABLE_COPY(system_entry);
    PS_DEFAULT_MOVE(system_entry);

    explicit operator bool() const {
        return *(reinterpret_cast<const uintptr_t*>(&m_entry)) != 0;
    }

    const entry_t* operator->() const {
        return &m_entry;
    }
};

class passwd_entry : public system_entry<struct passwd> {
public:
    explicit passwd_entry(const char* uname)
        : system_entry() {
        init(uname, ::getpwnam_r);
    }

    explicit passwd_entry(uid_t uid)
        : system_entry() {
        init(uid, ::getpwuid_r);
    }

    passwd_entry()
        : passwd_entry(::getenv("LOGNAME")) {}

    virtual PS_DEFAULT_DESTRUCTOR(passwd_entry);
    PS_DISABLE_COPY(passwd_entry);
    PS_DEFAULT_MOVE(passwd_entry);
};

class group_entry : public system_entry<struct group> {
public:
    explicit group_entry(const char* gname)
        : system_entry() {
        init(gname, ::getgrnam_r);
    }

    explicit group_entry(gid_t gid)
        : system_entry() {
        init(gid, ::getgrgid_r);
    }

    PS_DISABLE_DEFAULT_CONSTRUCTOR(group_entry);
    virtual PS_DEFAULT_DESTRUCTOR(group_entry);
    PS_DISABLE_COPY(group_entry);
    PS_DEFAULT_MOVE(group_entry);
};

prosoft::native_string_type gecos_name(const char* gecos) {
    const char* p = gecos;
    if (p) {
        while (char c = *p) {
            if (c == '\\' && *(p + 1) == ',') { // ignore escaped commas
                ++p;
            } else if (c == ',') {
                break;
            }
            ++p;
        }
        if (*p == ',' && p > gecos) {
            return prosoft::native_string_type{gecos, static_cast<size_t>(p - gecos)};
        }
    }

    return {};
}

class SIDProperties {
    union {
        void* m_ptr;
        passwd_entry* m_pwd;
        group_entry* m_grp;
    };
    identity_type m_type;

    passwd_entry* user() const {
        return m_type == identity_type::user ? m_pwd : nullptr;
    }

    group_entry* group() const {
        return m_type == identity_type::group ? m_grp : nullptr;
    }

public:
    SIDProperties(const identity& i)
        : m_ptr(nullptr) {
        const auto sid = i.system_identity();
        m_type = i.type();
        if (is_user(i)) {
            m_ptr = new passwd_entry{sid};
        } else if (is_group(i)) {
            m_ptr = new group_entry{sid};
        }
    }

    ~SIDProperties() {
        if (user()) {
            delete m_pwd;
        } else {
            delete m_grp;
        }
    }

    PS_DISABLE_COPY(SIDProperties);
    PS_DEFAULT_MOVE(SIDProperties);

    explicit operator bool() const {
        if (const auto u = user()) {
            return u->operator bool();
        } else if (const auto g = group()) {
            return g->operator bool();
        } else {
            return false;
        }
    }

    prosoft::native_string_type name() const {
        if (user()) {
            return gecos_name((*m_pwd)->pw_gecos);
        } else {
            return (*group())->gr_name;
        }
    }

    prosoft::native_string_type account_name() const {
        if (user()) {
            return prosoft::native_string_type{(*m_pwd)->pw_name};
        } else {
            return (*group())->gr_name;
        }
    }
};
#endif // _WIN32

} // anon

namespace prosoft {
inline namespace conversion {

template <>
class to_string<native_string_type, prosoft::system::identity> {
public:
    typedef native_string_type result_type;
    typedef prosoft::system::identity argument_type;
    result_type operator()(const argument_type& i) {
        if (!i) {
            return {};
        }

#if __APPLE__
        if (is_unknown(i)) {
            result_type s;
            constexpr auto reserveSize = sizeof(uuid_string_t);
            s.reserve(reserveSize);
            s.resize(reserveSize - 1); // don't include NULL term in size
            uuid_unparse(i.guid().g_guid, &s[0]);
            return s;
        }
#endif

#if !_WIN32
        return std::to_string(i.system_identity());
#else
        prosoft::windows::unique_local<wchar_t> ss;
        ::ConvertSidToStringSid(i.system_identity(), prosoft::handle(ss));
        if (ss) {
            return result_type{ss.get()};
        } else {
            return {};
        }
#endif
    }
};
using to_identity_string = to_string<native_string_type, prosoft::system::identity>;

} // conversion
} // prosoft

#if !_WIN32
constexpr const prosoft::system::identity::system_identity_type prosoft::system::identity::unknown_system_identity;
#endif

constexpr const prosoft::system::identity::system_identity_type prosoft::system::identity::invalid_system_identity;

#if __APPLE__
prosoft::system::identity::identity(const guid_t* uuid) {
    PS_THROW_IF_NULLPTR(uuid);
    int idtype;
    // XXX: mbr_uuid_to_id is documented as returning a synthesized uid/gid valid for the current session.
    // However, that is not the actual result. In 10.11 an unknown GUID returns causes mbr_uuid_to_id to fail
    // with ENOENT.
    int rc;
    // XXX: prior to 10.8.x this DOES NOT set errno. It was never documented to do so, but 10.8+ does it.
    if (0 == (rc = ::mbr_uuid_to_id(reinterpret_cast<const unsigned char*>(uuid), &m_sid, &idtype))) {
        switch (idtype) {
            case ID_TYPE_UID: m_type = identity_type::user; break;
            case ID_TYPE_GID: m_type = identity_type::group; break;
            default:
                throw std::system_error{EINVAL, std::system_category(), "unknown id type"};
        };
    } else if (ENOENT == rc) {
        m_type = identity_type::unknown;
        m_sid = unknown_system_identity;
        m_guid = *uuid;
    } else {
        throw std::system_error{rc, error_category(), "failed to lookup id via guid"};
    }
}
#endif

#if _WIN32

prosoft::system::identity::identity(const system_identity_type sid) {
    if (sid) {
        const auto sidsize = ::GetLengthSid(sid);
        m_sid.reset(reinterpret_cast<::SID*>(operator new(sidsize)));
        if (!::CopySid(sidsize, m_sid.get(), sid)) {
            throw system_error("failed to create identity");
        }
        m_type = to_identity_type{}(m_sid.get());
    }
}

prosoft::system::identity::identity(prosoft::native_string_type::const_pointer name) {
    PS_THROW_IF_NULLPTR(name);

    DWORD sidSize = 0, domainSize = 0;
    SID_NAME_USE nameUse;
    (void)::LookupAccountNameW(nullptr, name, nullptr, &sidSize, nullptr, &domainSize, &nameUse);
    if (sidSize > 0) {
        m_sid.reset(reinterpret_cast<::SID*>(operator new(sidSize)));
        auto dummy = make_malloc<wchar_t>(count_t(domainSize + 1 /* null term*/));
        if (::LookupAccountNameW(nullptr, name, m_sid.get(), &sidSize, dummy.get(), &domainSize, &nameUse)) {
            m_type = to_identity_type{}(nameUse);
        } else {
            throw system_error("failed to lookup identity name");
        }
    } else {
        throw system_error("failed to create identity from name");
    }
}

namespace {

prosoft::system::identity token_identity(const prosoft::windows::Handle& token, const char* errmsg) {
    PSASSERT_NOTNULL(errmsg);

    DWORD err;
    if (auto tuser = token_user(token, err)) {
        return identity(tuser->User.Sid);
    } else {
        error_code ec;
        ec.assign(err, error_category());
        throw std::system_error(ec, errmsg);
    }
}

} //anon

prosoft::system::identity identity::process_user() {
    prosoft::windows::Handle token;
    if (::OpenProcessToken(::GetCurrentProcess(), TOKEN_QUERY, prosoft::handle(token))) {
        return token_identity(token, "failed to get process user");
    } else {
        throw system_error("failed to get process token");
    }
}

prosoft::system::identity identity::thread_user() {
    prosoft::windows::Handle token;
    if (::OpenThreadToken(::GetCurrentThread(), TOKEN_QUERY, false, prosoft::handle(token))) {
        return token_identity(token, "failed to get thread user");
    } else if (::GetLastError() == ERROR_NO_TOKEN) {
        return process_user();
    } else {
        throw system_error("failed to get thread token");
    }
}

const prosoft::system::identity& identity::everyone_group() {
    static const identity eid{make_everyone_group_sid().get()};
    return eid;
}

#endif // !_WIN32

prosoft::native_string_type prosoft::system::identity::name() const {
    if (PS_UNEXPECTED(!*this)) {
        return {};
    }

    if (auto results = SIDProperties(*this)) {
        PSASSERT(!is_unknown(*this), "Broken assumption");
#if !_WIN32
        return results.name();
#else
        return std::move(results).name();
#endif
    } else if (!exists(*this)) {
        return prosoft::from_localized_string<prosoft::native_string_type>{}(PSLocalizedString("Unknown Account", "Identity"));
    } else {
        return {};
    }
}

prosoft::native_string_type prosoft::system::identity::account_name() const {
    if (PS_UNEXPECTED(!*this)) {
        return {};
    }

    if (auto results = SIDProperties(*this)) {
        PSASSERT(!is_unknown(*this), "Broken assumption");
        return results.account_name();
    } else if (!exists(*this)) {
        return to_identity_string{}(*this);
    } else {
        return {};
    }
}

#if __APPLE__
guid_t prosoft::system::identity::guid() const {
    if (is_user(*this)) {
        guid_t gi{};
        ::mbr_uid_to_uuid(system_identity(), gi.g_guid);
        return gi;
    } else if (is_group(*this)) {
        guid_t gi{};
        ::mbr_gid_to_uuid(system_identity(), gi.g_guid);
        return gi;
    } else {
        PSASSERT(is_unknown(*this), "WTF?");
        return m_guid;
    }
}
#endif

prosoft::system::identity prosoft::system::identity::console_user() {
#if __APPLE__
    system_identity_type sid;
    if (auto name = ::SCDynamicStoreCopyConsoleUser(nullptr, &sid, nullptr)) {
        ::CFRelease(name);
        return identity{identity_type::user, sid};
    }
#elif !_WIN32
    if (auto pwd = passwd_entry()) {
        return identity(identity_type::user, pwd->pw_uid);
    }
#else
    // Other potential options:
    // TokenLogonSid: Available in Vista+. This is actualy a special SID that does not directly correlate to a user.
    // See: https://blogs.msdn.com/b/david_leblanc/archive/2007/07/29/logon-id-sids.aspx
    //
    // http://www.experts-exchange.com/Programming/Microsoft_Development/Q_20788246.html
    struct WTSDelete {
        void operator()(void* p) {
            if (p) { ::WTSFreeMemory(p); }
        }
    };
    using WTSFree = std::unique_ptr<void, WTSDelete>;

    constexpr const DWORD kNoSession = 0xffffffffU;
    const auto ssid = ::WTSGetActiveConsoleSessionId();
    if (kNoSession != ssid) {
        WCHAR* name;
        DWORD count;
        if (::WTSQuerySessionInformationW(WTS_CURRENT_SERVER_HANDLE, ssid, WTS_INFO_CLASS::WTSUserName, &name, &count)) {
            WTSFree wf{name};
            return identity{name};
        }
    }
#endif

    return invalid_user(); // lookup has failed
}

bool prosoft::system::exists(const prosoft::system::identity& i) {
    return i && !is_unknown(i) && SIDProperties(i);
}

#if PSTEST_HARNESS

// Internal tests.
#include "catch.hpp"

TEST_CASE("system_identity internal") {
#ifdef PS_USING_PASSWD_API
    SECTION("gecos parse") {
        WHEN("gecos is empty") {
            THEN("gecos name is empty") {
                CHECK(gecos_name(",,,").empty());
            }
        }
        WHEN("gecos is invalid") {
            THEN("gecos name is empty") {
                CHECK(gecos_name(nullptr).empty());
                CHECK(gecos_name("").empty());
                CHECK(gecos_name("Prosoft Engineeering").empty());
            }
        }
        WHEN("gecos contains no name") {
            THEN("gecos name is empty") {
                CHECK(gecos_name(",test,test,test").empty());
            }
        }
        WHEN("gecos contains a name") {
            THEN("gecos name is the expected value") {
                CHECK(gecos_name("Prosoft Engineeering,,,") == "Prosoft Engineeering");
            }
        }
        WHEN("gecos contains embedded commas") {
            THEN("gecos name is the expected value") {
                CHECK(gecos_name("Prosoft Engineeering\\, Inc.,,,") == "Prosoft Engineeering\\, Inc.");
            }
        }
    }

    SECTION("passwd lookup") {
        WHEN("known id is used") {
            auto p = passwd_entry("root");
            auto p2 = passwd_entry{uid_t{0}};
            THEN("entry is valid") {
                CHECK(p);
                CHECK(p2);
            }
        }
    }
#endif
}

#endif // PSTEST_HARNESS
