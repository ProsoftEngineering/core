if(NOT TARGET filesystem)
    add_subdirectory(${CMAKE_CURRENT_LIST_DIR}/../../prosoft/core/modules/filesystem
                     ${CMAKE_BINARY_DIR}/filesystem
    )
endif()
