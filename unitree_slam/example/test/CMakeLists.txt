# Host-only unit tests for the keyDemo3 stair-climb math. No DDS, no SDK,
# no closed-source SLAM .so dependency - just plain C++17 + libm. This builds
# on every developer machine and on CI, so it can be wired into verify.sh.
#
# This subdirectory is included from the *root* CMakeLists.txt only when the
# BUILD_CLIMB_TESTS option is ON (default). Downstream consumers who pull the
# SDK in via add_subdirectory can opt out by setting it OFF.

cmake_minimum_required(VERSION 3.16)

add_executable(test_climb_control test_climb_control.cpp)

target_compile_features(test_climb_control PRIVATE cxx_std_17)

# Pure-math header lives next door at unitree_slam/example/include/
target_include_directories(test_climb_control PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../include
)

# Keep warnings tight on the test target itself, but DO NOT propagate them
# to the rest of the project (the upstream SDK code is not -Wall clean).
if (CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(test_climb_control PRIVATE
        -Wall -Wextra -Wpedantic -Wshadow
    )
endif()

# Register with CTest. The root CMakeLists calls enable_testing() inside the
# BUILD_CLIMB_TESTS branch, so `ctest` from the build dir will pick this up.
add_test(NAME climb_control_unit COMMAND test_climb_control)
