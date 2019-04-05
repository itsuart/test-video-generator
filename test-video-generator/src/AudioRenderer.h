#pragma once
#include <cstdint>

namespace test_video_generator {
    class AudioRenderer final {
    public:
        AudioRenderer(unsigned nChannels, unsigned samplesPerFrame, unsigned samplesPerSecond);

        AudioRenderer(const AudioRenderer& other) = delete;
        AudioRenderer& operator=(const AudioRenderer& other) = delete;

        AudioRenderer(AudioRenderer&& other) = delete;
        AudioRenderer& operator=(AudioRenderer&& other) = delete;

        uint64_t millisRendered() const;

        void renderFrameInto(float* pData);

    private:
        uint64_t m_millisRendered;
        unsigned m_samplesPerChannelPerFrame;
        unsigned m_samplesPerSecond;
        unsigned m_nChannels;
        unsigned m_millisPerFrame;
    };
}
