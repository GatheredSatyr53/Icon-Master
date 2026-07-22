#include "pch.h"
#include "Eraser.h"
#if __has_include("Eraser.g.cpp")
#include "Eraser.g.cpp"
#endif

#include <winrt/IconMaster.h>

namespace winrt::IconMaster::implementation
{
    void Eraser::Draw(winrt::IconMaster::DrawingContext const& context, int32_t x, int32_t y)
    {
        context.SetPixel(x, y, winrt::Windows::UI::Color{ 0x00, 0x00, 0x00, 0x00 });
    }
}
