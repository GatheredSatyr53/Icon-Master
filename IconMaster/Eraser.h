#pragma once

#include "Eraser.g.h"

namespace winrt::IconMaster::implementation
{
    struct Eraser : EraserT<Eraser>
    {
        Eraser() = default;

        void Draw(winrt::IconMaster::DrawingContext const& context, int32_t x, int32_t y);
    };
}

namespace winrt::IconMaster::factory_implementation
{
    struct Eraser : EraserT<Eraser, implementation::Eraser>
    {
    };
}
