if(NOT TARGET ps_core)
    add_subdirectory(${CMAKE_CURRENT_LIST_DIR}/../../prosoft/core/modules/core
                     ${CMAKE_BINARY_DIR}/ps_core
    )
endif()
