#include "pch.h"
#include "MainWindow.xaml.h"
#if __has_include("MainWindow.g.cpp")
#include "MainWindow.g.cpp"
#endif

#include <winrt/IconMaster.h>
#include <winrt/Microsoft.UI.Windowing.h>
#include <winrt/Windows.Graphics.h>
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
        // Loads the XAML and creates the named elements. Must run before any is accessed.
        InitializeComponent();

        // Open at a size that fits the toolbox, canvas, and colour palette
        // (the ColorPicker gets wide when its "More" panel is expanded).
        if (auto appWindow = AppWindow())
        {
            appWindow.Resize(winrt::Windows::Graphics::SizeInt32{ 1200, 820 });
        }

        m_context = winrt::IconMaster::DrawingContext(k_canvasSize, k_canvasSize);
        m_pen = winrt::IconMaster::Pen();
        m_eraser = winrt::IconMaster::Eraser();
        m_fill = winrt::IconMaster::Fill();
        m_eyedropper = winrt::IconMaster::Eyedropper();
        m_line = winrt::IconMaster::LineTool();
        m_rectangle = winrt::IconMaster::RectangleTool();
        m_ellipse = winrt::IconMaster::EllipseTool();

        m_toolKind = ToolKind::Pen;
        m_currentTool = m_pen.as<winrt::IconMaster::ITool>();

        // Sets the initial colour (also flows into the context via ColorChanged).
        ColorPickerControl().Color(winrt::Windows::UI::Color{ 0xFF, 0x00, 0x00, 0x00 });

        RebuildDisplay();
    }

    winrt::IconMaster::ITool MainWindow::ToolForKind(ToolKind kind)
    {
        switch (kind)
        {
        case ToolKind::Eraser:     return m_eraser.as<winrt::IconMaster::ITool>();
        case ToolKind::Fill:       return m_fill.as<winrt::IconMaster::ITool>();
        case ToolKind::Eyedropper: return m_eyedropper.as<winrt::IconMaster::ITool>();
        case ToolKind::Pen:
        default:                   return m_pen.as<winrt::IconMaster::ITool>();
        }
    }

    bool MainWindow::IsShapeTool(ToolKind kind)
    {
        return kind == ToolKind::Line || kind == ToolKind::Rectangle || kind == ToolKind::Ellipse;
    }

    winrt::IconMaster::IShapeTool MainWindow::ShapeToolForKind(ToolKind kind)
    {
        switch (kind)
        {
        case ToolKind::Line:      return m_line.as<winrt::IconMaster::IShapeTool>();
        case ToolKind::Rectangle: return m_rectangle.as<winrt::IconMaster::IShapeTool>();
        case ToolKind::Ellipse:   return m_ellipse.as<winrt::IconMaster::IShapeTool>();
        default:                  return nullptr;
        }
    }

    void MainWindow::OnToolSelected(IInspectable const& sender, RoutedEventArgs const&)
    {
        // Fires during XAML load before the context exists; ignore until ready.
        if (m_context == nullptr)
        {
            return;
        }

        auto element = sender.as<FrameworkElement>();
        auto tag = winrt::unbox_value_or<winrt::hstring>(element.Tag(), L"pen");

        if (tag == L"eraser")          { m_toolKind = ToolKind::Eraser; }
        else if (tag == L"fill")       { m_toolKind = ToolKind::Fill; }
        else if (tag == L"eyedropper") { m_toolKind = ToolKind::Eyedropper; }
        else if (tag == L"line")       { m_toolKind = ToolKind::Line; }
        else if (tag == L"rectangle")  { m_toolKind = ToolKind::Rectangle; }
        else if (tag == L"ellipse")    { m_toolKind = ToolKind::Ellipse; }
        else                           { m_toolKind = ToolKind::Pen; }

        m_currentShape = ShapeToolForKind(m_toolKind);
        if (!IsShapeTool(m_toolKind))
        {
            m_currentTool = ToolForKind(m_toolKind);
        }
    }

    void MainWindow::OnSwatchClick(IInspectable const& sender, RoutedEventArgs const&)
    {
        auto button = sender.as<Button>();
        auto tag = winrt::unbox_value_or<winrt::hstring>(button.Tag(), L"#FF000000");

        std::wstring text{ tag };
        if (!text.empty() && text.front() == L'#')
        {
            text.erase(0, 1);
        }

        const uint32_t argb = static_cast<uint32_t>(std::wcstoul(text.c_str(), nullptr, 16));
        const winrt::Windows::UI::Color color{
            static_cast<uint8_t>((argb >> 24) & 0xFF),
            static_cast<uint8_t>((argb >> 16) & 0xFF),
            static_cast<uint8_t>((argb >> 8) & 0xFF),
            static_cast<uint8_t>(argb & 0xFF)
        };

        // Route through the picker so its UI and the context stay in sync.
        ColorPickerControl().Color(color);
    }

    void MainWindow::OnColorChanged(ColorPicker const&, ColorChangedEventArgs const& args)
    {
        if (m_context == nullptr || m_suppressColorSync)
        {
            return;
        }
        m_context.Color(args.NewColor());
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
        if (m_context == nullptr)
        {
            return;
        }

        auto props = e.GetCurrentPoint(CanvasImage()).Properties();
        if (IsShapeTool(m_toolKind) && props.IsLeftButtonPressed())
        {
            PointerToPixelClamped(e, m_shapeStartX, m_shapeStartY);
            m_shapeActive = true;
            CanvasImage().CapturePointer(e.Pointer());
            RenderAll();
            OverlayShapePreview(m_shapeStartX, m_shapeStartY);
            return;
        }

        DrawFromPointer(e);
    }

    void MainWindow::OnCanvasPointerMoved(IInspectable const&, PointerRoutedEventArgs const& e)
    {
        if (m_shapeActive)
        {
            int32_t lx, ly;
            PointerToPixelClamped(e, lx, ly);
            RenderAll();
            OverlayShapePreview(lx, ly);
            StatusText().Text(L"x: " + winrt::to_hstring(lx) + L"  y: " + winrt::to_hstring(ly));
            return;
        }

        DrawFromPointer(e);
    }

    void MainWindow::OnCanvasPointerReleased(IInspectable const&, PointerRoutedEventArgs const& e)
    {
        if (!m_shapeActive)
        {
            return;
        }

        int32_t lx, ly;
        PointerToPixelClamped(e, lx, ly);
        m_shapeActive = false;
        CanvasImage().ReleasePointerCapture(e.Pointer());
        CommitShape(lx, ly);
        StatusText().Text(L"x: " + winrt::to_hstring(lx) + L"  y: " + winrt::to_hstring(ly));
    }

    void MainWindow::PointerToPixelClamped(PointerRoutedEventArgs const& e, int32_t& lx, int32_t& ly)
    {
        auto pos = e.GetCurrentPoint(CanvasImage()).Position();
        const int32_t x = static_cast<int32_t>(pos.X) / m_zoom;
        const int32_t y = static_cast<int32_t>(pos.Y) / m_zoom;
        lx = std::clamp(x, 0, m_context.PixelWidth() - 1);
        ly = std::clamp(y, 0, m_context.PixelHeight() - 1);
    }

    void MainWindow::PaintPreviewBlock(uint8_t* data, int32_t displayWidth, int32_t displayHeight, int32_t lx, int32_t ly, winrt::Windows::UI::Color const& color)
    {
        const int32_t x0 = lx * m_zoom;
        const int32_t y0 = ly * m_zoom;

        // Fill the block interior (leaving the grid lines intact).
        for (int32_t dy = y0 + 1; dy < y0 + m_zoom && dy < displayHeight; ++dy)
        {
            for (int32_t dx = x0 + 1; dx < x0 + m_zoom && dx < displayWidth; ++dx)
            {
                const size_t i = (static_cast<size_t>(dy) * displayWidth + dx) * 4;
                data[i + 0] = color.B;
                data[i + 1] = color.G;
                data[i + 2] = color.R;
                data[i + 3] = 0xFF;
            }
        }
    }

    void MainWindow::OverlayShapePreview(int32_t x1, int32_t y1)
    {
        if (m_display == nullptr || m_currentShape == nullptr)
        {
            return;
        }

        const winrt::Windows::UI::Color color = m_context.Color();
        if (color.A == 0)
        {
            // Nothing visible to preview (transparent colour).
            m_display.Invalidate();
            return;
        }

        const int32_t dw = m_display.PixelWidth();
        const int32_t dh = m_display.PixelHeight();
        uint8_t* data = DisplayData();

        auto points = m_currentShape.Rasterize(m_shapeStartX, m_shapeStartY, x1, y1);
        for (auto const& p : points)
        {
            PaintPreviewBlock(data, dw, dh, p.X, p.Y, color);
        }

        m_display.Invalidate();
    }

    void MainWindow::CommitShape(int32_t x1, int32_t y1)
    {
        if (m_currentShape == nullptr)
        {
            return;
        }

        const winrt::Windows::UI::Color color = m_context.Color();
        auto points = m_currentShape.Rasterize(m_shapeStartX, m_shapeStartY, x1, y1);
        for (auto const& p : points)
        {
            m_context.SetPixel(p.X, p.Y, color);
        }

        RenderAll();
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
            const winrt::Windows::UI::Color c = m_context.GetPixel(lx, ly);
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

        // Right button always erases, regardless of the selected tool.
        ToolKind kind = m_toolKind;
        winrt::IconMaster::ITool tool = m_currentTool;
        if (right && !left)
        {
            kind = ToolKind::Eraser;
            tool = m_eraser.as<winrt::IconMaster::ITool>();
        }

        tool.Draw(m_context, lx, ly);

        switch (kind)
        {
        case ToolKind::Fill:
            RenderAll();
            break;
        case ToolKind::Eyedropper:
            // No pixel changed; reflect the picked colour in the palette.
            m_suppressColorSync = true;
            ColorPickerControl().Color(m_context.Color());
            m_suppressColorSync = false;
            break;
        default:
            RenderPixel(lx, ly);
            break;
        }

        StatusText().Text(L"x: " + winrt::to_hstring(lx) + L"  y: " + winrt::to_hstring(ly));
    }
}
