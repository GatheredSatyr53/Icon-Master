#include "pch.h"
#include "MainWindow.xaml.h"
#if __has_include("MainWindow.g.cpp")
#include "MainWindow.g.cpp"
#endif

#include <winrt/IconMaster.h>
#include <robuffer.h>
#include <algorithm>
#include <string>

using namespace winrt;
using namespace winrt::Windows::Foundation;
using namespace winrt::Windows::UI;
using namespace winrt::Microsoft::UI::Xaml;
using namespace winrt::Microsoft::UI::Xaml::Controls;
using namespace winrt::Microsoft::UI::Xaml::Input;
using namespace winrt::Microsoft::UI::Xaml::Media::Imaging;

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

        RebuildDisplay();
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

    void MainWindow::OnZoomIn(IInspectable const&, RoutedEventArgs const&)
    {
        SetZoom(m_zoom + 4);
    }

    void MainWindow::OnZoomOut(IInspectable const&, RoutedEventArgs const&)
    {
        SetZoom(m_zoom - 4);
    }

    void MainWindow::OnCanvasPointerPressed(IInspectable const&, PointerRoutedEventArgs const& e)
    {
        DrawFromPointer(e);
    }

    void MainWindow::OnCanvasPointerMoved(IInspectable const&, PointerRoutedEventArgs const& e)
    {
        DrawFromPointer(e);
    }

    void MainWindow::SetZoom(int32_t zoom)
    {
        zoom = std::clamp(zoom, k_minZoom, k_maxZoom);
        if (zoom == m_zoom && m_display != nullptr)
        {
            return;
        }
        m_zoom = zoom;
        RebuildDisplay();
    }

    void MainWindow::RebuildDisplay()
    {
        if (m_context == nullptr)
        {
            return;
        }

        // One extra pixel so the closing grid line on the right/bottom is drawn.
        const int32_t dw = m_context.PixelWidth() * m_zoom + 1;
        const int32_t dh = m_context.PixelHeight() * m_zoom + 1;

        m_display = WriteableBitmap(dw, dh);
        CanvasImage().Source(m_display);
        RenderAll();

        ZoomText().Text(winrt::to_hstring(m_zoom * 100) + L"%");
    }

    uint8_t* MainWindow::DisplayData()
    {
        auto buffer = m_display.PixelBuffer();
        auto byteAccess = buffer.as<::Windows::Storage::Streams::IBufferByteAccess>();
        uint8_t* data{ nullptr };
        winrt::check_hresult(byteAccess->Buffer(&data));
        return data;
    }

    void MainWindow::WriteDisplayPixel(uint8_t* data, int32_t displayWidth, int32_t dx, int32_t dy)
    {
        const size_t i = (static_cast<size_t>(dy) * displayWidth + dx) * 4;

        uint8_t b, g, r;
        if ((dx % m_zoom == 0) || (dy % m_zoom == 0))
        {
            // Grid line.
            b = g = r = 0xA0;
        }
        else
        {
            const int32_t lx = dx / m_zoom;
            const int32_t ly = dy / m_zoom;
            const Color c = m_context.GetPixel(lx, ly);
            if (c.A == 0)
            {
                // Transparency checkerboard.
                const bool light = ((((dx / k_checkerCell) + (dy / k_checkerCell)) & 1) == 0);
                b = g = r = light ? 0xFF : 0xC8;
            }
            else
            {
                b = c.B;
                g = c.G;
                r = c.R;
            }
        }

        data[i + 0] = b;
        data[i + 1] = g;
        data[i + 2] = r;
        data[i + 3] = 0xFF;
    }

    void MainWindow::RenderAll()
    {
        if (m_display == nullptr)
        {
            return;
        }

        const int32_t dw = m_display.PixelWidth();
        const int32_t dh = m_display.PixelHeight();
        uint8_t* data = DisplayData();

        for (int32_t dy = 0; dy < dh; ++dy)
        {
            for (int32_t dx = 0; dx < dw; ++dx)
            {
                WriteDisplayPixel(data, dw, dx, dy);
            }
        }

        m_display.Invalidate();
    }

    void MainWindow::RenderPixel(int32_t lx, int32_t ly)
    {
        if (m_display == nullptr)
        {
            return;
        }

        const int32_t dw = m_display.PixelWidth();
        const int32_t dh = m_display.PixelHeight();
        uint8_t* data = DisplayData();

        const int32_t x0 = lx * m_zoom;
        const int32_t y0 = ly * m_zoom;

        // Redraw the pixel's block including its shared grid lines.
        for (int32_t dy = y0; dy <= y0 + m_zoom && dy < dh; ++dy)
        {
            for (int32_t dx = x0; dx <= x0 + m_zoom && dx < dw; ++dx)
            {
                WriteDisplayPixel(data, dw, dx, dy);
            }
        }

        m_display.Invalidate();
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

        auto pos = point.Position();
        if (pos.X < 0 || pos.Y < 0)
        {
            return;
        }

        const int32_t lx = static_cast<int32_t>(pos.X) / m_zoom;
        const int32_t ly = static_cast<int32_t>(pos.Y) / m_zoom;
        if (lx < 0 || lx >= m_context.PixelWidth() || ly < 0 || ly >= m_context.PixelHeight())
        {
            return;
        }

        winrt::IconMaster::ITool tool = m_currentTool;
        if (right && !left)
        {
            tool = m_eraser.as<winrt::IconMaster::ITool>();
        }

        tool.Draw(m_context, lx, ly);
        RenderPixel(lx, ly);

        StatusText().Text(L"x: " + winrt::to_hstring(lx) + L"  y: " + winrt::to_hstring(ly));
    }
}
