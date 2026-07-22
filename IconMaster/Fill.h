#pragma once

#include "Fill.g.h"

namespace winrt::IconMaster::implementation
{
    struct Fill : FillT<Fill>
    {
        Fill() = default;

        void Draw(winrt::IconMaster::DrawingContext const& context, int32_t x, int32_t y);
        void FillBounded(winrt::IconMaster::DrawingContext const& context, int32_t x, int32_t y,
                         int32_t left, int32_t top, int32_t right, int32_t bottom);
    };
}

namespace winrt::IconMaster::factory_implementation
{
    struct Fill : FillT<Fill, implementation::Fill>
    {
    };
}
