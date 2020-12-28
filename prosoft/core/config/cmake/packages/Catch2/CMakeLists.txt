cmake_minimum_required(VERSION 3.13)
project(Catch2 LANGUAGES CXX)

add_library(${PROJECT_NAME} INTERFACE)

target_compile_features(${PROJECT_NAME} INTERFACE cxx_std_14)

target_include_directories(${PROJECT_NAME} INTERFACE include)

add_library(Catch2::Catch2 ALIAS ${PROJECT_NAME})
