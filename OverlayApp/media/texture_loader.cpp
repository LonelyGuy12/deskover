#include <Windows.h>
#include "texture_loader.h"

bool TextureLoader::LoadFromMemory(
    ID3D11Device* device,
    const unsigned char* image_data,
    size_t image_size_bytes,
    int width,
    int height,
    ID3D11ShaderResourceView** out_srv)
{
    if (!device || !image_data || image_size_bytes == 0 || width <= 0 || height <= 0)
        return false;

    // Create texture description (immutable since it won't change)
    D3D11_TEXTURE2D_DESC desc = {};
    desc.Width = width;
    desc.Height = height;
    desc.MipLevels = 1;
    desc.ArraySize = 1;
    desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    desc.SampleDesc.Count = 1;
    desc.Usage = D3D11_USAGE_IMMUTABLE;
    desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;

    D3D11_SUBRESOURCE_DATA subResource = {};
    subResource.pSysMem = image_data;
    subResource.SysMemPitch = width * 4;
    subResource.SysMemSlicePitch = width * height * 4;

    ID3D11Texture2D* pTexture = nullptr;
    HRESULT hr = device->CreateTexture2D(&desc, &subResource, &pTexture);
    if (FAILED(hr))
        return false;

    // Create shader resource view
    D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
    srvDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    srvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
    srvDesc.Texture2D.MipLevels = desc.MipLevels;
    srvDesc.Texture2D.MostDetailedMip = 0;

    hr = device->CreateShaderResourceView(pTexture, &srvDesc, out_srv);
    pTexture->Release(); // Srv holds the reference now
    
    return SUCCEEDED(hr);
}

void TextureLoader::DrawTexture(ImVec2 pos, ImVec2 size, ID3D11ShaderResourceView* srv)
{
    if (!srv) return;

    // We create a completely transparent full-screen window to host the image
    ImGuiWindowFlags window_flags = 
        ImGuiWindowFlags_NoTitleBar      | 
        ImGuiWindowFlags_NoScrollbar     | 
        ImGuiWindowFlags_NoInputs        |
        ImGuiWindowFlags_NoSavedSettings |
        ImGuiWindowFlags_NoNav           |
        ImGuiWindowFlags_NoBackground    |
        ImGuiWindowFlags_NoBringToFrontOnFocus;

    ImGuiIO& io = ImGui::GetIO();
    ImGui::SetNextWindowPos(ImVec2(0, 0), ImGuiCond_Always);
    ImGui::SetNextWindowSize(io.DisplaySize, ImGuiCond_Always);
    
    // Push zero padding so that our positions are absolute relative to screen (0,0)
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
    
    // Ensure the window doesn't capture clicks
    if (ImGui::Begin("media_canvas", nullptr, window_flags))
    {
        ImDrawList* drawList = ImGui::GetWindowDrawList();
        drawList->AddImage((ImTextureID)srv, pos, ImVec2(pos.x + size.x, pos.y + size.y));
    }
    ImGui::End();
    
    ImGui::PopStyleVar();
}
