#pragma once

/**
 * @file video_pipeline.h
 * @brief Video capture pipeline interface
 *
 * Abstracts video capture from camera using GStreamer.
 * Platform-specific pipeline strings come from IPlatform.
 *
 * TEACHING: Separation of Concerns
 * ---------------------------------
 * This interface only handles VIDEO CAPTURE - getting frames from camera.
 * It doesn't know about:
 * - Windows or display (that's IWindow)
 * - OpenGL textures (that's TextureRenderer)
 * - OSD overlay (that's Phase 3)
 *
 * This makes each component testable and replaceable independently.
 */

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

// Forward declaration to avoid circular include
namespace robot_vision {
class IPlatform;
}

namespace robot_vision {

/**
 * Video frame data structure
 *
 * TEACHING: Value Semantics with Move
 * ------------------------------------
 * FrameData contains a vector of pixels (potentially 2.7MB for 1080p RGB).
 * We use std::shared_ptr<FrameData> to avoid copying this large buffer.
 * The frame can be shared between pipeline and renderer safely.
 */
struct FrameData {
    std::vector<uint8_t> pixels;    // RGB pixel data (width * height * 3 bytes)
    int width = 0;                   // Frame width in pixels
    int height = 0;                  // Frame height in pixels
    uint64_t timestamp_ns = 0;       // Capture timestamp in nanoseconds
    uint32_t frame_number = 0;       // Sequential frame counter

    /**
     * Get size of pixel buffer in bytes
     */
    size_t getPixelBufferSize() const {
        return static_cast<size_t>(width) * height * 3;  // RGB = 3 bytes per pixel
    }

    /**
     * Check if frame contains valid data
     */
    bool isValid() const {
        return width > 0 && height > 0 && pixels.size() == getPixelBufferSize();
    }
};

/**
 * Pipeline configuration
 */
struct PipelineConfig {
    int width = 1280;               // Desired frame width
    int height = 720;               // Desired frame height
    int fps = 30;                   // Desired frames per second
    std::string device = "";        // Camera device (empty = auto-detect)

    /**
     * Validate configuration
     *
     * @return true if config is valid
     */
    bool isValid() const {
        return width > 0 && width <= 4096 &&
               height > 0 && height <= 4096 &&
               fps > 0 && fps <= 120;
    }
};

/**
 * Pipeline state enumeration
 */
enum class PipelineState {
    Uninitialized,  // Not yet initialized
    Ready,          // Initialized, not running
    Running,        // Actively capturing frames
    Paused,         // Temporarily paused
    Error           // Error state
};

/**
 * Video pipeline interface
 *
 * TEACHING: Pull vs Push Model
 * ----------------------------
 * We use a PULL model: the main loop calls getLatestFrame() when ready.
 * This is simpler than callbacks and avoids threading complexity.
 *
 * Alternative would be PUSH model with callbacks:
 *   pipeline->setFrameCallback([](FrameData& frame) { ... });
 * But that requires thread synchronization.
 */
class IVideoPipeline {
public:
    virtual ~IVideoPipeline() = default;

    // ========================================================================
    // Lifecycle Management
    // ========================================================================

    /**
     * Initialize the video pipeline
     *
     * @param config Pipeline configuration (resolution, fps)
     * @return true on success, false on failure
     *
     * After initialize(), the pipeline is in Ready state but not capturing.
     * Call start() to begin capturing frames.
     *
     * Error conditions:
     * - Invalid config: returns false
     * - Camera not found: returns false
     * - GStreamer error: returns false, check getLastError()
     */
    virtual bool initialize(const PipelineConfig& config) = 0;

    /**
     * Start video capture
     *
     * @return true if started successfully
     *
     * Must call initialize() first.
     * After start(), frames are available via getLatestFrame().
     */
    virtual bool start() = 0;

    /**
     * Stop video capture
     *
     * Stops capturing and releases camera.
     * Can call start() again to resume.
     * Idempotent - safe to call multiple times.
     */
    virtual void stop() = 0;

    // ========================================================================
    // Frame Access
    // ========================================================================

    /**
     * Get the most recent captured frame
     *
     * @return Shared pointer to frame data, nullptr if no frame available
     *
     * Non-blocking: returns immediately with latest frame or nullptr.
     * The returned frame is valid until next getLatestFrame() call.
     *
     * TEACHING: shared_ptr for Frame Sharing
     * --------------------------------------
     * We return shared_ptr so multiple consumers can hold the frame:
     * - Main loop can render it
     * - OSD renderer can draw on it
     * - Frame saver can write it to disk
     * All without copying the large pixel buffer.
     */
    virtual std::shared_ptr<FrameData> getLatestFrame() = 0;

    /**
     * Check if a new frame is available
     *
     * @return true if getLatestFrame() will return a new frame
     *
     * Use this to avoid unnecessary texture uploads when frame hasn't changed.
     */
    virtual bool hasNewFrame() const = 0;

    // ========================================================================
    // State and Diagnostics
    // ========================================================================

    /**
     * Check if pipeline is currently running
     *
     * @return true if capturing frames
     */
    virtual bool isRunning() const = 0;

    /**
     * Get current pipeline state
     *
     * @return Current PipelineState
     */
    virtual PipelineState getState() const = 0;

    /**
     * Get human-readable state string
     *
     * @return State string: "uninitialized", "ready", "running", "paused", "error"
     */
    virtual std::string getStateString() const = 0;

    /**
     * Get last error message
     *
     * @return Error message, empty string if no error
     */
    virtual std::string getLastError() const = 0;

    /**
     * Get actual frame dimensions (may differ from requested)
     *
     * @param[out] width Actual frame width
     * @param[out] height Actual frame height
     */
    virtual void getFrameDimensions(int& width, int& height) const = 0;
};

// ============================================================================
// Factory Function
// ============================================================================

/**
 * Create a video pipeline for the given platform
 *
 * @param platform Platform instance for pipeline string generation
 * @return Unique pointer to platform-appropriate pipeline implementation
 *
 * TEACHING: Dependency Injection
 * ------------------------------
 * We pass IPlatform as a parameter rather than creating it inside.
 * This allows:
 * - Testing with mock platform
 * - Reusing the same platform instance
 * - Clear dependency graph
 */
std::unique_ptr<IVideoPipeline> createVideoPipeline(IPlatform& platform);

} // namespace robot_vision
