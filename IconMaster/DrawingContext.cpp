#include "pch.h"
#include "DrawingContext.h"
#if __has_include("DrawingContext.g.cpp")
#include "DrawingContext.g.cpp"
#endif

// Note: the class has a member named `Color` (the current-colour property), which
// shadows the type name inside member scope, so the type is spelled out in full here.

namespace winrt::IconMaster::implementation
{
    DrawingContext::DrawingContext(int32_t width, int32_t height)
        : m_width(width)
        , m_height(height)
        , m_pixels(static_cast<size_t>(width) * height * 4, 0) // fully transparent
    {
    }

    winrt::Windows::UI::Color DrawingContext::GetPixel(int32_t x, int32_t y) const
    {
        if (!InBounds(x, y))
        {
            return winrt::Windows::UI::Color{ 0x00, 0x00, 0x00, 0x00 };
        }

        const size_t i = (static_cast<size_t>(y) * m_width + x) * 4;
        // Stored as BGRA.
        return winrt::Windows::UI::Color{
            m_pixels[i + 3], // A
            m_pixels[i + 2], // R
            m_pixels[i + 1], // G
            m_pixels[i + 0]  // B
        };
    }

    void DrawingContext::SetPixel(int32_t x, int32_t y, winrt::Windows::UI::Color const& color)
    {
        if (!InBounds(x, y))
        {
            return;
        }

        const size_t i = (static_cast<size_t>(y) * m_width + x) * 4;
        m_pixels[i + 0] = color.B;
        m_pixels[i + 1] = color.G;
        m_pixels[i + 2] = color.R;
        m_pixels[i + 3] = color.A;
    }

    void DrawingContext::Clear(winrt::Windows::UI::Color const& color)
    {
        for (size_t i = 0; i < m_pixels.size(); i += 4)
        {
            m_pixels[i + 0] = color.B;
            m_pixels[i + 1] = color.G;
            m_pixels[i + 2] = color.R;
            m_pixels[i + 3] = color.A;
        }
    }
}
