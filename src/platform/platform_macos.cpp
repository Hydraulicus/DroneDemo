/**
 * @file platform_macos.cpp
 * @brief macOS-specific platform implementation
 *
 * This file is only compiled on macOS (see CMakeLists.txt).
 * It provides macOS-specific camera access and graphics configuration.
 */

#include "core/platform.h"
#include <sys/utsname.h>  // For uname() to get OS version

namespace robot_vision {

/**
 * macOS Platform Implementation
 *
 * TEACHING: Implementing an Interface
 * ------------------------------------
 * This class "implements" IPlatform by:
 * 1. Inheriting from IPlatform (`: public IPlatform`)
 * 2. Providing implementations for ALL pure virtual methods
 * 3. Using `override` keyword to catch errors if method signature doesn't match
 *
 * The `override` keyword is optional but HIGHLY recommended:
 * - If you misspell a method name, compiler will error
 * - If base class changes signature, compiler will error
 * - Documents intent: "I'm implementing a base class method"
 */
class MacOSPlatform : public IPlatform {
public:
    MacOSPlatform() {
        // Get macOS version using uname
        struct utsname sys_info;
        if (uname(&sys_info) == 0) {
            os_version_ = sys_info.release;  // e.g., "23.0.0" for macOS 14
        } else {
            os_version_ = "unknown";
        }
    }

    // ========================================================================
    // IPlatform Implementation
    // ========================================================================

    PlatformInfo getInfo() const override {
        PlatformInfo info;
        info.type = PlatformType::MacOS;
        info.name = "macOS";
        info.os_version = os_version_;
        info.graphics_api = GraphicsAPI::OpenGL;
        info.graphics_api_name = "OpenGL 2.1";
        info.has_gpu_acceleration = true;   // macOS has VideoToolbox
        info.has_cuda = false;              // No CUDA on macOS
        return info;
    }

    std::string getName() const override {
        return "macOS";
    }

    std::string getCameraPipeline(int width, int height, int fps) const override {
        /**
         * TEACHING: GStreamer Pipeline Syntax
         * ------------------------------------
         * A pipeline is a chain of elements connected by "!"
         *
         * autovideosrc    - Automatically finds camera (AVFoundation on macOS)
         * videoconvert    - Converts between video formats
         * video/x-raw,... - "Caps filter" - specifies required format
         * appsink         - Allows our code to receive frames
         *
         * The caps filter ensures we get RGB format at our desired resolution.
         * "name=sink" gives us a name to find this element later.
         * "emit-signals=true" means appsink will emit signals when frames arrive.
         * "max-buffers=1" and "drop=true" means drop old frames if we're slow.
         */
        std::string pipeline =
            "autovideosrc ! "
            "videoconvert ! "
            "video/x-raw,format=RGB,width=" + std::to_string(width) +
            ",height=" + std::to_string(height) +
            ",framerate=" + std::to_string(fps) + "/1 ! "
            "appsink name=sink emit-signals=true max-buffers=1 drop=true";

        return pipeline;
    }

    std::string getDisplayPipeline() const override {
        /**
         * autovideosink automatically chooses the best display method:
         * - On macOS: Uses OpenGL or Metal rendering
         * - Cross-platform compatible
         */
        return "autovideosink";
    }

    bool hasCamera() const override {
        /**
         * TEACHING: Camera Detection
         * ---------------------------
         * On macOS, we could use AVFoundation to enumerate cameras,
         * but that requires Objective-C++ (.mm file).
         *
         * For simplicity, we assume a camera exists on macOS.
         * GStreamer will give us a proper error if it doesn't.
         *
         * A more robust implementation would use:
         *   #import <AVFoundation/AVFoundation.h>
         *   [[AVCaptureDevice devicesWithMediaType:AVMediaTypeVideo] count] > 0
         */
        return true;  // Assume camera exists, GStreamer will error if not
    }

    bool supportsResolution(int width, int height) const override {
        /**
         * Common webcam resolutions
         * Most MacBook cameras support at least 720p
         */
        if (width == 640 && height == 480) return true;   // VGA
        if (width == 1280 && height == 720) return true;  // 720p HD
        if (width == 1920 && height == 1080) return true; // 1080p Full HD

        // For other resolutions, we'll try anyway
        // GStreamer will negotiate the closest supported resolution
        return (width > 0 && width <= 4096 && height > 0 && height <= 4096);
    }

    GraphicsAPI getGraphicsAPI() const override {
        return GraphicsAPI::OpenGL;
    }

    void* createGraphicsContext() const override {
        /**
         * STUB IMPLEMENTATION - Will be completed in Phase 3
         *
         * When NanoVG is added, this will call:
         *   #define NANOVG_GL2_IMPLEMENTATION
         *   #include "nanovg_gl.h"
         *   return nvgCreateGL2(NVG_ANTIALIAS | NVG_STENCIL_STROKES);
         *
         * For now, return nullptr to indicate "not yet implemented"
         */
        // TODO: Phase 3 - Implement when NanoVG is integrated
        return nullptr;
    }

    void destroyGraphicsContext(void* context) const override {
        /**
         * STUB IMPLEMENTATION - Will be completed in Phase 3
         *
         * When NanoVG is added, this will call:
         *   nvgDeleteGL2(static_cast<NVGcontext*>(context));
         */
        // TODO: Phase 3 - Implement when NanoVG is integrated
        (void)context;  // Suppress unused parameter warning
    }

private:
    std::string os_version_;
};

// ============================================================================
// Factory Function Implementation
// ============================================================================

/**
 * Create macOS platform instance
 *
 * TEACHING: std::make_unique
 * --------------------------
 * std::make_unique<T>() creates a new T and wraps it in unique_ptr.
 * It's safer than `new` because:
 * - No explicit delete needed
 * - Exception-safe (no leaks if constructor throws)
 * - Clear intent: "I'm creating a uniquely-owned object"
 */
std::unique_ptr<IPlatform> createPlatform() {
    return std::make_unique<MacOSPlatform>();
}

} // namespace robot_vision
