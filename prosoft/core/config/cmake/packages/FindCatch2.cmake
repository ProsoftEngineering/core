if(NOT TARGET Catch2::Catch2)
    include(FetchContent)
    FetchContent_Declare(catch2
        URL "https://github.com/catchorg/Catch2/releases/download/v2.13.3/catch.hpp"
        URL_HASH "MD5=c02aaee0799e87fc696ee6748dd62d85"
        DOWNLOAD_NO_EXTRACT TRUE
        DOWNLOAD_DIR ${CMAKE_CURRENT_BINARY_DIR}/_deps/catch2-src/include/catch2
        PATCH_COMMAND "${CMAKE_COMMAND}" -P "${CMAKE_CURRENT_LIST_DIR}/Catch2/patch.cmake"
    )
    FetchContent_MakeAvailable(catch2)
endif()
