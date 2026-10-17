# First check if GoogleTest is pre-installed locally
find_package(GTest QUIET)

if(NOT GTest_FOUND)
    message(STATUS "Pre-installed GoogleTest not found. Attempting to fetch pinned release via FetchContent...")
    include(FetchContent)
    FetchContent_Declare(
        googletest
        URL https://github.com/google/googletest/archive/refs/tags/release-1.12.1.tar.gz
        URL_HASH SHA256=81964fe578e9bd7c94dfdb09c8e4d6e67597372ee404c471ff76038466b0338f
    )
    # Prevent overriding parent compiler/linker settings on Windows
    set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
    set(BUILD_GMOCK OFF CACHE BOOL "" FORCE)
    set(INSTALL_GTEST OFF CACHE BOOL "" FORCE)

    FetchContent_MakeAvailable(googletest)

    if(NOT TARGET GTest::gtest AND TARGET gtest)
        add_library(GTest::gtest ALIAS gtest)
    endif()
    if(NOT TARGET GTest::gtest_main AND TARGET gtest_main)
        add_library(GTest::gtest_main ALIAS gtest_main)
    endif()
endif()

if(NOT TARGET GTest::gtest AND NOT TARGET gtest)
    message(FATAL_ERROR
        "GoogleTest is required to build the test suite, but was neither found locally nor fetched via network.\n"
        "To resolve:\n"
        "  - On Ubuntu/Debian: sudo apt-get install -y libgtest-dev\n"
        "  - On macOS: brew install googletest\n"
        "  - On Windows: vcpkg install gtest:x64-windows\n"
        "  - Or enable internet access during CMake configure to download the pinned release.\n"
        "  - Or build only the application without tests: cmake -S . -B build -DBUILD_TESTING=OFF\n"
    )
endif()

include(GoogleTest)

macro(add_loglens_test TEST_NAME SOURCE_FILE)
    add_executable(${TEST_NAME} ${SOURCE_FILE})
    target_link_libraries(${TEST_NAME} PRIVATE
        loglens_core
        GTest::gtest
        GTest::gtest_main
    )
    if(NOT MSVC)
        target_link_libraries(${TEST_NAME} PRIVATE pthread dl)
    endif()
    gtest_discover_tests(${TEST_NAME})
endmacro()

add_loglens_test(test_parser test_parser.cpp)
add_loglens_test(test_statistics test_statistics.cpp)
add_loglens_test(test_incident_detector test_incident_detector.cpp)
add_loglens_test(test_root_cause test_root_cause.cpp)
add_loglens_test(test_sla test_sla.cpp)
add_loglens_test(test_database test_database.cpp)
add_loglens_test(test_cli test_cli.cpp)
add_loglens_test(test_integration test_integration.cpp)
