#pragma once

/**
 * @file opengl.h
 * @brief Platform-agnostic OpenGL includes
 *
 * This header provides a unified way to include OpenGL headers
 * across different platforms (macOS, Linux/Jetson).
 *
 * TEACHING: Why This Header Exists
 * ---------------------------------
 * Different platforms have OpenGL headers in different locations:
 * - macOS: <OpenGL/gl.h> with deprecation warnings to silence
 * - Linux: <GL/gl.h>
 * - Jetson: <GLES2/gl2.h> for OpenGL ES
 *
 * By centralizing this in one header, we:
 * 1. Avoid code duplication (DRY principle)
 * 2. Make platform changes easier (single point of modification)
 * 3. Ensure consistent behavior across the codebase
 *
 * Usage:
 *   #include "core/opengl.h"
 *   // Now you can use glXxx() functions
 */

#ifdef PLATFORM_MACOS
    // Silence deprecation warnings for OpenGL on macOS
    // Apple deprecated OpenGL in favor of Metal, but it still works
    #define GL_SILENCE_DEPRECATION
    #include <OpenGL/gl.h>
#elif defined(PLATFORM_JETSON)
    // Jetson Nano uses OpenGL ES 2.0
    #include <GLES2/gl2.h>
#else
    // Standard Linux OpenGL
    #include <GL/gl.h>
#endif
