#pragma once
#include <cstdint>
#include "DeletableDc.h"
#include "HBitmap.h"
#include "HBrush.h"

namespace test_video_generator {
    class VideoRenderer final {
    public:
        explicit VideoRenderer(uint32_t width, uint32_t height);

        VideoRenderer(const VideoRenderer& other) = delete;
        VideoRenderer& operator=(const VideoRenderer& other) = delete;

        VideoRenderer(VideoRenderer&& other) = delete;
        VideoRenderer& operator=(VideoRenderer&& other) = delete;

        void render(uint64_t millis, bool isBeep);

        uint8_t* data() const;

    private:
        uint8_t* m_pData;
        uint32_t m_width;
        uint32_t m_height;
        helpers::DeletableDc m_drawDc;
        helpers::HBitmap m_bitmap;
        uint32_t m_backgroundColor;
        helpers::HBrush m_textBrush;
        helpers::HBrush m_backgroundBrush;
    };
}
