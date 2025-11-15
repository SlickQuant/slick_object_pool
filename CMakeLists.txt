cmake_minimum_required(VERSION 3.25)

project(slick_object_pool
    VERSION 0.1.2
    LANGUAGES CXX
)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Add header-only library
add_library(slick_object_pool INTERFACE)
add_library(slick::slick_object_pool ALIAS slick_object_pool)
target_include_directories(slick_object_pool INTERFACE 
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)
if(UNIX AND NOT APPLE)
    target_link_libraries(slick_object_pool INTERFACE rt atomic)
endif()

option(BUILD_SLICK_OBJECTPOOL_TESTS "Build tests" ON)
if(BUILD_SLICK_OBJECTPOOL_TESTS)
    if (WIN32)
        add_subdirectory(tests)
    else()
        add_subdirectory(tests EXCLUDE_FROM_ALL)
    endif()
endif()

# Installation rules
install(TARGETS slick_object_pool
    EXPORT slick_object_pool-targets
    INCLUDES DESTINATION include
)

install(DIRECTORY include/ DESTINATION include)

# Install CMake config files for find_package support
include(CMakePackageConfigHelpers)

write_basic_package_version_file(
    "${CMAKE_CURRENT_BINARY_DIR}/slick_object_pool-config-version.cmake"
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY SameMajorVersion
)

configure_package_config_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/cmake/slick_object_pool-config.cmake.in"
    "${CMAKE_CURRENT_BINARY_DIR}/slick_object_pool-config.cmake"
    INSTALL_DESTINATION lib/cmake/slick_object_pool
)

install(FILES
    "${CMAKE_CURRENT_BINARY_DIR}/slick_object_pool-config.cmake"
    "${CMAKE_CURRENT_BINARY_DIR}/slick_object_pool-config-version.cmake"
    DESTINATION lib/cmake/slick_object_pool
)

install(EXPORT slick_object_pool-targets
    FILE slick_object_pool-targets.cmake
    NAMESPACE slick::
    DESTINATION lib/cmake/slick_object_pool
)

message(STATUS "slick_object_pool: ${PROJECT_VERSION}")

if(CMAKE_BUILD_TYPE STREQUAL "Release")
    add_custom_target(dist_slick_object_pool ALL
        COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_CURRENT_SOURCE_DIR}/include ${CMAKE_BINARY_DIR}/dist/include
        WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
        COMMENT "Copying slick_object_pool headers to dist/include"
        VERBATIM
    )

    if (PROJECT_IS_TOP_LEVEL)
        add_custom_target(package_slick_object_pool ALL
            COMMAND ${CMAKE_COMMAND} -E tar "cfv" "${CMAKE_BINARY_DIR}/dist/slick_object_pool_${PROJECT_VERSION}.zip" --format=zip "include"
            WORKING_DIRECTORY "${CMAKE_BINARY_DIR}/dist"
            COMMENT "Creating zip archive"
        )

        add_dependencies(package_slick_object_pool dist_slick_object_pool)
    endif()
endif()
