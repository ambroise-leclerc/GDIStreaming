#ifdef _WIN64
#ifndef _AMD64_
#define _AMD64_
#endif
#else
#define _X86_
#endif

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <array>
#include <chrono>
#include <cstdint>
#include <random>
#include <string>

using namespace std;

class NoiseWindow {
  static constexpr LONG width{792}, height{793};
  static constexpr UINT timerId{1}, frameIntervalMs{1000 / 25};
  inline static const string windowClassName{"GrayscaleNoiseWindowClass"}, windowTitle{"Grayscale Noise"};
  inline static BITMAPINFO bmi{};
  inline static array<uint8_t, width * height> pixelBuffer{};
  inline static chrono::steady_clock::time_point lastFpsTime{};
  HDC hdc{nullptr};
  HWND hwnd{nullptr};

public:
  NoiseWindow() {
    initializeBitmapInfo();
    registerWindowClass();
    createWindow();
    lastFpsTime = chrono::steady_clock::now();
    if (!SetTimer(hwnd, timerId, frameIntervalMs, NULL)) throw runtime_error("Failed to set timer");
  }

  ~NoiseWindow() { KillTimer(hwnd, timerId); }

private:
  static void generateNoise() {
    static random_device rd;
    static mt19937 gen(rd());
    static uniform_int_distribution<> distrib(0, 255);
    for (auto &pixel : pixelBuffer) pixel = static_cast<uint8_t>(distrib(gen));
  }

  void initializeBitmapInfo() {
    bmi.bmiHeader = {sizeof(BITMAPINFOHEADER), width, -height, 1, 8, BI_RGB, width * height, 0, 0, 256, 256};
    for (int i = 0; i < 256; ++i) bmi.bmiColors[i] = {static_cast<BYTE>(i), static_cast<BYTE>(i), static_cast<BYTE>(i), 0};
  }

  void registerWindowClass() {
    WNDCLASS wc = {CS_OWNDC, WindowProc, 0, 0, GetModuleHandle(NULL), NULL, LoadCursor(NULL, IDC_ARROW), NULL, NULL,
             windowClassName.c_str()};
    if (!RegisterClass(&wc)) throw runtime_error("Window Registration Failed");
  }

  void createWindow() {
    RECT rect = {0, 0, width, height};
    AdjustWindowRect(&rect, WS_OVERLAPPEDWINDOW, FALSE);
    hwnd = CreateWindowEx(0, windowClassName.c_str(), windowTitle.c_str(), WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT,
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
        auto now = chrono::steady_clock::now();
        auto elapsed = chrono::duration_cast<chrono::seconds>(now - lastFpsTime);
        if (elapsed.count() >= 1) {
          SetWindowText(hwnd, (windowTitle + " - FPS: " + to_string(frameCount / elapsed.count())).c_str());
          frameCount = 0;
          lastFpsTime = now;
        }
        InvalidateRect(hwnd, NULL, FALSE);
      }
      return 0;
    case WM_PAINT: {
      PAINTSTRUCT ps;
      HDC hdc = BeginPaint(hwnd, &ps);
      SetDIBitsToDevice(hdc, 0, 0, width, height, 0, 0, 0, height, pixelBuffer.data(), &bmi, DIB_RGB_COLORS);
      EndPaint(hwnd, &ps);
      return 0;
    }
    case WM_ERASEBKGND: return 1;
    default: return DefWindowProc(hwnd, uMsg, wParam, lParam);
    }
  }
};

int WINAPI WinMain(HINSTANCE, HINSTANCE, LPSTR, int) {
  try {
    NoiseWindow noiseWindow;
    MSG msg{};
    while (GetMessage(&msg, NULL, 0, 0) > 0) {
      TranslateMessage(&msg);
      DispatchMessage(&msg);
    }
    return static_cast<int>(msg.wParam);
  } catch (const exception &e) {
    MessageBox(NULL, e.what(), "Error", MB_ICONEXCLAMATION | MB_OK);
    return -1;
  }
}
