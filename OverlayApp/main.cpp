#include <Windows.h>
#include <d3d11.h>
#include <tchar.h>
#include <vector>
#include <dwmapi.h>
#include <shellapi.h>

#include "vendor/imgui/imgui.h"
#include "vendor/imgui/imgui_impl_win32.h"
#include "vendor/imgui/imgui_impl_dx11.h"

// Media
#include "media/texture_loader.h"
#include "media/gif_player.h"

#include "vendor/stb_image.h"

// Forward declare message handler from imgui_impl_win32.cpp
extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

// D3D11 Globals
static ID3D11Device*           g_pd3dDevice = nullptr;
static ID3D11DeviceContext*    g_pd3dDeviceContext = nullptr;
static IDXGISwapChain*         g_pSwapChain = nullptr;
static ID3D11RenderTargetView* g_mainRenderTargetView = nullptr;

// Tray Icon messages
#define WM_TRAYICON (WM_USER + 1)
#define TRAY_QUIT_ID 1001

// Global Tray Icon Data
static NOTIFYICONDATAW g_nid = {};

// Helper prototypes
bool CreateDeviceD3D(HWND hWnd);
void CleanupDeviceD3D();
void CreateRenderTarget();
void CleanupRenderTarget();
void SetupTrayIcon(HWND hWnd);
void RemoveTrayIcon();
LRESULT WINAPI WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

// The Standalone Overlay Application
int APIENTRY wWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPWSTR lpCmdLine, int nCmdShow)
{
    // Define window style: Popup (no borders), Transparent (alpha blending), Layered (required for transparency), Topmost, ToolWindow (hides from taskbar)
    DWORD dwExStyle = WS_EX_TOPMOST | WS_EX_LAYERED | WS_EX_TRANSPARENT | WS_EX_TOOLWINDOW;
    DWORD dwStyle = WS_POPUP;

    WNDCLASSEXW wc = {
        sizeof(wc), CS_CLASSDC, WndProc, 0L, 0L, hInstance, nullptr, nullptr, nullptr, nullptr,
        L"WinUpOverlayClass", nullptr
    };
    ::RegisterClassExW(&wc);

    // Create the window covering the entire virtual screen
    int screenW = ::GetSystemMetrics(SM_CXVIRTUALSCREEN);
    int screenH = ::GetSystemMetrics(SM_CYVIRTUALSCREEN);
    int screenX = ::GetSystemMetrics(SM_XVIRTUALSCREEN);
    int screenY = ::GetSystemMetrics(SM_YVIRTUALSCREEN);

    HWND hwnd = ::CreateWindowExW(
        dwExStyle, wc.lpszClassName, L"WinUp Global Overlay",
        dwStyle,
        screenX, screenY, screenW, screenH,
        nullptr, nullptr, wc.hInstance, nullptr
    );

    // Initialize Direct3D
    if (!CreateDeviceD3D(hwnd))
    {
        CleanupDeviceD3D();
        ::UnregisterClassW(wc.lpszClassName, wc.hInstance);
        return 1;
    }

    // Extend window frame into the client area to allow DWM transparency
    MARGINS margins = { -1, -1, -1, -1 };
    ::DwmExtendFrameIntoClientArea(hwnd, &margins);

    // Show the window and set the layer opacity to allow transparent D3D11 rendering
    ::SetLayeredWindowAttributes(hwnd, 0, 255, LWA_ALPHA);
    ::ShowWindow(hwnd, SW_SHOW);
    ::UpdateWindow(hwnd);

    // Setup System Tray Icon
    SetupTrayIcon(hwnd);

    // Setup Dear ImGui context
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO(); (void)io;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;

    // Setup Dear ImGui style (dark)
    ImGui::StyleColorsDark();

    // Setup Platform/Renderer backends
    ImGui_ImplWin32_Init(hwnd);
    ImGui_ImplDX11_Init(g_pd3dDevice, g_pd3dDeviceContext);

    // Load user image
    ID3D11ShaderResourceView* userImageSrv = nullptr;
    int imgW = 0, imgH = 0, imgCh = 0;
    unsigned char* imgData = stbi_load("image.png", &imgW, &imgH, &imgCh, 4);
    if (imgData)
    {
        TextureLoader::LoadFromMemory(g_pd3dDevice, imgData, imgW * imgH * 4, imgW, imgH, &userImageSrv);
        stbi_image_free(imgData);
    }

    // Register Global Hotkey for INSERT (id 1)
    ::RegisterHotKey(hwnd, 1, 0, VK_INSERT);

    // Main loop
    bool done = false;
    bool g_OverlayLocked = true;

    while (!done)
    {
        // Poll and handle messages
        MSG msg;
        while (::PeekMessage(&msg, nullptr, 0U, 0U, PM_REMOVE))
        {
            if (msg.message == WM_HOTKEY && msg.wParam == 1)
            {
                g_OverlayLocked = !g_OverlayLocked;
                DWORD exStyle = ::GetWindowLongW(hwnd, GWL_EXSTYLE);
                if (g_OverlayLocked)
                    ::SetWindowLongW(hwnd, GWL_EXSTYLE, exStyle | WS_EX_TRANSPARENT);
                else
                    ::SetWindowLongW(hwnd, GWL_EXSTYLE, exStyle & ~WS_EX_TRANSPARENT);
                
                // Force window to update its style
                ::SetWindowPos(hwnd, g_OverlayLocked ? HWND_TOPMOST : HWND_NOTOPMOST, 0, 0, 0, 0, 
                    SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_FRAMECHANGED);
            }

            ::TranslateMessage(&msg);
            ::DispatchMessage(&msg);
            if (msg.message == WM_QUIT)
                done = true;
        }
        if (done)
            break;

        // Start the Dear ImGui frame
        ImGui_ImplDX11_NewFrame();
        ImGui_ImplWin32_NewFrame();
        ImGui::NewFrame();

        // ---------------------------------------------------------
        // OVERLAY LOGIC
        // ---------------------------------------------------------

        // If unlocked, draw a fullscreen dim background so the user knows desktop clicks are blocked
        if (!g_OverlayLocked)
        {
            ImGui::SetNextWindowPos(ImVec2(0, 0));
            ImGui::SetNextWindowSize(io.DisplaySize);
            ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.0f, 0.0f, 0.0f, 0.4f));
            ImGui::Begin("DimBg", nullptr, 
                ImGuiWindowFlags_NoDecoration | 
                ImGuiWindowFlags_NoInputs | 
                ImGuiWindowFlags_NoSavedSettings | 
                ImGuiWindowFlags_NoFocusOnAppearing | 
                ImGuiWindowFlags_NoBringToFrontOnFocus);
            ImGui::End();
            ImGui::PopStyleColor();
            
            // Show global instructions
            ImGui::SetNextWindowPos(ImVec2(screenW / 2.0f, 50.0f), ImGuiCond_Always, ImVec2(0.5f, 0.0f));
            ImGui::SetNextWindowBgAlpha(0.8f);
            if (ImGui::Begin("EditModeText", nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoInputs))
            {
                ImGui::TextColored(ImVec4(0, 1, 0, 1), "EDIT MODE ACTIVE");
                ImGui::Text("Drag your image to move it. Right-click the image to Exit.");
                ImGui::Text("Press [INSERT] to lock the overlay and return to your desktop.");
            }
            ImGui::End();
        }

        // Draw the image window
        if (userImageSrv)
        {
            ImGuiWindowFlags window_flags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_AlwaysAutoResize;
            
            if (g_OverlayLocked)
            {
                window_flags |= ImGuiWindowFlags_NoInputs | ImGuiWindowFlags_NoMove;
                ImGui::SetNextWindowBgAlpha(0.0f); // completely invisible background
            }
            else
            {
                // Slight background when editing to see bounds
                ImGui::SetNextWindowBgAlpha(0.2f);
            }

            // Default position in bottom right if no ini file is loaded
            ImGui::SetNextWindowPos(ImVec2(screenW - imgW - 20.0f, screenH - imgH - 20.0f), ImGuiCond_FirstUseEver);

            if (ImGui::Begin("OverlayImageWindow", nullptr, window_flags))
            {
                ImGui::Image((void*)userImageSrv, ImVec2((float)imgW, (float)imgH));

                // Right-click context menu (only accessible when not locked)
                if (!g_OverlayLocked && ImGui::BeginPopupContextWindow())
                {
                    if (ImGui::MenuItem("Quit Overlay"))
                        done = true;
                    ImGui::EndPopup();
                }
            }
            ImGui::End();
        }

        // ---------------------------------------------------------

        // Rendering
        ImGui::Render();

        const float clear_color_with_alpha[4] = { 0.0f, 0.0f, 0.0f, 0.0f }; // Fully transparent clear color
        g_pd3dDeviceContext->OMSetRenderTargets(1, &g_mainRenderTargetView, nullptr);
        g_pd3dDeviceContext->ClearRenderTargetView(g_mainRenderTargetView, clear_color_with_alpha);
        
        ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());

        // Present with sync interval 1 (vsync on)
        g_pSwapChain->Present(1, 0);
    }

    // Cleanup
    ::UnregisterHotKey(hwnd, 1);

    if (userImageSrv) { userImageSrv->Release(); }

    ImGui_ImplDX11_Shutdown();
    ImGui_ImplWin32_Shutdown();
    ImGui::DestroyContext();

    RemoveTrayIcon();
    CleanupDeviceD3D();
    ::DestroyWindow(hwnd);
    ::UnregisterClassW(wc.lpszClassName, wc.hInstance);

    return 0;
}

// ----------------------------------------------------------------------------------------------------
// D3D11 Helper Functions
// ----------------------------------------------------------------------------------------------------

bool CreateDeviceD3D(HWND hWnd)
{
    // Setup swap chain
    DXGI_SWAP_CHAIN_DESC sd;
    ZeroMemory(&sd, sizeof(sd));
    sd.BufferCount = 2;
    sd.BufferDesc.Width = 0;
    sd.BufferDesc.Height = 0;
    sd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    sd.BufferDesc.RefreshRate.Numerator = 60;
    sd.BufferDesc.RefreshRate.Denominator = 1;
    sd.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;
    sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    sd.OutputWindow = hWnd;
    sd.SampleDesc.Count = 1;
    sd.SampleDesc.Quality = 0;
    sd.Windowed = TRUE;
    sd.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;

    UINT createDeviceFlags = 0;
    D3D_FEATURE_LEVEL featureLevel;
    const D3D_FEATURE_LEVEL featureLevelArray[2] = { D3D_FEATURE_LEVEL_11_0, D3D_FEATURE_LEVEL_10_0, };
    
    HRESULT res = D3D11CreateDeviceAndSwapChain(
        nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, createDeviceFlags, featureLevelArray, 2,
        D3D11_SDK_VERSION, &sd, &g_pSwapChain, &g_pd3dDevice, &featureLevel, &g_pd3dDeviceContext);
        
    if (res == DXGI_ERROR_UNSUPPORTED) // Try high-performance WARP software driver if hardware is not available.
        res = D3D11CreateDeviceAndSwapChain(
            nullptr, D3D_DRIVER_TYPE_WARP, nullptr, createDeviceFlags, featureLevelArray, 2,
            D3D11_SDK_VERSION, &sd, &g_pSwapChain, &g_pd3dDevice, &featureLevel, &g_pd3dDeviceContext);
            
    if (res != S_OK)
        return false;

    CreateRenderTarget();
    return true;
}

void CleanupDeviceD3D()
{
    CleanupRenderTarget();
    if (g_pSwapChain) { g_pSwapChain->Release(); g_pSwapChain = nullptr; }
    if (g_pd3dDeviceContext) { g_pd3dDeviceContext->Release(); g_pd3dDeviceContext = nullptr; }
    if (g_pd3dDevice) { g_pd3dDevice->Release(); g_pd3dDevice = nullptr; }
}

void CreateRenderTarget()
{
    ID3D11Texture2D* pBackBuffer;
    g_pSwapChain->GetBuffer(0, IID_PPV_ARGS(&pBackBuffer));
    g_pd3dDevice->CreateRenderTargetView(pBackBuffer, nullptr, &g_mainRenderTargetView);
    pBackBuffer->Release();
}

void CleanupRenderTarget()
{
    if (g_mainRenderTargetView) { g_mainRenderTargetView->Release(); g_mainRenderTargetView = nullptr; }
}

void SetupTrayIcon(HWND hWnd)
{
    ZeroMemory(&g_nid, sizeof(NOTIFYICONDATAW));
    g_nid.cbSize = sizeof(NOTIFYICONDATAW);
    g_nid.hWnd = hWnd;
    g_nid.uID = 1;
    g_nid.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP;
    g_nid.uCallbackMessage = WM_TRAYICON;
    g_nid.hIcon = ::LoadIcon(nullptr, IDI_APPLICATION);
    wcscpy_s(g_nid.szTip, L"WinUp Global Overlay");

    ::Shell_NotifyIconW(NIM_ADD, &g_nid);
}

void RemoveTrayIcon()
{
    ::Shell_NotifyIconW(NIM_DELETE, &g_nid);
}

// Win32 message handler
LRESULT WINAPI WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    if (ImGui_ImplWin32_WndProcHandler(hWnd, msg, wParam, lParam))
        return true;

    switch (msg)
    {
        case WM_TRAYICON:
            if (lParam == WM_RBUTTONUP)
            {
                POINT pt;
                ::GetCursorPos(&pt);
                HMENU hMenu = ::CreatePopupMenu();
                ::InsertMenuW(hMenu, 0, MF_BYPOSITION | MF_STRING, TRAY_QUIT_ID, L"Quit Overlay");
                
                // SetForegroundWindow ensures the menu closes if clicked outside
                ::SetForegroundWindow(hWnd);
                ::TrackPopupMenu(hMenu, TPM_BOTTOMALIGN | TPM_LEFTALIGN, pt.x, pt.y, 0, hWnd, nullptr);
                ::DestroyMenu(hMenu);
            }
            return 0;

        case WM_COMMAND:
            if (LOWORD(wParam) == TRAY_QUIT_ID)
            {
                ::PostQuitMessage(0);
            }
            return 0;

        case WM_SIZE:
            if (g_pd3dDevice != nullptr && wParam != SIZE_MINIMIZED)
            {
                CleanupRenderTarget();
                g_pSwapChain->ResizeBuffers(0, (UINT)LOWORD(lParam), (UINT)HIWORD(lParam), DXGI_FORMAT_UNKNOWN, 0);
                CreateRenderTarget();
            }
            return 0;
        case WM_SYSCOMMAND:
            if ((wParam & 0xfff0) == SC_KEYMENU) // Disable ALT application menu
                return 0;
            break;
        case WM_DESTROY:
            ::PostQuitMessage(0);
            return 0;
    }
    return ::DefWindowProcW(hWnd, msg, wParam, lParam);
}
