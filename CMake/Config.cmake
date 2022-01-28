if (NOT DEFINED NW_PROJECT_DEFINE)
    set(NW_PROJECT_DEFINE TRUE)

    # Setup Language
    set(CMAKE_CXX_STANDARD 20)

    if (APPLE)
        find_package(Boost REQUIRED COMPONENTS filesystem)
        link_libraries(Boost::filesystem)
    endif()

    if (APPLE)
    endif()
    if (MSVC)
        set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} /source-charset:utf-8")
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /source-charset:utf-8")
    endif()

    if (CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
        find_package(GNUCAtomic REQUIRED)
        link_libraries(${GCCLIBATOMIC_LIBRARY})
    endif()

    # Check IPO Support
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
