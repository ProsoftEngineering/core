if(NOT TARGET ps_winutils)
    add_subdirectory(${CMAKE_CURRENT_LIST_DIR}/../../modules/winutils
                     ${CMAKE_BINARY_DIR}/ps_winutils
    )
endif()
