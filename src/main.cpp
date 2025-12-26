/**
 * @file main.cpp
 * @brief Robot Vision Demo - Application Entry Point
 *
 * Phase 4: Object Detection Integration
 * - Live camera feed with OSD overlay
 * - Connection to vision-detector service via IPC
 *
 * Build and run:
 *   cmake -B build && cmake --build build && ./build/robot_vision
 */

#include "core/platform.h"
#include "core/video_pipeline.h"
#include "core/window.h"
#include "core/osd.h"
#include "core/detection_client.h"
#include "rendering/texture_renderer.h"

#include <gst/gst.h>
#include <iostream>
#include <chrono>
#include <string>
#include <csignal>

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
    // Ignore SIGPIPE to prevent crash when writing to closed sockets
    std::signal(SIGPIPE, SIG_IGN);

    std::cout << "\n";
    std::cout << "========================================\n";
    std::cout << "   Robot Vision Demo v1.0.0            \n";
    std::cout << "   Phase 4: Object Detection           \n";
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
    window_config.title = "Robot Vision Demo - Phase 4";
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
    // Step 7: Create Detection Client (Phase 4)
    // ========================================================================
    std::cout << "\n--- Creating Detection Client ---\n";
    auto detector = createDetectionClient();
    bool detector_connected = false;

    // Try to connect to vision-detector service (non-blocking, optional)
    std::cout << "  Attempting to connect to vision-detector...\n";
    if (detector->connect()) {
        detector_connected = true;
        std::cout << "  Detection service connected!\n";

        // Test heartbeat
        if (detector->sendHeartbeat()) {
            std::cout << "  Heartbeat OK - connection verified!\n";
        }
    } else {
        std::cout << "  Detection service not available (running standalone)\n";
        std::cout << "  Start vision-detector service to enable detection\n";
    }

    // ========================================================================
    // Step 8: Start Video Capture
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
    if (detector_connected) {
        std::cout << "  Detection: ENABLED\n";
    } else {
        std::cout << "  Detection: DISABLED (no server)\n";
    }
    std::cout << "========================================\n\n";

    // ========================================================================
    // Step 9: Main Loop
    // ========================================================================

    /**
     * TEACHING: Main Loop Structure (Phase 4 Update)
     * -----------------------------------------------
     * Every real-time graphics application has a main loop:
     * 1. Poll events (handle input, window events)
     * 2. Update state (get new video frame)
     * 3. Send frame to detector (if connected)
     * 4. Receive detection results (if available)
     * 5. Render video (draw video texture to screen)
     * 6. Render OSD (draw overlay graphics on top, including detections)
     * 7. Swap buffers (show the rendered frame)
     * 8. Repeat until exit
     */

    uint32_t total_frames = 0;
    int frame_count = 0;
    float current_fps = 0.0f;
    auto start_time = std::chrono::steady_clock::now();
    auto last_fps_time = start_time;
    auto last_heartbeat_time = start_time;
    auto last_reconnect_time = start_time;

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

        // Draw detector status (bottom-right)
        std::string detector_status = detector_connected ? "Detector: ON" : "Detector: OFF";
        Color status_color = detector_connected ? Color::green() : Color{0.5f, 0.5f, 0.5f, 1.0f};
        osd->drawTextWithBackground(
            static_cast<float>(fb_width) - 120.0f,
            static_cast<float>(fb_height) - 30.0f,
            detector_status,
            status_color,
            Color::transparent(0.7f),
            4.0f,
            14.0f
        );

        osd->endFrame();

        // 6. Swap buffers
        window->swapBuffers();

        // Update FPS calculation and periodic heartbeat every second
        auto now = std::chrono::steady_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - last_fps_time).count();

        if (elapsed >= 1000) {
            current_fps = frame_count * 1000.0f / static_cast<float>(elapsed);
            std::string title = "Robot Vision Demo - " + std::to_string(static_cast<int>(current_fps)) + " FPS";
            window->setTitle(title);
            frame_count = 0;
            last_fps_time = now;

            // Periodic heartbeat to check connection health
            if (detector_connected) {
                auto heartbeat_elapsed = std::chrono::duration_cast<std::chrono::seconds>(
                    now - last_heartbeat_time).count();
                if (heartbeat_elapsed >= 5) {  // Every 5 seconds
                    if (!detector->sendHeartbeat()) {
                        std::cout << "WARNING: Lost connection to detector\n";
                        detector->disconnect();  // Clean up old connection
                        detector_connected = false;
                    }
                    last_heartbeat_time = now;
                }
            } else {
                // Try to reconnect every 3 seconds
                auto reconnect_elapsed = std::chrono::duration_cast<std::chrono::seconds>(
                    now - last_reconnect_time).count();
                if (reconnect_elapsed >= 3) {
                    if (detector->connect()) {
                        detector_connected = true;
                        std::cout << "Reconnected to detector!\n";
                        if (detector->sendHeartbeat()) {
                            std::cout << "Heartbeat OK\n";
                        }
                        last_heartbeat_time = now;
                    }
                    last_reconnect_time = now;
                }
            }
        }
    }

    // ========================================================================
    // Cleanup
    // ========================================================================
    std::cout << "\n--- Shutting Down ---\n";
    if (detector_connected) {
        detector->disconnect();
    }
    pipeline->stop();
    osd->shutdown();      // Shutdown OSD before window (needs OpenGL context)
    renderer.shutdown();
    window->shutdown();
    cleanupGStreamer();

    std::cout << "  Goodbye!\n\n";
    return 0;
}
