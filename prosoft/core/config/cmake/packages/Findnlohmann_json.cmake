if(NOT TARGET nlohmann_json::nlohmann_json)
    include(FetchContent)
    FetchContent_Declare(nlohmann_json
        # include.zip is smaller than json.hpp
        URL "https://github.com/nlohmann/json/releases/download/v3.8.0/include.zip"
        URL_HASH "MD5=ccd5d58f7344e7a53663a381030ddcd5"
        PATCH_COMMAND "${CMAKE_COMMAND}" -P "${CMAKE_CURRENT_LIST_DIR}/nlohmann_json/patch.cmake"
    )
    FetchContent_MakeAvailable(nlohmann_json)
endif()
