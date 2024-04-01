if(NOT TARGET ps_regex)
    add_subdirectory(${CMAKE_CURRENT_LIST_DIR}/../../prosoft/core/modules/regex
                     ${CMAKE_BINARY_DIR}/ps_regex
    )
endif()
