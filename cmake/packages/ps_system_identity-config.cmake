if(NOT TARGET ps_system_identity)
    add_subdirectory(${CMAKE_CURRENT_LIST_DIR}/../../modules/system_identity
                     ${CMAKE_BINARY_DIR}/ps_system_identity
    )
endif()
