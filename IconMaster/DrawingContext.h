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

        winrt::Windows::UI::Color GetPixel(int32_t x, int32_t y) const;
        void SetPixel(int32_t x, int32_t y, winrt::Windows::UI::Color const& color);
        void Clear(winrt::Windows::UI::Color const& color);

    private:
        bool InBounds(int32_t x, int32_t y) const noexcept
        {
            return x >= 0 && x < m_width && y >= 0 && y < m_height;
        }

        int32_t m_width;
        int32_t m_height;
        winrt::Windows::UI::Color m_color{ 0xFF, 0x00, 0x00, 0x00 }; // opaque black (A,R,G,B)
        std::vector<uint8_t> m_pixels;                                // BGRA8, row-major
    };
}

namespace winrt::IconMaster::factory_implementation
{
    struct DrawingContext : DrawingContextT<DrawingContext, implementation::DrawingContext>
    {
    };
}
