# GrayscaleNoise Application

A Windows application that generates and displays real-time grayscale noise patterns using GDI and network communication.

## Features

- Real-time grayscale noise generation at 25 FPS
- Client-server architecture using TCP/IP
- Windows GDI-based rendering
- FPS counter display
- Resolution: 792x793 pixels

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

1. Start the server first:
```bash
./build/Release/server.exe
```

2. Then run the client:
```bash
./build/Release/client.exe
```

The application will display a window showing real-time grayscale noise patterns with FPS counter in the title bar.

## Network Configuration

By default:
- Server listens on: 127.0.0.1:12345
- Client connects to: 127.0.0.1:12345

## License

MIT License