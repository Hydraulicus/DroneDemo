cmake_minimum_required(VERSION 3.15)
project(RobotVisionDemo
    VERSION 1.0.0
    DESCRIPTION "Cross-platform robot vision application"
    LANGUAGES CXX
)

# ============================================================================
# C++ Standard Configuration
# ============================================================================
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Export compile_commands.json for IDE support
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# ============================================================================
# Platform Detection
# ============================================================================
if(APPLE)
    set(PLATFORM_NAME "macos")
    set(PLATFORM_MACOS TRUE)
    add_compile_definitions(PLATFORM_MACOS=1)
    message(STATUS "Platform: macOS")
elseif(UNIX AND NOT APPLE)
    if(CMAKE_SYSTEM_PROCESSOR MATCHES "aarch64")
        set(PLATFORM_NAME "jetson")
        set(PLATFORM_JETSON TRUE)
        add_compile_definitions(PLATFORM_JETSON=1)
        message(STATUS "Platform: Jetson (ARM64 Linux)")
    else()
        set(PLATFORM_NAME "linux")
        set(PLATFORM_LINUX TRUE)
        add_compile_definitions(PLATFORM_LINUX=1)
        message(STATUS "Platform: Linux (x86_64)")
    endif()
else()
    message(FATAL_ERROR "Unsupported platform")
endif()

# ============================================================================
# Compiler Warnings (strict for quality code)
# ============================================================================
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang|AppleClang")
    add_compile_options(
        -Wall           # All warnings
        -Wextra         # Extra warnings
        -Wpedantic      # Strict ISO C++
        -Wno-unused-parameter  # Allow unused params (common in interfaces)
    )
endif()

# ============================================================================
# Build Type Configuration
# ============================================================================
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE "Debug" CACHE STRING "Build type" FORCE)
endif()

if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    add_compile_definitions(DEBUG=1)
    message(STATUS "Build type: Debug")
else()
    add_compile_definitions(NDEBUG=1)
    message(STATUS "Build type: ${CMAKE_BUILD_TYPE}")
endif()

# ============================================================================
# Find Dependencies
# ============================================================================
find_package(PkgConfig REQUIRED)

# GStreamer
pkg_check_modules(GSTREAMER REQUIRED gstreamer-1.0)
pkg_check_modules(GSTREAMER_APP REQUIRED gstreamer-app-1.0)
pkg_check_modules(GSTREAMER_VIDEO REQUIRED gstreamer-video-1.0)

message(STATUS "GStreamer version: ${GSTREAMER_VERSION}")
message(STATUS "GStreamer include: ${GSTREAMER_INCLUDE_DIRS}")

# GLFW (for window creation)
pkg_check_modules(GLFW REQUIRED glfw3)
message(STATUS "GLFW version: ${GLFW_VERSION}")

# OpenGL
if(PLATFORM_MACOS)
    find_package(OpenGL REQUIRED)
    message(STATUS "OpenGL found: ${OPENGL_LIBRARIES}")
else()
    # On Jetson/Linux, we'll use OpenGL ES
    # For now, just find standard OpenGL
    find_package(OpenGL REQUIRED)
endif()

# nlohmann/json (optional - for config files)
find_package(nlohmann_json QUIET)
if(nlohmann_json_FOUND)
    message(STATUS "nlohmann_json found")
    set(HAS_JSON TRUE)
    add_compile_definitions(HAS_JSON=1)
else()
    message(STATUS "nlohmann_json not found - config features disabled")
    set(HAS_JSON FALSE)
endif()

# ============================================================================
# Source Files
# ============================================================================

# Core sources (platform-independent)
set(CORE_SOURCES
    src/main.cpp
)

# Platform-specific sources
if(PLATFORM_MACOS)
    set(PLATFORM_SOURCES
        src/platform/platform_macos.cpp
    )
elseif(PLATFORM_JETSON OR PLATFORM_LINUX)
    set(PLATFORM_SOURCES
        src/platform/platform_linux.cpp
    )
endif()

# Utility sources
set(UTIL_SOURCES
    # Will add logger, config_parser, etc. later
)

# All sources
set(ALL_SOURCES
    ${CORE_SOURCES}
    ${PLATFORM_SOURCES}
    ${UTIL_SOURCES}
)

# ============================================================================
# Main Executable
# ============================================================================
add_executable(robot_vision ${ALL_SOURCES})

# Include directories
target_include_directories(robot_vision PRIVATE
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/third_party
    ${GSTREAMER_INCLUDE_DIRS}
    ${GSTREAMER_APP_INCLUDE_DIRS}
    ${GSTREAMER_VIDEO_INCLUDE_DIRS}
    ${GLFW_INCLUDE_DIRS}
)

# Link libraries
target_link_libraries(robot_vision PRIVATE
    ${GSTREAMER_LIBRARIES}
    ${GSTREAMER_APP_LIBRARIES}
    ${GSTREAMER_VIDEO_LIBRARIES}
    ${GLFW_LIBRARIES}
    ${OPENGL_LIBRARIES}
)

# Add library directories (needed for GStreamer on some systems)
target_link_directories(robot_vision PRIVATE
    ${GSTREAMER_LIBRARY_DIRS}
    ${GLFW_LIBRARY_DIRS}
)

# macOS-specific frameworks
if(PLATFORM_MACOS)
    target_link_libraries(robot_vision PRIVATE
        "-framework Cocoa"
        "-framework IOKit"
        "-framework CoreVideo"
    )
endif()

# nlohmann_json if available
if(HAS_JSON)
    target_link_libraries(robot_vision PRIVATE nlohmann_json::nlohmann_json)
endif()

# ============================================================================
# Installation (optional)
# ============================================================================
install(TARGETS robot_vision DESTINATION bin)

# ============================================================================
# Summary
# ============================================================================
message(STATUS "")
message(STATUS "=== Build Configuration ===")
message(STATUS "Platform:     ${PLATFORM_NAME}")
message(STATUS "Build type:   ${CMAKE_BUILD_TYPE}")
message(STATUS "C++ Standard: ${CMAKE_CXX_STANDARD}")
message(STATUS "GStreamer:    ${GSTREAMER_VERSION}")
message(STATUS "GLFW:         ${GLFW_VERSION}")
message(STATUS "JSON support: ${HAS_JSON}")
message(STATUS "===========================")
message(STATUS "")
