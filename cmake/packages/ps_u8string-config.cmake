if(NOT TARGET ps_u8string)
    add_subdirectory(${CMAKE_CURRENT_LIST_DIR}/../../modules/u8string
                     ${CMAKE_BINARY_DIR}/ps_u8string
    )
endif()
