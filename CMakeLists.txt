cmake_minimum_required(VERSION 3.11)

project(NEWorld)
include(${CMAKE_CURRENT_SOURCE_DIR}/CMake/Config.cmake)

file(GLOB_RECURSE SRC_BASE ${CMAKE_CURRENT_SOURCE_DIR}/Source/*.*)
add_library(NEWorld.Base STATIC ${SRC_BASE})
target_enable_ipo(NEWorld.Base)
target_include_directories(NEWorld.Base PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/Source)

#add_executable(Test Test/main.cpp)
#target_link_libraries(Test NEWorld.Base)
