/**
 * @file platform_macos.cpp
 * @brief macOS-specific platform implementation
 *
 * This file is only compiled on macOS (see CMakeLists.txt).
 * It provides macOS-specific camera access and graphics configuration.
 */

#include "core/platform.h"
#include <sys/utsname.h>  // For uname() to get OS version
#include <gst/gst.h>      // For camera probing
#include <iostream>

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
         * avfvideosrc     - AVFoundation video source (macOS specific)
         *   device-index  - 0 = built-in camera, 1+ = external cameras
         * videoconvert    - Converts between video formats
         * video/x-raw,... - "Caps filter" - specifies required format
         * appsink         - Allows our code to receive frames
         *
         * Camera Selection Strategy:
         * - Try external camera first (device-index=1)
         * - Fall back to built-in camera (device-index=0)
         */
        int camera_index = getPreferredCameraIndex();

        std::string pipeline =
            "avfvideosrc device-index=" + std::to_string(camera_index) + " ! "
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

    /**
     * Detect preferred camera index
     *
     * TEACHING: Camera Selection Strategy
     * ------------------------------------
     * On macOS, device-index maps to AVFoundation devices:
     *   0 = Built-in FaceTime camera (usually)
     *   1 = First external USB camera
     *   2+ = Additional cameras
     *
     * We probe device-index=1 first (external camera).
     * If it exists, use it; otherwise fall back to 0 (built-in).
     *
     * Probing works by creating a test pipeline and checking if it can
     * reach PAUSED state (which validates the device exists).
     */
    int getPreferredCameraIndex() const {
        // Try external camera first (index 1)
        if (probeCameraExists(1)) {
            std::cout << "  Camera: Using external USB camera (device-index=1)\n";
            return 1;
        }

        // Fall back to built-in camera (index 0)
        std::cout << "  Camera: Using built-in camera (device-index=0)\n";
        return 0;
    }

    /**
     * Probe if a camera at given device-index exists
     *
     * Creates a minimal test pipeline to check if device is available.
     * Returns true if device exists and can be opened.
     */
    bool probeCameraExists(int device_index) const {
        // Create a minimal test pipeline
        std::string test_pipeline =
            "avfvideosrc device-index=" + std::to_string(device_index) + " ! fakesink";

        GError* error = nullptr;
        GstElement* pipeline = gst_parse_launch(test_pipeline.c_str(), &error);

        if (error) {
            g_error_free(error);
            return false;
        }

        if (!pipeline) {
            return false;
        }

        // Try to set pipeline to PAUSED state (validates device exists)
        GstStateChangeReturn ret = gst_element_set_state(pipeline, GST_STATE_PAUSED);

        // Wait briefly for state change (with timeout)
        if (ret == GST_STATE_CHANGE_ASYNC) {
            GstState state;
            ret = gst_element_get_state(pipeline, &state, nullptr, GST_SECOND);  // 1 sec timeout
        }

        bool exists = (ret != GST_STATE_CHANGE_FAILURE);

        // Cleanup
        gst_element_set_state(pipeline, GST_STATE_NULL);
        gst_object_unref(pipeline);

        return exists;
    }
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
