
#define UNICODE
#define _UNICODE
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <array>
#include <chrono>
#include <cstdint>
#include <random>
#include <string>

#pragma comment(lib, "ws2_32.lib")

using namespace std;

// Network configuration
const unsigned short SERVER_PORT = 12345;
const char* SERVER_IP = "127.0.0.1";

struct Frame {
  uint32_t width;
  uint32_t height;
  uint32_t dataSize;
  uint32_t frameNumber;
  std::array<uint8_t, 792 * 793> data;
};

class NoiseWindow {
  static constexpr LONG width{792}, height{793};
  static constexpr UINT timerId{1}, frameIntervalMs{1000 / 25};
  inline static const wstring windowClassName{L"GrayscaleNoiseWindowClass"}, windowTitle{L"Grayscale Noise"};
  inline static BITMAPINFO bmi{};
  inline static Frame currentFrame{};  // Use default constructor
  inline static chrono::steady_clock::time_point lastFpsTime{};
  inline static SOCKET clientSocket = INVALID_SOCKET;
  HDC hdc{nullptr};
  HWND hwnd{nullptr};

public:
  NoiseWindow() {
    // Initialize frame properties before connecting
    currentFrame.width = width;
    currentFrame.height = height;
    currentFrame.dataSize = width * height;
    currentFrame.frameNumber = 0;
    
    initializeWinsock();
    connectToServer();
    initializeBitmapInfo();
    registerWindowClass();
    createWindow();
    lastFpsTime = chrono::steady_clock::now();
    if (!SetTimer(hwnd, timerId, frameIntervalMs, NULL)) throw runtime_error("Failed to set timer");
  }

  ~NoiseWindow() {
    KillTimer(hwnd, timerId);
    if (clientSocket != INVALID_SOCKET) {
      closesocket(clientSocket);
    }
    WSACleanup();
  }

private:
  void initializeWinsock() {
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
      throw runtime_error("Failed to initialize Winsock");
    }
  }

  void connectToServer() {
    clientSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (clientSocket == INVALID_SOCKET) {
      WSACleanup();
      throw runtime_error("Failed to create socket");
    }

    sockaddr_in serverAddr = {};
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(SERVER_PORT);
    inet_pton(AF_INET, SERVER_IP, &serverAddr.sin_addr);

    if (connect(clientSocket, (sockaddr*)&serverAddr, sizeof(serverAddr)) == SOCKET_ERROR) {
      closesocket(clientSocket);
      WSACleanup();
      throw runtime_error("Failed to connect to server");
    }
  }

  static void sendData(size_t frameCount, const Frame& frame) {
    if (clientSocket == INVALID_SOCKET) return;

    // Send frame header first
    if (send(clientSocket, reinterpret_cast<const char*>(&frame), sizeof(uint32_t) * 4, 0) == SOCKET_ERROR) {
      return;
    }

    // Send frame data
    const char* buffer = reinterpret_cast<const char*>(frame.data.data());
    size_t totalSent = 0;
    while (totalSent < frame.dataSize) {
      int sent = send(clientSocket, 
                     buffer + totalSent, 
                     static_cast<int>(frame.dataSize - totalSent), 
                     0);
      if (sent == SOCKET_ERROR) {
        return;
      }
      totalSent += sent;
    }
  }

  static void generateNoise() {
    static random_device rd;
    static mt19937 gen(rd());
    static uniform_int_distribution<> distrib(0, 255);
    currentFrame.width = width;
    currentFrame.height = height;
    currentFrame.frameNumber++;
    for (auto &pixel : currentFrame.data) pixel = static_cast<uint8_t>(distrib(gen));
  }

  void initializeBitmapInfo() {
    bmi.bmiHeader = {sizeof(BITMAPINFOHEADER), width, -height, 1, 8, BI_RGB, width * height, 0, 0, 256, 256};
    for (int i = 0; i < 256; ++i) bmi.bmiColors[i] = {static_cast<BYTE>(i), static_cast<BYTE>(i), static_cast<BYTE>(i), 0};
  }

  void registerWindowClass() {
    WNDCLASSW wc = {CS_OWNDC, WindowProc, 0, 0, GetModuleHandle(NULL), NULL, LoadCursor(NULL, IDC_ARROW), NULL, NULL,
             windowClassName.c_str()};
    if (!RegisterClassW(&wc)) throw runtime_error("Window Registration Failed");
  }

  void createWindow() {
    RECT rect = {0, 0, width, height};
    AdjustWindowRect(&rect, WS_OVERLAPPEDWINDOW, FALSE);
    hwnd = CreateWindowExW(0, windowClassName.c_str(), windowTitle.c_str(), WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT,
                rect.right - rect.left, rect.bottom - rect.top, NULL, NULL, GetModuleHandle(NULL), NULL);
    if (!hwnd) throw runtime_error("Window Creation Failed");
    ShowWindow(hwnd, SW_SHOW);
    UpdateWindow(hwnd);
  }

  static LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    static size_t frameCount{0};
    switch (uMsg) {
    case WM_CREATE: generateNoise(); return 0;
    case WM_DESTROY: PostQuitMessage(0); return 0;
    case WM_TIMER:
      if (wParam == timerId) {
        generateNoise();
        frameCount++;
        sendData(currentFrame);

        auto now = chrono::steady_clock::now();
        auto elapsed = chrono::duration_cast<chrono::seconds>(now - lastFpsTime);
        if (elapsed.count() >= 1) {
          wstring fps = windowTitle + L" - FPS: " + to_wstring(frameCount / elapsed.count());
          SetWindowTextW(hwnd, fps.c_str());
          frameCount = 0;
          lastFpsTime = now;
        }
        InvalidateRect(hwnd, NULL, FALSE);
      }
      return 0;
    case WM_PAINT: {
      PAINTSTRUCT ps;
      HDC hdc = BeginPaint(hwnd, &ps);
      SetDIBitsToDevice(hdc, 0, 0, width, height, 0, 0, 0, height, currentFrame.data.data(), &bmi, DIB_RGB_COLORS);
      EndPaint(hwnd, &ps);
      return 0;
    }
    case WM_ERASEBKGND: return 1;
    default: return DefWindowProc(hwnd, uMsg, wParam, lParam);
    }
  }
};

int WINAPI wWinMain(HINSTANCE /*hInstance*/, HINSTANCE /*hPrevInstance*/, LPWSTR /*lpCmdLine*/, int /*nCmdShow*/) {
  try {
    NoiseWindow noiseWindow;
    MSG msg{};
    while (GetMessage(&msg, NULL, 0, 0) > 0) {
      TranslateMessage(&msg);
      DispatchMessage(&msg);
    }
    return static_cast<int>(msg.wParam);
  } catch (const exception &e) {
    MessageBoxW(NULL, wstring(e.what(), e.what() + strlen(e.what())).c_str(), L"Error", MB_ICONEXCLAMATION | MB_OK);
    return -1;
  }
}
