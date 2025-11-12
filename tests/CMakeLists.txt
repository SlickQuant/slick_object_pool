find_package(GTest QUIET)

enable_testing()

if(NOT GTest_FOUND)
    include(FetchContent)
    FetchContent_Declare(
        googletest
        GIT_REPOSITORY https://github.com/google/googletest.git
        GIT_TAG v1.17.0
    )
    # For Windows: Prevent overriding the parent project's compiler/linker settings
    set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
    FetchContent_MakeAvailable(googletest)
endif()

add_executable(slick_object_pool_tests tests.cpp)

# Fix MSB8028 warning: Set unique intermediate directory for MSVC
if(MSVC)
    set_target_properties(slick_object_pool_tests PROPERTIES
        ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/lib"
        LIBRARY_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/lib"
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/bin"
    )
    # Set unique intermediate directory
    set_target_properties(slick_object_pool_tests PROPERTIES
        COMPILE_PDB_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/pdb"
    )
endif()

# Link with slick_object_pool library
target_link_libraries(slick_object_pool_tests PRIVATE
    slick_object_pool
    GTest::gtest
    GTest::gtest_main
)

# Add threading support
find_package(Threads REQUIRED)
target_link_libraries(slick_object_pool_tests PRIVATE Threads::Threads)

# Platform-specific libraries
if(UNIX AND NOT APPLE)
    target_link_libraries(slick_object_pool_tests PRIVATE rt atomic)
endif()

# AddressSanitizer support
option(ENABLE_ASAN "Enable AddressSanitizer" OFF)
option(ENABLE_UBSAN "Enable UndefinedBehaviorSanitizer" OFF)
option(ENABLE_TSAN "Enable ThreadSanitizer" OFF)

if(ENABLE_ASAN)
    message(STATUS "AddressSanitizer enabled")
    if(MSVC)
        # MSVC AddressSanitizer (requires Visual Studio 2022 17.7+)
        target_compile_options(slick_object_pool_tests PRIVATE /fsanitize=address)
        target_link_options(slick_object_pool_tests PRIVATE /INCREMENTAL:NO)

        # Copy ASAN runtime DLL to output directory (like GTest DLLs)
        # Find the ASAN DLL in the MSVC toolchain
        if(DEFINED ENV{VCToolsInstallDir})
            file(TO_CMAKE_PATH "$ENV{VCToolsInstallDir}" VC_TOOLS_DIR)
            set(ASAN_DLL_DIR "${VC_TOOLS_DIR}/bin/Hostx64/x64")
            set(ASAN_DLL_PATH "${ASAN_DLL_DIR}/clang_rt.asan_dynamic-x86_64.dll")

            if(EXISTS "${ASAN_DLL_PATH}")
                message(STATUS "Found ASAN DLL: ${ASAN_DLL_PATH}")
                add_custom_command(TARGET slick_object_pool_tests POST_BUILD
                    COMMAND ${CMAKE_COMMAND} -E copy_if_different
                        "${ASAN_DLL_PATH}"
                        "$<TARGET_FILE_DIR:slick_object_pool_tests>"
                    COMMENT "Copying ASAN runtime DLL to output directory"
                )
            else()
                message(WARNING "ASAN DLL not found at: ${ASAN_DLL_PATH}")
            endif()
        else()
            message(WARNING "VCToolsInstallDir not set. ASAN DLL won't be auto-copied.")
        endif()
    else()
        # GCC/Clang AddressSanitizer
        target_compile_options(slick_object_pool_tests PRIVATE
            -fsanitize=address
            -fno-omit-frame-pointer
            -g
        )
        target_link_options(slick_object_pool_tests PRIVATE
            -fsanitize=address
        )
    endif()
endif()

if(ENABLE_UBSAN)
    if(NOT MSVC)
        # UndefinedBehaviorSanitizer (GCC/Clang only)
        target_compile_options(slick_object_pool_tests PRIVATE
            -fsanitize=undefined
            -fno-omit-frame-pointer
            -g
        )
        target_link_options(slick_object_pool_tests PRIVATE
            -fsanitize=undefined
        )
        message(STATUS "UndefinedBehaviorSanitizer enabled")
    else()
        message(WARNING "UndefinedBehaviorSanitizer not supported on MSVC")
    endif()
endif()

if(ENABLE_TSAN)
    if(ENABLE_ASAN)
        message(FATAL_ERROR "Cannot enable both AddressSanitizer and ThreadSanitizer")
    endif()
    if(NOT MSVC)
        # ThreadSanitizer (GCC/Clang only)
        target_compile_options(slick_object_pool_tests PRIVATE
            -fsanitize=thread
            -fno-omit-frame-pointer
            -g
        )
        target_link_options(slick_object_pool_tests PRIVATE
            -fsanitize=thread
        )
        message(STATUS "ThreadSanitizer enabled")
    else()
        message(WARNING "ThreadSanitizer not supported on MSVC")
    endif()
endif()

# Test registration
if(MSVC AND ENABLE_ASAN)
    # Manual test registration for MSVC + ASan (GTest discovery fails due to interception)
    add_test(NAME slick_object_pool_tests COMMAND slick_object_pool_tests)
    # Set environment variable to continue on interception failure
    set_tests_properties(slick_object_pool_tests PROPERTIES
        ENVIRONMENT "ASAN_WIN_CONTINUE_ON_INTERCEPTION_FAILURE=1"
    )
else()
    # Normal GTest discovery for other configurations
    include(GoogleTest)
    gtest_discover_tests(slick_object_pool_tests)
endif()

