#ifdef _WIN64
#else
#define _X86_
#endif

#define UNICODE
#define _UNICODE
#define WIN32_LEAN_AND_MEAN
#include <array>
#include <atomic>
#include <cstdint>
#include <mutex>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>
#include <windows.h>
#include <winsock2.h>
#include <ws2tcpip.h>

#pragma comment(lib, "ws2_32.lib")

const unsigned short SERVER_PORT = 12345;
const wchar_t* WINDOW_TITLE = L"Image Receiver";
constexpr size_t maxBufferSize = 1024 * 1024 * sizeof(uint8_t);

struct Frame {
    uint32_t width;
    uint32_t height;
    uint32_t dataSize;
    uint32_t frameNumber;
    std::vector<uint8_t> data;

    Frame() : data(maxBufferSize) {} // Pre-allocate buffer
};

class ImageWindow {
    HWND hwnd{nullptr};
    static inline BITMAPINFO bmi{};
    static inline std::array<uint8_t, maxBufferSize> frontBuffer{};
    static inline std::array<uint8_t, maxBufferSize> backBuffer{};
    static inline std::mutex bufferMutex;
    static inline bool hasNewFrame = false;
    static inline LONG clientWidth{256}, clientHeight{256};
public:
    ImageWindow() {
        std::fill(frontBuffer.begin(), frontBuffer.end(), uint8_t{0});
        std::fill(backBuffer.begin(), backBuffer.end(), uint8_t{0});
        initializeBitmapInfo();
        registerWindowClass();
        createWindow();
    }

    ~ImageWindow() {
        if (hwnd) DestroyWindow(hwnd);
    }

    void updateImage(const Frame& frame) {
        if (frame.width != static_cast<uint32_t>(clientWidth) || frame.height != static_cast<uint32_t>(clientHeight)) {
            clientWidth = static_cast<LONG>(frame.width);
            clientHeight = static_cast<LONG>(frame.height);
            bmi.bmiHeader.biWidth = clientWidth;
            bmi.bmiHeader.biHeight = -clientHeight; // Negative to flip the image
            
            // Calculate new window size including window decorations
            RECT rect = {0, 0, clientWidth, clientHeight};
            AdjustWindowRect(&rect, WS_OVERLAPPEDWINDOW, FALSE);
            
            // Resize the window while maintaining its position
            RECT currentPos;
            GetWindowRect(hwnd, &currentPos);
            SetWindowPos(hwnd, NULL, 
                currentPos.left, currentPos.top,
                rect.right - rect.left, rect.bottom - rect.top,
                SWP_NOZORDER | SWP_NOACTIVATE);
                
            InvalidateRect(hwnd, NULL, TRUE);
        }
        if (frame.dataSize < maxBufferSize) {
            std::lock_guard<std::mutex> lock(bufferMutex);
            std::copy(frame.data.begin(), frame.data.begin() + frame.dataSize, backBuffer.begin());
            frontBuffer.swap(backBuffer);
            hasNewFrame = true;
        }
        else
            throw std::runtime_error("Received frame size exceeds buffer size");
        InvalidateRect(hwnd, NULL, FALSE);
    }

    void processMessages() {
        MSG msg{};
        while (GetMessage(&msg, NULL, 0, 0) > 0) {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
    }

private:
    void initializeBitmapInfo() {
        bmi.bmiHeader = {sizeof(BITMAPINFOHEADER), clientWidth, -clientHeight, 1, 8, BI_RGB, 0, 0, 0, 256, 256};

        // Set up grayscale palette
        for (int i = 0; i < 256; ++i) {
            bmi.bmiColors[i] = {static_cast<BYTE>(i), static_cast<BYTE>(i), static_cast<BYTE>(i), 0};
        }
    }

    void registerWindowClass() {
        const wchar_t* className = L"ImageReceiverClass";
        WNDCLASSW wc = {CS_HREDRAW | CS_VREDRAW,     WindowProc, 0,    0,        GetModuleHandle(NULL), NULL,
                        LoadCursor(NULL, IDC_ARROW), NULL,       NULL, className};

        if (!RegisterClassW(&wc)) { throw std::runtime_error("Failed to register window class"); }
    }

    void createWindow() {
        RECT rect = {0, 0, clientWidth, clientHeight};
        AdjustWindowRect(&rect, WS_OVERLAPPEDWINDOW, FALSE);

        hwnd = CreateWindowExW(0, L"ImageReceiverClass", WINDOW_TITLE, WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT,
                               rect.right - rect.left, rect.bottom - rect.top, NULL, NULL, GetModuleHandle(NULL), NULL);

        if (!hwnd) { throw std::runtime_error("Failed to create window"); }

        ShowWindow(hwnd, SW_SHOW);
        UpdateWindow(hwnd);
    }

    static LRESULT CALLBACK WindowProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
        switch (msg) {
        case WM_PAINT: {
            PAINTSTRUCT ps;
            HDC hdc = BeginPaint(hwnd, &ps);

            std::lock_guard<std::mutex> lock(bufferMutex);
            SetDIBitsToDevice(hdc, 0, 0, clientWidth, clientHeight, 0, 0, 0, clientHeight, frontBuffer.data(), &bmi,
                              DIB_RGB_COLORS);

            EndPaint(hwnd, &ps);
            return 0;
        }

        case WM_DESTROY: PostQuitMessage(0); return 0;
        }
        return DefWindowProcW(hwnd, msg, wParam, lParam);
    }
};

class ImageServer {
    SOCKET listenSocket;
    std::atomic<bool> running{true};
    ImageWindow& window;
    std::vector<std::thread> clientThreads;
    std::mutex clientThreadsMutex;

public:
    ImageServer(ImageWindow& win) : window(win) {
        WSADATA wsaData;
        if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) { throw std::runtime_error("Failed to initialize WinSock"); }

        listenSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
        if (listenSocket == INVALID_SOCKET) {
            WSACleanup();
            throw std::runtime_error("Failed to create socket");
        }

        // Enable socket reuse to prevent "address already in use" errors
        int reuseAddr = 1;
        if (setsockopt(listenSocket, SOL_SOCKET, SO_REUSEADDR, reinterpret_cast<char*>(&reuseAddr), sizeof(reuseAddr)) ==
            SOCKET_ERROR) {
            closesocket(listenSocket);
            WSACleanup();
            throw std::runtime_error("Failed to set socket options");
        }

        sockaddr_in serverAddr = {};
        serverAddr.sin_family = AF_INET;
        serverAddr.sin_addr.s_addr = INADDR_ANY;
        serverAddr.sin_port = htons(SERVER_PORT);

        if (bind(listenSocket, (sockaddr*) &serverAddr, sizeof(serverAddr)) == SOCKET_ERROR) {
            closesocket(listenSocket);
            WSACleanup();
            throw std::runtime_error("Failed to bind socket");
        }

        if (listen(listenSocket, SOMAXCONN) == SOCKET_ERROR) {
            closesocket(listenSocket);
            WSACleanup();
            throw std::runtime_error("Failed to listen on socket");
        }
    }

    ~ImageServer() {
        running = false;

        // Close the listen socket to stop accept()
        if (listenSocket != INVALID_SOCKET) {
            closesocket(listenSocket);
            listenSocket = INVALID_SOCKET;
        }

        // Wait for all client threads to finish
        std::lock_guard<std::mutex> lock(clientThreadsMutex);
        for (auto& thread : clientThreads) {
            if (thread.joinable()) { thread.join(); }
        }
        clientThreads.clear();

        WSACleanup();
    }

    void run() {
        while (running) {
            SOCKET clientSocket = accept(listenSocket, NULL, NULL);
            if (clientSocket == INVALID_SOCKET) {
                if (running) {
                    // Only log error if we're still supposed to be running
                    OutputDebugStringW(L"Accept failed\n");
                }
                continue;
            }

            // Create new thread and add it to our vector
            std::lock_guard<std::mutex> lock(clientThreadsMutex);
            clientThreads.emplace_back([this, clientSocket]() { handleClient(clientSocket); });
        }
    }

    void stop() {
        running = false;
        // Force accept() to return by closing the socket
        if (listenSocket != INVALID_SOCKET) {
            closesocket(listenSocket);
            listenSocket = INVALID_SOCKET;
        }
    }

private:
    void handleClient(SOCKET clientSocket) {
        Frame frame;

        while (running) {
            // Receive frame header
            int headerBytesReceived = recv(clientSocket, reinterpret_cast<char*>(&frame), sizeof(uint32_t) * 4, 0);
            if (headerBytesReceived <= 0) break;

            // Receive frame data
            size_t totalReceived = 0;
            while (totalReceived < frame.dataSize && running) {
                int received = recv(clientSocket, reinterpret_cast<char*>(frame.data.data()) + totalReceived,
                                    static_cast<int>(frame.dataSize - totalReceived), 0);
                if (received <= 0) {
                    closesocket(clientSocket);
                    return;
                }
                totalReceived += received;
            }

            // Only update the window if we're still running
            if (running) { window.updateImage(frame); }
        }

        closesocket(clientSocket);
    }
};

int WINAPI wWinMain(HINSTANCE, HINSTANCE, LPWSTR, int) {
    try {
        ImageWindow window;
        ImageServer server(window);

        // Start server in a separate thread
        std::thread serverThread([&server]() { server.run(); });

        // Process window messages in main thread
        window.processMessages();

        // Clean up server thread when window closes
        server.stop();
        if (serverThread.joinable()) { serverThread.join(); }

        return 0;
    }
    catch (const std::exception& e) {
        MessageBoxA(NULL, e.what(), "Error", MB_ICONERROR | MB_OK);
        return 1;
    }
}