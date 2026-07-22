#include "pch.h"
#include "LineTool.h"
#if __has_include("LineTool.g.cpp")
#include "LineTool.g.cpp"
#endif

#include <winrt/Windows.Graphics.h>
#include <winrt/Windows.Foundation.Collections.h>
#include <cstdlib>

using namespace winrt::Windows::Foundation::Collections;
using namespace winrt::Windows::Graphics;

namespace winrt::IconMaster::implementation
{
    IVector<PointInt32> LineTool::Rasterize(int32_t x0, int32_t y0, int32_t x1, int32_t y1)
    {
        auto points = winrt::single_threaded_vector<PointInt32>();

        // Bresenham's line algorithm.
        const int32_t dx = std::abs(x1 - x0);
        const int32_t dy = -std::abs(y1 - y0);
        const int32_t sx = x0 < x1 ? 1 : -1;
        const int32_t sy = y0 < y1 ? 1 : -1;
        int32_t err = dx + dy;

        int32_t x = x0;
        int32_t y = y0;
        while (true)
        {
            points.Append(PointInt32{ x, y });
            if (x == x1 && y == y1)
            {
                break;
            }
            const int32_t e2 = 2 * err;
            if (e2 >= dy)
            {
                err += dy;
                x += sx;
            }
            if (e2 <= dx)
            {
                err += dx;
                y += sy;
            }
        }

        return points;
    }
}
