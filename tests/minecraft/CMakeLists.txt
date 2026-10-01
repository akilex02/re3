cmake_minimum_required(VERSION 3.14)
project(mctests CXX)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

file(GLOB MC_CORE_SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/../../src/minecraft/core/*.cpp)
file(GLOB MC_TEST_SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/*.cpp)

add_executable(mctests ${MC_CORE_SOURCES} ${MC_TEST_SOURCES})
target_include_directories(mctests PRIVATE
	${CMAKE_CURRENT_SOURCE_DIR}
	${CMAKE_CURRENT_SOURCE_DIR}/../../src/minecraft/core
	${CMAKE_CURRENT_SOURCE_DIR}/../../src/minecraft/game)
target_compile_options(mctests PRIVATE -Wall -Wextra)
