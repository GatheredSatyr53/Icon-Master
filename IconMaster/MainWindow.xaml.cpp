#include "pch.h"
#include "MainWindow.xaml.h"
#if __has_include("MainWindow.g.cpp")
#include "MainWindow.g.cpp"
#endif

#include <winrt/IconMaster.h>
#include <string>

using namespace winrt;
using namespace winrt::Windows::Foundation;
using namespace winrt::Windows::UI;
using namespace winrt::Microsoft::UI::Xaml;
using namespace winrt::Microsoft::UI::Xaml::Controls;
using namespace winrt::Microsoft::UI::Xaml::Input;

namespace winrt::IconMaster::implementation
{
    MainWindow::MainWindow()
    {
        // Loads the XAML and creates the named elements (CanvasImage, buttons).
        // Must run before any of them is accessed.
        InitializeComponent();

        m_context = winrt::IconMaster::DrawingContext(k_canvasSize, k_canvasSize);
        m_pen = winrt::IconMaster::Pen();
        m_eraser = winrt::IconMaster::Eraser();
        m_currentTool = m_pen.as<winrt::IconMaster::ITool>();

        CanvasImage().Source(m_context.Bitmap());
    }

    void MainWindow::OnToolChanged(IInspectable const&, RoutedEventArgs const&)
    {
        // Fires during XAML load before the context exists; ignore until ready.
        if (m_context == nullptr)
        {
            return;
        }

        auto erased = EraserButton().IsChecked();
        if (erased && erased.Value())
        {
            m_currentTool = m_eraser.as<winrt::IconMaster::ITool>();
        }
        else
        {
            m_currentTool = m_pen.as<winrt::IconMaster::ITool>();
        }
    }

    void MainWindow::OnColorClick(IInspectable const& sender, RoutedEventArgs const&)
    {
        if (m_context == nullptr)
        {
            return;
        }

        auto button = sender.as<Button>();
        auto tag = winrt::unbox_value_or<winrt::hstring>(button.Tag(), L"#FF000000");

        std::wstring text{ tag };
        if (!text.empty() && text.front() == L'#')
        {
            text.erase(0, 1);
        }

        const uint32_t argb = static_cast<uint32_t>(std::wcstoul(text.c_str(), nullptr, 16));
        const Color color{
            static_cast<uint8_t>((argb >> 24) & 0xFF),
            static_cast<uint8_t>((argb >> 16) & 0xFF),
            static_cast<uint8_t>((argb >> 8) & 0xFF),
            static_cast<uint8_t>(argb & 0xFF)
        };
        m_context.Color(color);
    }

    void MainWindow::OnCanvasPointerPressed(IInspectable const&, PointerRoutedEventArgs const& e)
    {
        DrawFromPointer(e);
    }

    void MainWindow::OnCanvasPointerMoved(IInspectable const&, PointerRoutedEventArgs const& e)
    {
        DrawFromPointer(e);
    }

    void MainWindow::DrawFromPointer(PointerRoutedEventArgs const& e)
    {
        if (m_context == nullptr)
        {
            return;
        }

        auto point = e.GetCurrentPoint(CanvasImage());
        auto props = point.Properties();
        const bool left = props.IsLeftButtonPressed();
        const bool right = props.IsRightButtonPressed();
        if (!left && !right)
        {
            return;
        }

        const double actualWidth = CanvasImage().ActualWidth();
        const double actualHeight = CanvasImage().ActualHeight();
        if (actualWidth <= 0.0 || actualHeight <= 0.0)
        {
            return;
        }

        auto position = point.Position();
        const int32_t px = static_cast<int32_t>(position.X * m_context.PixelWidth() / actualWidth);
        const int32_t py = static_cast<int32_t>(position.Y * m_context.PixelHeight() / actualHeight);

        winrt::IconMaster::ITool tool = m_currentTool;
        if (right && !left)
        {
            tool = m_eraser.as<winrt::IconMaster::ITool>();
        }

        tool.Draw(m_context, px, py);
        m_context.Invalidate();

        StatusText().Text(L"x: " + winrt::to_hstring(px) + L"  y: " + winrt::to_hstring(py));
    }
}
