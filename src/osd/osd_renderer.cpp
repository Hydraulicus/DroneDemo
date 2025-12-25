/**
 * @file osd_renderer.cpp
 * @brief NanoVG-based OSD renderer implementation
 */

#include "osd_renderer.h"

/**
 * TEACHING: NanoVG Backend Selection
 * -----------------------------------
 * NanoVG supports multiple OpenGL backends:
 * - NANOVG_GL2_IMPLEMENTATION: OpenGL 2.1 (macOS)
 * - NANOVG_GL3_IMPLEMENTATION: OpenGL 3.x
 * - NANOVG_GLES2_IMPLEMENTATION: OpenGL ES 2.0 (Jetson)
 * - NANOVG_GLES3_IMPLEMENTATION: OpenGL ES 3.0
 *
 * We select based on platform define from CMake.
 * The implementation is included ONCE in this file.
 */

#include "core/opengl.h"

#ifdef PLATFORM_MACOS
    #define NANOVG_GL2_IMPLEMENTATION
#else
    #define NANOVG_GLES2_IMPLEMENTATION
#endif

#include <nanovg.h>
#include <nanovg_gl.h>

#include <iostream>
#include <chrono>
#include <iomanip>
#include <sstream>

namespace robot_vision {

// ============================================================================
// Constructor / Destructor
// ============================================================================

OSDRenderer::OSDRenderer() = default;

OSDRenderer::~OSDRenderer() {
    shutdown();
}

// ============================================================================
// Lifecycle
// ============================================================================

bool OSDRenderer::initialize(const OSDConfig& config) {
    if (initialized_) {
        std::cerr << "  OSD already initialized\n";
        return false;
    }

    /**
     * TEACHING: Creating NanoVG Context
     * ----------------------------------
     * NanoVG needs an OpenGL context to be current before creation.
     * The window must be initialized first!
     *
     * Flags:
     * - NVG_ANTIALIAS: Enable anti-aliasing for smooth edges
     * - NVG_STENCIL_STROKES: Use stencil buffer for accurate strokes
     *   (requires stencil buffer in framebuffer)
     */
#ifdef PLATFORM_MACOS
    vg_ = nvgCreateGL2(NVG_ANTIALIAS | NVG_STENCIL_STROKES);
#else
    vg_ = nvgCreateGLES2(NVG_ANTIALIAS | NVG_STENCIL_STROKES);
#endif

    if (!vg_) {
        std::cerr << "  ERROR: Failed to create NanoVG context\n";
        return false;
    }

    owns_context_ = true;
    default_font_size_ = config.default_font_size;

    // Load fonts
    font_regular_ = loadFont("regular", config.font_path);
    if (font_regular_ == -1) {
        std::cerr << "  ERROR: Failed to load regular font: " << config.font_path << "\n";
        return false;
    }

    // Bold font is optional
    if (!config.font_bold_path.empty()) {
        font_bold_ = loadFont("bold", config.font_bold_path);
        if (font_bold_ == -1) {
            std::cout << "  Warning: Failed to load bold font, using regular\n";
            font_bold_ = font_regular_;
        }
    } else {
        font_bold_ = font_regular_;
    }

    initialized_ = true;
    std::cout << "  OSD renderer initialized\n";
    return true;
}

void OSDRenderer::shutdown() {
    if (vg_ && owns_context_) {
        /**
         * TEACHING: Destroying NanoVG Context
         * ------------------------------------
         * Must use the matching destroy function for the create function:
         * - nvgCreateGL2 -> nvgDeleteGL2
         * - nvgCreateGLES2 -> nvgDeleteGLES2
         */
#ifdef PLATFORM_MACOS
        nvgDeleteGL2(vg_);
#else
        nvgDeleteGLES2(vg_);
#endif
    }

    vg_ = nullptr;
    owns_context_ = false;
    initialized_ = false;
    font_regular_ = -1;
    font_bold_ = -1;
}

bool OSDRenderer::isInitialized() const {
    return initialized_;
}

// ============================================================================
// Frame Management
// ============================================================================

void OSDRenderer::beginFrame(int width, int height, float device_pixel_ratio) {
    if (!initialized_ || in_frame_) {
        return;
    }

    /**
     * TEACHING: NanoVG Frame Setup
     * ----------------------------
     * nvgBeginFrame() sets up the viewport and clears internal state.
     * Parameters:
     * - width/height: Framebuffer size in pixels
     * - devicePixelRatio: For Retina displays (2.0), coordinates stay same
     *   but rendering happens at higher resolution.
     */
    nvgBeginFrame(vg_, static_cast<float>(width), static_cast<float>(height), device_pixel_ratio);
    in_frame_ = true;
}

void OSDRenderer::endFrame() {
    if (!in_frame_) {
        return;
    }

    nvgEndFrame(vg_);
    in_frame_ = false;
}

// ============================================================================
// Text Rendering
// ============================================================================

void OSDRenderer::drawText(float x, float y, const std::string& text,
                           Color color, float size, TextAlign align) {
    if (!in_frame_) return;

    setFont(size > 0 ? size : default_font_size_);

    // Set alignment
    int nvg_align = NVG_ALIGN_TOP;  // Vertical: top
    switch (align) {
        case TextAlign::Left:   nvg_align |= NVG_ALIGN_LEFT; break;
        case TextAlign::Center: nvg_align |= NVG_ALIGN_CENTER; break;
        case TextAlign::Right:  nvg_align |= NVG_ALIGN_RIGHT; break;
    }
    nvgTextAlign(vg_, nvg_align);

    // Set color and draw
    nvgFillColor(vg_, nvgRGBAf(color.r, color.g, color.b, color.a));
    nvgText(vg_, x, y, text.c_str(), nullptr);
}

void OSDRenderer::drawTextWithBackground(float x, float y, const std::string& text,
                                         Color text_color, Color bg_color,
                                         float padding, float size) {
    if (!in_frame_) return;

    float font_size = size > 0 ? size : default_font_size_;
    setFont(font_size);

    // Measure text bounds
    float bounds[4];
    nvgTextAlign(vg_, NVG_ALIGN_LEFT | NVG_ALIGN_TOP);
    nvgTextBounds(vg_, x, y, text.c_str(), nullptr, bounds);

    // Draw background
    float bg_x = bounds[0] - padding;
    float bg_y = bounds[1] - padding;
    float bg_w = bounds[2] - bounds[0] + 2 * padding;
    float bg_h = bounds[3] - bounds[1] + 2 * padding;

    nvgBeginPath(vg_);
    nvgRoundedRect(vg_, bg_x, bg_y, bg_w, bg_h, 3.0f);
    nvgFillColor(vg_, nvgRGBAf(bg_color.r, bg_color.g, bg_color.b, bg_color.a));
    nvgFill(vg_);

    // Draw text
    nvgFillColor(vg_, nvgRGBAf(text_color.r, text_color.g, text_color.b, text_color.a));
    nvgText(vg_, x, y, text.c_str(), nullptr);
}

// ============================================================================
// Shape Rendering
// ============================================================================

void OSDRenderer::drawRect(float x, float y, float width, float height, Color color) {
    if (!in_frame_) return;

    nvgBeginPath(vg_);
    nvgRect(vg_, x, y, width, height);
    nvgFillColor(vg_, nvgRGBAf(color.r, color.g, color.b, color.a));
    nvgFill(vg_);
}

void OSDRenderer::drawRectOutline(float x, float y, float width, float height,
                                  Color color, float stroke_width) {
    if (!in_frame_) return;

    nvgBeginPath(vg_);
    nvgRect(vg_, x, y, width, height);
    nvgStrokeColor(vg_, nvgRGBAf(color.r, color.g, color.b, color.a));
    nvgStrokeWidth(vg_, stroke_width);
    nvgStroke(vg_);
}

void OSDRenderer::drawRoundedRect(float x, float y, float width, float height,
                                  float radius, Color color) {
    if (!in_frame_) return;

    nvgBeginPath(vg_);
    nvgRoundedRect(vg_, x, y, width, height, radius);
    nvgFillColor(vg_, nvgRGBAf(color.r, color.g, color.b, color.a));
    nvgFill(vg_);
}

void OSDRenderer::drawLine(float x1, float y1, float x2, float y2,
                           Color color, float width) {
    if (!in_frame_) return;

    nvgBeginPath(vg_);
    nvgMoveTo(vg_, x1, y1);
    nvgLineTo(vg_, x2, y2);
    nvgStrokeColor(vg_, nvgRGBAf(color.r, color.g, color.b, color.a));
    nvgStrokeWidth(vg_, width);
    nvgStroke(vg_);
}

void OSDRenderer::drawCircle(float cx, float cy, float radius, Color color, bool filled) {
    if (!in_frame_) return;

    nvgBeginPath(vg_);
    nvgCircle(vg_, cx, cy, radius);

    if (filled) {
        nvgFillColor(vg_, nvgRGBAf(color.r, color.g, color.b, color.a));
        nvgFill(vg_);
    } else {
        nvgStrokeColor(vg_, nvgRGBAf(color.r, color.g, color.b, color.a));
        nvgStrokeWidth(vg_, 1.0f);
        nvgStroke(vg_);
    }
}

// ============================================================================
// Convenience Methods
// ============================================================================

void OSDRenderer::drawFPS(float fps, int width) {
    if (!in_frame_) return;

    std::ostringstream ss;
    ss << std::fixed << std::setprecision(1) << fps << " FPS";

    // Draw at top-right with padding
    float padding = 10.0f;
    float x = static_cast<float>(width) - padding;
    float y = padding;

    // Use bold font for FPS
    nvgFontFaceId(vg_, font_bold_);
    nvgFontSize(vg_, default_font_size_);
    nvgTextAlign(vg_, NVG_ALIGN_RIGHT | NVG_ALIGN_TOP);

    // Measure for background
    float bounds[4];
    nvgTextBounds(vg_, x, y, ss.str().c_str(), nullptr, bounds);

    // Background
    float bg_padding = 4.0f;
    nvgBeginPath(vg_);
    nvgRoundedRect(vg_,
                   bounds[0] - bg_padding,
                   bounds[1] - bg_padding,
                   bounds[2] - bounds[0] + 2 * bg_padding,
                   bounds[3] - bounds[1] + 2 * bg_padding,
                   3.0f);
    nvgFillColor(vg_, nvgRGBAf(0.0f, 0.0f, 0.0f, 0.7f));
    nvgFill(vg_);

    // Text - green if good FPS, yellow if low, red if very low
    Color fps_color = Color::green();
    if (fps < 20.0f) {
        fps_color = Color::red();
    } else if (fps < 28.0f) {
        fps_color = Color::yellow();
    }

    nvgFillColor(vg_, nvgRGBAf(fps_color.r, fps_color.g, fps_color.b, fps_color.a));
    nvgText(vg_, x, y, ss.str().c_str(), nullptr);
}

void OSDRenderer::drawTimestamp(float x, float y) {
    if (!in_frame_) return;

    std::string timestamp = getCurrentTimestamp();
    drawTextWithBackground(x, y, timestamp, Color::white(), Color::transparent(0.7f), 4.0f, 0);
}

void OSDRenderer::drawFrameCounter(uint32_t frame_number, float x, float y) {
    if (!in_frame_) return;

    std::ostringstream ss;
    ss << "Frame: " << frame_number;
    drawTextWithBackground(x, y, ss.str(), Color::white(), Color::transparent(0.7f), 4.0f, 0);
}

// ============================================================================
// Private Helpers
// ============================================================================

int OSDRenderer::loadFont(const std::string& name, const std::string& path) {
    /**
     * TEACHING: NanoVG Font Loading
     * -----------------------------
     * nvgCreateFont() loads a TTF file and returns a font ID.
     * The font ID is used in nvgFontFaceId() to select the font.
     * Returns -1 on failure (file not found, invalid TTF, etc.)
     */
    int font_id = nvgCreateFont(vg_, name.c_str(), path.c_str());

    if (font_id == -1) {
        std::cerr << "  Failed to load font: " << path << "\n";
    } else {
        std::cout << "  Loaded font '" << name << "' from: " << path << "\n";
    }

    return font_id;
}

void OSDRenderer::setFont(float size) {
    nvgFontFaceId(vg_, font_regular_);
    nvgFontSize(vg_, size);
}

std::string OSDRenderer::getCurrentTimestamp() const {
    auto now = std::chrono::system_clock::now();
    auto time = std::chrono::system_clock::to_time_t(now);
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        now.time_since_epoch()) % 1000;

    std::ostringstream ss;
    ss << std::put_time(std::localtime(&time), "%H:%M:%S");
    ss << "." << std::setfill('0') << std::setw(3) << ms.count();

    return ss.str();
}

// ============================================================================
// Factory Function
// ============================================================================

std::unique_ptr<IOSD> createOSD() {
    return std::make_unique<OSDRenderer>();
}

} // namespace robot_vision
