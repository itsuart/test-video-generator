#include "AudioRenderer.h"
#include <cassert>
#include <cstring>
#include <cmath>

// here we generate 2second long signal and just copy it as appropriate

namespace {
    std::vector<float> pregenerate_signal(unsigned nChannels, unsigned samplesPerSecond)
    {
        const std::size_t oneSecondSize = ((std::size_t)samplesPerSecond) * nChannels;
        static float ToneFrequencyInHz = 500;
        static float constexpr PI = 3.1415f;


        std::vector<float> result(2 * oneSecondSize, 0);

        // 1 kHz means that there is 1000 complete sine waves
        // each complete sine wave is 2*pi
        // so per sample angle is changed by:
        const float angleDeltaPerSample = ToneFrequencyInHz * 2 * PI / samplesPerSecond;
        float currentAngle = 0;

        float* buffer = &result[oneSecondSize];
        for (unsigned sampleIndex = 0; sampleIndex < samplesPerSecond; ++sampleIndex) {
            const float value = std::sinf(currentAngle);

            for (unsigned channelIndx = 0; channelIndx < nChannels; ++channelIndx) {
                buffer[nChannels * sampleIndex + channelIndx] = value;
            }

            currentAngle += angleDeltaPerSample;
        }

        return result;
    }
}

namespace test_video_generator {
    AudioRenderer::AudioRenderer(unsigned nChannels, unsigned samplesPerChannelPerFrame, unsigned samplesPerSecond)
        : m_millisRendered(0)
        , m_samplesPerChannelPerFrame(samplesPerChannelPerFrame)
        , m_samplesPerSecond(samplesPerSecond)
        , m_nChannels(nChannels)
        , m_millisPerFrame(1000 * samplesPerChannelPerFrame / samplesPerSecond)
        , m_signal(pregenerate_signal(nChannels, samplesPerSecond))
        , m_nextReadIndex(0)
    {}

    uint64_t AudioRenderer::millisRendered() const {
        return m_millisRendered;
    }

    void AudioRenderer::renderFrameInto(uint8_t* pData) {
        const std::size_t dataSizeInSamples = m_samplesPerChannelPerFrame * m_nChannels;
        const std::size_t sampleSize = sizeof(float);

        const std::size_t nextReadIndex = m_nextReadIndex + dataSizeInSamples;
        if (nextReadIndex < m_signal.size()) {
            std::memcpy(pData, &m_signal[m_nextReadIndex], dataSizeInSamples * sampleSize);
            m_nextReadIndex = nextReadIndex;
        }
        else {
            const std::size_t part1Size = m_signal.size() - m_nextReadIndex;
            std::memcpy(pData, &m_signal[m_nextReadIndex], part1Size * sampleSize);

            const std::size_t part2Size = nextReadIndex - m_signal.size();
            std::memcpy(pData + part1Size * sampleSize, &m_signal[0], part2Size * sampleSize);

            m_nextReadIndex = part2Size;

            assert(part1Size + part2Size == dataSizeInSamples);
        }

        m_millisRendered += m_millisPerFrame;
    }


}
