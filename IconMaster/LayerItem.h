#pragma once
#include "LayerItem.g.h"

namespace winrt::IconMaster::implementation
{
    struct LayerItem : LayerItemT<LayerItem>
    {
        LayerItem() = default;
        LayerItem(winrt::hstring const& name, bool visible) : m_name(name), m_visible(visible) {}

        winrt::hstring Name() const { return m_name; }
        void Name(winrt::hstring const& value);

        bool Visible() const { return m_visible; }
        void Visible(bool value);

        bool Editing() const { return m_editing; }
        void Editing(bool value);

        winrt::Microsoft::UI::Xaml::Visibility NameVisibility() const
        {
            return m_editing ? winrt::Microsoft::UI::Xaml::Visibility::Collapsed
                             : winrt::Microsoft::UI::Xaml::Visibility::Visible;
        }
        winrt::Microsoft::UI::Xaml::Visibility EditVisibility() const
        {
            return m_editing ? winrt::Microsoft::UI::Xaml::Visibility::Visible
                             : winrt::Microsoft::UI::Xaml::Visibility::Collapsed;
        }

        winrt::event_token PropertyChanged(winrt::Microsoft::UI::Xaml::Data::PropertyChangedEventHandler const& handler)
        {
            return m_propertyChanged.add(handler);
        }
        void PropertyChanged(winrt::event_token const& token) noexcept
        {
            m_propertyChanged.remove(token);
        }

    private:
        void Raise(winrt::hstring const& property);

        winrt::hstring m_name;
        bool m_visible{ true };
        bool m_editing{ false };
        winrt::event<winrt::Microsoft::UI::Xaml::Data::PropertyChangedEventHandler> m_propertyChanged;
    };
}

namespace winrt::IconMaster::factory_implementation
{
    struct LayerItem : LayerItemT<LayerItem, implementation::LayerItem>
    {
    };
}
