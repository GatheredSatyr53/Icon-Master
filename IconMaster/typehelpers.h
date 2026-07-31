#pragma once
#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.UI.Xaml.Interop.h>   // xaml_typename + TypeName
#include <winrt/Microsoft.UI.Xaml.h>

#define IM_WIDEN_(s)   L##s
#define IM_STR_(s)     IM_WIDEN_(#s)
#define IM_TYPEOF(...) winrt::xaml_typename<__VA_ARGS__>()

#define IM_DP_DECLARE(name, type)                                              \
public:                                                                        \
    type name() const                                                          \
    { return winrt::unbox_value<type>(GetValue(s_##name##Property)); }         \
    void name(type const& value)                                               \
    { SetValue(s_##name##Property, winrt::box_value(value)); }                  \
    static winrt::Microsoft::UI::Xaml::DependencyProperty name##Property()     \
    { return s_##name##Property; }                                             \
private:                                                                       \
    static winrt::Microsoft::UI::Xaml::DependencyProperty s_##name##Property;  \
public:

#define IM_DP_DEFINE(impl, projected, name, type, ...)                         \
    winrt::Microsoft::UI::Xaml::DependencyProperty impl::s_##name##Property =  \
        winrt::Microsoft::UI::Xaml::DependencyProperty::Register(              \
            IM_STR_(name),                                                     \
            IM_TYPEOF(type),                                                   \
            IM_TYPEOF(projected),                                              \
            winrt::Microsoft::UI::Xaml::PropertyMetadata{ __VA_ARGS__ });

namespace IconMaster::literals
{

    inline winrt::Windows::Foundation::IInspectable operator""_obj(long double value)
    {
        return winrt::box_value(static_cast<double>(value));
    }

    inline winrt::Windows::Foundation::IInspectable operator""_obj(unsigned long long value)
    {
        return winrt::box_value(static_cast<int32_t>(value));
    }

    inline winrt::Windows::Foundation::IInspectable operator""_obj(const wchar_t* str, std::size_t len)
    {
        return winrt::box_value(winrt::hstring{ std::wstring_view{ str, len } });
    }
}