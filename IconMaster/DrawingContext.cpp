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

    winrt::Windows::UI::Color DrawingContext::Quantize(winrt::Windows::UI::Color const& c) const noexcept
    {
        if (m_mode >= 32)
        {
            return c;
        }

        // Reduced modes carry a 1-bit alpha: a pixel is either fully opaque or clear.
        const uint8_t a = (c.A >= 128) ? 0xFF : 0x00;
        if (a == 0)
        {
            return winrt::Windows::UI::Color{ 0, 0, 0, 0 };
        }

        uint8_t r = c.R, g = c.G, b = c.B;
        switch (m_mode)
        {
        case 24: // full RGB
            break;
        case 8: // RGB332: 3 bits R, 3 bits G, 2 bits B
            r = static_cast<uint8_t>((c.R >> 5) * 255 / 7);
            g = static_cast<uint8_t>((c.G >> 5) * 255 / 7);
            b = static_cast<uint8_t>((c.B >> 6) * 255 / 3);
            break;
        case 4: // nearest of the 16 standard VGA colours
        {
            static const uint8_t pal[16][3] = {
                {   0,   0,   0 }, { 128,   0,   0 }, {   0, 128,   0 }, { 128, 128,   0 },
                {   0,   0, 128 }, { 128,   0, 128 }, {   0, 128, 128 }, { 192, 192, 192 },
                { 128, 128, 128 }, { 255,   0,   0 }, {   0, 255,   0 }, { 255, 255,   0 },
                {   0,   0, 255 }, { 255,   0, 255 }, {   0, 255, 255 }, { 255, 255, 255 } };
            int best = 0;
            long bestd = -1;
            for (int k = 0; k < 16; ++k)
            {
                const long dr = static_cast<long>(c.R) - pal[k][0];
                const long dg = static_cast<long>(c.G) - pal[k][1];
                const long db = static_cast<long>(c.B) - pal[k][2];
                const long d = dr * dr + dg * dg + db * db;
                if (bestd < 0 || d < bestd) { bestd = d; best = k; }
            }
            r = pal[best][0]; g = pal[best][1]; b = pal[best][2];
            break;
        }
        case 1: // black or white by luminance
        {
            const int luma = (299 * c.R + 587 * c.G + 114 * c.B) / 1000;
            r = g = b = (luma >= 128) ? 0xFF : 0x00;
            break;
        }
        default:
            break;
        }
        return winrt::Windows::UI::Color{ a, r, g, b };
    }

    void DrawingContext::SetPixel(int32_t x, int32_t y, winrt::Windows::UI::Color const& color)
    {
        if (!InBounds(x, y))
        {
            return;
        }

        const winrt::Windows::UI::Color q = Quantize(color);
        const size_t i = (static_cast<size_t>(y) * m_width + x) * 4;
        m_pixels[i + 0] = q.B;
        m_pixels[i + 1] = q.G;
        m_pixels[i + 2] = q.R;
        m_pixels[i + 3] = q.A;
    }

    void DrawingContext::Clear(winrt::Windows::UI::Color const& color)
    {
        const winrt::Windows::UI::Color q = Quantize(color);
        for (size_t i = 0; i < m_pixels.size(); i += 4)
        {
            m_pixels[i + 0] = q.B;
            m_pixels[i + 1] = q.G;
            m_pixels[i + 2] = q.R;
            m_pixels[i + 3] = q.A;
        }
    }
}
