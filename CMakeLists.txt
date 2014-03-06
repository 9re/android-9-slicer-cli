cmake_minimum_required(VERSION 2.4)
set(CMAKE_C_FLAGS_RELEASE "-Wall -O2 -std=c99")
set(CMAKE_C_FLAGS_DEBUG "-g")
set(CMAKE_BUILD_TYPE Release)

project(9slice C)
set(serial "0.0.1")

# Dependencies
find_package(PNG REQUIRED)

# Target
add_executable(9slice 9slice.c)
include_directories(${PNG_INCLUDE_DIRS})
target_link_libraries(9slice ${PNG_LIBRARY})
