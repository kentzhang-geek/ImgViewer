#include "DX12Renderer.h"
#include "ImgViewerUI.h"
#include "Logger.h"
#include "backends/imgui_impl_dx12.h"
#include "backends/imgui_impl_win32.h"
#include "framework.h"
#include "imgui.h"
#include "pch.h"
#include <boost/program_options.hpp>
#include <dwmapi.h>
#include <iostream>
#include <shellapi.h>
#include <string>

namespace po = boost::program_options;

#pragma comment(lib, "dwmapi.lib")

#define MAX_LOADSTRING 100
#define WINDOW_NAME L"BorderlessWindowClass"

// Global Variables:
HINSTANCE hInst; // Current Instance

// DX12 Renderer
DX12Renderer *g_pRenderer = nullptr;
ImgViewerUI *g_pViewerUI = nullptr;

// Borderless Window Config
const int g_ResizeBorderWidth = 8;
const int g_TitleBarHeight = 32;

// Forward declarations of functions included in this code module:
ATOM MyRegisterClass(HINSTANCE hInstance);
BOOL InitInstance(HINSTANCE, int);
LRESULT CALLBACK WndProc(HWND, UINT, WPARAM, LPARAM);

/**
 * @brief Main entry point of the application.
 */
int APIENTRY wWinMain(_In_ HINSTANCE hInstance,
                      _In_opt_ HINSTANCE hPrevInstance, _In_ LPWSTR lpCmdLine,
                      _In_ int nCmdShow) {
  UNREFERENCED_PARAMETER(hPrevInstance);
  UNREFERENCED_PARAMETER(lpCmdLine);

  // Parse command line arguments
  int argc;
  LPWSTR *argv = CommandLineToArgvW(GetCommandLineW(), &argc);

  bool verbose = false;
  std::wstring inputFilePath;

  try {
    po::options_description desc("Allowed options");
    desc.add_options()("help,h", "produce help message")(
        "verbose,v", "enable verbose logging")(
        "input-file", po::wvalue<std::wstring>(&inputFilePath),
        "input file to open");

    po::positional_options_description p;
    p.add("input-file", -1);

    po::variables_map vm;
    po::store(
        po::wcommand_line_parser(argc, argv).options(desc).positional(p).run(),
        vm);
    po::notify(vm);

    if (vm.count("help")) {
      if (AttachConsole(ATTACH_PARENT_PROCESS)) {
        freopen("CONOUT$", "w", stdout);
        freopen("CONOUT$", "w", stderr);
      }
      std::cout << desc << "\n";
      return 0;
    }

    if (vm.count("verbose")) {
      verbose = true;
    }

  } catch (const std::exception &e) {
    std::string what = e.what();
    std::wstring whatW(what.begin(), what.end());
    MessageBoxW(nullptr, whatW.c_str(), L"Error parsing command line arguments",
                MB_OK | MB_ICONERROR);
    return 1;
  }
  LocalFree(argv);

  // Initialize logger
  if (verbose) {
    Logger::Get().Init("log.txt");
  }
  LOG("=== ImgViewer Starting ===");

  // Enable DPI awareness for proper mouse coordinates with Windows scaling
  SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
  LOG("DPI awareness set to PER_MONITOR_AWARE_V2");

  MyRegisterClass(hInstance);

  if (!InitInstance(hInstance, nCmdShow)) {
    LOG_ERROR("InitInstance failed!");
    return FALSE;
  }

  // Load input file if present
  if (!inputFilePath.empty()) {
    char narrowFilename[MAX_PATH];
    size_t convertedChars = 0;
    wcstombs_s(&convertedChars, narrowFilename, MAX_PATH, inputFilePath.c_str(),
               _TRUNCATE);
    g_pViewerUI->HandleDragDrop(std::string(narrowFilename));
  }

  MSG msg = {};

  LOG("Entering main loop...");

  while (msg.message != WM_QUIT) {
    // Process all pending messages
    while (PeekMessage(&msg, NULL, 0U, 0U, PM_REMOVE)) {
      TranslateMessage(&msg);
      DispatchMessage(&msg);
      if (msg.message == WM_QUIT)
        break;
    }

    if (msg.message == WM_QUIT)
      break;

    // Render continuously
    if (g_pRenderer) {
      // Start ImGui frame
      ImGui_ImplDX12_NewFrame();
      ImGui_ImplWin32_NewFrame();
      ImGui::NewFrame();

      // Render UI (this collects ImGui draw commands and saves image render
      // info)
      if (g_pViewerUI) {
        g_pViewerUI->Render();
      }

      // Finalize ImGui
      ImGui::Render();

      // Start recording commands
      g_pRenderer->BeginRender();

      // Render image TO TEXTURE first
      // This renders the image content into an off-screen texture that ImGui
      // will display
      if (g_pViewerUI) {
        g_pViewerUI->RenderImageToTexture(g_pRenderer->GetCommandList());
      }

      // RESTORE Main Render Target (Back Buffer) and Viewport
      // We must switch back to the screen buffer before letting ImGui render
      // the UI
      D3D12_CPU_DESCRIPTOR_HANDLE backBufferRTV = g_pRenderer->GetCurrentRTV();
      g_pRenderer->GetCommandList()->OMSetRenderTargets(1, &backBufferRTV,
                                                        FALSE, nullptr);

      D3D12_VIEWPORT viewport = {0.0f,
                                 0.0f,
                                 (float)g_pRenderer->GetWidth(),
                                 (float)g_pRenderer->GetHeight(),
                                 0.0f,
                                 1.0f};
      D3D12_RECT scissor = {0, 0, (LONG)g_pRenderer->GetWidth(),
                            (LONG)g_pRenderer->GetHeight()};
      g_pRenderer->GetCommandList()->RSSetViewports(1, &viewport);
      g_pRenderer->GetCommandList()->RSSetScissorRects(1, &scissor);

      // Render ImGui NEXT (foreground)
      // This draws the UI, including the image widget (which samples the
      // texture we just rendered)
      ImGui_ImplDX12_RenderDrawData(ImGui::GetDrawData(),
                                    g_pRenderer->GetCommandList());

      // End recording and present
      g_pRenderer->EndRender();
    }
  }

  // Cleanup
  LOG("Shutting down...");

  if (g_pViewerUI) {
    delete g_pViewerUI;
    g_pViewerUI = nullptr;
  }

  if (g_pRenderer) {
    ImGui_ImplDX12_Shutdown();
    ImGui_ImplWin32_Shutdown();
    ImGui::DestroyContext();
    delete g_pRenderer;
    g_pRenderer = nullptr;
  }

  LOG("=== ImgViewer Shutdown Complete ===");
  Logger::Get().Close();

  return (int)msg.wParam;
}

/**
 * @brief Registers the window class.
 * @param hInstance Application instance handle.
 * @return ATOM identifying the registered class.
 */
ATOM MyRegisterClass(HINSTANCE hInstance) {
  // Register the windows class
  WNDCLASSEXW wcx{};
  wcx.cbSize = sizeof(wcx);
  wcx.style = CS_HREDRAW | CS_VREDRAW;
  wcx.hInstance = nullptr;
  wcx.lpfnWndProc = WndProc;
  wcx.lpszClassName = WINDOW_NAME;
  wcx.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_WINDOW + 1);
  wcx.hCursor = ::LoadCursorW(nullptr, IDC_ARROW);
  return RegisterClassExW(&wcx);
}

/**
 * @brief Saves instance handle and creates main window.
 *
 * In this function, we save the instance handle in a global variable and
 * create and display the main program window.
 *
 * @param hInstance Instance handle.
 * @param nCmdShow Show window command.
 * @return BOOL True on success, False on failure.
 */
BOOL InitInstance(HINSTANCE hInstance, int nCmdShow) {
  hInst = hInstance; // Store instance handle in our global variable

  // This example uses a non-resizable 640 by 480 viewport for simplicity.
  int nDefaultWidth = 1920;
  int nDefaultHeight = 1080;

  // Use WS_POPUP to remove all standard decorations, but we need some frames
  // for standard behavior logic internally often standard WS_OVERLAPPEDWINDOW
  // is fine if we handle WM_NCCALCSIZE to remove it visually Let's use
  // WS_OVERLAPPEDWINDOW but we will strip the client area via WM_NCCALCSIZE
  HWND hWnd = CreateWindowExW(0, WINDOW_NAME, L"ImgViewer",
                              WS_OVERLAPPEDWINDOW | WS_VISIBLE, CW_USEDEFAULT,
                              CW_USEDEFAULT, nDefaultWidth, nDefaultHeight,
                              nullptr, nullptr, hInstance, nullptr);

  // show and update window
  if (!hWnd) {
    return FALSE;
  }

  ShowWindow(hWnd, nCmdShow);
  UpdateWindow(hWnd);

  // DWM Rounded Corners (Windows 11+)
  // DWMWCP_ROUND = 2
  DWM_WINDOW_CORNER_PREFERENCE preference = DWMWCP_ROUND;
  DwmSetWindowAttribute(hWnd, DWMWA_WINDOW_CORNER_PREFERENCE, &preference,
                        sizeof(preference));

  // Initialize DX12 Renderer
  LOG("Initializing DX12 Renderer...");

  // Get actual client area size (may differ from window size due to borders,
  // DPI, etc.)
  RECT clientRect;
  GetClientRect(hWnd, &clientRect);
  UINT actualWidth = clientRect.right - clientRect.left;
  UINT actualHeight = clientRect.bottom - clientRect.top;
  LOG("Window size: %dx%d, Client area: %ux%u", nDefaultWidth, nDefaultHeight,
      actualWidth, actualHeight);

  g_pRenderer = new DX12Renderer();
  if (!g_pRenderer->Initialize(hWnd, actualWidth, actualHeight)) {
    LOG_ERROR("Failed to initialize DX12 Renderer!");
    delete g_pRenderer;
    g_pRenderer = nullptr;
    return FALSE;
  }
  LOG("DX12 Renderer initialized successfully");

  // Setup ImGui
  LOG("Initializing ImGui...");
  IMGUI_CHECKVERSION();
  ImGui::CreateContext();
  ImGuiIO &io = ImGui::GetIO();
  (void)io;
  io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
  io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;

  ImGui::StyleColorsDark();

  ImGui_ImplWin32_Init(hWnd);

  // Load Fonts
  // Default Font (Consolas 16px)
  io.Fonts->AddFontFromFileTTF("C:\\Windows\\Fonts\\consola.ttf", 16.0f);

  // Title Font (Consolas 24px) - We will access this as io.Fonts->Fonts[1]
  io.Fonts->AddFontFromFileTTF("C:\\Windows\\Fonts\\consola.ttf", 24.0f);

  // Initialize ImGui DX12 backend with new struct-based API
  ImGui_ImplDX12_InitInfo initInfo = {};
  initInfo.Device = g_pRenderer->GetDevice();
  initInfo.CommandQueue = g_pRenderer->GetCommandQueue();
  initInfo.NumFramesInFlight = 2;
  initInfo.RTVFormat = DXGI_FORMAT_R8G8B8A8_UNORM;
  initInfo.SrvDescriptorHeap = g_pRenderer->GetSrvHeap();
  initInfo.LegacySingleSrvCpuDescriptor =
      g_pRenderer->GetSrvHeap()->GetCPUDescriptorHandleForHeapStart();
  initInfo.LegacySingleSrvGpuDescriptor =
      g_pRenderer->GetSrvHeap()->GetGPUDescriptorHandleForHeapStart();

  LOG("ImGui DX12 init - SrvHeap=%p, CpuDesc=%llu, GpuDesc=%llu",
      initInfo.SrvDescriptorHeap, initInfo.LegacySingleSrvCpuDescriptor.ptr,
      initInfo.LegacySingleSrvGpuDescriptor.ptr);

  ImGui_ImplDX12_Init(&initInfo);
  LOG("ImGui initialized successfully");

  // Initialize ImgViewerUI
  g_pViewerUI = new ImgViewerUI();
  g_pViewerUI->Initialize(g_pRenderer);
  LOG("ImgViewerUI initialized successfully");

  // Enable drag and drop
  DragAcceptFiles(hWnd, TRUE);
  LOG("Drag and drop enabled");

  return TRUE;
}

/**
 * @brief Processes messages for the main window.
 *
 * WM_COMMAND  - Process the application menu
 * WM_PAINT    - Paint the main window
 * WM_DESTROY  - Post a quit message and return
 *
 * @param hWnd Window handle.
 * @param message Message ID.
 * @param wParam Word parameter.
 * @param lParam Long parameter.
 * @return LRESULT Message processing result.
 */
LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam,
                         LPARAM lParam) {
  extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(
      HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);
  if (ImGui_ImplWin32_WndProcHandler(hWnd, message, wParam, lParam))
    return true;

  switch (message) {
  case WM_NCCALCSIZE: {
    // Intercept this to remove the standard window frame while keeping the
    // window logic
    if (wParam == TRUE && g_pViewerUI) {
      // If we return 0, the Client Rect will be the same as the Window Rect
      // (removing borders/caption) However, we need to consider if the window
      // is maximized. When maximized, a standard window extends slightly
      // off-screen (shadow/borders). We should adjust for that to prevent
      // content being cut off.

      // Standard approach for borderless window:
      // Return 0 (handled) to indicate we are handling the client area
      // calculation. But we might need to adjust rgrc[0] if maximized? Let's
      // check IsZoomed(hWnd)
      if (IsZoomed(hWnd)) {
        // Adjust for the border that flows offscreen when maximized
        // Logic omitted for brevity, usually you get monitor info and clamp to
        // work area But usually simply returning 0 is "good enough" for basic
        // borderless, just edges might be hidden. Let's try just returning 0
        // first.
      }
      return 0;
    }
    break;
  }
  case WM_NCHITTEST: {
    // Custom Hit Testing for Resizing and Dragging
    // 1. Let Windows calculate the default result first? No, we need to
    // override client area behavior.

    POINT pt = {LOWORD(lParam), HIWORD(lParam)}; // Screen coordinates
    ScreenToClient(hWnd, &pt);

    RECT rcClient;
    GetClientRect(hWnd, &rcClient);

    // Check resize borders
    if (pt.y >= rcClient.bottom - g_ResizeBorderWidth) {
      if (pt.x <= g_ResizeBorderWidth)
        return HTBOTTOMLEFT;
      if (pt.x >= rcClient.right - g_ResizeBorderWidth)
        return HTBOTTOMRIGHT;
      return HTBOTTOM;
    }
    if (pt.y <= g_ResizeBorderWidth) {
      if (pt.x <= g_ResizeBorderWidth)
        return HTTOPLEFT;
      if (pt.x >= rcClient.right - g_ResizeBorderWidth)
        return HTTOPRIGHT;
      return HTTOP;
    }
    if (pt.x <= g_ResizeBorderWidth)
      return HTLEFT;
    if (pt.x >= rcClient.right - g_ResizeBorderWidth)
      return HTRIGHT;

    // Title Bar Dragging Logic
    if (pt.y < g_TitleBarHeight) {
      // Explicit Hit Testing for interactive areas to allow dragging in empty
      // space
      // 1. Menu Area (Left side): Icon + "File" menu.
      float interactWidth =
          g_pViewerUI ? g_pViewerUI->GetTitleBarInteractWidth() : 400.0f;
      if ((pt.x < interactWidth) && (pt.x > (interactWidth - 55)))
        return HTCLIENT;

      // 2. Window Controls (Right side): 3 Buttons * 46px = 138px.
      //    Let's use 150px to be safe.
      if (pt.x > rcClient.right - 150)
        return HTCLIENT; // Let ImGui handle buttons

      // Otherwise, we are in the "Title Bar Background".
      if (!IsZoomed(hWnd)) {
        return HTCAPTION;
      }
      // Allow dragging maximized window to restore/unsnap
      return HTCAPTION;
    }

    return HTCLIENT;
  }

  case WM_COMMAND:
    break;
  case WM_SIZE:
    if (g_pRenderer && wParam != SIZE_MINIMIZED) {
      UINT width = LOWORD(lParam);
      UINT height = HIWORD(lParam);
      g_pRenderer->OnResize(width, height);
    }
    break;
  case WM_DROPFILES: {
    HDROP hDrop = (HDROP)wParam;
    UINT fileCount = DragQueryFileW(hDrop, 0xFFFFFFFF, nullptr, 0);
    if (fileCount > 0 && g_pViewerUI) {
      WCHAR filename[MAX_PATH];
      if (DragQueryFileW(hDrop, 0, filename, MAX_PATH)) {
        // Convert wide string to narrow string
        char narrowFilename[MAX_PATH];
        size_t convertedChars = 0;
        wcstombs_s(&convertedChars, narrowFilename, MAX_PATH, filename,
                   _TRUNCATE);
        g_pViewerUI->HandleDragDrop(std::string(narrowFilename));
      }
    }
    DragFinish(hDrop);
  } break;
  case WM_PAINT: {
    ValidateRect(hWnd, NULL);
  }
    return 0;
  case WM_DESTROY:
    PostQuitMessage(0);
    break;
  // keyboard events
  case WM_KEYDOWN: {
    switch (wParam) {
    case VK_ESCAPE:
      exit(0);
      break;
    default:
      // wasd control position, up down left right control direction
      break;
    }
  } break;
  case WM_KEYUP: {
  } break;
  default:
    return DefWindowProc(hWnd, message, wParam, lParam);
  }
  return 0;
}
