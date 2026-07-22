#include "pch.h"
#include "RectangleTool.h"
#if __has_include("RectangleTool.g.cpp")
#include "RectangleTool.g.cpp"
#endif

#include <winrt/Windows.Graphics.h>
#include <winrt/Windows.Foundation.Collections.h>
#include <algorithm>

using namespace winrt::Windows::Foundation::Collections;
using namespace winrt::Windows::Graphics;

namespace winrt::IconMaster::implementation
{
    IVector<PointInt32> RectangleTool::Rasterize(int32_t x0, int32_t y0, int32_t x1, int32_t y1)
    {
        auto points = winrt::single_threaded_vector<PointInt32>();

        const int32_t left = std::min(x0, x1);
        const int32_t right = std::max(x0, x1);
        const int32_t top = std::min(y0, y1);
        const int32_t bottom = std::max(y0, y1);

        for (int32_t x = left; x <= right; ++x)
        {
            points.Append(PointInt32{ x, top });
            points.Append(PointInt32{ x, bottom });
        }
        for (int32_t y = top + 1; y < bottom; ++y)
        {
            points.Append(PointInt32{ left, y });
            points.Append(PointInt32{ right, y });
        }

        return points;
    }
}
