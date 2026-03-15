#pragma once

#include <d3d11.h>
#include <wrl/client.h>
#include <vector>

#include "vendor/imgui/imgui.h"

// Forward declaration
class TextureLoader;

struct GifFrame {
    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> srv;
    int durationMs;
};

class GifPlayer {
public:
    GifPlayer(ID3D11Device* device);
    ~GifPlayer() = default;

    // Add a frame from RGBA memory to the sequence
    bool AddFrame(const BYTE* srgba, int width, int height, int durationMs);

    // Call each frame with the time elapsed since the last render
    void Tick(float deltaMs);

    // Get the SRV for the currently active frame
    ID3D11ShaderResourceView* GetCurrentSRV() const;

private:
    Microsoft::WRL::ComPtr<ID3D11Device> m_device;
    std::vector<GifFrame> m_frames;

    size_t m_currentFrameIndex = 0;
    float m_playbackAccumulatorMs = 0.0f;
};
