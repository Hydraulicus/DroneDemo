#pragma once

/**
 * @file window.h
 * @brief Window management interface
 *
 * Abstracts window creation and management using GLFW.
 * Provides OpenGL context for rendering.
 *
 * TEACHING: Why Abstract the Window?
 * -----------------------------------
 * Even though we only use GLFW, abstracting behind an interface:
 * - Makes testing easier (can create mock window)
 * - Documents the required functionality clearly
 * - Could swap to SDL2 or native window later
 */

#include <memory>
#include <string>

namespace robot_vision {

/**
 * Window configuration
 */
struct WindowConfig {
    int width = 1280;                           // Initial window width
    int height = 720;                           // Initial window height
    std::string title = "Robot Vision Demo";   // Window title
    bool resizable = true;                      // Allow window resizing
    bool vsync = true;                          // Enable vertical sync

    bool isValid() const {
        return width > 0 && height > 0;
    }
};

/**
 * Window interface
 *
 * Manages a single window with OpenGL context.
 *
 * TEACHING: Single Responsibility
 * --------------------------------
 * This class ONLY handles:
 * - Window creation/destruction
 * - Event polling
 * - Buffer swapping
 *
 * It does NOT handle:
 * - What to render (that's the renderer's job)
 * - Input processing logic (that's the application's job)
 */
class IWindow {
public:
    virtual ~IWindow() = default;

    // ========================================================================
    // Lifecycle
    // ========================================================================

    /**
     * Initialize and create the window
     *
     * @param config Window configuration
     * @return true on success, false on failure
     *
     * Creates a window with OpenGL context.
     * After this call, OpenGL functions can be used.
     */
    virtual bool initialize(const WindowConfig& config) = 0;

    /**
     * Shutdown and destroy the window
     *
     * Releases all resources. Called automatically by destructor.
     */
    virtual void shutdown() = 0;

    // ========================================================================
    // Main Loop Support
    // ========================================================================

    /**
     * Check if window should close
     *
     * @return true if user clicked close button or requested exit
     *
     * Use this as main loop condition:
     *   while (!window->shouldClose()) { ... }
     */
    virtual bool shouldClose() const = 0;

    /**
     * Poll for window events
     *
     * Must be called each frame to handle:
     * - Window close requests
     * - Resize events
     * - Keyboard/mouse input
     *
     * TEACHING: Event Loop
     * --------------------
     * GLFW uses a polling model. Events queue up and are processed
     * when you call pollEvents(). This is different from callback-only
     * models where events interrupt your code.
     */
    virtual void pollEvents() = 0;

    /**
     * Swap front and back buffers
     *
     * TEACHING: Double Buffering
     * --------------------------
     * OpenGL draws to a "back buffer" while the "front buffer" is displayed.
     * swapBuffers() exchanges them, showing your rendered frame.
     * This prevents flickering and tearing.
     */
    virtual void swapBuffers() = 0;

    // ========================================================================
    // Window Properties
    // ========================================================================

    /**
     * Get current window width
     *
     * @return Width in pixels (may change if window resized)
     */
    virtual int getWidth() const = 0;

    /**
     * Get current window height
     *
     * @return Height in pixels (may change if window resized)
     */
    virtual int getHeight() const = 0;

    /**
     * Get framebuffer width (for high-DPI displays)
     *
     * @return Framebuffer width in pixels
     *
     * TEACHING: Window Size vs Framebuffer Size
     * ------------------------------------------
     * On Retina/high-DPI displays, the framebuffer may be larger
     * than the window size. Use framebuffer size for OpenGL viewport.
     * Use window size for UI layout.
     */
    virtual int getFramebufferWidth() const = 0;

    /**
     * Get framebuffer height (for high-DPI displays)
     *
     * @return Framebuffer height in pixels
     */
    virtual int getFramebufferHeight() const = 0;

    /**
     * Check if window is focused
     *
     * @return true if window has input focus
     */
    virtual bool isFocused() const = 0;

    /**
     * Get native window handle
     *
     * @return Pointer to GLFWwindow (cast to void* for abstraction)
     *
     * Use only when you need direct GLFW access.
     */
    virtual void* getNativeHandle() const = 0;

    // ========================================================================
    // Window Control
    // ========================================================================

    /**
     * Set window title
     *
     * @param title New window title
     *
     * Useful for showing FPS or status in title bar.
     */
    virtual void setTitle(const std::string& title) = 0;

    /**
     * Request window close
     *
     * Sets the close flag so shouldClose() returns true.
     */
    virtual void requestClose() = 0;
};

// ============================================================================
// Factory Function
// ============================================================================

/**
 * Create a window instance
 *
 * @return Unique pointer to window implementation
 */
std::unique_ptr<IWindow> createWindow();

} // namespace robot_vision
