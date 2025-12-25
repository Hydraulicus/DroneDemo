#pragma once

/**
 * @file osd.h
 * @brief On-Screen Display (OSD) interface
 *
 * Abstracts OSD rendering using NanoVG for vector graphics overlay.
 * Platform-specific graphics context creation is handled by IPlatform.
 *
 * TEACHING: OSD in Robotics
 * -------------------------
 * OSD overlays display real-time information on video:
 * - Telemetry data (speed, position, orientation)
 * - System status (FPS, CPU usage, battery)
 * - Debug information (detection boxes, markers)
 * - User interface elements
 *
 * The key challenge is rendering fast enough to not drop frames.
 * NanoVG provides hardware-accelerated vector graphics for this.
 */

#include <memory>
#include <string>

namespace robot_vision {

/**
 * OSD text alignment options
 */
enum class TextAlign {
    Left,
    Center,
    Right
};

/**
 * RGBA color structure
 */
struct Color {
    float r = 1.0f;
    float g = 1.0f;
    float b = 1.0f;
    float a = 1.0f;

    // Convenience constructors
    static Color white() { return {1.0f, 1.0f, 1.0f, 1.0f}; }
    static Color black() { return {0.0f, 0.0f, 0.0f, 1.0f}; }
    static Color red() { return {1.0f, 0.0f, 0.0f, 1.0f}; }
    static Color green() { return {0.0f, 1.0f, 0.0f, 1.0f}; }
    static Color blue() { return {0.0f, 0.0f, 1.0f, 1.0f}; }
    static Color yellow() { return {1.0f, 1.0f, 0.0f, 1.0f}; }
    static Color cyan() { return {0.0f, 1.0f, 1.0f, 1.0f}; }
    static Color transparent(float alpha = 0.5f) { return {0.0f, 0.0f, 0.0f, alpha}; }
};

/**
 * OSD configuration
 */
struct OSDConfig {
    std::string font_path;           // Path to TTF font file
    std::string font_bold_path;      // Path to bold TTF font (optional)
    float default_font_size = 18.0f; // Default font size in pixels
};

/**
 * OSD renderer interface
 *
 * TEACHING: NanoVG Rendering Model
 * --------------------------------
 * NanoVG uses a "frame" model:
 * 1. beginFrame() - Start a new frame
 * 2. Draw calls (text, shapes, etc.)
 * 3. endFrame() - Finish and flush to GPU
 *
 * All coordinates are in pixels, origin at top-left.
 * Y increases downward (standard screen coordinates).
 */
class IOSD {
public:
    virtual ~IOSD() = default;

    // ========================================================================
    // Lifecycle
    // ========================================================================

    /**
     * Initialize OSD renderer
     *
     * @param config OSD configuration (fonts, sizes)
     * @return true on success
     *
     * Must be called after OpenGL context is created (window initialized).
     * Creates NanoVG context internally based on platform.
     */
    virtual bool initialize(const OSDConfig& config) = 0;

    /**
     * Shutdown and release resources
     */
    virtual void shutdown() = 0;

    /**
     * Check if OSD is initialized
     */
    virtual bool isInitialized() const = 0;

    // ========================================================================
    // Frame Management
    // ========================================================================

    /**
     * Begin OSD rendering frame
     *
     * @param width Framebuffer width in pixels
     * @param height Framebuffer height in pixels
     * @param device_pixel_ratio Pixel ratio (1.0 for normal, 2.0 for Retina)
     *
     * Must be called before any drawing operations.
     */
    virtual void beginFrame(int width, int height, float device_pixel_ratio = 1.0f) = 0;

    /**
     * End OSD rendering frame
     *
     * Flushes all pending draw calls to GPU.
     * Must be called after all drawing operations.
     */
    virtual void endFrame() = 0;

    // ========================================================================
    // Text Rendering
    // ========================================================================

    /**
     * Draw text at position
     *
     * @param x X position
     * @param y Y position (baseline)
     * @param text Text to draw
     * @param color Text color
     * @param size Font size (0 = use default)
     * @param align Text alignment
     */
    virtual void drawText(float x, float y, const std::string& text,
                          Color color = Color::white(),
                          float size = 0,
                          TextAlign align = TextAlign::Left) = 0;

    /**
     * Draw text with background box
     *
     * @param x X position
     * @param y Y position
     * @param text Text to draw
     * @param text_color Text color
     * @param bg_color Background color
     * @param padding Padding around text
     * @param size Font size (0 = use default)
     */
    virtual void drawTextWithBackground(float x, float y, const std::string& text,
                                        Color text_color = Color::white(),
                                        Color bg_color = Color::transparent(0.7f),
                                        float padding = 4.0f,
                                        float size = 0) = 0;

    // ========================================================================
    // Shape Rendering
    // ========================================================================

    /**
     * Draw filled rectangle
     */
    virtual void drawRect(float x, float y, float width, float height,
                          Color color) = 0;

    /**
     * Draw rectangle outline
     */
    virtual void drawRectOutline(float x, float y, float width, float height,
                                 Color color, float stroke_width = 1.0f) = 0;

    /**
     * Draw rounded rectangle
     */
    virtual void drawRoundedRect(float x, float y, float width, float height,
                                 float radius, Color color) = 0;

    /**
     * Draw line
     */
    virtual void drawLine(float x1, float y1, float x2, float y2,
                          Color color, float width = 1.0f) = 0;

    /**
     * Draw circle
     */
    virtual void drawCircle(float cx, float cy, float radius,
                            Color color, bool filled = true) = 0;

    // ========================================================================
    // Convenience Methods
    // ========================================================================

    /**
     * Draw FPS counter at top-right corner
     */
    virtual void drawFPS(float fps, int width) = 0;

    /**
     * Draw timestamp at specified position
     */
    virtual void drawTimestamp(float x, float y) = 0;

    /**
     * Draw frame counter
     */
    virtual void drawFrameCounter(uint32_t frame_number, float x, float y) = 0;
};

// ============================================================================
// Factory Function
// ============================================================================

/**
 * Create OSD renderer instance
 */
std::unique_ptr<IOSD> createOSD();

} // namespace robot_vision
