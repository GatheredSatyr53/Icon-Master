#pragma once

#include "SizeInput.g.h"
#include <winrt/Microsoft.UI.Xaml.Controls.h>

namespace winrt::IconMaster::implementation
{
    struct SizeInput : SizeInputT<SizeInput>
    {
        SizeInput();

        void SetSize(int32_t width, int32_t height);
        int32_t SelectedWidth();
        int32_t SelectedHeight();

        void OnSquareChanged(winrt::Windows::Foundation::IInspectable const& sender, winrt::Microsoft::UI::Xaml::RoutedEventArgs const& args);
        void OnWidthChanged(winrt::Microsoft::UI::Xaml::Controls::NumberBox const& sender, winrt::Microsoft::UI::Xaml::Controls::NumberBoxValueChangedEventArgs const& args);
        void OnHeightChanged(winrt::Microsoft::UI::Xaml::Controls::NumberBox const& sender, winrt::Microsoft::UI::Xaml::Controls::NumberBoxValueChangedEventArgs const& args);

    private:
        bool IsSquare();
        static int32_t Sanitize(double value, int32_t fallback);

        bool m_guard{ false }; // suppress reentrant width/height mirroring
    };
}

namespace winrt::IconMaster::factory_implementation
{
    struct SizeInput : SizeInputT<SizeInput, implementation::SizeInput>
    {
    };
}
