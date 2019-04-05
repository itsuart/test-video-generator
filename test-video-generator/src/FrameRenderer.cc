#include "FrameRenderer.h"
#include <cassert>
namespace test_video_generator {
    FrameRenderer::FrameRenderer(uint32_t width, uint32_t height)
        : m_width(width)
        , m_height(height)
        , m_pData(nullptr)
        , m_drawDc(::CreateCompatibleDC(::GetDC(nullptr)))
        , m_bitmap()
        , m_backgroundColor(0xCC'CC'CC)
        , m_backgroundBrush(::CreateSolidBrush(m_backgroundColor))
        , m_textBrush(::CreateSolidBrush(0))
    {
        ::BITMAPINFO bitmapInfo = {};
        {
            ::BITMAPINFOHEADER& header = bitmapInfo.bmiHeader;
            header.biBitCount = 32;
            header.biClrImportant = 0;
            header.biClrUsed = 0;
            header.biCompression = BI_RGB;
            header.biHeight = -((int32_t)height);
            header.biPlanes = 1;
            header.biSize = sizeof(::BITMAPINFOHEADER);
            header.biSizeImage = 0;
            header.biWidth = width;
            header.biXPelsPerMeter = 1;
            header.biYPelsPerMeter = 1;
        }


        m_bitmap = helpers::HBitmap(::CreateDIBSection(m_drawDc, &bitmapInfo, DIB_RGB_COLORS, (void**)&m_pData, nullptr, 0));
        assert(m_bitmap);

        ::SetBkColor(m_drawDc, m_backgroundColor);
        ::SelectObject(m_drawDc, m_bitmap);
    }

    void FrameRenderer::render(uint64_t millis, bool isBeep) {

        RECT frame;
        frame.left = 0;
        frame.top = 0;
        frame.bottom = m_height;
        frame.right = m_width;
        ::FillRect(m_drawDc, &frame, m_backgroundBrush);

        if (isBeep) {
            ::SelectObject(m_drawDc, m_textBrush);
            ::TextOutA(m_drawDc, 0, 0, "BEEP", 4);
        }


        ::GdiFlush();
    }

    uint8_t* FrameRenderer::data() const {
        return m_pData;
    }
}
