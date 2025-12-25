/**
 * @file main.cpp
 * @brief Robot Vision Demo - Application Entry Point
 *
 * Phase 3: Graphics Overlay - Live camera feed with OSD overlay.
 *
 * Build and run:
 *   cmake -B build && cmake --build build && ./build/robot_vision
 */

#include "core/platform.h"
#include "core/video_pipeline.h"
#include "core/window.h"
#include "core/osd.h"
#include "rendering/texture_renderer.h"

#include <gst/gst.h>
#include <iostream>
#include <chrono>
#include <string>

using namespace robot_vision;

// ============================================================================
// GStreamer Initialization (from Phase 1)
// ============================================================================

bool initGStreamer() {
    gst_init(nullptr, nullptr);

    guint major, minor, micro, nano;
    gst_version(&major, &minor, &micro, &nano);

    std::cout << "  GStreamer: " << major << "." << minor << "." << micro << "\n";
    return true;
}

void cleanupGStreamer() {
    gst_deinit();
}

// ============================================================================
// Main Application
// ============================================================================

int main() {
    std::cout << "\n";
    std::cout << "========================================\n";
    std::cout << "   Robot Vision Demo v1.0.0            \n";
    std::cout << "   Phase 3: Graphics Overlay           \n";
    std::cout << "========================================\n";
    std::cout << "\n";

    // ========================================================================
    // Step 1: Initialize GStreamer
    // ========================================================================
    std::cout << "--- Initializing ---\n";
    if (!initGStreamer()) {
        std::cerr << "ERROR: Failed to initialize GStreamer!\n";
        return 1;
    }

    // ========================================================================
    // Step 2: Create Platform
    // ========================================================================
    auto platform = createPlatform();
    auto platform_info = platform->getInfo();
    std::cout << "  Platform: " << platform_info.name << "\n";
    std::cout << "  Graphics: " << platform_info.graphics_api_name << "\n";

    // ========================================================================
    // Step 3: Create Window
    // ========================================================================
    std::cout << "\n--- Creating Window ---\n";
    auto window = createWindow();

    WindowConfig window_config;
    window_config.width = 1280;
    window_config.height = 720;
    window_config.title = "Robot Vision Demo - Phase 3";
    window_config.vsync = true;

    if (!window->initialize(window_config)) {
        std::cerr << "ERROR: Failed to create window!\n";
        cleanupGStreamer();
        return 1;
    }

    // ========================================================================
    // Step 4: Create Video Pipeline
    // ========================================================================
    std::cout << "\n--- Creating Video Pipeline ---\n";
    auto pipeline = createVideoPipeline(*platform);

    PipelineConfig pipeline_config;
    pipeline_config.width = 1280;
    pipeline_config.height = 720;
    pipeline_config.fps = 30;

    if (!pipeline->initialize(pipeline_config)) {
        std::cerr << "ERROR: Failed to initialize video pipeline!\n";
        std::cerr << "  " << pipeline->getLastError() << "\n";
        cleanupGStreamer();
        return 1;
    }

    // ========================================================================
    // Step 5: Create Texture Renderer
    // ========================================================================
    std::cout << "\n--- Creating Texture Renderer ---\n";
    TextureRenderer renderer;
    if (!renderer.initialize(pipeline_config.width, pipeline_config.height)) {
        std::cerr << "ERROR: Failed to initialize texture renderer!\n";
        cleanupGStreamer();
        return 1;
    }

    // ========================================================================
    // Step 6: Create OSD Renderer (Phase 3)
    // ========================================================================
    std::cout << "\n--- Creating OSD Renderer ---\n";
    auto osd = createOSD();

    OSDConfig osd_config;
    osd_config.font_path = std::string(ASSETS_PATH) + "/fonts/RobotoMono-Regular.ttf";
    osd_config.font_bold_path = std::string(ASSETS_PATH) + "/fonts/RobotoMono-Bold.ttf";
    osd_config.default_font_size = 18.0f;

    if (!osd->initialize(osd_config)) {
        std::cerr << "ERROR: Failed to initialize OSD renderer!\n";
        cleanupGStreamer();
        return 1;
    }

    // ========================================================================
    // Step 7: Start Video Capture
    // ========================================================================
    std::cout << "\n--- Starting Video Capture ---\n";
    if (!pipeline->start()) {
        std::cerr << "ERROR: Failed to start video pipeline!\n";
        std::cerr << "  " << pipeline->getLastError() << "\n";
        cleanupGStreamer();
        return 1;
    }

    std::cout << "\n========================================\n";
    std::cout << "  Camera running! Close window to exit.\n";
    std::cout << "========================================\n\n";

    // ========================================================================
    // Step 8: Main Loop
    // ========================================================================

    /**
     * TEACHING: Main Loop Structure (Phase 3 Update)
     * -----------------------------------------------
     * Every real-time graphics application has a main loop:
     * 1. Poll events (handle input, window events)
     * 2. Update state (get new video frame)
     * 3. Render video (draw video texture to screen)
     * 4. Render OSD (draw overlay graphics on top)
     * 5. Swap buffers (show the rendered frame)
     * 6. Repeat until exit
     *
     * Note: OSD is rendered AFTER video but BEFORE swap buffers.
     * This ensures the overlay appears on top of the video.
     */

    uint32_t total_frames = 0;
    int frame_count = 0;
    float current_fps = 0.0f;
    auto start_time = std::chrono::steady_clock::now();
    auto last_fps_time = start_time;

    // Calculate device pixel ratio for Retina displays
    float pixel_ratio = static_cast<float>(window->getFramebufferWidth()) /
                        static_cast<float>(window->getWidth());

    while (!window->shouldClose()) {
        // 1. Poll window events
        window->pollEvents();

        // 2. Get latest video frame
        auto frame = pipeline->getLatestFrame();

        if (frame && frame->isValid()) {
            // 3. Upload frame to texture
            renderer.updateTexture(frame->pixels, frame->width, frame->height);
            frame_count++;
            total_frames++;
        }

        // 4. Render video texture to window
        renderer.render(window->getFramebufferWidth(), window->getFramebufferHeight());

        // 5. Render OSD overlay
        int fb_width = window->getFramebufferWidth();
        int fb_height = window->getFramebufferHeight();

        osd->beginFrame(fb_width, fb_height, pixel_ratio);

        // Draw FPS counter (top-right)
        osd->drawFPS(current_fps, fb_width);

        // Draw timestamp (top-left)
        osd->drawTimestamp(10.0f, 10.0f);

        // Draw frame counter (bottom-left)
        osd->drawFrameCounter(total_frames, 10.0f, static_cast<float>(fb_height) - 30.0f);

        osd->endFrame();

        // 6. Swap buffers
        window->swapBuffers();

        // Update FPS calculation every second
        auto now = std::chrono::steady_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - last_fps_time).count();

        if (elapsed >= 1000) {
            current_fps = frame_count * 1000.0f / static_cast<float>(elapsed);
            std::string title = "Robot Vision Demo - " + std::to_string(static_cast<int>(current_fps)) + " FPS";
            window->setTitle(title);
            frame_count = 0;
            last_fps_time = now;
        }
    }

    // ========================================================================
    // Cleanup
    // ========================================================================
    std::cout << "\n--- Shutting Down ---\n";
    pipeline->stop();
    osd->shutdown();      // Shutdown OSD before window (needs OpenGL context)
    renderer.shutdown();
    window->shutdown();
    cleanupGStreamer();

    std::cout << "  Goodbye!\n\n";
    return 0;
}
