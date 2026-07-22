#pragma once

#include "LineTool.g.h"

namespace winrt::IconMaster::implementation
{
    struct LineTool : LineToolT<LineTool>
    {
        LineTool() = default;

        winrt::Windows::Foundation::Collections::IVector<winrt::Windows::Graphics::PointInt32>
            Rasterize(int32_t x0, int32_t y0, int32_t x1, int32_t y1);
    };
}

namespace winrt::IconMaster::factory_implementation
{
    struct LineTool : LineToolT<LineTool, implementation::LineTool>
    {
    };
}
