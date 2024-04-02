if(NOT TARGET ps_filesystem)
    add_subdirectory(${CMAKE_CURRENT_LIST_DIR}/../../modules/filesystem
                     ${CMAKE_BINARY_DIR}/ps_filesystem
    )
endif()
