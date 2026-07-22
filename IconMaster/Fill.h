#pragma once

#include "Fill.g.h"

namespace winrt::IconMaster::implementation
{
    struct Fill : FillT<Fill>
    {
        Fill() = default;

        void Draw(winrt::IconMaster::DrawingContext const& context, int32_t x, int32_t y);
    };
}

namespace winrt::IconMaster::factory_implementation
{
    struct Fill : FillT<Fill, implementation::Fill>
    {
    };
}
