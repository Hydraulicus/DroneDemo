/**
 * @file texture_renderer.cpp
 * @brief OpenGL texture renderer implementation
 */

#include "texture_renderer.h"
#include "core/opengl.h"

#include <iostream>
#include <algorithm>

namespace robot_vision {

TextureRenderer::TextureRenderer() = default;

TextureRenderer::~TextureRenderer() {
    shutdown();
}

bool TextureRenderer::initialize(int width, int height) {
    if (initialized_) {
        return true;
    }

    /**
     * TEACHING: OpenGL Texture Creation
     * ----------------------------------
     * Textures are images stored on the GPU.
     * 1. glGenTextures() creates texture ID(s)
     * 2. glBindTexture() makes it the "current" texture
     * 3. glTexImage2D() allocates storage and optionally uploads data
     * 4. glTexParameteri() sets filtering/wrapping modes
     */

    glGenTextures(1, &texture_id_);
    glBindTexture(GL_TEXTURE_2D, texture_id_);

    // Set texture parameters
    // GL_LINEAR = smooth scaling (bilinear interpolation)
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    // GL_CLAMP_TO_EDGE = don't repeat at edges
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    // Allocate texture storage (no initial data)
    glTexImage2D(
        GL_TEXTURE_2D,      // Target
        0,                   // Mipmap level
        GL_RGB,              // Internal format
        width, height,       // Dimensions
        0,                   // Border (must be 0)
        GL_RGB,              // Format of input data
        GL_UNSIGNED_BYTE,    // Data type
        nullptr              // No initial data
    );

    texture_width_ = width;
    texture_height_ = height;
    initialized_ = true;

    std::cout << "  Texture renderer initialized: " << width << "x" << height << "\n";
    return true;
}

void TextureRenderer::updateTexture(const std::vector<uint8_t>& pixels, int width, int height) {
    if (!initialized_ || pixels.empty()) {
        return;
    }

    glBindTexture(GL_TEXTURE_2D, texture_id_);

    /**
     * TEACHING: glTexSubImage2D vs glTexImage2D
     * ------------------------------------------
     * - glTexImage2D: Allocates new storage AND uploads data
     * - glTexSubImage2D: Only uploads data (faster if size unchanged)
     *
     * We use glTexSubImage2D if dimensions match, otherwise reallocate.
     */

    if (width == texture_width_ && height == texture_height_) {
        // Same size - just update data (faster)
        glTexSubImage2D(
            GL_TEXTURE_2D,
            0,                   // Mipmap level
            0, 0,                // Offset (x, y)
            width, height,       // Size
            GL_RGB,              // Format
            GL_UNSIGNED_BYTE,    // Data type
            pixels.data()        // Pixel data
        );
    } else {
        // Size changed - reallocate
        glTexImage2D(
            GL_TEXTURE_2D,
            0,
            GL_RGB,
            width, height,
            0,
            GL_RGB,
            GL_UNSIGNED_BYTE,
            pixels.data()
        );
        texture_width_ = width;
        texture_height_ = height;
    }
}

void TextureRenderer::render(int viewport_width, int viewport_height) {
    if (!initialized_) {
        return;
    }

    /**
     * TEACHING: Aspect Ratio Preservation
     * ------------------------------------
     * If window aspect ratio differs from video, we add letterboxing
     * (black bars) to avoid stretching the image.
     */

    // Calculate aspect ratios
    float video_aspect = static_cast<float>(texture_width_) / texture_height_;
    float window_aspect = static_cast<float>(viewport_width) / viewport_height;

    float render_width, render_height;
    float x_offset = 0, y_offset = 0;

    if (video_aspect > window_aspect) {
        // Video is wider than window - letterbox top/bottom
        render_width = static_cast<float>(viewport_width);
        render_height = viewport_width / video_aspect;
        y_offset = (viewport_height - render_height) / 2.0f;
    } else {
        // Video is taller than window - letterbox left/right
        render_height = static_cast<float>(viewport_height);
        render_width = viewport_height * video_aspect;
        x_offset = (viewport_width - render_width) / 2.0f;
    }

    // Setup OpenGL state
    glViewport(0, 0, viewport_width, viewport_height);

    // Clear to black (for letterbox areas)
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);

    // Setup 2D orthographic projection
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glOrtho(0, viewport_width, viewport_height, 0, -1, 1);  // Top-left origin

    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();

    // Enable texturing
    glEnable(GL_TEXTURE_2D);
    glBindTexture(GL_TEXTURE_2D, texture_id_);

    /**
     * TEACHING: Textured Quad Rendering
     * ----------------------------------
     * We draw a rectangle (quad) with texture coordinates.
     * Texture coordinates go from (0,0) to (1,1):
     * - (0,0) = top-left of texture
     * - (1,1) = bottom-right of texture
     *
     * glBegin/glEnd is deprecated in modern OpenGL but works in 2.1.
     */

    glBegin(GL_QUADS);
    glColor3f(1.0f, 1.0f, 1.0f);  // White (no tinting)

    // Top-left
    glTexCoord2f(0.0f, 0.0f);
    glVertex2f(x_offset, y_offset);

    // Top-right
    glTexCoord2f(1.0f, 0.0f);
    glVertex2f(x_offset + render_width, y_offset);

    // Bottom-right
    glTexCoord2f(1.0f, 1.0f);
    glVertex2f(x_offset + render_width, y_offset + render_height);

    // Bottom-left
    glTexCoord2f(0.0f, 1.0f);
    glVertex2f(x_offset, y_offset + render_height);

    glEnd();

    glDisable(GL_TEXTURE_2D);
}

void TextureRenderer::shutdown() {
    if (texture_id_ != 0) {
        glDeleteTextures(1, &texture_id_);
        texture_id_ = 0;
        initialized_ = false;
    }
}

} // namespace robot_vision
