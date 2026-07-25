#pragma once
#include "StringFormatConverter.g.h"

namespace winrt::IconMaster::implementation
{
    struct StringFormatConverter : StringFormatConverterT<StringFormatConverter>
    {
        StringFormatConverter() = default;

        hstring Format() const { return format; }
        void Format(hstring const& value) { format = value; }

        Windows::Foundation::IInspectable Convert(
            Windows::Foundation::IInspectable const& value,
            Windows::UI::Xaml::Interop::TypeName const& targetType,
            Windows::Foundation::IInspectable const& parameter,
            hstring const& language);

        Windows::Foundation::IInspectable ConvertBack(
            Windows::Foundation::IInspectable const& value,
            Windows::UI::Xaml::Interop::TypeName const& targetType,
            Windows::Foundation::IInspectable const& parameter,
            hstring const& language);

    private:
        hstring format;
    };
}

namespace winrt::IconMaster::factory_implementation
{
    struct StringFormatConverter : StringFormatConverterT<StringFormatConverter, implementation::StringFormatConverter>
    {
    };
}