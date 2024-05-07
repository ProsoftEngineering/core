// Copyright © 2024, Prosoft Engineering, Inc. (A.K.A "Prosoft")
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

#ifndef PS_CORE_SNAPSHOT_WIN32_INTERNAL_HPP
#define PS_CORE_SNAPSHOT_WIN32_INTERNAL_HPP

#include <vss.h>
#include <vswriter.h>
#include <vsbackup.h>
#include <atlbase.h>

#include <prosoft/core/modules/filesystem/filesystem.hpp>
#include <prosoft/core/modules/filesystem/filesystem_snapshot.hpp>

namespace prosoft {
namespace filesystem {
inline namespace v1 {

enum snapshot_flags : unsigned {
    snapshot_attached = 0x1U,
};

class snapshot_manager {
public:
    static inline void set(snapshot& s, snapshot_flags f) {
        s.m_flags |= f;
    }

    static inline void clear(snapshot& s, snapshot_flags f) {
        s.m_flags &= ~f;
    }

    static inline void clear(snapshot& s) {
        s.clear();
    }
};

struct vss_writer_component {
    CComPtr<IVssWMComponent> com;
    PVSSCOMPONENTINFO  info;

    vss_writer_component()
        : com()
        , info() {}
    
    ~vss_writer_component() {
        free_info();
    }

    PS_DISABLE_COPY(vss_writer_component); // raw ptr is not safe to copy

    void free_info() {
        if (com && info) {
            com->FreeComponentInfo(info);
            info = nullptr;
        }
    }

    BSTR name() const {
        PSASSERT_NOTNULL(info);
        return info->bstrComponentName;
    }

    // Logcial paths and the selectable attribute form a complex set of rules regarding when components can be explicitly added to a snapshot.
    // https://msdn.microsoft.com/en-us/library/windows/desktop/aa384316(v=vs.85).aspx
    // https://technet.microsoft.com/en-us/subscriptions/aa384989(v=vs.85).aspx
    
    BSTR logical_path() const {
        PSASSERT_NOTNULL(info);
        return info->bstrLogicalPath;
    }

    bool optional() const {
         PSASSERT_NOTNULL(info);
         return info->bSelectable; // Stupid terms: "selectable" == optional, "not-selectable" == required.
    }

    bool required() const {
        return !optional();
    }

    bool root() const {
        return !logical_path() || 0 == wcscmp(name(), logical_path());
    }

    path absolute_path() const {
        if (auto lp = logical_path()) {
            path p{lp};
            p /= name();
            return p;
        }
        return path{name()};
    }

    path parent_path() const {
        if (auto lp  = logical_path()) {
            return path{lp};
        }
        return {};
    }

    UINT file_count() const {
        PSASSERT_NOTNULL(info);
        return info->cFileCount;
    }

    UINT database_count() const {
        PSASSERT_NOTNULL(info);
        return info->cDatabases; 
    }

    UINT log_count() const {
        PSASSERT_NOTNULL(info);
        return info->cLogFiles;
    }

    auto type() const {
         PSASSERT_NOTNULL(info);
         return info->type;
    }

    explicit operator bool() const {
        return com != nullptr;
    }
};

inline void clear(vss_writer_component& wc) {
    if (wc.com && wc.info) {
        wc.com->FreeComponentInfo(wc.info);
        wc.info = nullptr;
    }
    wc.com.Release();
}

struct vss_writer {
    CComPtr<IVssExamineWriterMetadata> info;
    GUID id;
    GUID class_id;
    GUID instance_id;
    CComBSTR name;

    enum class selectable {
        required,
        optional
    };

    using ctype = std::pair<selectable, path>;
    std::vector<ctype> components_added; // absolute paths, used for determining whether to add non-root components

    explicit operator bool() const {
        return info != nullptr;
    }
};

inline void insert(const vss_writer_component& c, vss_writer& w) {
    w.components_added.emplace_back(c.required() ? vss_writer::selectable::required : vss_writer::selectable::optional, c.absolute_path());
}

inline void clear(vss_writer& w) {
    w.info.Release();
    w.name = nullptr;
    w.components_added.clear();
}

inline bool should_add(const vss_writer_component& c, const vss_writer& w) {
    if (c.root()) {
        return true;
    } else {
        auto start = w.components_added.begin();
        auto eof = w.components_added.end();
        auto p = c.parent_path();
        // XXX: assumes parent components are encountered before children.
        while (!p.empty()) {
            auto i = std::find_if(start, eof, [&p](auto& val) {
                return val.second == p;
            });
            // A set is defined as a parent that is "selectable" (optional).
            // If a component is not-selectable (required), then it does not define a set even if parent-child relationships exist between commponents.
            // In that case, children of the required component MUST be added explicitly if there are no other selectable (optional) parents.
            if (eof != i && i->first == vss_writer::selectable::optional) {
                return false;
            }
            p = p.parent_path();
        }
        return true;
    }
}

inline const GUID& guid(const snapshot_id& sid) noexcept {
    return *reinterpret_cast<const GUID*>(&sid.m_id[0]);
}

inline const GUID& guid(const snapshot& snap) noexcept {
    return guid(snap.id());
}

} // namespace v1
} // namespace filesystem
} // namespace prosoft

#endif // PS_CORE_SNAPSHOT_WIN32_INTERNAL_HPP
