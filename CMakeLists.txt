cmake_minimum_required(VERSION 3.11)

project(NEWorld)
include(${CMAKE_CURRENT_SOURCE_DIR}/CMake/Config.cmake)
message("Configuring NEWorld Base on ${CMAKE_SYSTEM_NAME}/${CMAKE_SYSTEM_VERSION}")

file(GLOB_RECURSE SRC_BASE ${CMAKE_CURRENT_SOURCE_DIR}/Source/*.*)

if (CMAKE_SYSTEM_NAME STREQUAL "Linux")
    message("Configuring Linux 5 Specific Source")
    file(GLOB_RECURSE SRC_SYS ${CMAKE_CURRENT_SOURCE_DIR}/SourceLinux5/*.*)
    include(FindPkgConfig)
    pkg_check_modules(liburing REQUIRED IMPORTED_TARGET GLOBAL liburing>=2.0)
endif()

add_library(NEWorld.Base STATIC ${SRC_BASE} ${SRC_SYS})
target_enable_ipo(NEWorld.Base)
target_include_directories(NEWorld.Base PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/Source)

if (CMAKE_SYSTEM_NAME STREQUAL "Linux")
    target_link_libraries(NEWorld.Base PRIVATE PkgConfig::liburing)
endif()

add_executable(Test Test/main.cpp)
target_link_libraries(Test NEWorld.Base)

