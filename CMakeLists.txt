cmake_minimum_required(VERSION 3.14)

project(
    gtest-strace
    VERSION 0.0.1
    DESCRIPTION "Enable logging of syscalls during individual GTest tests."
    HOMEPAGE_URL "https://github.com/doodspav/gtest-strace"
    LANGUAGES CXX
)

set(CMAKE_CXX_STANDARD 14)

find_package(GTest REQUIRED)

add_executable(
    ${PROJECT_NAME} main.cpp include/gtest-strace/gtest-strace.hpp
    # impl/std
    include/gtest-strace/impl/std/expected.hpp
    # impl/util
    include/gtest-strace/impl/util/expected-assertion.hpp
    # impl
    include/gtest-strace/impl/linux-ptrace.hpp
)

target_include_directories(
    ${PROJECT_NAME}
    SYSTEM PUBLIC
    "$<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include>"
    "$<BUILD_INTERFACE:${PROJECT_BINARY_DIR}/include>"
)

target_link_libraries(
    ${PROJECT_NAME}
    PUBLIC
    GTest::gtest_main
)
