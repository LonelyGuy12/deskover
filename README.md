# WinUp Overlay: Global Desktop Media Manager

A premium C++ application for creating a global, click-through desktop overlay. Manage and display images and animated GIFs seamlessly on your desktop with a professional dashboard.

---

## Key Features

- **Global Overlay**: Displays media on top of all windows, including games and applications.
- **Smart Click-Through**: Toggle between "Interactive Dashboard" and "Seamless Click-Through" modes instantly.
- **Premium Dashboard**: A dedicated UI (**Press `INSERT`**) to manage your desktop overlays:
  - **Sidebar Navigation**: Quickly switch between added media with thumbnail previews.
  - **Per-Item Opacity**: Adjust transparency for each overlay individualy (0-100%).
  - **Real-Time Scaling**: Resize your images and GIFs from 0.1x to 5.0x size.
  - **Visibility Toggles**: Hide/Show media without deleting it.
- **GIF Support**: High-performance animated GIF playback with transparency.
- **Persistent Configuration**: All positions, scales, and opacity settings are saved automatically to `overlay_config.json`.
- **System Tray Integration**: Manage the application directly from your taskbar.

---

## Project Structure

```
winup/
├── CMakeLists.txt          # Build configuration
├── vcpkg.json              # Dependencies (nlohmann-json, etc.)
└── OverlayApp/
    ├── main.cpp            # Application entry, Window Management, Dashboard UI
    ├── media/
    │   ├── texture_loader.h/cpp # High-performance image loading (D3D11)
    │   └── gif_player.h/cpp     # Frame-sequenced GIF animation
    └── vendor/
        ├── imgui/          # Dear ImGui UI framework
        ├── stb_image.h     # Image decoding
        └── ...
```

---

## Building

### Prerequisites
- **CMake** (3.15+)
- **vcpkg** (for JSON support)
- **C++20 Compiler** (Visual Studio 2022 recommended)

### Build Steps
```powershell
# 1. Configure CMake
cmake -S . -B build -A x64 `
      -DCMAKE_TOOLCHAIN_FILE=C:/vcpkg/scripts/buildsystems/vcpkg.cmake

# 2. Build Release version
cmake --build build --config Release
```

The output will be located at `.\build\OverlayApp\Release\WinUpOverlay.exe`.

---

## Usage Guide

1. **Launch**: Run `WinUpOverlay.exe`.
2. **Dashboard Mode**: Press **`INSERT`** to unlock your mouse and open the Media Dashboard.
   - Use the **Menu Bar** at the top to add new images (`.png`, `.jpg`, `.gif`).
   - Select an item from the **Sidebar thumbnails** to edit its properties.
   - **Scale** and **Opacity** sliders apply changes in real-time.
3. **Overlay Mode**: Press **`INSERT`** again. The dashboard closes, and your images become click-through part of your desktop.
4. **Exit**: Right-click the system tray icon and select **Quit**, or use the "Quit Overlay" option in the Dashboard context menu (Right-click on any image in Edit Mode).

---

## Technical Details

| Component | Technology |
|---|---|
| Rendering | DirectX 11 |
| UI Framework | Dear ImGui |
| Configuration | JSON (nlohmann) |
| Image Decoding | stb_image |

---

## License & Credits

- **Dear ImGui** by Omar Cornut
- **nlohmann-json** for configuration management
- **stb_image** for media decoding

---

## Disclaimer

This tool is intended for personal desktop customization and development. Ensure you have the rights to use the media you display as overlays.
