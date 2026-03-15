#include <Windows.h>
#include "gif_player.h"

#include "texture_loader.h"

GifPlayer::GifPlayer(ID3D11Device* device)
    : m_device(device)
{
}

bool GifPlayer::AddFrame(const BYTE* srgba, int width, int height, int durationMs)
{
    if (!m_device) return false;

    ID3D11ShaderResourceView* srv = nullptr;
    if (TextureLoader::LoadFromMemory(m_device.Get(), srgba, static_cast<size_t>(width * height * 4), width, height, &srv))
    {
        GifFrame frame;
        frame.srv.Attach(srv);
        frame.durationMs = durationMs;
        m_frames.push_back(frame);
        return true;
    }
    return false;
}

void GifPlayer::Tick(float deltaMs)
{
    if (m_frames.empty()) return;

    m_playbackAccumulatorMs += deltaMs;

    int currentDuration = m_frames[m_currentFrameIndex].durationMs;
    // Advance frames if enough time has passed
    while (m_playbackAccumulatorMs >= currentDuration)
    {
        m_playbackAccumulatorMs -= currentDuration;
        m_currentFrameIndex = (m_currentFrameIndex + 1) % m_frames.size();
        currentDuration = m_frames[m_currentFrameIndex].durationMs;
    }
}

ID3D11ShaderResourceView* GifPlayer::GetCurrentSRV() const
{
    if (m_frames.empty()) return nullptr;
    return m_frames[m_currentFrameIndex].srv.Get();
}
