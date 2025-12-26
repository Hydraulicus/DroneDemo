#include "detection_client.h"

#include <iostream>
#include <chrono>
#include <cstring>
#include <sys/select.h>

namespace robot_vision {

using namespace detector_protocol;

// ============================================================================
// Factory Function
// ============================================================================

std::unique_ptr<IDetectionClient> createDetectionClient(const DetectionClientConfig& config) {
    return std::make_unique<DetectionClientImpl>(config);
}

// ============================================================================
// DetectionClientImpl
// ============================================================================

DetectionClientImpl::DetectionClientImpl(const DetectionClientConfig& config)
    : config_(config) {
}

DetectionClientImpl::~DetectionClientImpl() {
    disconnect();
}

bool DetectionClientImpl::connect() {
    if (state_ == ConnectionState::Connected) {
        return true;
    }

    // Clean up any previous connection attempt
    cleanup();
    state_ = ConnectionState::Connecting;

    // Connect to server socket
    socket_fd_ = UnixSocket::connect(config_.socket_path.c_str());
    if (socket_fd_ < 0) {
        setError("Failed to connect to server at " + config_.socket_path);
        state_ = ConnectionState::Error;
        return false;
    }

    // Open shared memory
    shm_ptr_ = SharedMemory::open(config_.shm_name.c_str(), SHM_SIZE, shm_fd_);
    if (!shm_ptr_) {
        setError("Failed to open shared memory: " + std::string(config_.shm_name));
        cleanup();
        state_ = ConnectionState::Error;
        return false;
    }

    // Perform handshake
    if (!performHandshake()) {
        cleanup();
        state_ = ConnectionState::Error;
        return false;
    }

    state_ = ConnectionState::Connected;
    std::cout << "  Connected to detector: " << server_info_.model_name << std::endl;
    std::cout << "  Model input: " << server_info_.model_input_width << "x"
              << server_info_.model_input_height << std::endl;
    std::cout << "  Classes: " << server_info_.num_classes << std::endl;

    return true;
}

void DetectionClientImpl::disconnect() {
    if (state_ == ConnectionState::Connected && socket_fd_ >= 0) {
        // Send shutdown message
        HeartbeatMessage msg;
        msg.type = MessageType::SHUTDOWN;
        msg.timestamp_ns = 0;
        send(socket_fd_, &msg, sizeof(msg), 0);
    }

    cleanup();
    state_ = ConnectionState::Disconnected;
}

bool DetectionClientImpl::isConnected() const {
    return state_ == ConnectionState::Connected;
}

ConnectionState DetectionClientImpl::getState() const {
    return state_;
}

const ServerInfo& DetectionClientImpl::getServerInfo() const {
    return server_info_;
}

const std::string& DetectionClientImpl::getLastError() const {
    return last_error_;
}

bool DetectionClientImpl::sendHeartbeat() {
    if (state_ != ConnectionState::Connected) {
        setError("Not connected");
        return false;
    }

    // Send heartbeat
    HeartbeatMessage msg;
    msg.type = MessageType::HEARTBEAT;
    auto now = std::chrono::steady_clock::now();
    msg.timestamp_ns = std::chrono::duration_cast<std::chrono::nanoseconds>(
        now.time_since_epoch()).count();

    if (send(socket_fd_, &msg, sizeof(msg), 0) != sizeof(msg)) {
        setError("Failed to send heartbeat");
        return false;
    }

    // Wait for response with timeout
    fd_set read_fds;
    FD_ZERO(&read_fds);
    FD_SET(socket_fd_, &read_fds);

    struct timeval timeout;
    timeout.tv_sec = 1;
    timeout.tv_usec = 0;

    int ready = select(socket_fd_ + 1, &read_fds, nullptr, nullptr, &timeout);
    if (ready <= 0) {
        setError("Heartbeat timeout");
        return false;
    }

    // Receive response
    HeartbeatMessage response;
    ssize_t n = recv(socket_fd_, &response, sizeof(response), 0);
    if (n != sizeof(response) || response.type != MessageType::HEARTBEAT) {
        setError("Invalid heartbeat response");
        return false;
    }

    return true;
}

bool DetectionClientImpl::sendFrame(const uint8_t* pixels, uint32_t width, uint32_t height,
                                     uint64_t frame_id) {
    if (state_ != ConnectionState::Connected) {
        setError("Not connected");
        return false;
    }

    if (!shm_ptr_) {
        setError("Shared memory not available");
        return false;
    }

    // Write frame header to shared memory
    auto* header = reinterpret_cast<FrameHeader*>(shm_ptr_);
    header->frame_id = frame_id;
    header->width = width;
    header->height = height;
    header->stride = width * BYTES_PER_PIXEL;
    header->format = 0;  // RGB
    auto now = std::chrono::steady_clock::now();
    header->timestamp_ns = std::chrono::duration_cast<std::chrono::nanoseconds>(
        now.time_since_epoch()).count();

    // Copy frame data
    uint8_t* frame_data = reinterpret_cast<uint8_t*>(shm_ptr_) + sizeof(FrameHeader);
    size_t frame_size = width * height * BYTES_PER_PIXEL;

    if (frame_size > MAX_FRAME_SIZE) {
        setError("Frame too large");
        return false;
    }

    std::memcpy(frame_data, pixels, frame_size);

    // Send frame ready notification
    FrameReadyMessage msg;
    msg.type = MessageType::FRAME_READY;
    msg.frame_id = frame_id;
    msg.width = width;
    msg.height = height;
    msg.timestamp_ns = header->timestamp_ns;

    if (send(socket_fd_, &msg, sizeof(msg), 0) != sizeof(msg)) {
        setError("Failed to send frame notification");
        return false;
    }

    return true;
}

bool DetectionClientImpl::receiveDetections(std::vector<detector_protocol::Detection>& detections,
                                             uint64_t& frame_id, float& inference_time_ms) {
    if (state_ != ConnectionState::Connected) {
        return false;
    }

    // Check if data available (non-blocking)
    fd_set read_fds;
    FD_ZERO(&read_fds);
    FD_SET(socket_fd_, &read_fds);

    struct timeval timeout;
    timeout.tv_sec = 0;
    timeout.tv_usec = 0;  // Non-blocking

    int ready = select(socket_fd_ + 1, &read_fds, nullptr, nullptr, &timeout);
    if (ready <= 0) {
        return false;  // No data available
    }

    // Receive result message
    DetectionResultMessage result;
    ssize_t n = recv(socket_fd_, &result, sizeof(result), 0);

    if (n <= 0) {
        if (n == 0) {
            setError("Server disconnected");
            state_ = ConnectionState::Disconnected;
        }
        return false;
    }

    if (result.type != MessageType::DETECTION_RESULT) {
        return false;
    }

    frame_id = result.frame_id;
    inference_time_ms = result.inference_time_ms;

    detections.clear();
    for (uint32_t i = 0; i < result.num_detections; ++i) {
        detections.push_back(result.detections[i]);
    }

    return true;
}

bool DetectionClientImpl::performHandshake() {
    // Send handshake request
    HandshakeRequest request;
    request.type = MessageType::HANDSHAKE_REQUEST;
    request.protocol_version = PROTOCOL_VERSION;
    request.max_frame_width = MAX_FRAME_WIDTH;
    request.max_frame_height = MAX_FRAME_HEIGHT;

    if (send(socket_fd_, &request, sizeof(request), 0) != sizeof(request)) {
        setError("Failed to send handshake request");
        return false;
    }

    // Wait for response with timeout
    fd_set read_fds;
    FD_ZERO(&read_fds);
    FD_SET(socket_fd_, &read_fds);

    struct timeval timeout;
    timeout.tv_sec = config_.connect_timeout_ms / 1000;
    timeout.tv_usec = (config_.connect_timeout_ms % 1000) * 1000;

    int ready = select(socket_fd_ + 1, &read_fds, nullptr, nullptr, &timeout);
    if (ready <= 0) {
        setError("Handshake timeout");
        return false;
    }

    // Receive response
    HandshakeResponse response;
    ssize_t n = recv(socket_fd_, &response, sizeof(response), 0);
    if (n != sizeof(response)) {
        setError("Invalid handshake response");
        return false;
    }

    if (response.type != MessageType::HANDSHAKE_RESPONSE) {
        setError("Unexpected message type in handshake");
        return false;
    }

    if (!response.accepted) {
        setError("Handshake rejected by server (protocol version mismatch?)");
        return false;
    }

    // Store server info
    server_info_.protocol_version = response.protocol_version;
    server_info_.accepted = response.accepted;
    server_info_.model_input_width = response.model_input_width;
    server_info_.model_input_height = response.model_input_height;
    server_info_.num_classes = response.num_classes;
    server_info_.model_name = std::string(response.model_name);

    return true;
}

void DetectionClientImpl::setError(const std::string& error) {
    last_error_ = error;
    std::cerr << "DetectionClient: " << error << std::endl;
}

void DetectionClientImpl::cleanup() {
    if (socket_fd_ >= 0) {
        UnixSocket::close(socket_fd_);
        socket_fd_ = -1;
    }

    if (shm_ptr_) {
        SharedMemory::close(shm_ptr_, SHM_SIZE, shm_fd_);
        shm_ptr_ = nullptr;
        shm_fd_ = -1;
    }
}

} // namespace robot_vision