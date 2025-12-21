# Robot Vision Demo

A cross-platform robot vision application demonstrating video capture, real-time processing, on-screen display (OSD) overlay, and data persistence.

## Features

- **Cross-platform**: Develops on macOS, deploys to NVIDIA Jetson Nano
- **Real-time video**: GStreamer-based video pipeline
- **OSD overlay**: NanoVG vector graphics on video stream
- **Modular architecture**: Clean separation of concerns

## Requirements

### macOS (Development)
```bash
brew install cmake pkg-config
brew install gstreamer gst-plugins-base gst-plugins-good gst-plugins-bad
brew install glfw nlohmann-json
```

### Jetson Nano (Deployment)
```bash
# GStreamer pre-installed with JetPack
sudo apt install libglfw3-dev cmake build-essential
```

## Building

```bash
# Clone with submodules (if you have access to private specs)
git clone --recursive https://github.com/Hydraulicus/DroneDemo.git

# Or clone public code only
git clone https://github.com/Hydraulicus/DroneDemo.git

# Build
cd DroneDemo
cmake -B build
cmake --build build

# Run
./build/robot_vision
```

## Project Structure

```
DroneDemo/
├── src/
│   ├── core/           # Interfaces
│   ├── platform/       # Platform-specific code
│   ├── video/          # Video pipeline (Phase 2)
│   ├── rendering/      # OSD renderer (Phase 3)
│   └── main.cpp
├── tests/              # Unit & integration tests
├── config/             # Configuration files
├── third_party/        # External dependencies (NanoVG)
└── private/            # Private specs (submodule, optional)
```

## Development Phases

- [x] **Phase 1**: Foundation - Platform abstraction, GStreamer init
- [ ] **Phase 2**: Video Pipeline - Camera capture, display
- [ ] **Phase 3**: Graphics Overlay - NanoVG integration
- [ ] **Phase 4**: OSD Features - Frame counter, FPS, telemetry
- [ ] **Phase 5**: File Operations - Frame saving, config, logging
- [ ] **Phase 6**: Testing & Optimization

## License

MIT License
