#pragma once

namespace winrt::IconMaster::implementation
{
    struct UvMeasure
    {
        float U = 0.0f;
        float V = 0.0f;

        UvMeasure() = default;

        UvMeasure(winrt::Microsoft::UI::Xaml::Controls::Orientation orientation,
            winrt::Windows::Foundation::Size const& size)
        {
            if (orientation == winrt::Microsoft::UI::Xaml::Controls::Orientation::Horizontal)
            {
                U = size.Width;
                V = size.Height;
            }
            else
            {
                U = size.Height;
                V = size.Width;
            }
        }

        winrt::Windows::Foundation::Point GetPoint(
            winrt::Microsoft::UI::Xaml::Controls::Orientation orientation) const noexcept
        {
            return orientation == winrt::Microsoft::UI::Xaml::Controls::Orientation::Horizontal
                ? winrt::Windows::Foundation::Point{ U, V }
            : winrt::Windows::Foundation::Point{ V, U };
        }

        winrt::Windows::Foundation::Size GetSize(
            winrt::Microsoft::UI::Xaml::Controls::Orientation orientation) const noexcept
        {
            return orientation == winrt::Microsoft::UI::Xaml::Controls::Orientation::Horizontal
                ? winrt::Windows::Foundation::Size{ U, V }
            : winrt::Windows::Foundation::Size{ V, U };
        }

        friend bool operator==(UvMeasure const& left, UvMeasure const& right) = default;
    };
}