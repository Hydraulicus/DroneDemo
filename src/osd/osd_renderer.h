#pragma once

/**
 * @file osd_renderer.h
 * @brief NanoVG-based OSD renderer implementation
 */

#include "core/osd.h"

// Forward declaration to avoid including NanoVG in header
struct NVGcontext;

namespace robot_vision {

/**
 * NanoVG OSD renderer implementation
 *
 * TEACHING: NanoVG Font Handling
 * ------------------------------
 * NanoVG loads fonts once and references them by ID (integer).
 * We store font IDs after loading to avoid repeated lookups.
 * Font loading must happen after OpenGL context is created.
 */
class OSDRenderer : public IOSD {
public:
    OSDRenderer();
    ~OSDRenderer() override;

    // Non-copyable
    OSDRenderer(const OSDRenderer&) = delete;
    OSDRenderer& operator=(const OSDRenderer&) = delete;

    // ========================================================================
    // IOSD Implementation
    // ========================================================================

    bool initialize(const OSDConfig& config) override;
    void shutdown() override;
    bool isInitialized() const override;

    void beginFrame(int width, int height, float device_pixel_ratio) override;
    void endFrame() override;

    void drawText(float x, float y, const std::string& text,
                  Color color, float size, TextAlign align) override;

    void drawTextWithBackground(float x, float y, const std::string& text,
                                Color text_color, Color bg_color,
                                float padding, float size) override;

    void drawRect(float x, float y, float width, float height, Color color) override;
    void drawRectOutline(float x, float y, float width, float height,
                         Color color, float stroke_width) override;
    void drawRoundedRect(float x, float y, float width, float height,
                         float radius, Color color) override;
    void drawLine(float x1, float y1, float x2, float y2,
                  Color color, float width) override;
    void drawCircle(float cx, float cy, float radius, Color color, bool filled) override;

    void drawFPS(float fps, int width) override;
    void drawTimestamp(float x, float y) override;
    void drawFrameCounter(uint32_t frame_number, float x, float y) override;

private:
    /**
     * Load a font from file
     * @return Font ID or -1 on failure
     */
    int loadFont(const std::string& name, const std::string& path);

    /**
     * Set current font and size
     */
    void setFont(float size);

    /**
     * Get current timestamp string
     */
    std::string getCurrentTimestamp() const;

private:
    NVGcontext* vg_ = nullptr;       // NanoVG context (owned, created in initialize)
    bool initialized_ = false;
    bool owns_context_ = false;      // True if we created the context

    // Font IDs
    int font_regular_ = -1;
    int font_bold_ = -1;

    // Configuration
    float default_font_size_ = 18.0f;

    // Frame state
    bool in_frame_ = false;
};

} // namespace robot_vision
