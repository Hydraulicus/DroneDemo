/**
 * @file main.cpp
 * @brief Robot Vision Demo - Application Entry Point
 *
 * This is the main entry point for the robot vision application.
 * Currently implements Phase 1: Foundation - Platform Detection & GStreamer Init
 *
 * Build and run:
 *   mkdir -p build && cd build
 *   cmake ..
 *   make
 *   ./robot_vision
 */

#include "core/platform.h"
#include <gst/gst.h>  // GStreamer main header
#include <iostream>
#include <iomanip>

// ============================================================================
// GStreamer Initialization
// ============================================================================

/**
 * Initialize GStreamer framework
 *
 * @return true if initialization succeeded, false otherwise
 *
 * TEACHING: GStreamer Initialization
 * -----------------------------------
 * gst_init() must be called before using any GStreamer functions.
 * It:
 * - Initializes all internal data structures
 * - Loads and registers all plugins
 * - Parses GStreamer-specific command line options
 *
 * We pass nullptr for argc/argv since we don't need GStreamer
 * to process command line arguments.
 */
bool initGStreamer() {
    // Initialize GStreamer
    gst_init(nullptr, nullptr);

    // Verify initialization by checking version
    guint major, minor, micro, nano;
    gst_version(&major, &minor, &micro, &nano);

    std::cout << "  GStreamer initialized: "
              << major << "." << minor << "." << micro;
    if (nano == 1) {
        std::cout << " (CVS)";
    } else if (nano == 2) {
        std::cout << " (prerelease)";
    }
    std::cout << "\n";

    return true;
}

/**
 * Cleanup GStreamer resources
 *
 * TEACHING: Proper Cleanup
 * ------------------------
 * gst_deinit() frees all resources allocated by GStreamer.
 * Should be called when the application exits.
 * After this call, no GStreamer functions should be used.
 */
void cleanupGStreamer() {
    gst_deinit();
}

// ============================================================================
// Platform Information Display
// ============================================================================

/**
 * Print platform information in a formatted way
 *
 * TEACHING: const reference parameter
 * ------------------------------------
 * We pass PlatformInfo by const reference (`const PlatformInfo&`):
 * - `const`: We promise not to modify the object
 * - `&`: We pass by reference (no copy), efficient for larger objects
 *
 * For small objects (int, bool), pass by value.
 * For larger objects (strings, structs), pass by const reference.
 */
void printPlatformInfo(const robot_vision::PlatformInfo& info) {
    std::cout << "\n";
    std::cout << "========================================\n";
    std::cout << "     Robot Vision Demo - Platform Info  \n";
    std::cout << "========================================\n";
    std::cout << "\n";

    // Platform type as string
    std::string type_str;
    switch (info.type) {
        case robot_vision::PlatformType::MacOS:   type_str = "macOS"; break;
        case robot_vision::PlatformType::Jetson:  type_str = "Jetson"; break;
        case robot_vision::PlatformType::Linux:   type_str = "Linux"; break;
        default:                                  type_str = "Unknown"; break;
    }

    std::cout << "  Platform:      " << info.name << "\n";
    std::cout << "  Type:          " << type_str << "\n";
    std::cout << "  OS Version:    " << info.os_version << "\n";
    std::cout << "  Graphics API:  " << info.graphics_api_name << "\n";
    std::cout << "  GPU Accel:     " << (info.has_gpu_acceleration ? "Yes" : "No") << "\n";
    std::cout << "  CUDA Support:  " << (info.has_cuda ? "Yes" : "No") << "\n";
    std::cout << "\n";
}

/**
 * Test platform capabilities
 */
void testPlatformCapabilities(const robot_vision::IPlatform& platform) {
    std::cout << "--- Capability Tests ---\n";
    std::cout << "\n";

    // Test camera detection
    std::cout << "  Camera available: "
              << (platform.hasCamera() ? "Yes" : "No") << "\n";

    // Test resolution support
    struct Resolution { int w; int h; const char* name; };
    Resolution resolutions[] = {
        {640, 480, "VGA (640x480)"},
        {1280, 720, "HD 720p (1280x720)"},
        {1920, 1080, "Full HD 1080p (1920x1080)"}
    };

    std::cout << "\n";
    std::cout << "  Resolution Support:\n";
    for (const auto& res : resolutions) {
        bool supported = platform.supportsResolution(res.w, res.h);
        std::cout << "    " << std::setw(25) << std::left << res.name
                  << " : " << (supported ? "Supported" : "Not supported") << "\n";
    }

    std::cout << "\n";
}

/**
 * Show GStreamer pipeline strings (for debugging)
 */
void showPipelineStrings(const robot_vision::IPlatform& platform) {
    std::cout << "--- GStreamer Pipelines ---\n";
    std::cout << "\n";

    // Camera pipeline for 720p @ 30fps
    std::string camera_pipeline = platform.getCameraPipeline(1280, 720, 30);
    std::cout << "  Camera Pipeline (720p30):\n";
    std::cout << "    " << camera_pipeline << "\n";
    std::cout << "\n";

    // Display pipeline
    std::string display_pipeline = platform.getDisplayPipeline();
    std::cout << "  Display Pipeline:\n";
    std::cout << "    " << display_pipeline << "\n";
    std::cout << "\n";
}

/**
 * Main entry point
 *
 * TEACHING: Return codes
 * ----------------------
 * By convention:
 * - return 0: Success
 * - return non-zero: Error (different codes for different errors)
 *
 * This allows shell scripts to check: `if ./robot_vision; then ...`
 */
int main() {
    std::cout << "\n";
    std::cout << "Robot Vision Demo v1.0.0\n";
    std::cout << "Phase 1: Foundation\n";
    std::cout << "\n";

    // ========================================================================
    // Step 1: Initialize GStreamer
    // ========================================================================
    std::cout << "--- Initializing GStreamer ---\n";
    if (!initGStreamer()) {
        std::cerr << "ERROR: Failed to initialize GStreamer!\n";
        return 1;
    }
    std::cout << "\n";

    // ========================================================================
    // Step 2: Detect Platform
    // ========================================================================
    // TEACHING: auto keyword
    // `auto` lets the compiler deduce the type automatically.
    // Here it deduces: std::unique_ptr<robot_vision::IPlatform>
    auto platform = robot_vision::createPlatform();

    // Get and display platform info
    auto info = platform->getInfo();
    printPlatformInfo(info);

    // Test capabilities
    testPlatformCapabilities(*platform);

    // Show pipeline strings
    showPipelineStrings(*platform);

    // ========================================================================
    // Step 3: Test Graphics Context (stub for now)
    // ========================================================================
    std::cout << "--- Graphics Context ---\n";
    std::cout << "\n";
    std::cout << "  Graphics API: " << info.graphics_api_name << "\n";
    void* gfx_context = platform->createGraphicsContext();
    if (gfx_context == nullptr) {
        std::cout << "  NanoVG context: Not yet implemented (Phase 3)\n";
    } else {
        std::cout << "  NanoVG context: Created successfully\n";
        platform->destroyGraphicsContext(gfx_context);
    }
    std::cout << "\n";

    // ========================================================================
    // Phase 1 Complete
    // ========================================================================
    std::cout << "========================================\n";
    std::cout << "   Phase 1 Complete: Foundation Ready   \n";
    std::cout << "========================================\n";
    std::cout << "\n";

    std::cout << "Verified:\n";
    std::cout << "  [x] Platform detection working\n";
    std::cout << "  [x] GStreamer initialized\n";
    std::cout << "  [x] Pipeline strings configured\n";
    std::cout << "  [ ] NanoVG context (Phase 3)\n";
    std::cout << "\n";

    // Next steps hint
    std::cout << "Next: Phase 2 - Video Pipeline\n";
    std::cout << "  - Create camera capture pipeline\n";
    std::cout << "  - Open GLFW window\n";
    std::cout << "  - Display video in window\n";
    std::cout << "\n";

    // ========================================================================
    // Cleanup
    // ========================================================================
    cleanupGStreamer();

    return 0;
}
