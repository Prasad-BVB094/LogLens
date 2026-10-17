cmake_minimum_required(VERSION 3.16)
project(LogLens VERSION 1.0.0 LANGUAGES CXX C)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Export compile commands for language servers
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Compiler Warnings Configuration
if(MSVC)
    add_compile_options(/W4 /permissive- /EHsc)
else()
    add_compile_options(-Wall -Wextra -Wpedantic -Wshadow -Wconversion -Wnon-virtual-dtor)
endif()

# Sanitizer Support Option
option(ENABLE_SANITIZERS "Enable Address and Undefined Behavior Sanitizers" OFF)
if(ENABLE_SANITIZERS AND NOT MSVC)
    message(STATUS "Enabling Address and Undefined Behavior Sanitizers")
    add_compile_options(-fsanitize=address,undefined -fno-omit-frame-pointer)
    add_link_options(-fsanitize=address,undefined)
endif()

# Find SQLite3
find_package(SQLite3 REQUIRED)

# Common library for LogLens core logic
add_library(loglens_core STATIC
    src/Timestamp.cpp
    src/LogParser.cpp
    src/StatisticsAnalyzer.cpp
    src/IncidentDetector.cpp
    src/RootCauseEngine.cpp
    src/SLAAnalyzer.cpp
    src/DatabaseManager.cpp
    src/ReportGenerator.cpp
    src/ArgumentParser.cpp
    src/Application.cpp
)

target_include_directories(loglens_core PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)

target_link_libraries(loglens_core PUBLIC
    SQLite3::SQLite3
)

if(UNIX)
    target_link_libraries(loglens_core PUBLIC pthread dl)
endif()

# Main CLI Executable
add_executable(loglens src/main.cpp)
target_link_libraries(loglens PRIVATE loglens_core)

# Testing configuration
option(BUILD_TESTING "Build the testing tree." ON)
if(BUILD_TESTING)
    enable_testing()
    add_subdirectory(tests)
endif()
