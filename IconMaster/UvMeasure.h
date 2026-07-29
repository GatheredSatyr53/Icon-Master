#pragma once

namespace winrt::IconMaster::implementation
{
    struct UvMeasure
    {
        double U = 0;
        double V = 0;

        explicit UvMeasure() = default;

        explicit UvMeasure(winrt::Microsoft::UI::Xaml::Controls::Orientation orientation, winrt::Windows::Foundation::Size const& size)
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

        winrt::Windows::Foundation::Point GetPoint(winrt::Microsoft::UI::Xaml::Controls::Orientation orientation) const
        {
            if (orientation == winrt::Microsoft::UI::Xaml::Controls::Orientation::Horizontal)
            {
                return winrt::Windows::Foundation::Point(U, V);
            }
            else {
                return winrt::Windows::Foundation::Point(V, U);
            }
        }

        winrt::Windows::Foundation::Size GetSize(winrt::Microsoft::UI::Xaml::Controls::Orientation orientation) const
        {
            if (orientation == winrt::Microsoft::UI::Xaml::Controls::Orientation::Horizontal)
            {
                return winrt::Windows::Foundation::Size(U, V);
            }
            else {
                return winrt::Windows::Foundation::Size(V, U);
            }
        }

        friend bool operator==(const UvMeasure& left, const UvMeasure& right) = default;
    };
}