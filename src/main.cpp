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
#include <vector>

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

    // Detection state (Phase 4 Milestone 2)
    // current_detections: stores latest detections for OSD rendering (Milestone 3)
    // last_detection_frame_id: correlates results to frames (for future latency tracking)
    std::vector<detector_protocol::Detection> current_detections;
    uint64_t last_detection_frame_id = 0;
    float last_inference_time_ms = 0.0f;
    (void)last_detection_frame_id;  // Will be used in Milestone 3 for latency calculation

    // Frame throttling: don't overwhelm detector (target ~10 FPS for detection)
    auto last_frame_sent_time = std::chrono::steady_clock::now();
    constexpr int DETECTION_TARGET_FPS = 10;
    constexpr auto DETECTION_FRAME_INTERVAL = std::chrono::milliseconds(1000 / DETECTION_TARGET_FPS);

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

            // 4. Send frame to detector (Phase 4 Milestone 2)
            // Throttle to avoid overwhelming detector - only send at DETECTION_TARGET_FPS
            if (detector_connected) {
                auto now = std::chrono::steady_clock::now();
                if (now - last_frame_sent_time >= DETECTION_FRAME_INTERVAL) {
                    if (!detector->sendFrame(frame->pixels.data(), frame->width, frame->height, total_frames)) {
                        // Frame send failed - connection may be broken
                        if (!detector->isConnected()) {
                            std::cout << "WARNING: Lost connection to detector during frame send\n";
                            detector_connected = false;
                        }
                    }
                    last_frame_sent_time = now;
                }
            }
        }

        // 5. Receive detection results (non-blocking poll)
        if (detector_connected) {
            uint64_t result_frame_id = 0;
            float inference_time = 0.0f;
            std::vector<detector_protocol::Detection> new_detections;

            if (detector->receiveDetections(new_detections, result_frame_id, inference_time)) {
                current_detections = std::move(new_detections);
                last_detection_frame_id = result_frame_id;
                last_inference_time_ms = inference_time;

                // Debug: log when detections are received
                if (!current_detections.empty()) {
                    std::cout << "Received " << current_detections.size() << " detections (frame "
                              << result_frame_id << ", " << inference_time << "ms):\n";
                    for (const auto& det : current_detections) {
                        std::cout << "  - " << det.label << " " << (det.confidence * 100) << "% at ["
                                  << det.x << "," << det.y << " " << det.width << "x" << det.height << "]\n";
                    }
                }
            }
        }

        // 6. Render video texture to window
        renderer.render(window->getFramebufferWidth(), window->getFramebufferHeight());

        // 7. Render OSD overlay
        int fb_width = window->getFramebufferWidth();
        int fb_height = window->getFramebufferHeight();

        // Relative font sizes (percentage of screen height)
        float label_font_size = static_cast<float>(fb_height) * 0.025f;      // 2.5% for detection labels
        float status_font_size = static_cast<float>(fb_height) * 0.022f;     // 2.2% for status text
        float label_padding = static_cast<float>(fb_height) * 0.005f;        // 0.5% padding
        float box_line_width = static_cast<float>(fb_height) * 0.003f;       // 0.3% line width
        float label_offset_y = label_font_size * 1.5f;                       // Space above box for label
        float status_margin = static_cast<float>(fb_height) * 0.03f;         // 3% margin from edge

        osd->beginFrame(fb_width, fb_height, pixel_ratio);

        // Draw FPS counter (top-right)
        osd->drawFPS(current_fps, fb_width);

        // Draw timestamp (top-left)
        osd->drawTimestamp(10.0f, 10.0f);

        // Draw model info (top-left, below timestamp) when connected
        if (detector_connected) {
            const auto& info = detector->getServerInfo();
            std::string model_text = info.model_name + " (" + info.getModelTypeString() + ") " + info.getModelSizeString();
            osd->drawTextWithBackground(
                10.0f,
                10.0f + status_font_size * 1.8f,  // Below timestamp
                model_text,
                Color::cyan(),
                Color::transparent(0.6f),
                label_padding,
                status_font_size * 0.85f
            );
        }

        // Draw frame counter (bottom-left)
        osd->drawFrameCounter(total_frames, 10.0f, static_cast<float>(fb_height) - status_margin);

        // Draw detection bounding boxes (Phase 4 Milestone 3)
        for (const auto& det : current_detections) {
            // Convert normalized coordinates to screen pixels
            float box_x = det.x * static_cast<float>(fb_width);
            float box_y = det.y * static_cast<float>(fb_height);
            float box_w = det.width * static_cast<float>(fb_width);
            float box_h = det.height * static_cast<float>(fb_height);

            // Color based on confidence (green = high, yellow = medium, red = low)
            Color box_color;
            if (det.confidence >= 0.7f) {
                box_color = Color::green();
            } else if (det.confidence >= 0.4f) {
                box_color = Color::yellow();
            } else {
                box_color = Color::red();
            }

            // Draw bounding box
            osd->drawRectOutline(box_x, box_y, box_w, box_h, box_color, box_line_width);

            // Draw label with confidence
            std::string label = std::string(det.label) + " " +
                std::to_string(static_cast<int>(det.confidence * 100)) + "%";
            osd->drawTextWithBackground(
                box_x, box_y - label_offset_y,  // Above the box
                label,
                Color::white(),
                Color{box_color.r, box_color.g, box_color.b, 0.7f},  // Semi-transparent bg
                label_padding,
                label_font_size
            );
        }

        // Draw detector status and detection count (bottom-right)
        std::string detector_status;
        Color status_color;
        if (detector_connected) {
            detector_status = "Det: " + std::to_string(current_detections.size());
            status_color = current_detections.empty() ? Color::green() : Color::yellow();
        } else {
            detector_status = "Det: OFF";
            status_color = Color{0.5f, 0.5f, 0.5f, 1.0f};
        }
        float status_x = static_cast<float>(fb_width) - status_margin * 4.0f;
        float status_y = static_cast<float>(fb_height) - status_margin;
        osd->drawTextWithBackground(
            status_x,
            status_y,
            detector_status,
            status_color,
            Color::transparent(0.7f),
            label_padding,
            status_font_size
        );

        // Draw inference time if available (above detector status)
        if (detector_connected && last_inference_time_ms > 0.0f) {
            std::string inf_text = std::to_string(static_cast<int>(last_inference_time_ms)) + "ms";
            osd->drawTextWithBackground(
                status_x,
                status_y - status_font_size * 1.8f,
                inf_text,
                Color::cyan(),
                Color::transparent(0.7f),
                label_padding,
                status_font_size * 0.9f
            );
        }

        osd->endFrame();

        // 8. Swap buffers
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
