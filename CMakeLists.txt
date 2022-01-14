cmake_minimum_required(VERSION 3.11)

project(NEWorld)

# Check IPO Support
if (NOT DEFINED NWCONF_IPO_CONFIGURED)
    set(NWCONF_IPO_CONFIGURED TRUE)
    cmake_policy(SET CMP0069 NEW)
    include(CheckIPOSupported)
    check_ipo_supported(RESULT NWCONF_IPO_SUPPORT OUTPUT NWCONF_IPO_SUPPORT_MESSAGE)
    if (NWCONF_IPO_SUPPORT)
        message(STATUS "IPO IS SUPPORTED, ENABLED")
    else()
        message(STATUS "IPO IS NOT SUPPORTED: ${NWCONF_IPO_SUPPORT_MESSAGE}, DISABLED")
    endif()

    function(target_enable_ipo NAME)
        if (NWCONF_IPO_SUPPORT)
            set_property(TARGET ${NAME} PROPERTY INTERPROCEDURAL_OPTIMIZATION $<$<CONFIG:Debug>:FALSE>:TRUE)
        endif ()
    endfunction()
endif()

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} ${CMAKE_SOURCE_DIR}/Build/CMake)

if (APPLE)
    find_package(Boost REQUIRED COMPONENTS filesystem)
endif()

if (MSVC)
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} /source-charset:utf-8")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /source-charset:utf-8")
endif()

file(GLOB_RECURSE SRC_BASE Source/*.*)
add_library(NEWorld.Base STATIC ${SRC_BASE})
target_include_directories(NEWorld.Base PUBLIC Source)
target_enable_ipo(NEWorld.Base)
add_executable(Test Test/main.cpp)
if (APPLE)
    target_link_libraries(NEWorld.Base PRIVATE Boost::filesystem)
endif()
target_link_libraries(Test NEWorld.Base)
