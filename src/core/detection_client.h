#pragma once

/**
 * @file detection_client.h
 * @brief Detection service client interface
 *
 * Provides IPC communication with the vision-detector service.
 * Uses Unix sockets for commands and shared memory for frame data.
 *
 * Phase 4: Object Detection Integration
 */

#include <memory>
#include <string>
#include <vector>
#include <functional>
#include <detector_protocol/protocol.h>

namespace robot_vision {

/**
 * Connection state
 */
enum class ConnectionState {
    Disconnected,
    Connecting,
    Connected,
    Error
};

/**
 * Detection client configuration
 */
struct DetectionClientConfig {
    std::string socket_path = detector_protocol::SOCKET_PATH;
    std::string shm_name = detector_protocol::SHM_NAME;
    int connect_timeout_ms = 1000;
    bool auto_reconnect = true;
};

/**
 * Handshake result from server (Protocol v2)
 */
struct ServerInfo {
    uint32_t protocol_version;
    bool accepted;

    // Model information (from ModelInfo struct)
    std::string model_name;          // Human-readable name
    std::string model_description;   // Model description
    detector_protocol::ModelType model_type;  // Architecture type
    uint32_t model_input_width;
    uint32_t model_input_height;
    uint32_t num_classes;
    uint64_t model_size_bytes;       // Model file size
    std::string device;              // Device info (e.g., "Darwin-arm64")

    // Helper to get model type as string
    std::string getModelTypeString() const {
        switch (model_type) {
            case detector_protocol::ModelType::SSD_MOBILENET: return "SSD-MobileNet";
            case detector_protocol::ModelType::YOLOV8: return "YOLOv8";
            case detector_protocol::ModelType::YOLOV5: return "YOLOv5";
            case detector_protocol::ModelType::EFFICIENTDET: return "EfficientDet";
            default: return "Unknown";
        }
    }

    // Helper to get model size as human-readable string
    std::string getModelSizeString() const {
        if (model_size_bytes >= 1024 * 1024) {
            return std::to_string(model_size_bytes / (1024 * 1024)) + "MB";
        } else if (model_size_bytes >= 1024) {
            return std::to_string(model_size_bytes / 1024) + "KB";
        }
        return std::to_string(model_size_bytes) + "B";
    }
};

/**
 * Detection client interface
 *
 * Connects to vision-detector service and exchanges detection data.
 */
class IDetectionClient {
public:
    virtual ~IDetectionClient() = default;

    // ========================================================================
    // Connection Management
    // ========================================================================

    /**
     * Connect to detection server
     * @return true if connection and handshake successful
     */
    virtual bool connect() = 0;

    /**
     * Disconnect from server
     */
    virtual void disconnect() = 0;

    /**
     * Check if connected
     */
    virtual bool isConnected() const = 0;

    /**
     * Get connection state
     */
    virtual ConnectionState getState() const = 0;

    /**
     * Get server info (after successful handshake)
     */
    virtual const ServerInfo& getServerInfo() const = 0;

    /**
     * Get last error message
     */
    virtual const std::string& getLastError() const = 0;

    // ========================================================================
    // Health Check
    // ========================================================================

    /**
     * Send heartbeat and wait for response
     * @return true if server responded
     */
    virtual bool sendHeartbeat() = 0;

    // ========================================================================
    // Frame Processing (for later phases)
    // ========================================================================

    /**
     * Send frame for detection
     * @param pixels RGB pixel data
     * @param width Frame width
     * @param height Frame height
     * @param frame_id Unique frame identifier
     * @return true if frame sent successfully
     */
    virtual bool sendFrame(const uint8_t* pixels, uint32_t width, uint32_t height,
                           uint64_t frame_id) = 0;

    /**
     * Receive detection results (non-blocking)
     * @param detections Output vector for detections
     * @param frame_id Output frame ID that results belong to
     * @param inference_time_ms Output inference time
     * @return true if results available
     */
    virtual bool receiveDetections(std::vector<detector_protocol::Detection>& detections,
                                   uint64_t& frame_id, float& inference_time_ms) = 0;
};

// ============================================================================
// Factory Function
// ============================================================================

/**
 * Create detection client instance
 */
std::unique_ptr<IDetectionClient> createDetectionClient(const DetectionClientConfig& config = {});

} // namespace robot_vision