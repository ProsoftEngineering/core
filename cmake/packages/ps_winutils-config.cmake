if(NOT TARGET ps_winutils)
    add_subdirectory(${CMAKE_CURRENT_LIST_DIR}/../../prosoft/core/modules/winutils
                     ${CMAKE_BINARY_DIR}/ps_winutils
    )
endif()
