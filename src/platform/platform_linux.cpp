/**
 * @file platform_linux.cpp
 * @brief Linux/Jetson platform implementation
 *
 * This file is compiled on Linux systems (see CMakeLists.txt).
 * It provides Linux-specific and Jetson-specific configurations.
 *
 * NOTE: This is a stub implementation for future Jetson porting.
 * The macOS version is the primary development target.
 */

#include "core/platform.h"
#include <sys/utsname.h>
#include <fstream>

namespace robot_vision {

/**
 * Detect if running on NVIDIA Jetson
 *
 * Jetson devices have a special file: /etc/nv_tegra_release
 * or can be detected via /proc/device-tree/model
 */
static bool isJetsonDevice() {
    // Check for Jetson-specific file
    std::ifstream tegra_file("/etc/nv_tegra_release");
    if (tegra_file.good()) {
        return true;
    }

    // Check device tree model
    std::ifstream model_file("/proc/device-tree/model");
    if (model_file.good()) {
        std::string model;
        std::getline(model_file, model);
        if (model.find("Jetson") != std::string::npos) {
            return true;
        }
    }

    return false;
}

/**
 * Linux/Jetson Platform Implementation
 */
class LinuxPlatform : public IPlatform {
public:
    LinuxPlatform() {
        // Detect if this is a Jetson device
        is_jetson_ = isJetsonDevice();

        // Get Linux version
        struct utsname sys_info;
        if (uname(&sys_info) == 0) {
            os_version_ = std::string(sys_info.sysname) + " " + sys_info.release;
        } else {
            os_version_ = "Linux unknown";
        }
    }

    PlatformInfo getInfo() const override {
        PlatformInfo info;
        info.type = is_jetson_ ? PlatformType::Jetson : PlatformType::Linux;
        info.name = is_jetson_ ? "Jetson Nano" : "Linux";
        info.os_version = os_version_;
        info.graphics_api = is_jetson_ ? GraphicsAPI::OpenGLES : GraphicsAPI::OpenGL;
        info.graphics_api_name = is_jetson_ ? "OpenGL ES 2.0" : "OpenGL 2.1";
        info.has_gpu_acceleration = is_jetson_;  // Jetson has hardware video
        info.has_cuda = is_jetson_;              // Jetson has CUDA
        return info;
    }

    std::string getName() const override {
        return is_jetson_ ? "Jetson" : "Linux";
    }

    std::string getCameraPipeline(int width, int height, int fps) const override {
        if (is_jetson_) {
            /**
             * Jetson CSI Camera Pipeline (nvarguscamerasrc)
             *
             * nvarguscamerasrc  - NVIDIA's camera source for CSI cameras
             * nvvidconv         - NVIDIA's hardware-accelerated format converter
             *
             * Uses NVMM (NVIDIA Memory Management) for zero-copy performance
             */
            return
                "nvarguscamerasrc ! "
                "video/x-raw(memory:NVMM),width=" + std::to_string(width) +
                ",height=" + std::to_string(height) +
                ",format=NV12,framerate=" + std::to_string(fps) + "/1 ! "
                "nvvidconv ! "
                "video/x-raw,format=RGB ! "
                "appsink name=sink emit-signals=true max-buffers=1 drop=true";
        } else {
            /**
             * Generic Linux USB Camera Pipeline (v4l2src)
             *
             * v4l2src - Video4Linux2 source, works with USB webcams
             */
            return
                "v4l2src device=/dev/video0 ! "
                "videoconvert ! "
                "video/x-raw,format=RGB,width=" + std::to_string(width) +
                ",height=" + std::to_string(height) +
                ",framerate=" + std::to_string(fps) + "/1 ! "
                "appsink name=sink emit-signals=true max-buffers=1 drop=true";
        }
    }

    std::string getDisplayPipeline() const override {
        if (is_jetson_) {
            // Jetson hardware overlay (fastest)
            return "nvoverlaysink";
        } else {
            // Generic X11 sink
            return "autovideosink";
        }
    }

    bool hasCamera() const override {
        // Check if /dev/video0 exists (common camera device)
        std::ifstream video_device("/dev/video0");
        return video_device.good();
    }

    bool supportsResolution(int width, int height) const override {
        // Common supported resolutions
        if (width == 640 && height == 480) return true;
        if (width == 1280 && height == 720) return true;
        if (width == 1920 && height == 1080) return true;

        return (width > 0 && width <= 4096 && height > 0 && height <= 4096);
    }

    GraphicsAPI getGraphicsAPI() const override {
        return is_jetson_ ? GraphicsAPI::OpenGLES : GraphicsAPI::OpenGL;
    }

    void* createGraphicsContext() const override {
        /**
         * STUB IMPLEMENTATION - Will be completed in Phase 3
         *
         * When NanoVG is added, this will call:
         *   if (is_jetson_) {
         *       #define NANOVG_GLES2_IMPLEMENTATION
         *       #include "nanovg_gl.h"
         *       return nvgCreateGLES2(NVG_ANTIALIAS | NVG_STENCIL_STROKES);
         *   } else {
         *       #define NANOVG_GL2_IMPLEMENTATION
         *       #include "nanovg_gl.h"
         *       return nvgCreateGL2(NVG_ANTIALIAS | NVG_STENCIL_STROKES);
         *   }
         */
        // TODO: Phase 3 - Implement when NanoVG is integrated
        return nullptr;
    }

    void destroyGraphicsContext(void* context) const override {
        /**
         * STUB IMPLEMENTATION - Will be completed in Phase 3
         *
         * When NanoVG is added, this will call:
         *   if (is_jetson_) {
         *       nvgDeleteGLES2(static_cast<NVGcontext*>(context));
         *   } else {
         *       nvgDeleteGL2(static_cast<NVGcontext*>(context));
         *   }
         */
        // TODO: Phase 3 - Implement when NanoVG is integrated
        (void)context;  // Suppress unused parameter warning
    }

private:
    bool is_jetson_ = false;
    std::string os_version_;
};

// Factory function
std::unique_ptr<IPlatform> createPlatform() {
    return std::make_unique<LinuxPlatform>();
}

} // namespace robot_vision
