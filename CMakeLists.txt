cmake_minimum_required(VERSION 3.21)
project(bedrock LANGUAGES C VERSION 1.0.0)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

option(BEDROCK_BUILD_TESTS "Build tests" ${PROJECT_IS_TOP_LEVEL})
option(BEDROCK_BUILD_EXAMPLES "Build examples" ${PROJECT_IS_TOP_LEVEL})
option(BEDROCK_INSTALL "Generate install targets" ${PROJECT_IS_TOP_LEVEL})
option(BEDROCK_THREAD_SAFE "Lock dynvec and dynmap with pthreads" OFF)

if(BEDROCK_THREAD_SAFE)
    set(THREADS_PREFER_PTHREAD_FLAG ON)
    find_package(Threads REQUIRED)
    if(NOT CMAKE_USE_PTHREADS_INIT)
        message(FATAL_ERROR "BEDROCK_THREAD_SAFE requires pthreads")
    endif()
endif()

set(BEDROCK_SOURCES
        src/arena_allocator.c
        src/args.c
        src/parse.c
        src/vmem.c
        src/pinned_colony.c
)

add_library(bedrock_shared SHARED ${BEDROCK_SOURCES})
add_library(bedrock_static STATIC ${BEDROCK_SOURCES})

set_target_properties(bedrock_shared PROPERTIES
        OUTPUT_NAME bedrock
        EXPORT_NAME bedrock         # exported as bedrock::bedrock
        VERSION ${PROJECT_VERSION}
        SOVERSION ${PROJECT_VERSION_MAJOR}
)
set_target_properties(bedrock_static PROPERTIES
        OUTPUT_NAME bedrock
        EXPORT_NAME bedrock_static  # exported as bedrock::bedrock_static
)

# Namespaced aliases for in-tree / FetchContent consumers
add_library(bedrock::bedrock ALIAS bedrock_shared)
add_library(bedrock::bedrock_static ALIAS bedrock_static)

# Non-namespaced alias
add_library(bedrock ALIAS bedrock_shared)

target_include_directories(bedrock_shared
        PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
        PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
)
target_include_directories(bedrock_static
        PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
        PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
)

# dynvec and dynmap are header-only, so the macros must reach every
# translation unit that includes them. This does not change libbedrock.
if(BEDROCK_THREAD_SAFE)
    foreach(bedrock_target bedrock_shared bedrock_static)
        target_compile_definitions(${bedrock_target}
                PUBLIC
                DYNVEC_THREAD_SAFE
                DYNMAP_THREAD_SAFE
        )
        target_link_libraries(${bedrock_target} PUBLIC Threads::Threads)
    endforeach()
endif()

if(BEDROCK_BUILD_TESTS)
    add_subdirectory(tests)
endif()

if(BEDROCK_BUILD_EXAMPLES)
    add_subdirectory(examples)
endif()

if(BEDROCK_INSTALL)
    include(GNUInstallDirs)
    include(CMakePackageConfigHelpers)

    install(TARGETS bedrock_shared bedrock_static
            EXPORT bedrockTargets
            RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
            LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
            ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
            INCLUDES DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
    )

    install(DIRECTORY include/
            DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
            FILES_MATCHING
            PATTERN "bedrock.h"
            PATTERN "bedrock/*.h"
    )

    install(EXPORT bedrockTargets
            FILE bedrockTargets.cmake
            NAMESPACE bedrock::
            DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/bedrock
    )

    configure_package_config_file(
            "${CMAKE_CURRENT_SOURCE_DIR}/cmake/bedrockConfig.cmake.in"
            "${CMAKE_CURRENT_BINARY_DIR}/bedrockConfig.cmake"
            INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/bedrock
    )

    write_basic_package_version_file(
            "${CMAKE_CURRENT_BINARY_DIR}/bedrockConfigVersion.cmake"
            VERSION ${PROJECT_VERSION}
            COMPATIBILITY SameMajorVersion
    )

    install(FILES
            "${CMAKE_CURRENT_BINARY_DIR}/bedrockConfig.cmake"
            "${CMAKE_CURRENT_BINARY_DIR}/bedrockConfigVersion.cmake"
            DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/bedrock
    )
endif()
