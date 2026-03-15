#include <Windows.h>
#include <d3d11.h>
#include <tchar.h>
#include <vector>
#include <string>
#include <fstream>
#include <dwmapi.h>
#include <shellapi.h>

#include <nlohmann/json.hpp>
using json = nlohmann::json;

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

// Media item structure
struct MediaItem {
    std::string filepath;
    float x = 50.0f;
    float y = 50.0f;
    float width = 200.0f;
    float height = 200.0f;
    float scale = 1.0f;
    float opacity = 1.0f;
    bool isVisible = true;
    
    // Internal D3D references
    ID3D11ShaderResourceView* srv = nullptr;
    GifPlayer* gif = nullptr;
    bool isGif = false;
};

// Global config state
static std::vector<MediaItem> g_mediaItems;
static int g_SelectedItemIndex = -1;
const std::string CONFIG_FILE = "overlay_config.json";

// Helper prototypes
bool CreateDeviceD3D(HWND hWnd);
void CleanupDeviceD3D();
void CreateRenderTarget();
void CleanupRenderTarget();
void SetupTrayIcon(HWND hWnd);
void RemoveTrayIcon();
void LoadConfig();
void SaveConfig();
void ClearMedia();
void AddMediaItem(HWND owner);
bool LoadMediaFromFile(const std::string& path, MediaItem& out_item);
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

    // Load configuration
    LoadConfig();

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
                
                // Force window to update its style and Z-order
                ::SetWindowPos(hwnd, g_OverlayLocked ? HWND_TOPMOST : HWND_NOTOPMOST, 0, 0, 0, 0, 
                    SWP_NOMOVE | SWP_NOSIZE | SWP_FRAMECHANGED);

                if (!g_OverlayLocked) {
                    ::SetForegroundWindow(hwnd);
                    ::SetActiveWindow(hwnd);
                }
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

            // Media Manager Window
            ImGui::SetNextWindowPos(ImVec2(100, 100), ImGuiCond_FirstUseEver);
            ImGui::SetNextWindowSize(ImVec2(600, 400), ImGuiCond_FirstUseEver);
            if (ImGui::Begin("Media Dashboard", nullptr, ImGuiWindowFlags_MenuBar))
            {
                if (ImGui::BeginMenuBar()) {
                    if (ImGui::MenuItem("Add New Media")) { AddMediaItem(hwnd); }
                    if (ImGui::MenuItem("Save All")) { SaveConfig(); }
                    if (ImGui::MenuItem("Clear All")) { g_mediaItems.clear(); g_SelectedItemIndex = -1; }
                    ImGui::EndMenuBar();
                }

                // Left Sidebar: Media List
                ImGui::BeginChild("Sidebar", ImVec2(200, 0), true);
                for (size_t i = 0; i < g_mediaItems.size(); ++i) {
                    ImGui::PushID((int)i);
                    bool isSelected = (g_SelectedItemIndex == (int)i);
                    
                    // Thumbnail (tiny)
                    ID3D11ShaderResourceView* thumbSrv = g_mediaItems[i].srv;
                    if (g_mediaItems[i].gif) thumbSrv = g_mediaItems[i].gif->GetCurrentSRV();
                    
                    if (thumbSrv) {
                        ImGui::Image(ImTextureRef((ImU64)(size_t)thumbSrv), ImVec2(30, 30));
                        ImGui::SameLine();
                    }

                    // Extract filename from path
                    std::string fullPath = g_mediaItems[i].filepath;
                    size_t lastSlash = fullPath.find_last_of("\\/");
                    std::string fileName = (lastSlash == std::string::npos) ? fullPath : fullPath.substr(lastSlash + 1);

                    if (ImGui::Selectable(fileName.c_str(), isSelected)) {
                        g_SelectedItemIndex = (int)i;
                    }
                    ImGui::PopID();
                }
                ImGui::EndChild();

                ImGui::SameLine();

                // Right Panel: Settings
                ImGui::BeginGroup();
                if (g_SelectedItemIndex >= 0 && g_SelectedItemIndex < (int)g_mediaItems.size()) {
                    auto& sel = g_mediaItems[g_SelectedItemIndex];
                    ImGui::Text("Settings: %s", sel.filepath.c_str());
                    ImGui::Separator();
                    
                    ImGui::Checkbox("Visible", &sel.isVisible);
                    ImGui::SliderFloat("Opacity", &sel.opacity, 0.0f, 1.0f);
                    ImGui::SliderFloat("Scale", &sel.scale, 0.1f, 5.0f);
                    
                    ImGui::Separator();
                    ImGui::Text("Manual Position (X, Y)");
                    ImGui::DragFloat("X", &sel.x, 1.0f);
                    ImGui::DragFloat("Y", &sel.y, 1.0f);
                    
                    ImGui::Separator();
                    if (ImGui::Button("Remove Item", ImVec2(-1, 0))) {
                        if (sel.srv) sel.srv->Release();
                        if (sel.gif) delete sel.gif;
                        g_mediaItems.erase(g_mediaItems.begin() + g_SelectedItemIndex);
                        g_SelectedItemIndex = -1;
                    }
                } else {
                    ImGui::Text("Select an item to edit settings.");
                }
                ImGui::EndGroup();
            }
            ImGui::End();
        }

        // Draw all media items
        for (size_t i = 0; i < g_mediaItems.size(); ++i)
        {
            auto& item = g_mediaItems[i];
            if (!item.srv && !item.gif) continue; // Skip invalid

            ImGuiWindowFlags window_flags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_AlwaysAutoResize;
            
            if (g_OverlayLocked)
            {
                window_flags |= ImGuiWindowFlags_NoInputs | ImGuiWindowFlags_NoMove;
            }

            ImGui::SetNextWindowBgAlpha(0.0f); // Always invisible background
            ImGui::SetNextWindowPos(ImVec2(item.x, item.y), ImGuiCond_FirstUseEver);

            // Create a unique window name for each item
            char windowName[64];
            sprintf_s(windowName, "MediaWindow_%zu", i);

            // Remove all padding and borders for the floating image effect
            ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
            ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);

            if (ImGui::Begin(windowName, nullptr, window_flags))
            {
                // Update X/Y strictly from ImGui's drag system so SaveConfig captures the new position
                if (!g_OverlayLocked) {
                    ImVec2 pos = ImGui::GetWindowPos();
                    item.x = pos.x;
                    item.y = pos.y;
                }

                // If GIF, advance animation and get SRV
                ID3D11ShaderResourceView* currentSrv = item.srv;
                if (item.gif) {
                    item.gif->Tick(io.DeltaTime * 1000.0f);
                    currentSrv = item.gif->GetCurrentSRV();
                }

                if (currentSrv && item.isVisible) {
                    ImGui::ImageWithBg(ImTextureRef((ImU64)(size_t)currentSrv), 
                        ImVec2(item.width * item.scale, item.height * item.scale),
                        ImVec2(0,0), ImVec2(1,1),
                        ImVec4(0,0,0,0),
                        ImVec4(1,1,1, item.opacity));
                }

                // Right-click context menu
                if (!g_OverlayLocked && ImGui::BeginPopupContextWindow())
                {
                    if (ImGui::MenuItem("Quit Overlay"))
                        done = true;
                    ImGui::EndPopup();
                }
            }
            ImGui::End();
            ImGui::PopStyleVar(2);
        }

        // Rendering
        ImGui::Render();

        // Fully transparent clear color when locked, slightly opaque when unlocked to aid hit-testing
        const float clearAlpha = g_OverlayLocked ? 0.0f : 0.01f; 
        const float clear_color_with_alpha[4] = { 0.0f, 0.0f, 0.0f, clearAlpha }; 
        g_pd3dDeviceContext->OMSetRenderTargets(1, &g_mainRenderTargetView, nullptr);
        g_pd3dDeviceContext->ClearRenderTargetView(g_mainRenderTargetView, clear_color_with_alpha);
        
        ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());

        // Present with sync interval 1 (vsync on)
        g_pSwapChain->Present(1, 0);
    }

    // Cleanup
    ::UnregisterHotKey(hwnd, 1);
    SaveConfig();
    ClearMedia();

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

void ClearMedia()
{
    for (auto& item : g_mediaItems) {
        if (item.srv) { item.srv->Release(); item.srv = nullptr; }
        if (item.gif) { delete item.gif; item.gif = nullptr; }
    }
    g_mediaItems.clear();
}

void SaveConfig()
{
    json j = json::array();
    for (const auto& item : g_mediaItems) {
        j.push_back({
            {"filepath", item.filepath},
            {"x", item.x},
            {"y", item.y},
            {"width", item.width},
            {"height", item.height},
            {"scale", item.scale},
            {"opacity", item.opacity},
            {"isVisible", item.isVisible},
            {"isGif", item.isGif}
        });
    }
    std::ofstream out(CONFIG_FILE);
    out << j.dump(4);
}

void LoadConfig()
{
    ClearMedia();
    std::ifstream in(CONFIG_FILE);
    if (!in.is_open()) return;

    try {
        json j;
        in >> j;
        for (const auto& elem : j) {
            MediaItem item;
            item.filepath = elem.value("filepath", "");
            item.x = elem.value("x", 50.0f);
            item.y = elem.value("y", 50.0f);
            item.width = elem.value("width", 200.0f);
            item.height = elem.value("height", 200.0f);
            item.scale = elem.value("scale", 1.0f);
            item.opacity = elem.value("opacity", 1.0f);
            item.isVisible = elem.value("isVisible", true);
            item.isGif = elem.value("isGif", false);
            
            if (!item.filepath.empty()) {
                LoadMediaFromFile(item.filepath, item);
            }
            g_mediaItems.push_back(item);
        }
    } catch (...) {
        // basic error handling
    }
}

bool LoadMediaFromFile(const std::string& path, MediaItem& out_item)
{
    out_item.filepath = path;
    
    // Check if GIF
    bool isGifFile = false;
    if (path.length() > 4) {
        std::string ext = path.substr(path.length() - 4);
        for (auto& c : ext) c = (char)tolower(c);
        if (ext == ".gif") isGifFile = true;
    }

    if (isGifFile) {
        std::ifstream file(path, std::ios::binary | std::ios::ate);
        if (!file.is_open()) return false;
        std::streamsize size = file.tellg();
        file.seekg(0, std::ios::beg);
        std::vector<char> buffer((size_t)size);
        if (!file.read(buffer.data(), size)) return false;

        out_item.gif = new GifPlayer(g_pd3dDevice);
        out_item.isGif = true;

        int* delays = nullptr;
        int w, h, frames, comp;
        unsigned char* data = stbi_load_gif_from_memory((const stbi_uc*)buffer.data(), (int)buffer.size(), &delays, &w, &h, &frames, &comp, 4);
        
        if (data) {
            for (int i = 0; i < frames; ++i) {
                out_item.gif->AddFrame(data + (w * h * 4 * i), w, h, delays[i]);
            }
            stbi_image_free(data);
            if (delays) stbi_image_free(delays);
            if (out_item.width == 200.0f && out_item.height == 200.0f) {
                out_item.width = (float)w;
                out_item.height = (float)h;
            }
            return true;
        } else {
            delete out_item.gif;
            out_item.gif = nullptr;
            return false;
        }
    } else {
        int w, h, ch;
        unsigned char* imgData = stbi_load(path.c_str(), &w, &h, &ch, 4);
        if (imgData) {
            TextureLoader::LoadFromMemory(g_pd3dDevice, imgData, (size_t)w * h * 4, w, h, &out_item.srv);
            stbi_image_free(imgData);
            if (out_item.width == 200.0f && out_item.height == 200.0f) {
                out_item.width = (float)w;
                out_item.height = (float)h;
            }
            out_item.isGif = false;
            return true;
        }
    }
    return false;
}

void AddMediaItem(HWND owner)
{
    OPENFILENAMEW ofn;
    wchar_t szFile[260] = { 0 };

    ZeroMemory(&ofn, sizeof(ofn));
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = owner;
    ofn.lpstrFile = szFile;
    ofn.nMaxFile = sizeof(szFile);
    ofn.lpstrFilter = L"Images\0*.PNG;*.JPG;*.JPEG;*.BMP;*.GIF\0All\0*.*\0";
    ofn.nFilterIndex = 1;
    ofn.lpstrFileTitle = nullptr;
    ofn.nMaxFileTitle = 0;
    ofn.lpstrInitialDir = nullptr;
    ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST | OFN_NOCHANGEDIR;

    if (GetOpenFileNameW(&ofn) == TRUE)
    {
        // Convert wide string to narrow string for json/stbi
        int narrowSize = WideCharToMultiByte(CP_UTF8, 0, szFile, -1, nullptr, 0, nullptr, nullptr);
        std::string narrowPath(narrowSize - 1, '\0');
        WideCharToMultiByte(CP_UTF8, 0, szFile, -1, narrowPath.data(), narrowSize, nullptr, nullptr);

        MediaItem item;
        if (LoadMediaFromFile(narrowPath, item)) {
            g_mediaItems.push_back(item);
            g_SelectedItemIndex = (int)g_mediaItems.size() - 1;
            SaveConfig();
        }
    }
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
