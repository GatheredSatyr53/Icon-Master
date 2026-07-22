#pragma once

#include "Eyedropper.g.h"

namespace winrt::IconMaster::implementation
{
    struct Eyedropper : EyedropperT<Eyedropper>
    {
        Eyedropper() = default;

        void Draw(winrt::IconMaster::DrawingContext const& context, int32_t x, int32_t y);
    };
}

namespace winrt::IconMaster::factory_implementation
{
    struct Eyedropper : EyedropperT<Eyedropper, implementation::Eyedropper>
    {
    };
}
