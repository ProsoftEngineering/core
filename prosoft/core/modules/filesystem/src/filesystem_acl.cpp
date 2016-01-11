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

#if !_WIN32
#include <sys/types.h>
#include <sys/acl.h>
#include <sys/stat.h>
#endif

#if __linux__
#include <acl/libacl.h>
inline int acl_get_perm_np(acl_permset_t pset, acl_perm_t p) {
    return ::acl_get_perm(pset, p);
}
#endif

#define PS_HAVE_ACL_DIR_DEFAULT (__linux__ || __FreeBSD__)

#include <unique_resource.hpp>

#include "filesystem.hpp"
#include "filesystem_private.hpp"

// Notes: UNIX model includes mirrors of the owner/group/other perms and mask entry as ell.
// The current implementation ignores these. If a set is attempted for the owner/group then it will go through chmod().
// http://www.qnx.com/developers/docs/qnxcar2/index.jsp?topic=%2Fcom.qnx.doc.neutrino.user_guide%2Ftopic%2Ffiles_acls.html

namespace {
using namespace prosoft::filesystem;
using namespace prosoft::filesystem::v1; // necessary for VS2015

#if !_WIN32
struct acl_t_delete {
    void operator()(void* p) {
        if (p) { ::acl_free(p); }
    }
};
template <typename Ptr>
using unique_acl_t = std::unique_ptr<typename std::remove_pointer<Ptr>::type, acl_t_delete>;
#endif

struct to_ID {
#if __APPLE__
    typedef guid_t* acl_qualifier_type;

    access_control_identity operator()(acl_entry_t ace) {
        if (auto sid = unique_acl_t<void*>{::acl_get_qualifier(ace)}) {
            return access_control_identity{reinterpret_cast<acl_qualifier_type>(sid.get())};
        } else {
            throw prosoft::system::system_error("failed to get ACE identity");
        }
    }
#elif !_WIN32
    static constexpr const auto no_identity = access_control_identity_type::other;
    typedef uid_t* acl_qualifier_type;

    access_control_identity operator()(acl_entry_t ace, access_control_identity_type idt) {
        if (no_identity == idt) {
            return access_control_identity::invalid_user();
        } else if (auto sid = unique_acl_t<void*>{::acl_get_qualifier(ace)}) {
            return access_control_identity{idt, *reinterpret_cast<acl_qualifier_type>(sid.get())};
        } else {
            throw prosoft::system::system_error("failed to get ACE identity");
        }
    }

    access_control_identity operator()(acl_entry_t ace) {
        return operator()(ace, id_type(ace));
    }

    access_control_identity_type id_type(acl_entry_t ace) {
        acl_tag_t tag;
        if (0 == ::acl_get_tag_type(ace, &tag)) {
            access_control_identity_type idt;
            switch (tag) {
                case ACL_USER: idt = access_control_identity_type::user; break;
                case ACL_GROUP: idt = access_control_identity_type::group; break;

                case ACL_UNDEFINED_TAG:
                    throw std::system_error(EINVAL, std::system_category(), "ACE type is undefined");
                    break;

                default:
                    idt = no_identity;
                    break; // XXX: ACL_USER_OBJECT, ACL_GROUP_OBJECT, ACL_OTHER and ACL_MASK
            };
            return idt;
        } else {
            throw prosoft::system::system_error("failed to get ACE ID type");
        }
    }
#else // __APPLE__
    ::PSID operator()(const ::PACCESS_ALLOWED_ACE ace) {
        return reinterpret_cast<const ::PSID>(&ace->SidStart);
    }

    ::PSID operator()(const ::PACCESS_DENIED_ACE ace) {
        return reinterpret_cast<const ::PSID>(&ace->SidStart);
    }

    ::PSID owner(::PSECURITY_DESCRIPTOR sd) {
        PSID sdo;
        BOOL defaulted;
        ::GetSecurityDescriptorOwner(sd, &sdo, &defaulted);
        return sdo;
    }

    ::PSID group(::PSECURITY_DESCRIPTOR sd) {
        PSID sdg;
        BOOL defaulted;
        ::GetSecurityDescriptorGroup(sd, &sdg, &defaulted);
        return sdg;
    }
#endif // __APPLE__
};

struct to_entry {
    static access_control_entry all_access_allowed() {
        auto ae = access_control_entry{access_control_identity::invalid_user()};
        PSASSERT(ae.type() == access_control_type::allow, "Broken assumption");
        PSASSERT(ae.flags() == access_control_flags::no_inherit, "Broken assumption");
        ae.flags(ae.flags() | access_control_flags::inherited); // so set() is an error/NOP
        ae.perms(access_control_perms::all);
        return ae;
    }

    static access_control_entry no_access_allowed() {
        auto ae = access_control_entry{access_control_identity::invalid_user()};
        PSASSERT(ae.type() == access_control_type::allow, "Broken assumption");
        PSASSERT(ae.flags() == access_control_flags::no_inherit, "Broken assumption");
        PSASSERT(ae.perms() == access_control_perms::none, "Broken assumption");
        ae.flags(ae.flags() | access_control_flags::inherited); // so set() is an error/NOP
        return ae;
    }

#if !_WIN32
    access_control_entry operator()(acl_entry_t ace) {
        PSASSERT_NOTNULL(ace);
        return access_control_entry{ent_type(ace),
            flags(ace),
            ac_perms(ace),
            access_control_identity{to_ID{}(ace)}};
    }

private:
#if __APPLE__
    access_control_type ent_type(acl_entry_t ace) {
        acl_tag_t tag;
        if (0 == ::acl_get_tag_type(ace, &tag)) {
            access_control_type act;
            switch (tag) {
                case ACL_EXTENDED_ALLOW: act = access_control_type::allow; break;
                case ACL_EXTENDED_DENY: act = access_control_type::deny; break;
                default:
                    throw std::system_error(EINVAL, std::system_category(), "ACE type is undefined");
            }
            return act;
        } else {
            throw prosoft::system::system_error("failed to get ACE type");
        }
    }

    access_control_flags flags(acl_entry_t ace) {
        acl_flagset_t fset;
        if (0 == ::acl_get_flagset_np(ace, &fset)) {
            access_control_flags f{};
            if (1 == ::acl_get_flag_np(fset, ACL_ENTRY_INHERITED)) { f |= access_control_flags::inherited; }
            if (1 == ::acl_get_flag_np(fset, ACL_ENTRY_FILE_INHERIT)) { f |= access_control_flags::inherit_file; }
            if (1 == ::acl_get_flag_np(fset, ACL_ENTRY_DIRECTORY_INHERIT)) { f |= access_control_flags::inherit_directory; }
            if (1 == ::acl_get_flag_np(fset, ACL_ENTRY_LIMIT_INHERIT)) { f |= access_control_flags::inherit_limited; }
            if (1 == ::acl_get_flag_np(fset, ACL_ENTRY_ONLY_INHERIT)) { f |= access_control_flags::inherit_only; }
            return f;
        } else {
            throw prosoft::system::system_error("failed to get ACE flags");
        }
    }

    access_control_perms ac_perms(acl_entry_t acl) {
        acl_permset_mask_t pset;
        if (0 == ::acl_get_permset_mask_np(acl, &pset)) {
            access_control_perms p{};
            if (pset & ACL_READ_DATA) { p |= access_control_perms::read_data; }
            if (pset & ACL_WRITE_DATA) { p |= access_control_perms::write_data; }
            if (pset & ACL_APPEND_DATA) { p |= access_control_perms::append_data; }
            if (pset & ACL_LIST_DIRECTORY) { p |= access_control_perms::list_directory; }
            if (pset & ACL_ADD_FILE) { p |= access_control_perms::add_file; }
            if (pset & ACL_ADD_SUBDIRECTORY) { p |= access_control_perms::add_sub_directory; }
            if (pset & ACL_EXECUTE) { p |= access_control_perms::execute; }
            if (pset & ACL_SEARCH) { p |= access_control_perms::search; }
            if (pset & ACL_DELETE) { p |= access_control_perms::remove; }
            if (pset & ACL_DELETE_CHILD) { p |= access_control_perms::remove_child; }
            if (pset & ACL_READ_ATTRIBUTES) { p |= access_control_perms::read_attrs; }
            if (pset & ACL_WRITE_ATTRIBUTES) { p |= access_control_perms::write_attrs; }
            if (pset & ACL_READ_EXTATTRIBUTES) { p |= access_control_perms::read_extended_attrs; }
            if (pset & ACL_WRITE_EXTATTRIBUTES) { p |= access_control_perms::write_extended_attrs; }
            if (pset & ACL_READ_SECURITY) { p |= access_control_perms::read_security; }
            if (pset & ACL_WRITE_SECURITY) { p |= access_control_perms::write_security; }
            if (pset & ACL_CHANGE_OWNER) { p |= access_control_perms::change_owner; }
            if (pset & ACL_SYNCHRONIZE) { p |= access_control_perms::synchronize; }
            return p;
        } else {
            throw prosoft::system::system_error("failed to get ACE flags");
        }
    }
#else // __APPLE__
    constexpr access_control_type ent_type(acl_entry_t) const { // UNIX model is explicit allow and implicit deny
        return access_control_type::allow;
    }

    constexpr access_control_flags flags(acl_entry_t) const {
        return access_control_flags::no_inherit;
    }

    // XXX: we don't support NFS acls
    access_control_perms ac_perms(acl_permset_t pset) noexcept {
        access_control_perms p{};
        if (1 == ::acl_get_perm_np(pset, ACL_READ)) { p |= access_control_perms::read; }
        if (1 == ::acl_get_perm_np(pset, ACL_WRITE)) { p |= access_control_perms::write; }
        if (1 == ::acl_get_perm_np(pset, ACL_EXECUTE)) { p |= access_control_perms::execute; }
        return p;
    }

    access_control_perms ac_perms(acl_entry_t ace) {
        acl_permset_t pset;
        if (0 == ::acl_get_permset(ace, &pset)) {
            return ac_perms(pset);
        } else {
            throw prosoft::system::system_error("failed to get ACE flags");
        }
    }
#endif // __APPLE__
#else // _WIN32
    access_control_entry operator()(const ::PACCESS_ALLOWED_ACE ace) {
        return access_control_entry{access_control_type::allow,
            flags(ace->Header.AceFlags & VALID_INHERIT_FLAGS),
            ac_perms(ace->Mask),
            access_control_identity{to_ID{}(ace)}};
    }

    access_control_entry operator()(const ::PACCESS_DENIED_ACE ace) {
        return access_control_entry{access_control_type::deny,
            flags(ace->Header.AceFlags & VALID_INHERIT_FLAGS),
            ac_perms(ace->Mask),
            access_control_identity{to_ID{}(ace)}};
    }

private:
    access_control_flags flags(const ::BYTE val) {
        access_control_flags f{};
        if (val & OBJECT_INHERIT_ACE) {
            f |= access_control_flags::inherit_file;
        }
        if (val & CONTAINER_INHERIT_ACE) {
            f |= access_control_flags::inherit_directory;
        }
        if (val & NO_PROPAGATE_INHERIT_ACE) {
            f |= access_control_flags::inherit_limited;
        }
        if (val & INHERIT_ONLY_ACE) {
            f |= access_control_flags::inherit_only;
        }
        if (val & INHERITED_ACE) {
            f |= access_control_flags::inherited;
        }
        return f;
    }

    access_control_perms map_generic(const ::ACCESS_MASK mask) {
        access_control_perms p{};
        if (mask & GENERIC_ALL) {
            p |= access_control_perms::read | access_control_perms::write | access_control_perms::execute;
        } else {
            if (mask & GENERIC_READ) {
                p |= access_control_perms::read;
            }
            if (mask & GENERIC_WRITE) {
                p |= access_control_perms::write;
            }
            if (mask & GENERIC_EXECUTE) {
                p |= access_control_perms::execute;
            }
        }
        return p;
    }

    access_control_perms map_file(const ::ACCESS_MASK mask) {
        access_control_perms p{};
        if (mask & FILE_READ_DATA) {
            p |= access_control_perms::read_data;
        }
        if (mask & FILE_WRITE_DATA) {
            p |= access_control_perms::write_data;
        }
        if (mask & FILE_APPEND_DATA) {
            p |= access_control_perms::append_data;
        }
        if (mask & FILE_EXECUTE) {
            p |= access_control_perms::execute;
        }
        if (mask & FILE_TRAVERSE) { // XXX: not quite sure on this mapping
            p |= access_control_perms::search;
        }
        if (mask & FILE_LIST_DIRECTORY) {
            p |= access_control_perms::list_directory;
        }
        if (mask & FILE_ADD_FILE) {
            p |= access_control_perms::add_file;
        }
        if (mask & FILE_ADD_SUBDIRECTORY) {
            p |= access_control_perms::add_sub_directory;
        }
        if (mask & FILE_DELETE_CHILD) {
            p |= access_control_perms::remove_child;
        }
        if (mask & FILE_READ_ATTRIBUTES) {
            p |= access_control_perms::read_attrs;
        }
        if (mask & FILE_WRITE_ATTRIBUTES) {
            p |= access_control_perms::write_attrs;
        }
        if (mask & FILE_READ_EA) {
            p |= access_control_perms::read_extended_attrs;
        }
        if (mask & FILE_WRITE_EA) {
            p |= access_control_perms::write_extended_attrs;
        }
        return p;
    }

    access_control_perms ac_perms(const ::ACCESS_MASK mask) {
        access_control_perms p = map_file(mask);
        // map_generic() may set some of these, so just to future proof handle the not set case.
        if (mask & DELETE) {
            p |= access_control_perms::remove;
        } else {
            p &= ~access_control_perms::remove;
        }
        if (mask & READ_CONTROL) {
            p |= access_control_perms::read_security;
        } else {
            p &= ~access_control_perms::read_security;
        }
        if (mask & WRITE_DAC) {
            p |= access_control_perms::write_security;
        } else {
            p &= ~access_control_perms::write_security;
        }
        if (mask & WRITE_OWNER) {
            p |= access_control_perms::change_owner;
        } else {
            p &= ~access_control_perms::change_owner;
        }
        if (mask & SYNCHRONIZE) {
            p |= access_control_perms::synchronize;
        } else {
            p &= ~access_control_perms::synchronize;
        }
        return p;
    }

#if notyet
    DWORD get_access(const PSID sid, error_code& ec) {
        if (::ImpersonateSelf(SecurityImpersonation)) {
            prosoft::windows::Handle token;
            if (::OpenThreadToken(::GetCurrentThread(), TOKEN_QUERY, true, handle(token))) {
                ::GENERIC_MAPPING map{FILE_GENERIC_READ, FILE_GENERIC_WRITE, FILE_GENERIC_EXECUTE, FILE_ALL_ACCESS};
                DWORD access = FILE_ALL_ACCESS;
                MapGenericMask(&access, &map);

                DWORD accessGranted;
                BOOL accessStatus;
                if (AccessCheck(sid, token.get(), access, &map, nullptr, nullptr, &accessGranted, &accessStatus) && accessStatus) {
                    return accessGranted;
                }
            }
        }

        // If we get here there was an error.
        ifilesystem::system_error(ec);
        return 0;
    }
#endif // notyet
#endif // _WIN32
};

struct to_acl {
    access_control_list operator()(const ifilesystem::native_path_type& p, error_code& ec, bool link = false) {
        ec.clear();

        auto sd = operator()(p, acl_kind, ec, link);
#if !PS_HAVE_ACL_DIR_DEFAULT
        return sd ? operator()(sd, ec) : access_control_list{};
#else
        access_control_list al;
        if (sd) {
            auto fal = operator()(sd, ec);
            if (!fal.empty()) {
                al.reserve(fal.size());
                std::move(fal.begin(), fal.end(), std::back_inserter(al));
            }
        }

        if (is_directory(p) && !ec) { // XXX: links to dirs are ignored
            sd = operator()(p, ACL_TYPE_DEFAULT, ec, link);
            if (sd) {
                auto dal = operator()(sd, ec);
                for (auto& i : dal) {
                    i.flags(i.flags() | access_control_flags::inherit_only);
                    al.push_back(std::move(i));
                }
            }
        }

        return al;
#endif // PS_HAVE_ACL_DIR_DEFAULT
    }

#if __APPLE__
    static constexpr const auto acl_kind = ACL_TYPE_EXTENDED;
#elif !_WIN32
    static constexpr const auto acl_kind = ACL_TYPE_ACCESS;
#endif

#if !_WIN32
    using SECURITY_DESCRIPTOR_PTR = unique_acl_t<::acl_t>;

    access_control_list operator()(const SECURITY_DESCRIPTOR_PTR& sd, error_code& ec) {
        auto toent = to_entry{};
        access_control_list al;
        acl_entry_t face{};
// XXX: OS X returns 0 on success, -1 for error. UNIX returns 1 on success, 0 for an empty ACL (not an error) and -1 for error.
#if __APPLE__
        constexpr int entry_found = 0;
#else
        constexpr int entry_found = 1;
#endif
        int rc;
        if (entry_found == (rc = ::acl_get_entry(sd.get(), ACL_FIRST_ENTRY, &face))) {
            do {
                if (auto e = toent(face)) { // can throw
                    al.emplace_back(std::move(e));
                }
            } while (entry_found == ::acl_get_entry(sd.get(), ACL_NEXT_ENTRY, &face));
        } else if (-1 == rc) {
            prosoft::system::system_error(ec);
        }
        return al;
    }

    SECURITY_DESCRIPTOR_PTR operator()(const ifilesystem::native_path_type& p, acl_type_t kind, error_code& ec, bool link) noexcept {
        __typeof__(::acl_get_file)* getacl;
        __typeof__(::stat)* getstat;
        if (!link) {
            getacl = ::acl_get_file;
            getstat = ::stat;
        } else {
#if PS_HAVE_SYMLINK_ACL
            getacl = ::acl_get_link_np;
            getstat = ::lstat;
#else
            PSASSERT_UNREACHABLE("BUG");
            throw std::invalid_argument("link ACL not supported!");
#endif
        }

        // No-ACL behavior:
        // UNIX will return no err and a set of mirrored-perm object entries.
        // ACL_TYPE_DEFAULT will return no err and an empty set if not present.
        // OS X will return ENOENT for a missing ACL entry or a missing path. We need to test for which one.
        if (SECURITY_DESCRIPTOR_PTR fa{getacl(p.c_str(), kind)}) {
            return fa;
        } else {
            prosoft::system::system_error(ec);
            if (ENOENT == ec.value()) {
                struct stat sb;
                if (0 == getstat(p.c_str(), &sb)) {
                    ec.clear();
                }
            }
            return {};
        }
    }
#else // _WIN32
    using SECURITY_DESCRIPTOR_PTR = prosoft::unique_malloc<SECURITY_DESCRIPTOR>;

    access_control_list operator()(const SECURITY_DESCRIPTOR_PTR& sd, error_code& ec) {
        return operator()(sd.get(), ec);
    }
    
    access_control_list operator()(const PSECURITY_DESCRIPTOR sd, error_code& ec) {
        ::PACL dacl = nullptr;
        BOOL daclPresent = false, daclDefaulted = false;
        if (::GetSecurityDescriptorDacl(sd, &daclPresent, &dacl, &daclDefaulted)) {
            if (!daclPresent || !dacl) {
                return all_access();
            }

            const auto aclCount = dacl->AceCount;
            if (!aclCount) {
                return no_access();
            }

            access_control_list al;
            for (WORD i = 0; i < aclCount; ++i) {
                void* ace;
                if (::GetAce(dacl, i, &ace)) {
                    const auto aceh = (PACE_HEADER)ace;
                    if (aceh->AceType == ACCESS_ALLOWED_ACE_TYPE) {
                        al.emplace_back(to_entry{}(reinterpret_cast<PACCESS_ALLOWED_ACE>(ace)));
                    } else if (aceh->AceType == ACCESS_DENIED_ACE_TYPE) {
                        al.emplace_back(to_entry{}(reinterpret_cast<PACCESS_DENIED_ACE>(ace)));
                    } // else, some other ACE type, ignore
                }
            }
            return al;
        } else {
            ifilesystem::system_error(ec);
            return {};
        }
    }

    SECURITY_DESCRIPTOR_PTR operator()(const ifilesystem::native_path_type& p, SECURITY_INFORMATION flags, error_code& ec, bool /*link*/) noexcept {
        DWORD len = 0;
        (void)::GetFileSecurityW(p.c_str(), flags, nullptr, 0, &len);
        auto sd = (len > 0) ? prosoft::make_malloc<SECURITY_DESCRIPTOR>(len) : SECURITY_DESCRIPTOR_PTR{};
        if (sd && ::GetFileSecurityW(p.c_str(), flags, sd.get(), len, &len)) {
            return std::move(sd);
        } else {
            if (sd) {
                ifilesystem::system_error(ec);
            } else {
                ifilesystem::error(ERROR_OUTOFMEMORY, ec);
            }
            return {};
        }
    }

private:
    // LABEL_SECURITY_INFORMATION is only valid for Vista and above.
    static constexpr const SECURITY_INFORMATION acl_kind = DACL_SECURITY_INFORMATION | GROUP_SECURITY_INFORMATION | OWNER_SECURITY_INFORMATION | LABEL_SECURITY_INFORMATION;
#endif // _WIN32
};

#if _WIN32
constexpr const SECURITY_INFORMATION to_acl::acl_kind;
#endif

} // anon

namespace prosoft {
namespace filesystem {
inline namespace v1 {

access_control_list acl(const path& p, error_code& ec) {
    return to_acl{}(ifilesystem::to_native_path{}(p.native()), ec);
}

access_control_list acl(const path& p) {
    error_code ec;
    auto a = acl(p, ec);
    PS_THROW_IF(ec.value(), filesystem_error("failed to get ACL", p, ec));
    return a;
}

void acl(const path&, const access_control_list&, error_code& ec) {
    // should error if any ACE has inherited set

    ifilesystem::error(ENOTSUP, ec);
}

void acl(const path& p, const access_control_list& a) {
    error_code ec;
    acl(p, a, ec);
    PS_THROW_IF(ec.value(), filesystem_error("failed to set ACL", p, ec));
}

access_control_list acl_link(const path& p, error_code& ec) {
#if PS_HAVE_SYMLINK_ACL
    return to_acl{}(ifilesystem::to_native_path{}(p.native()), ec, true);
#else
    (void)p;
    ifilesystem::error(ENOTSUP, ec);
    return {};
#endif
}

access_control_list acl_link(const path& p) {
    error_code ec;
    auto a = acl_link(p, ec);
    PS_THROW_IF(ec.value(), filesystem_error("failed to get link ACL", p, ec));
    return a;
}

void acl_link(const path&, const access_control_list&, error_code& ec) {
    ifilesystem::error(ENOTSUP, ec);
}

void acl_link(const path& p, const access_control_list& a) {
    error_code ec;
    acl_link(p, a, ec);
    PS_THROW_IF(ec.value(), filesystem_error("failed to set link ACL", p, ec));
}

const access_control_list& all_access() noexcept {
    static const auto acl = access_control_list{access_control_list{to_entry::all_access_allowed()}};
    return acl;
}

const access_control_list& no_access() noexcept {
    static const auto acl = access_control_list{access_control_list{to_entry::no_access_allowed()}};
    return acl;
}

#if _WIN32
namespace ifilesystem {

owner make_owner(const path& p, error_code& ec) noexcept {
    if (auto sd = to_acl{}(ifilesystem::to_native_path{}(p.native()), OWNER_SECURITY_INFORMATION | GROUP_SECURITY_INFORMATION, ec, false)) {
        ec.clear();
        to_ID toid;
        const auto osid = toid.owner(sd.get());
        const auto gsid = toid.group(sd.get()); // XXX: if null, id type will default to user
        return owner{access_control_identity{osid}, access_control_identity{gsid}};
    } else {
        return owner::invalid_owner();
    }
}

} // ifilesystem
#endif // _WIN32

} // v1
} // filesystem
} // prosoft
