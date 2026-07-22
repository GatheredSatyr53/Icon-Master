#include "pch.h"
#include "Eyedropper.h"
#if __has_include("Eyedropper.g.cpp")
#include "Eyedropper.g.cpp"
#endif

#include <winrt/IconMaster.h>

namespace winrt::IconMaster::implementation
{
    void Eyedropper::Draw(winrt::IconMaster::DrawingContext const& context, int32_t x, int32_t y)
    {
        // Read the pixel under the cursor into the context's current colour.
        context.Color(context.GetPixel(x, y));
    }
}
