if(NOT TARGET winutils)
    add_subdirectory(${CMAKE_CURRENT_LIST_DIR}/../../prosoft/core/modules/winutils
                     ${CMAKE_BINARY_DIR}/winutils
    )
endif()
