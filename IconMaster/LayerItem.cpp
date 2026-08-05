#include "pch.h"
#include "LayerItem.h"
#include "LayerItem.g.cpp"

namespace winrt::IconMaster::implementation
{
    void LayerItem::Name(winrt::hstring const& value)
    {
        if (m_name != value)
        {
            m_name = value;
            Raise(L"Name");
        }
    }

    void LayerItem::Visible(bool value)
    {
        if (m_visible != value)
        {
            m_visible = value;
            Raise(L"Visible");
        }
    }

    void LayerItem::Editing(bool value)
    {
        if (m_editing != value)
        {
            m_editing = value;
            Raise(L"Editing");
            Raise(L"NameVisibility");
            Raise(L"EditVisibility");
        }
    }

    void LayerItem::Raise(winrt::hstring const& property)
    {
        m_propertyChanged(*this, winrt::Microsoft::UI::Xaml::Data::PropertyChangedEventArgs{ property });
    }
}
