cmake_minimum_required(VERSION 3.20)

add_library(tspectra_core STATIC
    include/tspectra/tspectra_api.h
    src/tspectra_api.cpp
)

target_include_directories(tspectra_core
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_compile_features(tspectra_core PUBLIC cxx_std_20)

if(MSVC)
    target_compile_options(tspectra_core PRIVATE /W4 /permissive-)
else()
    target_compile_options(tspectra_core PRIVATE -Wall -Wextra -Wpedantic)
endif()
