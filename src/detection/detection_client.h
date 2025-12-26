#pragma once

/**
 * @file detection_client.h
 * @brief Detection client implementation
 *
 * Implements IPC communication with vision-detector service.
 */

#include "core/detection_client.h"
#include <detector_protocol/ipc_common.h>

namespace robot_vision {

class DetectionClientImpl : public IDetectionClient {
public:
    explicit DetectionClientImpl(const DetectionClientConfig& config);
    ~DetectionClientImpl() override;

    // Non-copyable
    DetectionClientImpl(const DetectionClientImpl&) = delete;
    DetectionClientImpl& operator=(const DetectionClientImpl&) = delete;

    // IDetectionClient interface
    bool connect() override;
    void disconnect() override;
    bool isConnected() const override;
    ConnectionState getState() const override;
    const ServerInfo& getServerInfo() const override;
    const std::string& getLastError() const override;

    bool sendHeartbeat() override;

    bool sendFrame(const uint8_t* pixels, uint32_t width, uint32_t height,
                   uint64_t frame_id) override;
    bool receiveDetections(std::vector<detector_protocol::Detection>& detections,
                           uint64_t& frame_id, float& inference_time_ms) override;

private:
    DetectionClientConfig config_;
    ConnectionState state_ = ConnectionState::Disconnected;
    ServerInfo server_info_{};
    std::string last_error_;

    int socket_fd_ = -1;
    int shm_fd_ = -1;
    void* shm_ptr_ = nullptr;

    bool performHandshake();
    void setError(const std::string& error);
    void cleanup();
};

} // namespace robot_vision