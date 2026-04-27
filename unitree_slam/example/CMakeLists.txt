# cmake_minimum_required must come BEFORE project() per CMake docs:
# https://cmake.org/cmake/help/latest/command/cmake_minimum_required.html
# Some policies are evaluated as part of project(), so swapping the two
# can silently flip behavior.
cmake_minimum_required(VERSION 3.16)
project(slam_example LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include_directories(/usr/local/include include)

find_package(unitree_sdk2 REQUIRED)

add_executable(keyDemo src/keyDemo.cpp)
target_link_libraries(keyDemo unitree_sdk2)

add_executable(keyDemo2 src/keyDemo2.cpp)
target_link_libraries(keyDemo2 unitree_sdk2)

add_executable(keyDemo3 src/keyDemo3.cpp)
target_link_libraries(keyDemo3 unitree_sdk2)
