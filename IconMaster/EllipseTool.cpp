#include "pch.h"
#include "EllipseTool.h"
#if __has_include("EllipseTool.g.cpp")
#include "EllipseTool.g.cpp"
#endif

#include <winrt/Windows.Graphics.h>
#include <winrt/Windows.Foundation.Collections.h>
#include <algorithm>
#include <cstdlib>

using namespace winrt::Windows::Foundation::Collections;
using namespace winrt::Windows::Graphics;

namespace winrt::IconMaster::implementation
{
    // Bresenham ellipse inscribed in a rectangle (Zingl's "rasterizing algorithm"),
    // which handles both odd and even bounding-box dimensions.
    IVector<PointInt32> EllipseTool::Rasterize(int32_t x0, int32_t y0, int32_t x1, int32_t y1)
    {
        auto points = winrt::single_threaded_vector<PointInt32>();

        int32_t a = std::abs(x1 - x0);
        int32_t b = std::abs(y1 - y0);
        int32_t b1 = b & 1;

        if (x0 > x1) { x0 = x1; x1 += a; }
        if (y0 > y1) { y0 = y1; }
        y0 += (b + 1) / 2;
        y1 = y0 - b1;

        long dx = 4 * (1 - a) * static_cast<long>(b) * b;
        long dy = 4 * (b1 + 1) * static_cast<long>(a) * a;
        long err = dx + dy + b1 * static_cast<long>(a) * a;
        long a2 = 8L * a * a;
        long b2 = 8L * b * b;

        do
        {
            points.Append(PointInt32{ x1, y0 }); // quadrant I
            points.Append(PointInt32{ x0, y0 }); // quadrant II
            points.Append(PointInt32{ x0, y1 }); // quadrant III
            points.Append(PointInt32{ x1, y1 }); // quadrant IV

            const long e2 = 2 * err;
            if (e2 <= dy)
            {
                ++y0;
                --y1;
                err += dy += a2;
            }
            if (e2 >= dx || 2 * err > dy)
            {
                ++x0;
                --x1;
                err += dx += b2;
            }
        } while (x0 <= x1);

        // Finish the top and bottom flat sections for very thin ellipses.
        while (y0 - y1 < b)
        {
            points.Append(PointInt32{ x0 - 1, y0 });
            points.Append(PointInt32{ x1 + 1, y0 });
            ++y0;
            points.Append(PointInt32{ x0 - 1, y1 });
            points.Append(PointInt32{ x1 + 1, y1 });
            --y1;
        }

        return points;
    }
}
