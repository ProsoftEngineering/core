if(NOT TARGET system_identity)
    add_subdirectory(${CMAKE_CURRENT_LIST_DIR}/../../prosoft/core/modules/system_identity
                     ${CMAKE_BINARY_DIR}/system_identity
    )
endif()
