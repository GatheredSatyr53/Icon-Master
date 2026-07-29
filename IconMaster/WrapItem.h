#pragma once
#include "UvMeasure.h"

namespace winrt::IconMaster::implementation
{
    struct WrapItem
    {
        int32_t index;
        std::optional<UvMeasure> measure;
        std::optional<UvMeasure> position;
        std::optional<winrt::Microsoft::UI::Xaml::UIElement> element;

        explicit WrapItem(int32_t index) : index(index) {}
    };
}
