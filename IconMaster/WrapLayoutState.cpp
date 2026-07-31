#include "pch.h"
#include <ranges>
#include "WrapLayoutState.h"
#include "WrapLayoutState.g.cpp"

namespace winrt::IconMaster::implementation
{
	WrapItem& WrapLayoutState::GetItemAt(int32_t index)
	{
        if (index < 0)
        {
            throw winrt::hresult_out_of_bounds{};
        }

        // Return a reference to the stored item so callers persist the measured
        // size/position back into the layout state. The layout realizes items in
        // order, so an index past the end is always the next new item.
        if (static_cast<size_t>(index) < items.size())
        {
            return items[index];
        }
        else
        {
            items.push_back(WrapItem(index));
            return items.back();
        }
	}

	void WrapLayoutState::Clear()
	{
        items.clear();
	}

    void WrapLayoutState::RemoveFromIndex(int32_t index)
    {
        if (index >= items.size())
        {
            // Item was added/removed but we haven't realized that far yet
            return;
        }

        auto numToRemove = items.size() - index;
        auto from = items.begin() + index;
        items.erase(from, from + numToRemove);
    }

    void WrapLayoutState::SetOrientation(winrt::Microsoft::UI::Xaml::Controls::Orientation o)
    {
        for (auto& item : items | std::views::filter([](WrapItem const& i) { return i.measure.has_value(); }))
        {
            UvMeasure measure = item.measure.value();
            measure.V += measure.U;
            measure.U = measure.V - measure.U;
            measure.V -= measure.U;
            item.measure = std::optional<UvMeasure>(measure);
            item.position.reset();
        }

        this->orientation = o;
        this->availableU = 0;
    }

    void WrapLayoutState::ClearPositions()
    {
        for (auto& item : items)
        {
            item.position.reset();
        }
    }

    float WrapLayoutState::GetHeight()
    {
        if (items.empty())
        {
            return 0;
        }

        std::optional<UvMeasure> lastPosition;
        float maxV = 0;

        for (auto const& item : items | std::views::reverse)
        {
            if (!item.position.has_value() || !item.measure.has_value())
            {
                continue;
            }

            if (lastPosition.has_value() && lastPosition.value().V > item.position.value().V)
            {
                // This is a row above the last item.
                break;
            }

            lastPosition = item.position;
            maxV = std::max(maxV, item.measure.value().V);
        }

        return lastPosition.has_value() ? lastPosition.value().V + maxV : 0;
    }

    void WrapLayoutState::RecycleElementAt(int32_t index) const
    {
        auto element = context.GetOrCreateElementAt(index);
        context.RecycleElement(element);
    }
}
