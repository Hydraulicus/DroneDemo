/**
 * @file glfw_window.cpp
 * @brief GLFW window implementation
 */

#include "glfw_window.h"

// OpenGL must be included before GLFW on macOS
#ifdef PLATFORM_MACOS
    #define GL_SILENCE_DEPRECATION  // Silence OpenGL deprecation warnings on macOS
    #include <OpenGL/gl.h>
#else
    #include <GL/gl.h>
#endif

#include <GLFW/glfw3.h>
#include <iostream>

namespace robot_vision {

// Static member initialization
bool GLFWWindow::glfw_initialized_ = false;
int GLFWWindow::window_count_ = 0;

// ============================================================================
// Constructor / Destructor
// ============================================================================

GLFWWindow::GLFWWindow() = default;

GLFWWindow::~GLFWWindow() {
    shutdown();
}

// ============================================================================
// IWindow Implementation
// ============================================================================

bool GLFWWindow::initialize(const WindowConfig& config) {
    if (window_) {
        std::cerr << "  Window already initialized\n";
        return false;
    }

    if (!config.isValid()) {
        std::cerr << "  Invalid window configuration\n";
        return false;
    }

    /**
     * TEACHING: GLFW Initialization
     * -----------------------------
     * glfwInit() initializes the GLFW library.
     * It should only be called once, before any other GLFW functions.
     * We use a static flag to ensure single initialization.
     */
    if (!glfw_initialized_) {
        if (!glfwInit()) {
            std::cerr << "  Failed to initialize GLFW\n";
            return false;
        }
        glfw_initialized_ = true;
        std::cout << "  GLFW initialized: " << glfwGetVersionString() << "\n";
    }

    /**
     * TEACHING: OpenGL Context Hints
     * ------------------------------
     * Before creating a window, we set "hints" that affect context creation.
     * These request specific OpenGL version and profile.
     */
#ifdef PLATFORM_MACOS
    // macOS requires forward-compatible core profile for OpenGL 3.2+
    // But we use OpenGL 2.1 for NanoVG compatibility, so minimal hints
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 2);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 1);
#else
    // Linux/Jetson
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 2);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 0);
    glfwWindowHint(GLFW_CLIENT_API, GLFW_OPENGL_ES_API);  // Use OpenGL ES on Jetson
#endif

    glfwWindowHint(GLFW_RESIZABLE, config.resizable ? GLFW_TRUE : GLFW_FALSE);

    // Create window
    window_ = glfwCreateWindow(
        config.width,
        config.height,
        config.title.c_str(),
        nullptr,  // Monitor (nullptr = windowed)
        nullptr   // Share (nullptr = no sharing)
    );

    if (!window_) {
        std::cerr << "  Failed to create GLFW window\n";
        return false;
    }

    // Make OpenGL context current
    glfwMakeContextCurrent(window_);

    // Enable/disable VSync
    glfwSwapInterval(config.vsync ? 1 : 0);

    // Store dimensions
    glfwGetWindowSize(window_, &width_, &height_);
    glfwGetFramebufferSize(window_, &fb_width_, &fb_height_);

    window_count_++;

    std::cout << "  Window created: " << width_ << "x" << height_;
    if (fb_width_ != width_) {
        std::cout << " (framebuffer: " << fb_width_ << "x" << fb_height_ << ")";
    }
    std::cout << "\n";

    // Print OpenGL info
    std::cout << "  OpenGL: " << glGetString(GL_VERSION) << "\n";
    std::cout << "  Renderer: " << glGetString(GL_RENDERER) << "\n";

    return true;
}

void GLFWWindow::shutdown() {
    if (window_) {
        glfwDestroyWindow(window_);
        window_ = nullptr;
        window_count_--;

        // Terminate GLFW if no windows left
        if (window_count_ == 0 && glfw_initialized_) {
            glfwTerminate();
            glfw_initialized_ = false;
            std::cout << "  GLFW terminated\n";
        }
    }
}

bool GLFWWindow::shouldClose() const {
    return window_ ? glfwWindowShouldClose(window_) : true;
}

void GLFWWindow::pollEvents() {
    glfwPollEvents();

    // Update cached dimensions (in case of resize)
    if (window_) {
        glfwGetWindowSize(window_, &width_, &height_);
        glfwGetFramebufferSize(window_, &fb_width_, &fb_height_);
    }
}

void GLFWWindow::swapBuffers() {
    if (window_) {
        glfwSwapBuffers(window_);
    }
}

int GLFWWindow::getWidth() const {
    return width_;
}

int GLFWWindow::getHeight() const {
    return height_;
}

int GLFWWindow::getFramebufferWidth() const {
    return fb_width_;
}

int GLFWWindow::getFramebufferHeight() const {
    return fb_height_;
}

bool GLFWWindow::isFocused() const {
    return window_ ? glfwGetWindowAttrib(window_, GLFW_FOCUSED) : false;
}

void* GLFWWindow::getNativeHandle() const {
    return window_;
}

void GLFWWindow::setTitle(const std::string& title) {
    if (window_) {
        glfwSetWindowTitle(window_, title.c_str());
    }
}

void GLFWWindow::requestClose() {
    if (window_) {
        glfwSetWindowShouldClose(window_, GLFW_TRUE);
    }
}

// ============================================================================
// Factory Function
// ============================================================================

std::unique_ptr<IWindow> createWindow() {
    return std::make_unique<GLFWWindow>();
}

} // namespace robot_vision
