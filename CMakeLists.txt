#cmake_minimum_required(VERSION 2.8)

project(dummy_futex)

set(Boost_USE_MULTITHREADED ON)
find_package(Boost 1.62.0 REQUIRED)
find_package(GTest REQUIRED)

if(NOT Boost_FOUND)
	message(FATAL_ERROR "Could not find Boost library")
endif()

include_directories(${Boost_INCLUDE_DIR})
include_directories(include)

add_subdirectory(tests)
enable_testing()