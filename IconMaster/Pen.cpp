#include "pch.h"
#include "Pen.h"
#if __has_include("Pen.g.cpp")
#include "Pen.g.cpp"
#endif

#include <winrt/IconMaster.h>

namespace winrt::IconMaster::implementation
{
    void Pen::Draw(winrt::IconMaster::DrawingContext const& context, int32_t x, int32_t y)
    {
        context.SetPixel(x, y, context.Color());
    }
}
