#include "AudioRenderer.h"
#include <cassert>
#include <cstring>

namespace test_video_generator {
    AudioRenderer::AudioRenderer(unsigned nChannels, unsigned samplesPerChannelPerFrame, unsigned samplesPerSecond)
        : m_millisRendered(0)
        , m_samplesPerChannelPerFrame(samplesPerChannelPerFrame)
        , m_samplesPerSecond(samplesPerSecond)
        , m_nChannels(nChannels)
        , m_millisPerFrame(1000 * samplesPerChannelPerFrame / samplesPerSecond)
    {}

    uint64_t AudioRenderer::millisRendered() const {
        return m_millisRendered;
    }

    void AudioRenderer::renderFrameInto(float* pData) {
        // render silence
        std::memset(pData, 0, sizeof(float) * m_samplesPerChannelPerFrame * m_nChannels);
        m_millisRendered += m_millisPerFrame;
    }


}
