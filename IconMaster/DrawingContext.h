#pragma once

#include "DrawingContext.g.h"

#include <vector>

namespace winrt::IconMaster::implementation
{
    struct DrawingContext : DrawingContextT<DrawingContext>
    {
        DrawingContext(int32_t width, int32_t height);

        int32_t PixelWidth() const noexcept { return m_width; }
        int32_t PixelHeight() const noexcept { return m_height; }

        winrt::Windows::UI::Color Color() const noexcept { return m_color; }
        void Color(winrt::Windows::UI::Color const& value) noexcept { m_color = value; }

        int32_t ColorMode() const noexcept { return m_mode; }
        void ColorMode(int32_t value); // rebuilds the palette + reindexes for 8/4/1

        int32_t PaletteSize() const noexcept { return static_cast<int32_t>(m_palette.size()); }
        winrt::Windows::UI::Color PaletteColor(int32_t index) const;
        void SetPaletteEntry(int32_t index, winrt::Windows::UI::Color const& value);

        winrt::Windows::UI::Color GetPixel(int32_t x, int32_t y) const;
        void SetPixel(int32_t x, int32_t y, winrt::Windows::UI::Color const& color);
        void Clear(winrt::Windows::UI::Color const& color);

    private:
        bool InBounds(int32_t x, int32_t y) const noexcept
        {
            return x >= 0 && x < m_width && y >= 0 && y < m_height;
        }

        // Direct-colour snap for the non-indexed modes (24/32). 32-bit passes through;
        // 24-bit keeps full RGB with a 1-bit alpha.
        winrt::Windows::UI::Color Quantize(winrt::Windows::UI::Color const& c) const noexcept;

        void BuildDefaultPalette();                                     // fill m_palette for the current mode
        int32_t NearestIndex(winrt::Windows::UI::Color const& c) const; // closest palette entry by RGB distance
        void ReindexPixels();                                           // snap existing opaque pixels to the palette

        int32_t m_width;
        int32_t m_height;
        int32_t m_mode{ 32 };                                         // colour depth (bits): 32/24/8/4/1
        winrt::Windows::UI::Color m_color{ 0xFF, 0x00, 0x00, 0x00 }; // opaque black (A,R,G,B)
        std::vector<uint8_t> m_pixels;                                // BGRA8, row-major
        std::vector<winrt::Windows::UI::Color> m_palette;             // indexed modes only (8/4/1); empty for 24/32
        std::vector<uint8_t> m_indices;                               // per-pixel palette index (indexed modes)
    };
}

namespace winrt::IconMaster::factory_implementation
{
    struct DrawingContext : DrawingContextT<DrawingContext, implementation::DrawingContext>
    {
    };
}
