#include "pch.h"
#include "Fill.h"
#if __has_include("Fill.g.cpp")
#include "Fill.g.cpp"
#endif

#include <winrt/IconMaster.h>
#include <algorithm>
#include <vector>
#include <utility>

using namespace winrt::Windows::UI;

namespace
{
    bool ColorsEqual(Color const& a, Color const& b)
    {
        return a.A == b.A && a.R == b.R && a.G == b.G && a.B == b.B;
    }
}

namespace winrt::IconMaster::implementation
{
    void Fill::Draw(winrt::IconMaster::DrawingContext const& context, int32_t x, int32_t y)
    {
        FillBounded(context, x, y, 0, 0, context.PixelWidth() - 1, context.PixelHeight() - 1);
    }

    void Fill::FillBounded(winrt::IconMaster::DrawingContext const& context, int32_t x, int32_t y,
                           int32_t left, int32_t top, int32_t right, int32_t bottom)
    {
        // Clamp the region to the canvas.
        left = std::max(0, left);
        top = std::max(0, top);
        right = std::min(context.PixelWidth() - 1, right);
        bottom = std::min(context.PixelHeight() - 1, bottom);
        if (x < left || x > right || y < top || y > bottom)
        {
            return;
        }

        const Color target = context.GetPixel(x, y);
        const Color replacement = context.Color();
        if (ColorsEqual(target, replacement))
        {
            return;
        }

        // Iterative 4-connected flood fill, bounded to [left..right] x [top..bottom].
        std::vector<std::pair<int32_t, int32_t>> stack;
        stack.emplace_back(x, y);
        while (!stack.empty())
        {
            auto [cx, cy] = stack.back();
            stack.pop_back();

            if (cx < left || cx > right || cy < top || cy > bottom)
            {
                continue;
            }
            if (!ColorsEqual(context.GetPixel(cx, cy), target))
            {
                continue;
            }

            context.SetPixel(cx, cy, replacement);
            stack.emplace_back(cx + 1, cy);
            stack.emplace_back(cx - 1, cy);
            stack.emplace_back(cx, cy + 1);
            stack.emplace_back(cx, cy - 1);
        }
    }
}
