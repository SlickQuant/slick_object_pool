cmake_minimum_required(VERSION 3.25)

set(BUILD_VERSION 0.1.1)
project(slick_object_pool VERSION ${BUILD_VERSION} LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Add header-only library
add_library(slick_object_pool INTERFACE)
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
install(DIRECTORY include/ DESTINATION include)

message(STATUS "slick_object_pool: ${BUILD_VERSION}")

if(CMAKE_BUILD_TYPE STREQUAL "Release")
    add_custom_target(dist_slick_object_pool ALL
        COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_CURRENT_SOURCE_DIR}/include ${CMAKE_BINARY_DIR}/dist/include
        WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
        COMMENT "Copying slick_object_pool headers to dist/include"
        VERBATIM
    )

    if (PROJECT_IS_TOP_LEVEL)
        add_custom_target(package_slick_object_pool ALL
            COMMAND ${CMAKE_COMMAND} -E tar "cfv" "${CMAKE_BINARY_DIR}/dist/slick_object_pool_${BUILD_VERSION}.zip" --format=zip "include"
            WORKING_DIRECTORY "${CMAKE_BINARY_DIR}/dist"
            COMMENT "Creating zip archive"
        )

        add_dependencies(package_slick_object_pool dist_slick_object_pool)
    endif()
endif()
