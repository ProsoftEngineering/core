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

#if _WIN32
#include <algorithm>
#include <sstream>
#include <utility>
#include <vector>

#include <vss.h>
#include <vswriter.h>
#include <vsbackup.h>
#include <atlbase.h>

#include "winutils.hpp"
#include "win_token_privs.hpp"
#endif

#include "filesystem.hpp"
#include "filesystem_snapshot.hpp"

#if PS_HAVE_FILESYSTEM_SNAPSHOT

namespace fs = prosoft::filesystem;

namespace {

#if DEBUG
auto& dbgout = std::cout;
#else
std::ostream cnull{nullptr};
auto& dbgout = cnull;
#endif

using stream_guard = prosoft::stream_guard;

inline decltype(dbgout) dbg(const char* msg, const std::error_code& ec) {
    stream_guard ss{dbgout};
    return dbgout << msg << ": " << ec.message() << " (" << std::hex << ec.value() << ")\n";
}

inline decltype(dbgout) dbg(const std::string& msg, const std::error_code& ec) {
    return dbg(msg.c_str(), ec);
}

inline decltype(dbgout) dbg(const std::system_error& e) {
    stream_guard ss{dbgout};
    return dbgout << e.what() << " (" << std::hex << e.code().value() << ")\n";
}

inline decltype(dbgout) dbg(const std::exception& e) {
    return dbgout << "Unknown exception: " << e.what() << "\n";
}

inline std::string make_msg(const char* msg, const GUID& g) {
    std::string s{msg};
    s.append(" ");
    s.append(prosoft::windows::guid_string(g));
    return s;
}

#if _WIN32
using namespace prosoft::windows;

std::error_category& snapshot_category();

using backup_components = CComPtr<IVssBackupComponents>;

backup_components vss(fs::error_code& ec) {
    PSASSERT(!ec, "Broken assumption");
    backup_components p;
    const auto result = CreateVssBackupComponents(&p);
    check(result, ec, snapshot_category());
    return p;
}

backup_components vss_backup(LONG ctx, fs::error_code& ec) {
    PSASSERT(!ec, "Broken assumption");
    if (auto backup = vss(ec)) {
        const auto& cat = snapshot_category();
        if (check(backup->InitializeForBackup(), ec, cat)) {
            if (check(backup->SetContext(ctx), ec, cat)) {
                return backup;
            }
        }
    }
    return {};
}

// Persistent so we don't have to expose COM objects externally. Drawback is that the snapshot is not auto-removed during a crash.
// Persistence is also required to use ExposeSnapshot (though we could manually create a symlink to a non-persistent device).
inline backup_components vss_backup(fs::error_code& ec) {
    return vss_backup(VSS_CTX_APP_ROLLBACK, ec);
}

struct vss_writer_component;

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

    using ctype = std::pair<selectable, fs::path>;
    std::vector<ctype> components_added; // absolute paths, used for determining whether to add non-root components

    explicit operator bool() const {
        return info != nullptr;
    }
};

std::ostream& operator<<(std::ostream& os, const vss_writer& w) {
    CW2A s{w.name};
    return os << s;
}

void clear(vss_writer& w) {
    w.info.Release();
    w.name = nullptr;
    w.components_added.clear();
}

struct vss_writers {
    UINT count;
    UINT idx;
    vss_writer current;

    vss_writers(backup_components& backup, std::error_code& ec) {
        idx = 0;
        count = 0;
        check(backup->GetWriterMetadataCount(&count), ec, snapshot_category());
    }

    vss_writer& next(backup_components& backup, std::error_code& ec) {
        PSASSERT(!ec, "Broken assumpiton");
        if (idx < count) {
            const auto& cat = snapshot_category();
            current.info.Release();
            const auto i = idx++;
            if (check(backup->GetWriterMetadata(i, &current.id, &current.info), ec, cat)) {
                VSS_USAGE_TYPE utype;
                VSS_SOURCE_TYPE stype;
                check(current.info->GetIdentity(&current.instance_id, &current.class_id, &current.name, &utype, &stype), ec, cat);
            } else {
                clear(current);
            }
        } else {
            clear(current);
        }
        return current;
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

    fs::path absolute_path() const {
        if (auto lp = logical_path()) {
            fs::path p{lp};
            p /= name();
            return p;
        }
        return fs::path{name()};
    }

    fs::path parent_path() const {
        if (auto lp  = logical_path()) {
            return fs::path{lp};
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

std::ostream& operator<<(std::ostream& os, const vss_writer_component& wc) {
    CW2A n{wc.name()};
    auto lp = wc.logical_path();
    CW2A p{lp ? lp : L""};
    return os << n << " ::" << " path: " << p <<  " optional: " << wc.optional() << " files: " << wc.file_count() << " databases: " << wc.database_count() << " logs: " << wc.log_count();
}

void clear(vss_writer_component& wc) {
    if (wc.com && wc.info) {
        wc.com->FreeComponentInfo(wc.info);
        wc.info = nullptr;
    }
    wc.com.Release();
}

struct vss_writer_components {
    UINT count;
    UINT idx;
    vss_writer_component current;

    vss_writer_components(vss_writer& writer, std::error_code& ec) {
        UINT implicit_count;
        UINT excluded_count;
        idx = 0;
        count = 0;
        check(writer.info->GetFileCounts(&implicit_count, &excluded_count, &count), ec, snapshot_category());
    }

    vss_writer_component& next(vss_writer& writer, std::error_code& ec) {
        PSASSERT(!ec, "Broken assumption");
        if (idx < count) {
            const auto& cat = snapshot_category();
            const auto i = idx++;
            clear(current);
            if (check(writer.info->GetComponent(i, &current.com), ec, cat)) {
                PSASSERT_NULL(current.info);
                if (!check(current.com->GetComponentInfo(&current.info), ec, cat)) {
                    clear(current);
                }
            }
        } else {
            clear(current);
        }
        return current;
    }
};

void insert(const vss_writer_component& c, vss_writer& w) {
    w.components_added.emplace_back(c.required() ? vss_writer::selectable::required : vss_writer::selectable::optional, c.absolute_path());
}

bool should_add(const vss_writer_component& c, const vss_writer& w) {
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

bool add(const vss_writer_component& c, vss_writer& w, backup_components& backup, std::error_code& ec) {
    PSASSERT(!ec, "Broken assumption");
    const bool doit = should_add(c, w);
    dbgout << "\t" << (doit ? "Added" : "Ignored") << ": " << c << "\n";
    if (doit) {
        check(backup->AddComponent(w.instance_id, w.class_id, c.type(), c.logical_path(), c.name()), ec, snapshot_category());
        if (!ec) {
            insert(c, w);
        } else if (ec.value() == VSS_E_OBJECT_ALREADY_EXISTS) {
            dbgout << "Tried to add VSS component that already exists, ignoring. (" << c << ")\n";
            ec.clear();
        }
    }
    return doit;
}

GUID add(const fs::path& mount, backup_components& backup, GUID& provider, std::error_code& ec) {
    GUID mountid{};
    check(backup->AddToSnapshotSet(const_cast<VSS_PWSZ>(mount.c_str()), provider, &mountid), ec, snapshot_category());
    return mountid;
}

void prepare_writers(backup_components& backup, std::error_code& ec) {
    PSASSERT(!ec, "Broken assumption");
    const auto& cat = snapshot_category();
    CComPtr<IVssAsync> mdstat;
    if (check(backup->GatherWriterMetadata(&mdstat), ec, cat) ){
        if (check(mdstat->Wait(), ec, cat)) {
            HRESULT status;
            if (check(mdstat->QueryStatus(&status, nullptr), ec, cat)) {
                if (VSS_S_ASYNC_CANCELLED == status) {
                    ec = std::error_code{status, cat};
                }
            }
        }
    }
};

void update_writers(backup_components& backup, std::error_code& ec) {
    PSASSERT(!ec, "Broken assumption");
    const auto& cat = snapshot_category();

    CComPtr<IVssAsync> status;
    if (check(backup->GatherWriterStatus(&status), ec, cat)) {
        if (check(status->Wait(), ec, cat)) {
            HRESULT result;
            status->QueryStatus(&result, nullptr);
            if (check(result, ec, cat)) {
                ;
            } else {
                dbg("A writer returned a failed status", ec);
            }
        }
    }
}

void add_writer_commponents(backup_components& backup, std::error_code& ec) {
    vss_writers writers{backup, ec};
    PS_THROW_IF(ec, std::system_error(ec, "Failed to find VSS writers"));

    while (auto& w = writers.next(backup, ec)) {
        vss_writer_components components{w, ec};
        dbgout << "VSS Writer " << w << ": " << components.count << " components" << "\n";
        while (auto& c = components.next(w, ec)) {
            add(c, w, backup, ec);
            PS_THROW_IF(ec, std::system_error(ec, "Failed to add VSS writer component"));
        }
        PS_THROW_IF(ec, std::system_error(ec, "Failed to get all VSS writer components"));
    }
    PS_THROW_IF(ec, std::system_error(ec, "Failed to get all VSS writers"));
}

void create_snapshot(backup_components& backup, GUID& snapid, std::error_code& ec) {
    check(backup->StartSnapshotSet(&snapid), ec, snapshot_category());
}

bool start_snapshot(backup_components& backup, std::error_code& ec) {
    PSASSERT(!ec, "Broken assumption");
    const auto& cat = snapshot_category();

    CComPtr<IVssAsync> status;
    if (check(backup->PrepareForBackup(&status), ec, cat)) {
        if (check(status->Wait(), ec, cat)) {
            HRESULT result;
            status->QueryStatus(&result, nullptr);
            if (check(result, ec, cat)) {
                return true;
            } else {
                dbg("PrepareForBackup status failed", ec);
            }
        }
    }

    return false;
}

bool commit_snapshot(backup_components& backup, std::error_code& ec) {
    PSASSERT(!ec, "Broken assumption");

    const auto& cat = snapshot_category();
    CComPtr<IVssAsync> status;
    if (check(backup->DoSnapshotSet(&status), ec, cat)) {
        if (check(status->Wait(), ec, cat)) {
            HRESULT result;
            status->QueryStatus(&result, nullptr);
            if (check(result, ec, cat)) {
                return true;
            } else {
                dbg("DoSnapshotSet status failed", ec);
            }
        }
    }

    return false;
}

#if notyet
// This is necessary (along with SetBackupSucceeded) for pre-windows 7 IF we are using commponent mode.
bool end_snapshot(backup_components& backup, std::error_code& ec) {
    PSASSERT(!ec, "Broken assumption");
    const auto& cat = snapshot_category();

    CComPtr<IVssAsync> status;
    if (check(backup->BackupComplete(&status), ec, cat)) {
        if (check(status->Wait(), ec, cat)) {
            HRESULT result;
            status->QueryStatus(&result, nullptr);
            if (check(result, ec, cat)) {
                return true;
            } else {
                dbg("BackupComplete status failed", ec);
            }
        }
    }

    return false;
}
#endif

void vss_delete_snapshot(backup_components& backup, const GUID& snapid,  std::error_code& ec) {
    LONG count;
    GUID ignored;
    check(backup->DeleteSnapshots(snapid, VSS_OBJECT_SNAPSHOT, true, &count, &ignored), ec, snapshot_category());
}

fs::path sysroot() noexcept {
    wchar_t* buf;
    size_t len;
    if (0 == _wdupenv_s(&buf, &len, L"SystemRoot")) {
        return fs::path{std::wstring{buf, len}};
    }
    return fs::path{L"C:\\windows"};
}

GUID make_snapshot(const fs::path& p, backup_components& backup, GUID& provider, std::error_code& ec) {
    static const auto rootdir = sysroot();

    // MSDN Overview of Processing a backup: https://msdn.microsoft.com/en-us/library/windows/desktop/aa384589(v=vs.85).aspx
    // Technet Troubleshooting the Volume Shadow Copy Service: https://technet.microsoft.com/en-us/library/ff597980(v=exchg.80).aspx
    constexpr bool selectComponentsForBackup = false; // must be true if add_writer_components() is called
    constexpr bool partialFileSupport = false;

    auto mountPath = fs::mount_path(p, ec);
    PS_THROW_IF(ec, fs::filesystem_error("Failed to get mount point", p, ec));

    const bool backupBootableSystemState = fs::equivalent(rootdir.root_path(), mountPath, ec);
    ec.clear();
    dbgout << mountPath << "is " << (backupBootableSystemState ? "bootable" : "not bootable") << "\n";

    check(backup->SetBackupState(selectComponentsForBackup, backupBootableSystemState, VSS_BT_FULL, partialFileSupport), ec, snapshot_category());
    PS_THROW_IF(ec, std::system_error(ec, "Failed to set backup state"));

    if (selectComponentsForBackup) {
        prepare_writers(backup, ec);
        PS_THROW_IF(ec, std::system_error(ec, "Failed to prepare snapshot components"));

        add_writer_commponents(backup, ec);
        PS_THROW_IF(ec, std::system_error(ec, "Failed to add snapshot components"));
    }

    GUID setid; // setid is not maintained as it's no longer needed
    create_snapshot(backup, setid, ec);
    PS_THROW_IF(ec, std::system_error(ec, "Failed to create snapshot"));

    GUID snapid = add(mountPath, backup, provider, ec);
    PS_THROW_IF(ec, std::system_error(ec, "Failed to add volume snapshot"));

    dbgout << "Added volume snapshot " << snapid << " for " << mountPath << "\n";

    start_snapshot(backup, ec);
    PS_THROW_IF(ec, std::system_error(ec, "Failed to start snapshot"));

    update_writers(backup, ec);
    PS_THROW_IF(ec, std::system_error(ec, "Failed to update writers pre-commit"));

    commit_snapshot(backup, ec);
    PS_THROW_IF(ec, std::system_error(ec, "Failed to commit snapshot"));

    update_writers(backup, ec);
    PS_THROW_IF(ec, std::system_error(ec, "Failed to update writers post-commit"));

    return snapid;
}

inline bool is_null(const GUID& g) {
    return 0 == std::memcmp(&g, &GUID_NULL, sizeof(GUID));
}

// Multiple providers can be installed and some 3rd party providers may cause errors.
GUID system_provider(backup_components& backup, fs::error_code& ec) {
    GUID provider{};
    CComPtr<IVssEnumObject> i;
    if (check(backup->Query(GUID_NULL, VSS_OBJECT_NONE, VSS_OBJECT_PROVIDER, &i), ec, snapshot_category())) {
        do {
            VSS_OBJECT_PROP prop;
            ULONG fetched{};
            if (ok(i->Next(1, &prop, &fetched), ec, snapshot_category())) {
                CoTaskMemFree(prop.Obj.Prov.m_pwszProviderName);
                CoTaskMemFree(prop.Obj.Prov.m_pwszProviderVersion);

                if (prop.Obj.Prov.m_eProviderType == VSS_PROV_SYSTEM) {
                    provider = prop.Obj.Prov.m_ProviderId;
                    break;
                }
            } else if (ec.value() == S_FALSE) {
                ec.clear();
                break;
            } else {
                break;
            }
        } while(1);
    }
    return provider;
}
#endif // WIN32

bool initialize(std::error_code& ec) {
#if _WIN32
    static std::error_code err{};
    static const bool good = enable_process_backup_privilege(err);
    ec = err;
    return good;
#else
    ec.clear();
    return true;
#endif
}

class snapshot_error_category : public std::error_category {
    virtual const char* name() const noexcept override {
        return "Snapshot";
    }

    virtual std::string message(int ec) const override {
        switch (static_cast<HRESULT>(ec)) {
            case E_ACCESSDENIED:
                return std::system_category().message(ERROR_ACCESS_DENIED);
            break;

            case E_INVALIDARG:
                return std::system_category().message(ERROR_BAD_ARGUMENTS);
            break;

            case E_OUTOFMEMORY:
                return std::system_category().message(ERROR_OUTOFMEMORY);
            break;

            // VSS (and all COM) errors must be decoded via GetErrorInfo(), but that is only valid immediately after the failed call.
            case VSS_E_BAD_STATE:
                return "VSS component is in an invalid state.";
            break;

            // VSS_E_UNEXPECTED, usually means that a VSS writer is in somme state other than "Stable". Use vssadmin list writers to check.
            // In particular, the 'System Writer' and 'MSSearch Service Writer' writers may get into a "waiting for commpletion" state and prevent snapshots.
            // In may be possible to recover from this particular scenerio with the following (from an Admin shell):
            /*
            net stop vss
            net stop CryptSvc
            net start CryptSvc
            net start vss
            */
            // Prior to Windows 8, it could also indicate a failure due to a 4K-block drive. These are only supported in Win8 and later.
            case VSS_E_UNEXPECTED:
                return "A Volume Shadow Copy Service component encountered an unexpected error. Check the Application event log for more information.";
            break;

            case VSS_E_VOLUME_NOT_SUPPORTED:
            case VSS_E_VOLUME_NOT_SUPPORTED_BY_PROVIDER:
                return "Shadow copying the specified volume is not supported.";
            break;

            case VSS_E_SNAPSHOT_SET_IN_PROGRESS:
                return "Another shadow copy creation is already in progress. Wait a few moments and try again.";
            break;

            case VSS_E_MAXIMUM_NUMBER_OF_SNAPSHOTS_REACHED:
                return "The specified volume has already reached its maximum number of shadow copies.";
            break;

            case VSS_E_MAXIMUM_NUMBER_OF_VOLUMES_REACHED:
                return "The maximum number of volumes or remote file shares have been added to the shadow copy set. The specified volume or remote file share was not added to the shadow copy set.";
            break;

            case VSS_E_INSUFFICIENT_STORAGE:
                return "Insufficient storage available to create either the shadow copy storage file or other shadow copy data.";
            break;

            case VSS_E_OBJECT_NOT_FOUND:
                return "The specified object was not found.";
            break;

            case VSS_E_NESTED_VOLUME_LIMIT:
                return "The specified volume is nested too deeply to participate in the VSS operation.";
            break;

            case VSS_S_ASYNC_CANCELLED:
                return "A volume asynchronous operation has been cancelled.";
            break;

            case VSS_E_HOLD_WRITES_TIMEOUT:
                return "The shadow copy provider timed out while holding writes to the volume being shadow copied. This is probably due to excessive activity on the volume by an application or a system service. Try again later when activity on the volume is reduced.";
            break;

            case VSS_E_WRITERERROR_RETRYABLE:
                return "A transient VSS writer error occurred. Try again.";
            break;

            default:
                return "Unknown VSS error.";
            break;
        }
    }
};

std::error_category& snapshot_category() {
    static snapshot_error_category cat{};
    return cat;
}

enum snapshot_flags : uint8_t {
    snapshot_attached = 0x1,
};

inline bool attached(const fs::snapshot& snap) noexcept {
    return 0 != (snap.reserved() & snapshot_attached);
}

inline const GUID& guid(const fs::snapshot& snap) noexcept {
    return *reinterpret_cast<const GUID*>(&snap.id().m_id[0]);
}

} // anon

namespace prosoft {
namespace filesystem {
inline namespace v1 {

/*
XXX: When operating on persistent snapshots (deletion, or list via Query) the backup componenents context MUST match the context of the snapshot or the snapshot will be ignored.
*/

static bool detach_snapshot(backup_components& backup, snapshot& snap, std::error_code& ec) {
    const auto& snapid = guid(snap);
    CComQIPtr<IVssBackupComponentsEx2> backup2(backup);
    if (backup2 && check(backup2->UnexposeSnapshot(snapid), ec, snapshot_category())) {
        return true;
    } else {
        if (!ec) {
            ec = std::error_code{E_NOINTERFACE, std::system_category()};
        }
        dbg(make_msg("Failed to detach snapshot", snapid), ec);
        return false;
    }
}

static void delete_snapshot(backup_components& backup, snapshot& snap, std::error_code& ec) {
    vss_delete_snapshot(backup, guid(snap), ec);
    if (ec) {
        dbg(make_msg("Failed to delete snapshot", guid(snap)), ec);
    }
}

snapshot_id::snapshot_id(native_string_type::const_pointer s) {
    // UuidFromString does not require braces, but it may require a separate link lib.
    native_string_type tmp;
    if (*s != L'{') {
        tmp.append(L"{");
        tmp.append(s);
        tmp.append(L"}");
        s = tmp.c_str();
    }
    const auto result = IIDFromString(s, reinterpret_cast<GUID*>(&m_id[0]));
    PS_THROW_IF(result != S_OK, std::invalid_argument("Invalid GUID string"));
}

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

snapshot::~snapshot() {
    static_assert(sizeof(flags_type) == sizeof(snapshot_flags), "Internal flag size is different!");
    
    const auto& snapid = guid(*this);
    if (snapid != GUID_NULL) {
        std::error_code ec;
        com_init com{ec};
        if (!ec) {
            if (auto backup = vss_backup(ec)) {
                if (attached(*this)) {
                    detach_snapshot(backup, *this, ec);
                }
                delete_snapshot(backup, *this, ec);
            }
        }
    }
}

snapshot create_snapshot(const path& p, const snapshot_create_options& opts) {
    error_code ec;
    auto snap = create_snapshot(p, opts, ec);
    PS_THROW_IF(ec.value(), filesystem_error("Could not create snapshot", p, ec));
    return snap;
}

snapshot create_snapshot(const path& p, const snapshot_create_options&, error_code& ec) {
    try {
        com_init com{ec};
        PS_THROW_IF(ec, std::system_error(ec, "Failed to init COM"));

        initialize(ec);
        PS_THROW_IF(ec, std::system_error(ec, "Failed to init system"));
        auto backup = vss_backup(ec);
        PS_THROW_IF(!backup, std::system_error(ec, "Failed to init VSS"));

        auto provider = system_provider(backup, ec);
        PS_THROW_IF(is_null(provider), std::system_error(ec, "Failed to find VSS system provider"));

        auto snapid = make_snapshot(p, backup, provider, ec);
        return snapshot{reinterpret_cast<unsigned char*>(&snapid)};
    } catch(const std::system_error& e) {
        dbg(e);
    } catch(const std::exception& e) {
        dbg(e);
    }

    return snapshot{snapshot_id{}};
}

void attach_snapshot(snapshot& s, const path& p) {
    error_code ec;
    attach_snapshot(s, p, ec);
    PS_THROW_IF(ec.value(), filesystem_error("Could not attach snapshot", p, ec));
}

void attach_snapshot(snapshot& snap, const path& p, std::error_code& ec) {
    com_init com{ec};
    if (!ec) {
        const auto& cat = snapshot_category();
        if (auto backup = vss_backup(ec)) {
            const auto& snapid = *reinterpret_cast<const GUID*>(&snap.id().m_id[0]);
            wchar_t* exposedPath;
            if (check(backup->ExposeSnapshot(snapid, nullptr, VSS_VOLSNAP_ATTR_EXPOSED_LOCALLY, const_cast<VSS_PWSZ>(p.c_str()), &exposedPath), ec, cat)) {
                snapshot_manager::set(snap, snapshot_attached);
                CoTaskMemFree(exposedPath);
            }
        }
    }
}

void detach_snapshot(snapshot& snap) {
    error_code ec;
    detach_snapshot(snap, ec);
    PS_THROW_IF(ec.value(), filesystem_error("Could not detach snapshot", guid_wstring(guid(snap)), ec));
}

void detach_snapshot(snapshot& snap, std::error_code& ec) {
    com_init com{ec};
    if (!ec) {
        if (auto backup = vss_backup(ec)) {
            if (detach_snapshot(backup, snap, ec)) {
                snapshot_manager::clear(snap, snapshot_attached);
            }
        }
    }
}

void delete_snapshot(snapshot& snap) {
    error_code ec;
    delete_snapshot(snap, ec);
    PS_THROW_IF(ec.value(), filesystem_error("Could not delete snapshot", guid_wstring(guid(snap)), ec));
}

void delete_snapshot(snapshot& snap, std::error_code& ec) {
    PS_THROW_IF(attached(snap), filesystem_error("Invalid state: Snapshot is attached!", guid_wstring(guid(snap)), error_code(EINVAL, system::posix_category())));

    com_init com{ec};
    if (!ec) {
        if (auto backup = vss_backup(ec)) {
            delete_snapshot(backup, snap, ec);
            if (!ec) {
                snapshot_manager::clear(snap);
            }
        }
    }
}

} // v1
} // filesystem
} // prosoft

#if PSTEST_HARNESS
// Internal tests.
#include <memory>

#include "catch.hpp"

TEST_CASE("snapshot_internal") {
    WHEN("changing snapshot state") {
        using namespace fs;
        snapshot snap{snapshot_id{reinterpret_cast<const unsigned char*>(&GUID_DEVINTERFACE_FLOPPY)}};
        CHECK(snap.reserved() == 0);
        snapshot_manager::set(snap, snapshot_attached);
        CHECK(snap.reserved() == snapshot_attached);
        snapshot_manager::clear(snap, snapshot_attached);
        CHECK(snap.reserved() == 0);
        snapshot_manager::set(snap, snapshot_attached);
        CHECK(snap.reserved() != 0);
        snapshot_manager::clear(snap);
        CHECK(guid(snap) == GUID_NULL);
        CHECK(snap.reserved() == 0);
    }

    WHEN("clearing a writer") {
        vss_writer w;
        CHECK_FALSE(w);
        w.name = decltype(w.name){L"test"};
        CHECK(w.name);
        w.components_added.emplace_back(vss_writer::selectable::optional, L"test");
        CHECK_FALSE(w.components_added.empty());

        clear(w);
        CHECK_FALSE(w.name);
        CHECK(w.components_added.empty());
    }

    WHEN("clearing a component") {
        vss_writer_component c;
        CHECK_FALSE(c);
        clear(c);
    }

    WHEN("processing components") {
        static auto set = [](auto& v, wchar_t* name, wchar_t* path = nullptr, bool selectable = false) {
            auto i = &v.second;
            i->bstrComponentName = name;
            i->bstrLogicalPath = path;
            i->bSelectable = selectable;
            v.first.info = i;
        };

        vss_writer w;

        using val = std::pair<vss_writer_component, VSS_COMPONENTINFO>;
        // From MSDN "Logical Pathing of Components" doc.
        val executables;
        set(executables, L"Executables");
        auto c = &executables.first;
        CHECK_FALSE(c->optional());
        CHECK(c->required());
        CHECK(c->root());
        CHECK(c->absolute_path().native() == c->name());
        CHECK(c->parent_path().empty());
        CHECK(should_add(*c, w));
        insert(*c, w);
        REQUIRE(w.components_added.size() > 0);
        CHECK(w.components_added.back().first == vss_writer::selectable::required);
        CHECK(w.components_added.back().second.native() == c->absolute_path());

        val configFiles;
        set(configFiles, L"ConfigFiles", executables.first.name());
        c = &configFiles.first;
        CHECK_FALSE(c->optional());
        CHECK(c->required());
        CHECK_FALSE(c->root());
        CHECK(c->absolute_path().native() == L"Executables\\ConfigFiles");
        CHECK(c->parent_path().native() == executables.first.name());
        CHECK(should_add(*c, w));
        insert(*c, w);

        val licenseInfo;
        set(licenseInfo, L"LicenseInfo", nullptr, true);
        c = &licenseInfo.first;
        CHECK(c->optional());
        CHECK_FALSE(c->required());
        CHECK(should_add(*c, w));
        insert(*c, w);

        val security;
        set(security, L"Security", nullptr, true);
        c = &security.first;
        CHECK(should_add(*c, w));
        insert(*c, w);

        val userInfo;
        set(userInfo, L"UserInfo", security.first.name());
        c = &userInfo.first;
        CHECK_FALSE(should_add(*c, w));

        val certificates;
        set(certificates, L"Certificates", security.first.name());
        c = &certificates.first;
        CHECK_FALSE(should_add(*c, w));

        val writerData;
        set(writerData, L"writerData", nullptr, true);
        c = &writerData.first;
        CHECK(should_add(*c, w));
        insert(*c, w);

        val set1;
        set(set1, L"Set1", writerData.first.name());
        c= &set1.first;
        CHECK_FALSE(should_add(*c, w));

        val jan;
        set(jan, L"Jan", L"writerData\\Set1");
        c = &jan.first;
        CHECK(c->absolute_path().native() == L"writerData\\Set1\\Jan");
        CHECK(c->parent_path().native() == c->logical_path());
        CHECK_FALSE(should_add(*c, w)); // Set1 is not a set, but writerData is

        val dec;
        set(dec, L"Dec", L"writerData\\Set1");
        c = &dec.first;
        CHECK_FALSE(should_add(*c, w));

        val set2;
        set(set2, L"Set2", writerData.first.name());
        c = &set2.first;
        CHECK_FALSE(should_add(*c, w));

        val jan2;
        set(jan2, L"Jan", L"writerData\\Set2");
        c = &jan2.first;
        CHECK_FALSE(should_add(*c, w));

        val dec2;
        set(dec2, L"Dec", L"writerData\\Set2");
        c = &dec2.first;
        CHECK_FALSE(should_add(*c, w));

        val query;
        set(query, L"Query", L"writerData\\QueryLogs");
        c = &query.first;
        CHECK_FALSE(should_add(*c, w));

        val usage;
        set(usage, L"Usage", writerData.first.name(), true);
        c = &usage.first;
        CHECK_FALSE(should_add(*c, w));

        val janU;
        set(janU, L"Jan", L"writerData\\Usage");
        c = &janU.first;
        CHECK_FALSE(should_add(*c, w));

        val decU;
        set(decU, L"Dec", L"writerData\\Usage");
        c = &decU.first;
        CHECK_FALSE(should_add(*c, w));

        clear(w);
        writerData.second.bSelectable = false; // no longer a set root
        c = &writerData.first;
        CHECK(c->required());
        CHECK(should_add(*c, w));
        insert(*c, w);

        c = &set1.first;
        CHECK(should_add(*c, w));
        insert(*c, w);

        c = &jan.first;
        CHECK(should_add(*c, w));
        insert(*c, w);

        c = &usage.first;
        CHECK(should_add(*c, w));
        insert(*c, w);
    }
}
#endif // PSTEST_HARNESS

#endif // PS_HAVE_FILESYSTEM_SNAPSHOT
