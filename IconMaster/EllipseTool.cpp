#include "pch.h"
#include "EllipseTool.h"
#if __has_include("EllipseTool.g.cpp")
#include "EllipseTool.g.cpp"
#endif

#include <winrt/Windows.Graphics.h>
#include <winrt/Windows.Foundation.Collections.h>
#include <algorithm>
#include <cstdlib>
#include <map>
#include <utility>

using namespace winrt::Windows::Foundation::Collections;
using namespace winrt::Windows::Graphics;

namespace winrt::IconMaster::implementation
{
    // Bresenham ellipse inscribed in a rectangle (Zingl's "rasterizing algorithm"),
    // which handles both odd and even bounding-box dimensions.
    IVector<PointInt32> EllipseTool::Rasterize(int32_t x0, int32_t y0, int32_t x1, int32_t y1, bool filled)
    {
        auto points = winrt::single_threaded_vector<PointInt32>();

        // When filling, record each row's leftmost/rightmost boundary x so the
        // interior can be swept as horizontal spans after the outline walk.
        std::map<int32_t, std::pair<int32_t, int32_t>> rowSpan;
        auto emit = [&](int32_t x, int32_t y)
        {
            if (filled)
            {
                auto it = rowSpan.find(y);
                if (it == rowSpan.end()) { rowSpan.emplace(y, std::pair{ x, x }); }
                else
                {
                    it->second.first = std::min(it->second.first, x);
                    it->second.second = std::max(it->second.second, x);
                }
            }
            else
            {
                points.Append(PointInt32{ x, y });
            }
        };

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
            emit(x1, y0); // quadrant I
            emit(x0, y0); // quadrant II
            emit(x0, y1); // quadrant III
            emit(x1, y1); // quadrant IV

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
            emit(x0 - 1, y0);
            emit(x1 + 1, y0);
            ++y0;
            emit(x0 - 1, y1);
            emit(x1 + 1, y1);
            --y1;
        }

        if (filled)
        {
            for (auto const& [y, span] : rowSpan)
            {
                for (int32_t x = span.first; x <= span.second; ++x)
                {
                    points.Append(PointInt32{ x, y });
                }
            }
        }

        return points;
    }
}
