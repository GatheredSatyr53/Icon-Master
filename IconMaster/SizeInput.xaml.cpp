#include "pch.h"
#include "SizeInput.xaml.h"
#if __has_include("SizeInput.g.cpp")
#include "SizeInput.g.cpp"
#endif

#include <algorithm>
#include <cmath>

using namespace winrt;
using winrt::Windows::Foundation::IInspectable;
using namespace winrt::Microsoft::UI::Xaml;
using namespace winrt::Microsoft::UI::Xaml::Controls;

namespace winrt::IconMaster::implementation
{
    SizeInput::SizeInput()
    {
        InitializeComponent();
        SetSize(32, 32);
    }

    void SizeInput::SetSize(int32_t width, int32_t height)
    {
        width = std::clamp(width, 1, 1024);
        height = std::clamp(height, 1, 1024);
        m_guard = true;
        WidthBox().Value(width);
        HeightBox().Value(height);
        SquareBox().IsChecked(width == height);
        m_guard = false;
    }

    int32_t SizeInput::Sanitize(double value, int32_t fallback)
    {
        if (std::isnan(value)) { return fallback; }
        return std::clamp(static_cast<int32_t>(std::lround(value)), 1, 1024);
    }

    int32_t SizeInput::SelectedWidth()  { return Sanitize(WidthBox().Value(), 1); }
    int32_t SizeInput::SelectedHeight() { return Sanitize(HeightBox().Value(), 1); }

    bool SizeInput::IsSquare()
    {
        auto ic = SquareBox().IsChecked();
        return ic && ic.Value();
    }

    void SizeInput::OnSquareChanged(IInspectable const&, RoutedEventArgs const&)
    {
        if (m_guard || !IsSquare()) { return; }
        const double v = WidthBox().Value();
        if (std::isnan(v)) { return; }
        m_guard = true;
        HeightBox().Value(v);
        m_guard = false;
    }

    void SizeInput::OnWidthChanged(NumberBox const&, NumberBoxValueChangedEventArgs const& args)
    {
        if (m_guard || !IsSquare()) { return; }
        const double v = args.NewValue();
        if (std::isnan(v)) { return; }
        m_guard = true;
        HeightBox().Value(v);
        m_guard = false;
    }

    void SizeInput::OnHeightChanged(NumberBox const&, NumberBoxValueChangedEventArgs const& args)
    {
        if (m_guard || !IsSquare()) { return; }
        const double v = args.NewValue();
        if (std::isnan(v)) { return; }
        m_guard = true;
        WidthBox().Value(v);
        m_guard = false;
    }
}
