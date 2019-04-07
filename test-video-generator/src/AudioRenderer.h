#pragma once
#include <cstdint>
#include <vector>

namespace test_video_generator {
    class AudioRenderer final {
    public:
        AudioRenderer(unsigned nChannels, unsigned samplesPerFrame, unsigned samplesPerSecond);

        AudioRenderer(const AudioRenderer& other) = delete;
        AudioRenderer& operator=(const AudioRenderer& other) = delete;

        AudioRenderer(AudioRenderer&& other) = delete;
        AudioRenderer& operator=(AudioRenderer&& other) = delete;

        uint64_t millisRendered() const;

        void renderFrameInto(uint8_t* pData);

    private:
        uint64_t m_millisRendered;
        std::size_t m_samplesPerChannelPerFrame;
        std::size_t m_samplesPerSecond;
        std::size_t m_nChannels;
        std::size_t m_millisPerFrame;
        std::vector<float> m_signal;
        std::size_t m_nextReadIndex;
    };
}
