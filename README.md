# DX11 Hook Framework

A 64-bit C++ framework for **DLL injection** and **DirectX 11 API hooking** with a transparent Dear ImGui overlay and GIF/texture rendering support.

---

## Project Structure

```
winup/
├── CMakeLists.txt          # Root build file
├── vcpkg.json              # Dependency manifest (MinHook via vcpkg)
├── Injector/
│   ├── CMakeLists.txt
│   └── Injector.cpp        # LoadLibrary injector (OpenProcess / VirtualAllocEx / CreateRemoteThread)
└── DLL/
    ├── CMakeLists.txt
    ├── dllmain.cpp          # DLL entry point + init thread
    ├── hooks/
    │   ├── hook_manager.h/cpp  # MinHook init/uninit wrappers
    │   └── dx11_hook.h/cpp    # IDXGISwapChain::Present detour (vtable method)
    ├── overlay/
    │   └── overlay.h/cpp      # Dear ImGui init, render, click-through window patch
    └── media/
        ├── texture_loader.h/cpp # Load D3D11 texture from raw RGBA byte array
        └── gif_player.h/cpp     # Frame-sequenced GIF playback using texture_loader
```

---

## Dependencies

### 1. MinHook (via vcpkg)
```powershell
# Install vcpkg if you haven't already
git clone https://github.com/microsoft/vcpkg.git C:\vcpkg
C:\vcpkg\bootstrap-vcpkg.bat

# Install minhook for x64
C:\vcpkg\vcpkg install minhook:x64-windows-static
```

### 2. Dear ImGui (manual vendor)
Download from https://github.com/ocornut/imgui and place the following files into `DLL/vendor/imgui/`:
```
imgui.h / imgui.cpp
imgui_draw.cpp
imgui_tables.cpp
imgui_widgets.cpp
imgui_internal.h
imconfig.h
imgui_impl_dx11.h / imgui_impl_dx11.cpp
imgui_impl_win32.h / imgui_impl_win32.cpp
```

---

## Building

```powershell
cmake -S . -B build -A x64 `
      -DCMAKE_TOOLCHAIN_FILE=C:/vcpkg/scripts/buildsystems/vcpkg.cmake `
      -DVCPKG_TARGET_TRIPLET=x64-windows-static

cmake --build build --config Release
```

Outputs:
- `build/Release/Injector.exe`
- `build/Release/HookDLL.dll`

---

## Usage

```powershell
# 1. Launch your target DirectX 11 application and note its PID (Task Manager -> Details tab)
# 2. Run the injector
.\build\Release\Injector.exe <PID> "C:\full\path\to\HookDLL.dll"
```

The ImGui overlay will appear over the target window. It is **transparent and click-through** by default.

---

## GIF Rendering

Decode your GIF externally (e.g., with stb_image or a GIF decoder lib) to produce per-frame RGBA pixel buffers, then:

```cpp
GifPlayer gif;
gif.AddFrame(framePixels0, width, height, 33 /*ms*/);
gif.AddFrame(framePixels1, width, height, 33);
// ... in your render loop:
gif.Tick(deltaMs);
TextureLoader::DrawTexture({100.f, 100.f}, {width, height}, gif.GetCurrentSRV());
```

---

## Technical Notes

| Topic | Detail |
|---|---|
| Hook method | Vtable pointer harvest via a scratch D3D11 device; `MH_CreateHook` at `Present` index 8 |
| Overlay window | `WS_EX_TRANSPARENT \| WS_EX_LAYERED` applied to the target HWND — ImGui receives no mouse input |
| Texture format | `DXGI_FORMAT_R8G8B8A8_UNORM`, immutable usage — one SRV per frame |
| Thread safety | Init done once from `DLL_PROCESS_ATTACH` thread; hook context assumed single-threaded render |

---

## Disclaimer

> This tool is intended for **research, game modding, and development tooling** on applications you own or have permission to modify. Do not use it to circumvent anti-cheat systems or violate software terms of service.
