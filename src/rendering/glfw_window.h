#pragma once

/**
 * @file glfw_window.h
 * @brief GLFW window implementation
 */

#include "core/window.h"

// Forward declare GLFW types to avoid including GLFW in header
struct GLFWwindow;

namespace robot_vision {

/**
 * GLFW-based window implementation
 *
 * TEACHING: Forward Declarations
 * ------------------------------
 * We forward-declare GLFWwindow instead of including GLFW/glfw3.h.
 * This keeps the header clean and speeds up compilation.
 * Only the .cpp file needs the full GLFW header.
 */
class GLFWWindow : public IWindow {
public:
    GLFWWindow();
    ~GLFWWindow() override;

    // Non-copyable
    GLFWWindow(const GLFWWindow&) = delete;
    GLFWWindow& operator=(const GLFWWindow&) = delete;

    // IWindow implementation
    bool initialize(const WindowConfig& config) override;
    void shutdown() override;

    bool shouldClose() const override;
    void pollEvents() override;
    void swapBuffers() override;

    int getWidth() const override;
    int getHeight() const override;
    int getFramebufferWidth() const override;
    int getFramebufferHeight() const override;

    bool isFocused() const override;
    void* getNativeHandle() const override;

    void setTitle(const std::string& title) override;
    void requestClose() override;

private:
    GLFWwindow* window_ = nullptr;
    int width_ = 0;
    int height_ = 0;
    int fb_width_ = 0;   // Framebuffer width (for Retina)
    int fb_height_ = 0;  // Framebuffer height

    static bool glfw_initialized_;
    static int window_count_;
};

} // namespace robot_vision
