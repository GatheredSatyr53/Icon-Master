#include "pch.h"
#include "DrawingContext.h"
#if __has_include("DrawingContext.g.cpp")
#include "DrawingContext.g.cpp"
#endif

#include <algorithm>

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
            return c; // 32-bit: passthrough
        }

        // Only the non-indexed reduced mode (24-bit) reaches here: full RGB, 1-bit alpha.
        const uint8_t a = (c.A >= 128) ? 0xFF : 0x00;
        return (a == 0) ? winrt::Windows::UI::Color{ 0, 0, 0, 0 }
                        : winrt::Windows::UI::Color{ a, c.R, c.G, c.B };
    }

    void DrawingContext::ColorMode(int32_t value)
    {
        m_mode = value;
        if (value == 1 || value == 4 || value == 8)
        {
            BuildDefaultPalette();
            m_indices.assign(static_cast<size_t>(m_width) * m_height, 0);
            ReindexPixels();
        }
        else
        {
            m_palette.clear();
            m_indices.clear();
        }
    }

    void DrawingContext::BuildDefaultPalette()
    {
        m_palette.clear();
        if (m_mode == 1)
        {
            m_palette.push_back(winrt::Windows::UI::Color{ 0xFF, 0x00, 0x00, 0x00 }); // black
            m_palette.push_back(winrt::Windows::UI::Color{ 0xFF, 0xFF, 0xFF, 0xFF }); // white
        }
        else if (m_mode == 4)
        {
            static const uint8_t vga[16][3] = {
                {   0,   0,   0 }, { 128,   0,   0 }, {   0, 128,   0 }, { 128, 128,   0 },
                {   0,   0, 128 }, { 128,   0, 128 }, {   0, 128, 128 }, { 192, 192, 192 },
                { 128, 128, 128 }, { 255,   0,   0 }, {   0, 255,   0 }, { 255, 255,   0 },
                {   0,   0, 255 }, { 255,   0, 255 }, {   0, 255, 255 }, { 255, 255, 255 } };
            for (auto const& e : vga)
            {
                m_palette.push_back(winrt::Windows::UI::Color{ 0xFF, e[0], e[1], e[2] });
            }
        }
        else if (m_mode == 8)
        {
            // 256-entry RGB332: 3 bits red, 3 bits green, 2 bits blue.
            for (int32_t i = 0; i < 256; ++i)
            {
                const uint8_t r = static_cast<uint8_t>(((i >> 5) & 7) * 255 / 7);
                const uint8_t g = static_cast<uint8_t>(((i >> 2) & 7) * 255 / 7);
                const uint8_t b = static_cast<uint8_t>((i & 3) * 255 / 3);
                m_palette.push_back(winrt::Windows::UI::Color{ 0xFF, r, g, b });
            }
        }
    }

    int32_t DrawingContext::NearestIndex(winrt::Windows::UI::Color const& c) const
    {
        int32_t best = 0;
        long bestd = -1;
        for (size_t k = 0; k < m_palette.size(); ++k)
        {
            const long dr = static_cast<long>(c.R) - m_palette[k].R;
            const long dg = static_cast<long>(c.G) - m_palette[k].G;
            const long db = static_cast<long>(c.B) - m_palette[k].B;
            const long d = dr * dr + dg * dg + db * db;
            if (bestd < 0 || d < bestd) { bestd = d; best = static_cast<int32_t>(k); }
        }
        return best;
    }

    void DrawingContext::ReindexPixels()
    {
        if (m_palette.empty()) { return; }
        const size_t count = static_cast<size_t>(m_width) * m_height;
        for (size_t p = 0; p < count; ++p)
        {
            const size_t i = p * 4;
            if (m_pixels[i + 3] == 0) { m_indices[p] = 0; continue; } // transparent
            const winrt::Windows::UI::Color cur{ 0xFF, m_pixels[i + 2], m_pixels[i + 1], m_pixels[i + 0] };
            const int32_t idx = NearestIndex(cur);
            m_indices[p] = static_cast<uint8_t>(idx);
            const auto& pc = m_palette[static_cast<size_t>(idx)];
            m_pixels[i + 0] = pc.B;
            m_pixels[i + 1] = pc.G;
            m_pixels[i + 2] = pc.R;
            m_pixels[i + 3] = 0xFF;
        }
    }

    winrt::Windows::UI::Color DrawingContext::PaletteColor(int32_t index) const
    {
        if (index < 0 || index >= static_cast<int32_t>(m_palette.size()))
        {
            return winrt::Windows::UI::Color{ 0, 0, 0, 0 };
        }
        return m_palette[static_cast<size_t>(index)];
    }

    void DrawingContext::SetPaletteEntry(int32_t index, winrt::Windows::UI::Color const& value)
    {
        if (index < 0 || index >= static_cast<int32_t>(m_palette.size()))
        {
            return;
        }
        // Palette entries are opaque; the pixel's own alpha carries transparency.
        const winrt::Windows::UI::Color entry{ 0xFF, value.R, value.G, value.B };
        m_palette[static_cast<size_t>(index)] = entry;

        const size_t count = static_cast<size_t>(m_width) * m_height;
        for (size_t p = 0; p < count && p < m_indices.size(); ++p)
        {
            const size_t i = p * 4;
            if (m_pixels[i + 3] != 0 && m_indices[p] == index)
            {
                m_pixels[i + 0] = entry.B;
                m_pixels[i + 1] = entry.G;
                m_pixels[i + 2] = entry.R;
            }
        }
    }

    void DrawingContext::SetPixel(int32_t x, int32_t y, winrt::Windows::UI::Color const& color)
    {
        if (!InBounds(x, y))
        {
            return;
        }

        const size_t p = static_cast<size_t>(y) * m_width + x;
        const size_t i = p * 4;

        if (m_palette.empty())
        {
            const winrt::Windows::UI::Color q = Quantize(color);
            m_pixels[i + 0] = q.B;
            m_pixels[i + 1] = q.G;
            m_pixels[i + 2] = q.R;
            m_pixels[i + 3] = q.A;
            return;
        }

        // Indexed mode: keep a 1-bit alpha and store the nearest palette index.
        if (color.A < 128)
        {
            m_pixels[i + 0] = m_pixels[i + 1] = m_pixels[i + 2] = m_pixels[i + 3] = 0;
            if (p < m_indices.size()) { m_indices[p] = 0; }
            return;
        }
        const int32_t idx = NearestIndex(color);
        const auto& pc = m_palette[static_cast<size_t>(idx)];
        m_pixels[i + 0] = pc.B;
        m_pixels[i + 1] = pc.G;
        m_pixels[i + 2] = pc.R;
        m_pixels[i + 3] = 0xFF;
        if (p < m_indices.size()) { m_indices[p] = static_cast<uint8_t>(idx); }
    }

    void DrawingContext::Clear(winrt::Windows::UI::Color const& color)
    {
        if (m_palette.empty())
        {
            const winrt::Windows::UI::Color q = Quantize(color);
            for (size_t i = 0; i < m_pixels.size(); i += 4)
            {
                m_pixels[i + 0] = q.B;
                m_pixels[i + 1] = q.G;
                m_pixels[i + 2] = q.R;
                m_pixels[i + 3] = q.A;
            }
            return;
        }

        if (color.A < 128)
        {
            std::fill(m_pixels.begin(), m_pixels.end(), static_cast<uint8_t>(0));
            std::fill(m_indices.begin(), m_indices.end(), static_cast<uint8_t>(0));
            return;
        }
        const int32_t idx = NearestIndex(color);
        const auto& pc = m_palette[static_cast<size_t>(idx)];
        for (size_t p = 0; p * 4 < m_pixels.size(); ++p)
        {
            const size_t i = p * 4;
            m_pixels[i + 0] = pc.B;
            m_pixels[i + 1] = pc.G;
            m_pixels[i + 2] = pc.R;
            m_pixels[i + 3] = 0xFF;
            if (p < m_indices.size()) { m_indices[p] = static_cast<uint8_t>(idx); }
        }
    }
}
