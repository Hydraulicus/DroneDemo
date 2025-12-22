#pragma once

/**
 * @file gstreamer_pipeline.h
 * @brief GStreamer-based video pipeline implementation
 */

#include "core/video_pipeline.h"
#include "core/platform.h"
#include <gst/gst.h>
#include <gst/app/gstappsink.h>
#include <mutex>
#include <atomic>

namespace robot_vision {

/**
 * GStreamer video pipeline implementation
 *
 * TEACHING: RAII with GStreamer
 * -----------------------------
 * GStreamer objects use reference counting (gst_object_ref/unref).
 * We must unref everything we create or get a reference to.
 * The destructor handles cleanup automatically.
 */
class GStreamerPipeline : public IVideoPipeline {
public:
    /**
     * Constructor
     *
     * @param platform Reference to platform for pipeline string
     */
    explicit GStreamerPipeline(IPlatform& platform);

    /**
     * Destructor - cleans up GStreamer resources
     */
    ~GStreamerPipeline() override;

    // Non-copyable, non-movable (GStreamer resources can't be copied)
    GStreamerPipeline(const GStreamerPipeline&) = delete;
    GStreamerPipeline& operator=(const GStreamerPipeline&) = delete;
    GStreamerPipeline(GStreamerPipeline&&) = delete;
    GStreamerPipeline& operator=(GStreamerPipeline&&) = delete;

    // ========================================================================
    // IVideoPipeline Implementation
    // ========================================================================

    bool initialize(const PipelineConfig& config) override;
    bool start() override;
    void stop() override;

    std::shared_ptr<FrameData> getLatestFrame() override;
    bool hasNewFrame() const override;

    bool isRunning() const override;
    PipelineState getState() const override;
    std::string getStateString() const override;
    std::string getLastError() const override;
    void getFrameDimensions(int& width, int& height) const override;

private:
    /**
     * Create GStreamer pipeline from pipeline string
     */
    bool createPipeline(const std::string& pipeline_str);

    /**
     * Setup appsink for frame access
     */
    bool setupAppSink();

    /**
     * Pull a frame from the appsink
     */
    std::shared_ptr<FrameData> pullFrame();

    /**
     * Set error message
     */
    void setError(const std::string& error);

    /**
     * Convert GStreamer state to our state enum
     */
    static PipelineState gstStateToState(GstState gst_state);

private:
    IPlatform& platform_;                   // Platform reference

    GstElement* pipeline_ = nullptr;        // GStreamer pipeline
    GstElement* appsink_ = nullptr;         // AppSink element for frame access

    PipelineConfig config_;                 // Current configuration
    PipelineState state_ = PipelineState::Uninitialized;
    std::string last_error_;

    // Frame management
    std::shared_ptr<FrameData> latest_frame_;
    mutable std::mutex frame_mutex_;        // Protects latest_frame_
    std::atomic<bool> new_frame_available_{false};
    std::atomic<uint32_t> frame_counter_{0};

    // Actual dimensions (may differ from requested)
    int actual_width_ = 0;
    int actual_height_ = 0;
};

} // namespace robot_vision
