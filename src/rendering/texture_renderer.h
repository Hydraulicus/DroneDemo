#pragma once

/**
 * @file texture_renderer.h
 * @brief OpenGL texture renderer for video frames
 *
 * Renders video frames as full-screen textured quads.
 * Uses OpenGL 2.1 fixed-function pipeline for simplicity.
 */

#include <cstdint>
#include <vector>

namespace robot_vision {

/**
 * Simple texture renderer for video frames
 *
 * TEACHING: OpenGL Fixed-Function Pipeline
 * -----------------------------------------
 * We use OpenGL 2.1's fixed-function pipeline (glBegin/glEnd)
 * instead of modern shaders. This is:
 * - Simpler to understand
 * - Compatible with more hardware
 * - Good enough for our needs
 *
 * Modern OpenGL (3.3+) requires shaders for everything,
 * but NanoVG handles that complexity for us in Phase 3.
 */
class TextureRenderer {
public:
    TextureRenderer();
    ~TextureRenderer();

    // Non-copyable
    TextureRenderer(const TextureRenderer&) = delete;
    TextureRenderer& operator=(const TextureRenderer&) = delete;

    /**
     * Initialize the renderer
     *
     * @param width Video frame width
     * @param height Video frame height
     * @return true on success
     *
     * Creates OpenGL texture with initial dimensions.
     */
    bool initialize(int width, int height);

    /**
     * Update texture with new frame data
     *
     * @param pixels RGB pixel data
     * @param width Frame width
     * @param height Frame height
     *
     * Uploads new pixel data to GPU texture.
     */
    void updateTexture(const std::vector<uint8_t>& pixels, int width, int height);

    /**
     * Render the texture to fill the viewport
     *
     * @param viewport_width Current viewport width
     * @param viewport_height Current viewport height
     *
     * Renders a textured quad that fills the screen.
     * Maintains aspect ratio with letterboxing if needed.
     */
    void render(int viewport_width, int viewport_height);

    /**
     * Cleanup OpenGL resources
     */
    void shutdown();

private:
    unsigned int texture_id_ = 0;  // OpenGL texture ID
    int texture_width_ = 0;
    int texture_height_ = 0;
    bool initialized_ = false;
};

} // namespace robot_vision
