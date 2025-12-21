#pragma once

/**
 * @file platform.h
 * @brief Platform abstraction interface
 *
 * This interface isolates all platform-specific details (macOS vs Jetson)
 * allowing the rest of the application to be platform-agnostic.
 *
 * TEACHING: Interface-First Design
 * --------------------------------
 * We define WHAT the platform must provide, not HOW it provides it.
 * - macOS will use AVFoundation camera, OpenGL 2.1
 * - Jetson will use V4L2/NVIDIA camera, OpenGL ES 2.0
 * Both implement the same interface, so main.cpp doesn't care which.
 */

#include <memory>
#include <string>

namespace robot_vision {

/**
 * Supported platform types
 */
enum class PlatformType {
    MacOS,      // Development platform (your Mac)
    Jetson,     // NVIDIA Jetson Nano (ARM64 Linux with NVIDIA GPU)
    Linux,      // Generic Linux (x86_64, fallback)
    Unknown     // Unsupported platform
};

/**
 * Graphics API supported by the platform
 */
enum class GraphicsAPI {
    OpenGL,     // OpenGL 2.1+ (macOS, desktop Linux)
    OpenGLES,   // OpenGL ES 2.0 (Jetson, embedded)
    None        // No graphics support
};

/**
 * Platform information structure
 *
 * TEACHING: Plain Old Data (POD) struct
 * -------------------------------------
 * This is just data - no behavior, no invariants to protect.
 * We use a struct (public by default) instead of a class.
 * All members are value types, so copying is safe and cheap.
 */
struct PlatformInfo {
    PlatformType type = PlatformType::Unknown;
    std::string name;                  // Human-readable: "macOS", "Jetson Nano"
    std::string os_version;            // e.g., "14.0", "Ubuntu 18.04"
    GraphicsAPI graphics_api = GraphicsAPI::None;
    std::string graphics_api_name;     // "OpenGL 2.1", "OpenGL ES 2.0"
    bool has_gpu_acceleration = false; // True if hardware video encoding available
    bool has_cuda = false;             // True on Jetson (NVIDIA GPU compute)
};

/**
 * Platform abstraction interface
 *
 * TEACHING: Pure Virtual Interface
 * --------------------------------
 * All methods are "= 0" (pure virtual), meaning:
 * - This class cannot be instantiated directly
 * - Any class that inherits MUST implement all methods
 * - This is C++'s way of defining a "contract"
 *
 * The virtual destructor ensures proper cleanup when deleting
 * through a base class pointer (prevents memory leaks).
 */
class IPlatform {
public:
    /**
     * Virtual destructor - required for proper cleanup
     *
     * TEACHING: Why virtual destructor?
     * ---------------------------------
     * Without this, if you do:
     *   IPlatform* p = new MacOSPlatform();
     *   delete p;  // Would NOT call MacOSPlatform destructor!
     * With virtual destructor, the correct destructor is called.
     */
    virtual ~IPlatform() = default;

    // ========================================================================
    // Platform Information
    // ========================================================================

    /**
     * Get detailed platform information
     *
     * @return PlatformInfo struct with all platform details
     *
     * This is a const method - it doesn't modify the object.
     * Returns by value (not reference) because PlatformInfo is small
     * and we want the caller to have their own copy.
     */
    virtual PlatformInfo getInfo() const = 0;

    /**
     * Get short platform name for logging
     *
     * @return Platform name: "macOS", "Jetson", "Linux"
     */
    virtual std::string getName() const = 0;

    // ========================================================================
    // GStreamer Pipeline Configuration
    // ========================================================================

    /**
     * Get GStreamer pipeline string for camera capture
     *
     * @param width   Desired frame width (e.g., 1280)
     * @param height  Desired frame height (e.g., 720)
     * @param fps     Desired frames per second (e.g., 30)
     * @return GStreamer pipeline string ready for gst_parse_launch()
     *
     * Example return values:
     * - macOS:  "autovideosrc ! videoconvert ! video/x-raw,format=RGB,width=1280,height=720 ! appsink"
     * - Jetson: "nvarguscamerasrc ! nvvidconv ! video/x-raw,format=RGB,width=1280,height=720 ! appsink"
     */
    virtual std::string getCameraPipeline(int width, int height, int fps) const = 0;

    /**
     * Get GStreamer pipeline string for video display
     *
     * @return GStreamer pipeline string for display sink
     *
     * Example return values:
     * - macOS:  "autovideosink"
     * - Jetson: "nvoverlaysink" (hardware accelerated)
     */
    virtual std::string getDisplayPipeline() const = 0;

    // ========================================================================
    // Capability Queries
    // ========================================================================

    /**
     * Check if a camera is available on this platform
     *
     * @return true if at least one camera is detected
     *
     * Note: This is a quick check, not a full camera test.
     * The actual camera may still fail to initialize.
     */
    virtual bool hasCamera() const = 0;

    /**
     * Check if a resolution is supported
     *
     * @param width   Frame width to check
     * @param height  Frame height to check
     * @return true if the platform likely supports this resolution
     *
     * Common supported resolutions:
     * - 640x480 (VGA)
     * - 1280x720 (720p HD)
     * - 1920x1080 (1080p Full HD)
     */
    virtual bool supportsResolution(int width, int height) const = 0;

    // ========================================================================
    // Graphics Configuration
    // ========================================================================

    /**
     * Get the graphics API for this platform
     *
     * @return GraphicsAPI enum value
     *
     * This determines which NanoVG backend to use:
     * - OpenGL: nvgCreateGL2()
     * - OpenGLES: nvgCreateGLES2()
     */
    virtual GraphicsAPI getGraphicsAPI() const = 0;

    /**
     * Create NanoVG graphics context for this platform
     *
     * @return Pointer to NVGcontext (void* to avoid NanoVG header dependency)
     *         Returns nullptr if creation fails
     *
     * TEACHING: Why void* instead of NVGcontext*?
     * -------------------------------------------
     * We don't want platform.h to depend on NanoVG headers.
     * Using void* keeps the interface clean and decoupled.
     * The caller (who includes NanoVG) casts it back:
     *   NVGcontext* vg = static_cast<NVGcontext*>(platform->createGraphicsContext());
     *
     * Note: Must be called AFTER OpenGL context is created (via GLFW)
     * The implementation will use:
     * - macOS: nvgCreateGL2(NVG_ANTIALIAS | NVG_STENCIL_STROKES)
     * - Jetson: nvgCreateGLES2(NVG_ANTIALIAS | NVG_STENCIL_STROKES)
     */
    virtual void* createGraphicsContext() const = 0;

    /**
     * Destroy NanoVG graphics context
     *
     * @param context Pointer to NVGcontext to destroy
     *
     * Must be called before destroying the OpenGL context
     */
    virtual void destroyGraphicsContext(void* context) const = 0;
};

// ============================================================================
// Factory Function
// ============================================================================

/**
 * Create the appropriate platform implementation
 *
 * @return Unique pointer to platform-specific IPlatform implementation
 *
 * TEACHING: Factory Pattern
 * -------------------------
 * Instead of the caller knowing about MacOSPlatform, LinuxPlatform, etc.,
 * they just call createPlatform() and get the right one automatically.
 *
 * This function is implemented in platform_macos.cpp or platform_linux.cpp
 * depending on which one is compiled (see CMakeLists.txt).
 *
 * TEACHING: std::unique_ptr
 * -------------------------
 * unique_ptr is a "smart pointer" that:
 * - Owns the object exclusively (can't be copied)
 * - Automatically deletes the object when it goes out of scope
 * - Prevents memory leaks without manual delete
 *
 * Usage:
 *   auto platform = createPlatform();  // Creates platform
 *   platform->getInfo();               // Use it
 *   // No delete needed! Automatic cleanup when 'platform' goes out of scope
 */
std::unique_ptr<IPlatform> createPlatform();

} // namespace robot_vision
