#pragma once

namespace winrt::IconMaster::implementation
{
    struct UvBounds
    {
        double umin;
        double umax;
        double vmin;
        double vmax;

        explicit UvBounds(winrt::Microsoft::UI::Xaml::Controls::Orientation orientation, winrt::Windows::Foundation::Rect rect)
        {
            if (orientation == winrt::Microsoft::UI::Xaml::Controls::Orientation::Horizontal)
            {
                umin = rect.X;
                umax = rect.X + rect.Width;
                vmin = rect.Y;
                vmax = rect.Y + rect.Height;
            }
            else
            {
                umin = rect.Y;
                umax = rect.Y + rect.Height;
                vmin = rect.X;
                vmax = rect.X + rect.Width;
            }
        }
    };
}