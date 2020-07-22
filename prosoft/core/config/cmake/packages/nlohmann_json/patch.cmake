execute_process(COMMAND "${Patch_EXECUTABLE}" -p1 -i "${CMAKE_CURRENT_LIST_DIR}/v3.8.0.patch"
                RESULT_VARIABLE error_code)
if(error_code)
    message(FATAL_ERROR "Patch failed")
endif()
execute_process(COMMAND "${CMAKE_COMMAND}" -E copy "${CMAKE_CURRENT_LIST_DIR}/CMakeLists.txt" .
                RESULT_VARIABLE error_code)
if(error_code)
    message(FATAL_ERROR "Copy failed")
endif()
