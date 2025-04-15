# GrayscaleNoise Application

A Windows application that generates and displays real-time grayscale noise patterns using GDI and network communication, with support for multicasting to multiple server instances.

## Features

- Real-time grayscale noise generation at 25 FPS
- Client-server architecture using TCP/IP
- Support for multicasting to multiple server instances
- Configurable server ports via command line
- Windows GDI-based rendering
- FPS counter display

## Requirements

- Windows OS
- Visual Studio 2019 or later with C++ desktop development workload
- CMake 3.15 or later

## Building

1. Clone the repository:
```bash
git clone https://github.com/yourusername/GrayscaleNoise.git
cd GrayscaleNoise
```

2. Generate Visual Studio solution:
```bash
mkdir build
cd build
cmake ..
```

3. Build the solution:
```bash
cmake --build . --config Release
```

## Usage

1. Start the first server instance (default port 12345):
```bash
./build/Release/server.exe
```

   Or specify a custom port:
```bash
./build/Release/server.exe 12345
```

2. Start additional server instances on different ports:
```bash
./build/Release/server.exe 12346
```

3. Run the client (automatically connects to configured servers):
```bash
./build/Release/client.exe
```

The client will display a window showing real-time grayscale noise patterns and stream them to all connected servers. Each window shows its FPS counter in the title bar.

## Network Configuration

By default:
- First server listens on: 127.0.0.1:12345
- Second server listens on: 127.0.0.1:12346
- Client automatically connects to both servers

To modify server connections in the client, edit the SERVERS array in client.cpp:
```cpp
vector<ServerInfo> SERVERS = {
    {"127.0.0.1", 12345, INVALID_SOCKET},
    {"127.0.0.1", 12346, INVALID_SOCKET}
};
```

## Performance Optimizations

- TCP_NODELAY enabled for reduced latency
- Double buffering for smooth display
- Pre-allocated frame buffers
- Thread-safe buffer management
- Efficient GDI rendering with 8-bit grayscale

## License

MIT License