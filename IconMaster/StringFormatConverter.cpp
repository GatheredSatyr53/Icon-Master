#include "pch.h"
#include "StringFormatConverter.h"
#include "StringFormatConverter.g.cpp"

namespace
{
    winrt::hstring ToText(winrt::Windows::Foundation::IInspectable const& value)
    {
        using namespace winrt::Windows::Foundation;
        if (!value) return L"";
        if (auto s = value.try_as<IStringable>()) return s.ToString();
        // Ѕоксированные примитивы IStringable не реализуют Ч добавь нужные типы:
        if (auto n = value.try_as<IReference<int32_t>>()) return winrt::to_hstring(n.Value());
        if (auto d = value.try_as<IReference<double>>())  return winrt::to_hstring(d.Value());
        return winrt::unbox_value_or<winrt::hstring>(value, L"");
    }
}

namespace winrt::IconMaster::implementation
{
    Windows::Foundation::IInspectable StringFormatConverter::Convert(
        Windows::Foundation::IInspectable const& value,
        Windows::UI::Xaml::Interop::TypeName const& /*targetType*/,
        Windows::Foundation::IInspectable const& /*parameter*/,   // не используем Ч формат зашит в свойстве
        hstring const& /*language*/)
    {
        std::wstring result{ format.c_str() };
        std::wstring const text{ ToText(value).c_str() };

        for (size_t pos = result.find(L"{0}");
            pos != std::wstring::npos;
            pos = result.find(L"{0}", pos + text.size()))
        {
            result.replace(pos, 3, text);
        }

        return box_value(hstring{ result });
    }

    Windows::Foundation::IInspectable StringFormatConverter::ConvertBack(
        Windows::Foundation::IInspectable const&,
        Windows::UI::Xaml::Interop::TypeName const&,
        Windows::Foundation::IInspectable const&,
        hstring const&)
    {
        throw hresult_not_implemented();
    }
}