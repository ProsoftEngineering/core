if(NOT TARGET ps_regex)
    add_subdirectory(${CMAKE_CURRENT_LIST_DIR}/../../modules/regex
                     ${CMAKE_BINARY_DIR}/ps_regex
    )
endif()
