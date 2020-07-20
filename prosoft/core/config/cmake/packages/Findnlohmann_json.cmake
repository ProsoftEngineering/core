if(NOT TARGET nlohmann_json::nlohmann_json)
    include(FetchContent)
    FetchContent_Declare(nlohmann_json
        # smaller than json.hpp
        URL "https://github.com/nlohmann/json/releases/download/v3.7.3/include.zip"
        URL_HASH "MD5=fb96f95cdf609143e998db401ca4f324"
        PATCH_COMMAND "${CMAKE_COMMAND}" -P "${CMAKE_CURRENT_LIST_DIR}/nlohmann_json/patch.cmake"
    )
    FetchContent_MakeAvailable(nlohmann_json)
endif()
