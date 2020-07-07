cmake_minimum_required(VERSION 3.13)
project(nlohmann_json LANGUAGES CXX)

add_library(${PROJECT_NAME} INTERFACE)

target_compile_features(${PROJECT_NAME} INTERFACE cxx_std_14)

target_include_directories(${PROJECT_NAME} INTERFACE single_include)

add_library(nlohmann_json::nlohmann_json ALIAS ${PROJECT_NAME})
