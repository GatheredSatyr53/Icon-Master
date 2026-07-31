#pragma once
#include "typehelpers.h"
#include "WrapLayout.g.h"

namespace winrt::IconMaster::implementation
{
    struct WrapLayout : WrapLayoutT<WrapLayout>
    {
        WrapLayout() = default;

        IM_DP_DECLARE(HorizontalSpacing, double)

        IM_DP_DECLARE(VerticalSpacing, double)

        IM_DP_DECLARE(Orientation, winrt::Microsoft::UI::Xaml::Controls::Orientation)

    private:
        static void OnLayoutPropertyChanged(winrt::Microsoft::UI::Xaml::DependencyObject const& d, winrt::Microsoft::UI::Xaml::DependencyPropertyChangedEventArgs const& e);

    public:
        void InitializeForContextCore(winrt::Microsoft::UI::Xaml::Controls::VirtualizingLayoutContext const& context);
        void UninitializeForContextCore(winrt::Microsoft::UI::Xaml::Controls::VirtualizingLayoutContext const& context);
        void OnItemsChangedCore(winrt::Microsoft::UI::Xaml::Controls::VirtualizingLayoutContext const& context, winrt::Windows::Foundation::IInspectable const& source, winrt::Microsoft::UI::Xaml::Interop::NotifyCollectionChangedEventArgs const& args);
        winrt::Windows::Foundation::Size MeasureOverride(winrt::Microsoft::UI::Xaml::Controls::VirtualizingLayoutContext const& context, winrt::Windows::Foundation::Size const& availableSize);
        winrt::Windows::Foundation::Size ArrangeOverride(winrt::Microsoft::UI::Xaml::Controls::VirtualizingLayoutContext const& context, winrt::Windows::Foundation::Size const& finalSize);
    };
}

namespace winrt::IconMaster::factory_implementation
{
    struct WrapLayout : WrapLayoutT<WrapLayout, implementation::WrapLayout>
    {
    };
}
