#include "pch.h"
#include "UvBounds.h"
#include "WrapLayoutState.h"
#include "WrapLayout.h"
#include "WrapLayout.g.cpp"

using namespace IconMaster::literals;

namespace winrt::IconMaster::implementation
{
    IM_DP_DEFINE(WrapLayout, winrt::IconMaster::WrapLayout, HorizontalSpacing, double, 0.0_obj)

    IM_DP_DEFINE(WrapLayout, winrt::IconMaster::WrapLayout, VerticalSpacing, double, 0.0_obj)

    IM_DP_DEFINE(WrapLayout, winrt::IconMaster::WrapLayout, Orientation, winrt::Microsoft::UI::Xaml::Controls::Orientation, winrt::box_value(winrt::Microsoft::UI::Xaml::Controls::Orientation::Horizontal))

	void WrapLayout::OnLayoutPropertyChanged(winrt::Microsoft::UI::Xaml::DependencyObject const& d, winrt::Microsoft::UI::Xaml::DependencyPropertyChangedEventArgs const& e) 
	{
		if (auto wp = d.try_as<WrapLayout>())
		{
			wp->InvalidateMeasure();
			wp->InvalidateArrange();
		}
	}

	void WrapLayout::InitializeForContextCore(winrt::Microsoft::UI::Xaml::Controls::VirtualizingLayoutContext const& context)
	{
		context.LayoutState(winrt::make<implementation::WrapLayoutState>(context));
		base_type::InitializeForContextCore(context);
	}

	void WrapLayout::UninitializeForContextCore(winrt::Microsoft::UI::Xaml::Controls::VirtualizingLayoutContext const& context)
	{
		context.LayoutState(nullptr);
		base_type::UninitializeForContextCore(context);
	}

    void WrapLayout::OnItemsChangedCore(winrt::Microsoft::UI::Xaml::Controls::VirtualizingLayoutContext const& context, winrt::Windows::Foundation::IInspectable const& source, winrt::Microsoft::UI::Xaml::Interop::NotifyCollectionChangedEventArgs const& args)
    {
        auto state = context.LayoutState().try_as<WrapLayoutState>();
        if (!state)
        {
            return;
        }

        switch (args.Action())
        {
        using enum winrt::Microsoft::UI::Xaml::Interop::NotifyCollectionChangedAction;
        case Add:
            state->RemoveFromIndex(args.NewStartingIndex());
            break;
        case Move:
            state->RemoveFromIndex(std::min(args.NewStartingIndex(), args.OldStartingIndex()));
            state->RecycleElementAt(args.OldStartingIndex());
            state->RecycleElementAt(args.NewStartingIndex());
            break;
        case Remove:
            state->RemoveFromIndex(args.OldStartingIndex());
            break;
        case Replace:
            state->RemoveFromIndex(args.NewStartingIndex());
            state->RecycleElementAt(args.NewStartingIndex());
            break;
        case Reset:
            state->Clear();
            break;
        }

        base_type::OnItemsChangedCore(context, source, args);
    }

    winrt::Windows::Foundation::Size WrapLayout::MeasureOverride(winrt::Microsoft::UI::Xaml::Controls::VirtualizingLayoutContext const& context, winrt::Windows::Foundation::Size const& availableSize)
    {
        UvMeasure parentMeasure(Orientation(), availableSize);
        UvMeasure spacingMeasure(Orientation(), winrt::Windows::Foundation::Size(HorizontalSpacing(), VerticalSpacing()));

        auto state = context.LayoutState().try_as<WrapLayoutState>();
        if (!state)
        {
            return winrt::Windows::Foundation::Size();
        }

        if (state->Orientation() != Orientation())
        {
            state->SetOrientation(Orientation());
        }

        if (spacingMeasure != state->Spacing() || state->AvailableU() != parentMeasure.U)
        {
            state->ClearPositions();
            state->Spacing(spacingMeasure);
            state->AvailableU(parentMeasure.U);
        }

        double currentV = 0;
        UvBounds realizationBounds(Orientation(), context.RealizationRect());
        UvMeasure position;
        for (int32_t i = 0; i < context.ItemCount(); ++i)
        {
            bool measured = false;
            WrapItem& item = state->GetItemAt(i);
            if (!item.measure.has_value())
            {
                item.element = context.GetOrCreateElementAt(i);
                item.element.value().Measure(availableSize);
                item.measure = UvMeasure(Orientation(), item.element.value().DesiredSize());
                measured = true;
            }

            auto& currentMeasure = item.measure.value();

            if (!item.position.has_value())
            {
                if (parentMeasure.U < position.U + currentMeasure.U)
                {
                    // New Row
                    position.U = 0;
                    position.V += currentV + spacingMeasure.V;
                    currentV = 0;
                }

                item.position = position;
            }

            position = item.position.value();

            if (auto vEnd = position.V + currentMeasure.V; vEnd < realizationBounds.vmin)
            {
                // Item is "above" the bounds
                if (item.element.has_value())
                {
                    context.RecycleElement(item.element.value());
                    item.element.reset();
                }

                continue;
            }
            else if (position.V > realizationBounds.vmax)
            {
                // Item is "below" the bounds.
                if (item.element.has_value())
                {
                    context.RecycleElement(item.element.value());
                    item.element.reset();
                }

                // We don't need to measure anything below the bounds
                break;
            }
            else if (!measured)
            {
                // Always measure elements that are within the bounds
                item.element = context.GetOrCreateElementAt(i);
                item.element.value().Measure(availableSize);

                currentMeasure = UvMeasure(Orientation(), item.element.value().DesiredSize());
                if (currentMeasure != item.measure)
                {
                    // this item changed size; we need to recalculate layout for everything after this
                    state->RemoveFromIndex(i + 1);
                    item.measure = currentMeasure;

                    // did the change make it go into the new row?
                    if (parentMeasure.U < position.U + currentMeasure.U)
                    {
                        // New Row
                        position.U = 0;
                        position.V += currentV + spacingMeasure.V;
                        currentV = 0;
                    }

                    item.position = position;
                }
            }

            position.U += currentMeasure.U + spacingMeasure.U;
            currentV = std::max(currentMeasure.V, currentV);
        }

        // update value with the last line
        // if the the last loop is(parentMeasure.U > currentMeasure.U + lineMeasure.U) the total isn't calculated then calculate it
        // if the last loop is (parentMeasure.U > currentMeasure.U) the currentMeasure isn't added to the total so add it here
        // for the last condition it is zeros so adding it will make no difference
        // this way is faster than an if condition in every loop for checking the last item
        // Propagating an infinite size causes a crash. This can happen if the parent is scrollable and infinite in the opposite
        // axis to the panel. Clearing to zero prevents the crash.
        // This is likely an incorrect use of the control by the developer, however we need stability here so setting a default that won't crash.

        UvMeasure totalMeasure;
        totalMeasure.U = std::isinf(parentMeasure.U) ? 0 : std::ceil(parentMeasure.U);
        totalMeasure.V = state->GetHeight();

        return totalMeasure.GetSize(Orientation());
    }

    winrt::Windows::Foundation::Size WrapLayout::ArrangeOverride(winrt::Microsoft::UI::Xaml::Controls::VirtualizingLayoutContext const& context, winrt::Windows::Foundation::Size const& finalSize)
    {
        if (context.ItemCount() > 0)
        {
            UvBounds realizationBounds(Orientation(), context.RealizationRect());

            auto state = context.LayoutState().try_as<WrapLayoutState>();
            if (!state)
            {
                return winrt::Windows::Foundation::Size();
            }

            auto ArrangeItem = [&realizationBounds, context, this](const WrapItem& item)
                {
                    if (!item.position.has_value() || !item.measure.has_value())
                    {
                        return false;
                    }

                    auto desiredMeasure = item.measure.value();


                    if (auto position = item.position.value(); realizationBounds.vmin <= position.V + desiredMeasure.V && position.V <= realizationBounds.vmax)
                    {
                        // place the item
                        auto child = context.GetOrCreateElementAt(item.index);
                        child.Arrange(winrt::Windows::Foundation::Rect(position.GetPoint(Orientation()), desiredMeasure.GetSize(Orientation())));
                    }
                    else if (position.V > realizationBounds.vmax)
                    {
                        return false;
                    }

                    return true;
                };

            for (int32_t i = 0; i < context.ItemCount(); ++i)
            {
                if (!ArrangeItem(state->GetItemAt(i)))
                {
                    break;
                }
            }
        }

        return finalSize;
    }
}
