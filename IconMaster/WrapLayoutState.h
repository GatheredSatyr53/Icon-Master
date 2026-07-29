#pragma once
#include "WrapItem.h"
#include "UvMeasure.h"
#include "WrapLayoutState.g.h"

namespace winrt::IconMaster::implementation
{
    struct WrapLayoutState : WrapLayoutStateT<WrapLayoutState>
    {
        explicit WrapLayoutState(winrt::Microsoft::UI::Xaml::Controls::VirtualizingLayoutContext const& context) 
            : context(context) {}

        winrt::Microsoft::UI::Xaml::Controls::Orientation Orientation() const { return orientation; }
        
        UvMeasure Spacing() const { return spacing; }
        void Spacing(UvMeasure const& value) { spacing = value; }

        double AvailableU() const { return availableU; }
        void AvailableU(double value) { availableU = value; }

        WrapItem& GetItemAt(int32_t index);
        void Clear();
        void RemoveFromIndex(int32_t index);
        void SetOrientation(winrt::Microsoft::UI::Xaml::Controls::Orientation orientation);
        void ClearPositions();
        double GetHeight();
        void RecycleElementAt(int32_t index) const;

    private:
        winrt::Microsoft::UI::Xaml::Controls::VirtualizingLayoutContext context;
        std::vector<WrapItem> items;
        winrt::Microsoft::UI::Xaml::Controls::Orientation orientation;
        UvMeasure spacing;
        double availableU;
    };
}

