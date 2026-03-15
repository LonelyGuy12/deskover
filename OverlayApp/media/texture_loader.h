#pragma once

#include <d3d11.h>
#include "vendor/imgui/imgui.h"

// Static utility class for loading textures to D3D11 SRVs
class TextureLoader {
public:
    // Load an RGBA buffer into a D3D11 Shader Resource View (Immutable usage)
    static bool LoadFromMemory(
        ID3D11Device* device,
        const unsigned char* image_data,
        size_t image_size_bytes, // should be width * height * 4
        int width,
        int height,
        ID3D11ShaderResourceView** out_srv);

    // Helper to draw the loaded texture onto an invisible fullscreen ImGui canvas
    static void DrawTexture(ImVec2 pos, ImVec2 size, ID3D11ShaderResourceView* srv);
};
