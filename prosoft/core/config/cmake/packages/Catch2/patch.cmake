execute_process(COMMAND "${CMAKE_COMMAND}" -E copy "${CMAKE_CURRENT_LIST_DIR}/CMakeLists.txt" .
                RESULT_VARIABLE error_code)
if(error_code)
    message(FATAL_ERROR "copy failed")
endif()
