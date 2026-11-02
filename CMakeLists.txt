cmake_minimum_required(VERSION 3.20)

project(tspectra
    VERSION 0.1.0
    LANGUAGES C CXX
)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Export compile commands for IDE code intelligence
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Add core engine library
add_subdirectory(core)
