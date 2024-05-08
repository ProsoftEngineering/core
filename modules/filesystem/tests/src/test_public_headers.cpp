#include <prosoft/core/modules/filesystem/filesystem.hpp>   // first (required for other includes)

#include <prosoft/core/modules/filesystem/filesystem_acl.hpp>
#include <prosoft/core/modules/filesystem/filesystem_change_iterator.hpp>
#include <prosoft/core/modules/filesystem/filesystem_change_monitor.hpp>
#include <prosoft/core/modules/filesystem/filesystem_have_change_monitor.hpp>
#include <prosoft/core/modules/filesystem/filesystem_iterator.hpp>
#include <prosoft/core/modules/filesystem/filesystem_path.hpp>
#include <prosoft/core/modules/filesystem/filesystem_primatives.hpp>
#include <prosoft/core/modules/filesystem/filesystem_snapshot.hpp>
#include <prosoft/core/modules/filesystem/path_utils.hpp>

#if __cplusplus != 201402L // C++14
    #error "Error: unexpected __cplusplus"
#endif

void test_public_headers() {}   // prevent warning "no object file members in the library define global symbols"
