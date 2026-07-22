#include "pch.h"
#include "DrawingContext.h"
#if __has_include("DrawingContext.g.cpp")
#include "DrawingContext.g.cpp"
#endif

#include <robuffer.h>

using namespace winrt;
using namespace winrt::Windows::UI;
using namespace winrt::Microsoft::UI::Xaml::Media::Imaging;

namespace winrt::IconMaster::implementation
{
    DrawingContext::DrawingContext(int32_t width, int32_t height)
        : m_width(width)
        , m_height(height)
        , m_bitmap(width, height)
    {
        // Start fully transparent.
        Clear(Windows::UI::Color{ 0x00, 0x00, 0x00, 0x00 });
    }

    uint8_t* DrawingContext::BufferData()
    {
        auto buffer = m_bitmap.PixelBuffer();
        auto byteAccess = buffer.as<::Windows::Storage::Streams::IBufferByteAccess>();
        uint8_t* data{ nullptr };
        winrt::check_hresult(byteAccess->Buffer(&data));
        return data;
    }

    void DrawingContext::SetPixel(int32_t x, int32_t y, Windows::UI::Color const& color)
    {
        if (x < 0 || x >= m_width || y < 0 || y >= m_height)
        {
            return;
        }

        uint8_t* data = BufferData();
        const int32_t i = (y * m_width + x) * 4;
        // WriteableBitmap uses BGRA8 layout.
        data[i + 0] = color.B;
        data[i + 1] = color.G;
        data[i + 2] = color.R;
        data[i + 3] = color.A;
    }

    void DrawingContext::Clear(Windows::UI::Color const& color)
    {
        uint8_t* data = BufferData();
        const int32_t count = m_width * m_height;
        for (int32_t p = 0; p < count; ++p)
        {
            const int32_t i = p * 4;
            data[i + 0] = color.B;
            data[i + 1] = color.G;
            data[i + 2] = color.R;
            data[i + 3] = color.A;
        }
    }

    void DrawingContext::Invalidate()
    {
        m_bitmap.Invalidate();
    }
}
