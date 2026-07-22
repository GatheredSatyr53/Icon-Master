#pragma once

#include "Pen.g.h"

namespace winrt::IconMaster::implementation
{
    struct Pen : PenT<Pen>
    {
        Pen() = default;

        void Draw(winrt::IconMaster::DrawingContext const& context, int32_t x, int32_t y);
    };
}

namespace winrt::IconMaster::factory_implementation
{
    struct Pen : PenT<Pen, implementation::Pen>
    {
    };
}
