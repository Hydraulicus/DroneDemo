/**
 * @file gstreamer_pipeline.cpp
 * @brief GStreamer video pipeline implementation
 */

#include "gstreamer_pipeline.h"
#include <iostream>
#include <cstring>

namespace robot_vision {

// ============================================================================
// Constructor / Destructor
// ============================================================================

GStreamerPipeline::GStreamerPipeline(IPlatform& platform)
    : platform_(platform)
{
}

GStreamerPipeline::~GStreamerPipeline() {
    stop();

    if (pipeline_) {
        gst_object_unref(pipeline_);
        pipeline_ = nullptr;
    }
    appsink_ = nullptr;  // Owned by pipeline, no unref needed
}

// ============================================================================
// Lifecycle
// ============================================================================

bool GStreamerPipeline::initialize(const PipelineConfig& config) {
    if (state_ != PipelineState::Uninitialized) {
        setError("Pipeline already initialized");
        return false;
    }

    if (!config.isValid()) {
        setError("Invalid pipeline configuration");
        return false;
    }

    config_ = config;

    // Get pipeline string from platform
    std::string pipeline_str = platform_.getCameraPipeline(
        config.width, config.height, config.fps);

    std::cout << "  Creating pipeline: " << pipeline_str << "\n";

    // Create the pipeline
    if (!createPipeline(pipeline_str)) {
        return false;
    }

    // Setup appsink for frame access
    if (!setupAppSink()) {
        return false;
    }

    state_ = PipelineState::Ready;
    actual_width_ = config.width;
    actual_height_ = config.height;

    std::cout << "  Pipeline initialized successfully\n";
    return true;
}

bool GStreamerPipeline::start() {
    if (state_ != PipelineState::Ready && state_ != PipelineState::Paused) {
        setError("Cannot start: pipeline not ready");
        return false;
    }

    // Set pipeline to PLAYING state
    GstStateChangeReturn ret = gst_element_set_state(pipeline_, GST_STATE_PLAYING);

    if (ret == GST_STATE_CHANGE_FAILURE) {
        setError("Failed to start pipeline");
        state_ = PipelineState::Error;
        return false;
    }

    // Wait for state change to complete (with timeout)
    GstState current_state;
    ret = gst_element_get_state(pipeline_, &current_state, nullptr, 5 * GST_SECOND);

    if (ret == GST_STATE_CHANGE_FAILURE) {
        setError("Pipeline failed to reach PLAYING state");
        state_ = PipelineState::Error;
        return false;
    }

    state_ = PipelineState::Running;
    std::cout << "  Pipeline started, capturing frames...\n";
    return true;
}

void GStreamerPipeline::stop() {
    if (pipeline_ && (state_ == PipelineState::Running || state_ == PipelineState::Paused)) {
        gst_element_set_state(pipeline_, GST_STATE_NULL);
        state_ = PipelineState::Ready;
        std::cout << "  Pipeline stopped\n";
    }
}

// ============================================================================
// Frame Access
// ============================================================================

std::shared_ptr<FrameData> GStreamerPipeline::getLatestFrame() {
    if (state_ != PipelineState::Running) {
        return nullptr;
    }

    // Try to pull a new frame
    auto new_frame = pullFrame();

    if (new_frame) {
        std::lock_guard<std::mutex> lock(frame_mutex_);
        latest_frame_ = new_frame;
        new_frame_available_.store(false);
    }

    std::lock_guard<std::mutex> lock(frame_mutex_);
    return latest_frame_;
}

bool GStreamerPipeline::hasNewFrame() const {
    return new_frame_available_.load();
}

std::shared_ptr<FrameData> GStreamerPipeline::pullFrame() {
    if (!appsink_) {
        return nullptr;
    }

    /**
     * TEACHING: GStreamer AppSink
     * ---------------------------
     * gst_app_sink_try_pull_sample() is non-blocking with timeout.
     * - Returns GstSample* containing the frame
     * - GstSample contains GstBuffer (actual pixel data)
     * - We map the buffer to read pixels, then unmap
     * - Must unref the sample when done
     */

    // Try to pull a sample with short timeout (10ms)
    GstSample* sample = gst_app_sink_try_pull_sample(
        GST_APP_SINK(appsink_), 10 * GST_MSECOND);

    if (!sample) {
        return nullptr;  // No frame available
    }

    // Get buffer from sample
    GstBuffer* buffer = gst_sample_get_buffer(sample);
    if (!buffer) {
        gst_sample_unref(sample);
        return nullptr;
    }

    // Get buffer info (size, timestamps)
    GstMapInfo map;
    if (!gst_buffer_map(buffer, &map, GST_MAP_READ)) {
        gst_sample_unref(sample);
        return nullptr;
    }

    // Get caps to determine frame dimensions
    GstCaps* caps = gst_sample_get_caps(sample);
    GstStructure* structure = gst_caps_get_structure(caps, 0);

    int width = 0, height = 0;
    gst_structure_get_int(structure, "width", &width);
    gst_structure_get_int(structure, "height", &height);

    // Create frame data
    auto frame = std::make_shared<FrameData>();
    frame->width = width;
    frame->height = height;
    frame->timestamp_ns = GST_BUFFER_PTS(buffer);
    frame->frame_number = frame_counter_.fetch_add(1);

    // Copy pixel data
    frame->pixels.resize(map.size);
    std::memcpy(frame->pixels.data(), map.data, map.size);

    // Update actual dimensions if they changed
    if (width != actual_width_ || height != actual_height_) {
        actual_width_ = width;
        actual_height_ = height;
        std::cout << "  Frame dimensions: " << width << "x" << height << "\n";
    }

    // Cleanup
    gst_buffer_unmap(buffer, &map);
    gst_sample_unref(sample);

    new_frame_available_.store(true);
    return frame;
}

// ============================================================================
// State and Diagnostics
// ============================================================================

bool GStreamerPipeline::isRunning() const {
    return state_ == PipelineState::Running;
}

PipelineState GStreamerPipeline::getState() const {
    return state_;
}

std::string GStreamerPipeline::getStateString() const {
    switch (state_) {
        case PipelineState::Uninitialized: return "uninitialized";
        case PipelineState::Ready: return "ready";
        case PipelineState::Running: return "running";
        case PipelineState::Paused: return "paused";
        case PipelineState::Error: return "error";
        default: return "unknown";
    }
}

std::string GStreamerPipeline::getLastError() const {
    return last_error_;
}

void GStreamerPipeline::getFrameDimensions(int& width, int& height) const {
    width = actual_width_;
    height = actual_height_;
}

// ============================================================================
// Private Helpers
// ============================================================================

bool GStreamerPipeline::createPipeline(const std::string& pipeline_str) {
    GError* error = nullptr;

    /**
     * TEACHING: gst_parse_launch()
     * ----------------------------
     * Creates a complete pipeline from a string description.
     * Much simpler than creating elements individually.
     * The string format is: "element1 ! element2 ! element3"
     */
    pipeline_ = gst_parse_launch(pipeline_str.c_str(), &error);

    if (error) {
        setError(std::string("Pipeline parse error: ") + error->message);
        g_error_free(error);
        return false;
    }

    if (!pipeline_) {
        setError("Failed to create pipeline");
        return false;
    }

    return true;
}

bool GStreamerPipeline::setupAppSink() {
    /**
     * TEACHING: Finding Elements in Pipeline
     * ---------------------------------------
     * gst_bin_get_by_name() finds an element by its "name" property.
     * In our pipeline string, we have: "appsink name=sink"
     * So we can find it by name "sink".
     */
    appsink_ = gst_bin_get_by_name(GST_BIN(pipeline_), "sink");

    if (!appsink_) {
        setError("Could not find appsink element (name=sink)");
        return false;
    }

    /**
     * TEACHING: AppSink Configuration
     * --------------------------------
     * - emit-signals: We don't use signals (pull model instead)
     * - drop: Drop old buffers if we're too slow (prevents lag)
     * - max-buffers: Only keep 1 buffer (we want latest frame)
     * - sync: False for lowest latency (don't sync to clock)
     */
    g_object_set(appsink_,
        "emit-signals", FALSE,
        "drop", TRUE,
        "max-buffers", 1,
        "sync", FALSE,
        nullptr);

    // Unref the extra reference from gst_bin_get_by_name
    gst_object_unref(appsink_);

    return true;
}

void GStreamerPipeline::setError(const std::string& error) {
    last_error_ = error;
    state_ = PipelineState::Error;
    std::cerr << "  ERROR: " << error << "\n";
}

PipelineState GStreamerPipeline::gstStateToState(GstState gst_state) {
    switch (gst_state) {
        case GST_STATE_NULL:
        case GST_STATE_READY:
            return PipelineState::Ready;
        case GST_STATE_PAUSED:
            return PipelineState::Paused;
        case GST_STATE_PLAYING:
            return PipelineState::Running;
        default:
            return PipelineState::Uninitialized;
    }
}

// ============================================================================
// Factory Function
// ============================================================================

std::unique_ptr<IVideoPipeline> createVideoPipeline(IPlatform& platform) {
    return std::make_unique<GStreamerPipeline>(platform);
}

} // namespace robot_vision
