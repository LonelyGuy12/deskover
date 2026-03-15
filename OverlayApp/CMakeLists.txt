cmake_minimum_required(VERSION 3.20)

# ---------------------------------------------------------------------------
# Setup ImGui Sources
# ---------------------------------------------------------------------------
set(IMGUI_DIR "${CMAKE_CURRENT_SOURCE_DIR}/vendor/imgui")
file(GLOB IMGUI_SOURCES
    "${IMGUI_DIR}/imgui.cpp"
    "${IMGUI_DIR}/imgui_demo.cpp"
    "${IMGUI_DIR}/imgui_draw.cpp"
    "${IMGUI_DIR}/imgui_tables.cpp"
    "${IMGUI_DIR}/imgui_widgets.cpp"
    "${IMGUI_DIR}/imgui_impl_win32.cpp"
    "${IMGUI_DIR}/imgui_impl_dx11.cpp"
)

# ---------------------------------------------------------------------------
# Setup Media Sources
# ---------------------------------------------------------------------------
set(MEDIA_SOURCES
    "${CMAKE_CURRENT_SOURCE_DIR}/media/texture_loader.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/media/gif_player.cpp"
)

# ---------------------------------------------------------------------------
# Dependencies
# ---------------------------------------------------------------------------
find_package(nlohmann_json CONFIG REQUIRED)

# ---------------------------------------------------------------------------
# Define Executable
# ---------------------------------------------------------------------------
add_executable(WinUpOverlay WIN32
    main.cpp
    vendor/stb_image.cpp
    ${IMGUI_SOURCES}
    ${MEDIA_SOURCES}
)

# ---------------------------------------------------------------------------
# Include Directories & Libraries
# ---------------------------------------------------------------------------
target_include_directories(WinUpOverlay PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}"
    "${IMGUI_DIR}"
)

# Link against DirectX 11 libraries and JSON
target_link_libraries(WinUpOverlay PRIVATE
    d3d11
    dxgi
    d3dcompiler
    dwmapi
    nlohmann_json::nlohmann_json
)

# Enforce C++17
target_compile_features(WinUpOverlay PRIVATE cxx_std_17)
