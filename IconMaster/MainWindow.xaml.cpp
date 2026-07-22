#include "pch.h"
#include "MainWindow.xaml.h"
#if __has_include("MainWindow.g.cpp")
#include "MainWindow.g.cpp"
#endif

#include <winrt/IconMaster.h>
#include <winrt/Microsoft.UI.Windowing.h>
#include <winrt/Windows.Graphics.h>
#include <winrt/Windows.Graphics.Imaging.h>
#include <winrt/Windows.Storage.h>
#include <winrt/Windows.Storage.Pickers.h>
#include <winrt/Windows.Storage.Streams.h>
#include <microsoft.ui.xaml.window.h>
#include <shobjidl_core.h>
#include <robuffer.h>
#include <algorithm>
#include <string>
#include <vector>

using namespace winrt;
// Narrow using-declaration (not the whole Windows::Foundation namespace) so that
// winrt's IUnknown does not clash with ::IUnknown from the classic COM headers
// (<shobjidl_core.h>) used for the file pickers.
using winrt::Windows::Foundation::IInspectable;
using namespace winrt::Windows::UI;
using namespace winrt::Microsoft::UI::Xaml;
using namespace winrt::Microsoft::UI::Xaml::Controls;
using namespace winrt::Microsoft::UI::Xaml::Input;
using namespace winrt::Microsoft::UI::Xaml::Media::Imaging;

namespace
{
    constexpr winrt::Windows::UI::Color kTransparent{ 0x00, 0x00, 0x00, 0x00 };
}

namespace winrt::IconMaster::implementation
{
    MainWindow::MainWindow()
    {
        // Create the first document BEFORE InitializeComponent: loading the XAML
        // raises events (e.g. the Pen RadioButton's Checked) that call doc(), so
        // m_docs must already have an element.
        m_docs.emplace_back();
        m_active = 0;

        // Loads the XAML and creates the named elements. Must run before any is accessed.
        InitializeComponent();

        // Open at a size that fits the toolbox, canvas, and colour palette.
        if (auto appWindow = AppWindow())
        {
            appWindow.Resize(winrt::Windows::Graphics::SizeInt32{ 1200, 820 });
        }

        doc().context = winrt::IconMaster::DrawingContext(k_canvasSize, k_canvasSize);
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

        // Tab for the initial document.
        m_docCounter = 1;
        doc().title = L"Icon 1";
        m_updatingTabs = true;
        {
            auto item = winrt::Microsoft::UI::Xaml::Controls::TabViewItem();
            item.Header(winrt::box_value(doc().title));
            item.IsClosable(true);
            Tabs().TabItems().Append(item);
            Tabs().SelectedIndex(0);
        }
        m_updatingTabs = false;

        RebuildDisplay();
    }

    // ---- Tool selection -----------------------------------------------------

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
        if (doc().context == nullptr)
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
        else if (tag == L"select")     { m_toolKind = ToolKind::Select; }
        else                           { m_toolKind = ToolKind::Pen; }

        m_currentShape = ShapeToolForKind(m_toolKind);
        if (!IsShapeTool(m_toolKind) && m_toolKind != ToolKind::Select)
        {
            m_currentTool = ToolForKind(m_toolKind);
        }
    }

    // ---- Colour -------------------------------------------------------------

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

        ColorPickerControl().Color(color);
    }

    void MainWindow::OnColorChanged(ColorPicker const&, ColorChangedEventArgs const& args)
    {
        if (doc().context == nullptr || m_suppressColorSync)
        {
            return;
        }
        doc().context.Color(args.NewColor());
    }

    // ---- Zoom ---------------------------------------------------------------

    void MainWindow::OnZoomIn(IInspectable const&, RoutedEventArgs const&) { SetZoom(doc().zoom + 4); }
    void MainWindow::OnZoomOut(IInspectable const&, RoutedEventArgs const&) { SetZoom(doc().zoom - 4); }

    void MainWindow::SetZoom(int32_t zoom)
    {
        zoom = std::clamp(zoom, k_minZoom, k_maxZoom);
        if (zoom == doc().zoom && m_display != nullptr)
        {
            return;
        }
        doc().zoom = zoom;
        RebuildDisplay();
    }

    // ---- Pointer ------------------------------------------------------------

    void MainWindow::PointerToPixelClamped(PointerRoutedEventArgs const& e, int32_t& lx, int32_t& ly)
    {
        auto pos = e.GetCurrentPoint(CanvasImage()).Position();
        const int32_t x = static_cast<int32_t>(pos.X) / doc().zoom;
        const int32_t y = static_cast<int32_t>(pos.Y) / doc().zoom;
        lx = std::clamp(x, 0, doc().context.PixelWidth() - 1);
        ly = std::clamp(y, 0, doc().context.PixelHeight() - 1);
    }

    void MainWindow::OnCanvasPointerPressed(IInspectable const&, PointerRoutedEventArgs const& e)
    {
        if (doc().context == nullptr)
        {
            return;
        }

        auto props = e.GetCurrentPoint(CanvasImage()).Properties();
        const bool left = props.IsLeftButtonPressed();

        if (m_toolKind == ToolKind::Select)
        {
            if (!left)
            {
                return;
            }

            int32_t px, py;
            PointerToPixelClamped(e, px, py);
            CanvasImage().CapturePointer(e.Pointer());

            if (InsideSelection(px, py))
            {
                PushUndo();
                LiftSelection();
                m_moving = true;
                m_moveAnchorX = px;
                m_moveAnchorY = py;
                m_moveDX = 0;
                m_moveDY = 0;
            }
            else
            {
                m_selecting = true;
                m_selAnchorX = px;
                m_selAnchorY = py;
                SetSelectionFromPoints(px, py, px, py);
            }
            Render();
            return;
        }

        if (IsShapeTool(m_toolKind) && left)
        {
            PointerToPixelClamped(e, m_shapeStartX, m_shapeStartY);
            m_shapeCurX = m_shapeStartX;
            m_shapeCurY = m_shapeStartY;
            m_shapeActive = true;
            CanvasImage().CapturePointer(e.Pointer());
            Render();
            return;
        }

        // Pixel tools: snapshot once at the start of a stroke (not on every move),
        // except the eyedropper which does not change pixels.
        const bool right = props.IsRightButtonPressed();
        if (right || (left && m_toolKind != ToolKind::Eyedropper))
        {
            PushUndo();
        }
        DrawFromPointer(e);
    }

    void MainWindow::OnCanvasPointerMoved(IInspectable const&, PointerRoutedEventArgs const& e)
    {
        if (m_moving)
        {
            int32_t px, py;
            PointerToPixelClamped(e, px, py);
            m_moveDX = px - m_moveAnchorX;
            m_moveDY = py - m_moveAnchorY;
            Render();
            StatusText().Text(L"x: " + winrt::to_hstring(px) + L"  y: " + winrt::to_hstring(py));
            return;
        }

        if (m_selecting)
        {
            int32_t px, py;
            PointerToPixelClamped(e, px, py);
            SetSelectionFromPoints(m_selAnchorX, m_selAnchorY, px, py);
            Render();
            StatusText().Text(L"selection " + winrt::to_hstring(doc().selW) + L" x " + winrt::to_hstring(doc().selH));
            return;
        }

        if (m_shapeActive)
        {
            PointerToPixelClamped(e, m_shapeCurX, m_shapeCurY);
            Render();
            StatusText().Text(L"x: " + winrt::to_hstring(m_shapeCurX) + L"  y: " + winrt::to_hstring(m_shapeCurY));
            return;
        }

        DrawFromPointer(e);
    }

    void MainWindow::OnCanvasPointerReleased(IInspectable const&, PointerRoutedEventArgs const& e)
    {
        if (m_moving)
        {
            int32_t px, py;
            PointerToPixelClamped(e, px, py);
            m_moveDX = px - m_moveAnchorX;
            m_moveDY = py - m_moveAnchorY;
            StampFloating(doc().selX + m_moveDX, doc().selY + m_moveDY);
            doc().selX += m_moveDX;
            doc().selY += m_moveDY;
            m_moving = false;
            m_moveDX = 0;
            m_moveDY = 0;
            m_floatPixels.clear();
            CanvasImage().ReleasePointerCapture(e.Pointer());
            Render();
            return;
        }

        if (m_selecting)
        {
            m_selecting = false;
            // A click without a drag deselects.
            if (doc().selW <= 1 && doc().selH <= 1)
            {
                doc().hasSelection = false;
            }
            CanvasImage().ReleasePointerCapture(e.Pointer());
            Render();
            return;
        }

        if (m_shapeActive)
        {
            int32_t px, py;
            PointerToPixelClamped(e, px, py);
            m_shapeActive = false;
            CanvasImage().ReleasePointerCapture(e.Pointer());
            PushUndo();
            CommitShape(px, py);
            return;
        }
    }

    void MainWindow::DrawFromPointer(PointerRoutedEventArgs const& e)
    {
        if (doc().context == nullptr)
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

        const int32_t lx = static_cast<int32_t>(pos.X) / doc().zoom;
        const int32_t ly = static_cast<int32_t>(pos.Y) / doc().zoom;
        if (lx < 0 || lx >= doc().context.PixelWidth() || ly < 0 || ly >= doc().context.PixelHeight())
        {
            return;
        }

        ToolKind kind = m_toolKind;
        winrt::IconMaster::ITool tool = m_currentTool;
        if (right && !left)
        {
            kind = ToolKind::Eraser;
            tool = m_eraser.as<winrt::IconMaster::ITool>();
        }

        if (kind == ToolKind::Fill && doc().hasSelection)
        {
            // Fill is constrained to the selection rectangle.
            if (!InsideSelection(lx, ly))
            {
                return;
            }
            m_fill.FillBounded(doc().context, lx, ly,
                               doc().selX, doc().selY, doc().selX + doc().selW - 1, doc().selY + doc().selH - 1);
        }
        else
        {
            tool.Draw(doc().context, lx, ly);
        }

        if (kind == ToolKind::Eyedropper)
        {
            m_suppressColorSync = true;
            ColorPickerControl().Color(doc().context.Color());
            m_suppressColorSync = false;
        }
        else
        {
            Render();
        }

        StatusText().Text(L"x: " + winrt::to_hstring(lx) + L"  y: " + winrt::to_hstring(ly));
    }

    void MainWindow::CommitShape(int32_t x1, int32_t y1)
    {
        if (m_currentShape == nullptr)
        {
            return;
        }

        const winrt::Windows::UI::Color color = doc().context.Color();
        auto points = m_currentShape.Rasterize(m_shapeStartX, m_shapeStartY, x1, y1);
        for (auto const& p : points)
        {
            doc().context.SetPixel(p.X, p.Y, color);
        }
        Render();
    }

    // ---- Selection / clipboard ---------------------------------------------

    bool MainWindow::InsideSelection(int32_t px, int32_t py) const
    {
        return doc().hasSelection &&
               px >= doc().selX && px < doc().selX + doc().selW &&
               py >= doc().selY && py < doc().selY + doc().selH;
    }

    void MainWindow::SetSelectionFromPoints(int32_t ax, int32_t ay, int32_t bx, int32_t by)
    {
        const int32_t left = std::min(ax, bx);
        const int32_t top = std::min(ay, by);
        const int32_t right = std::max(ax, bx);
        const int32_t bottom = std::max(ay, by);
        doc().selX = left;
        doc().selY = top;
        doc().selW = right - left + 1;
        doc().selH = bottom - top + 1;
        doc().hasSelection = true;
    }

    void MainWindow::ClearRegion(int32_t x, int32_t y, int32_t w, int32_t h)
    {
        for (int32_t j = 0; j < h; ++j)
        {
            for (int32_t i = 0; i < w; ++i)
            {
                doc().context.SetPixel(x + i, y + j, kTransparent);
            }
        }
    }

    void MainWindow::LiftSelection()
    {
        m_floatW = doc().selW;
        m_floatH = doc().selH;
        m_floatPixels.assign(static_cast<size_t>(m_floatW) * m_floatH, kTransparent);
        for (int32_t j = 0; j < m_floatH; ++j)
        {
            for (int32_t i = 0; i < m_floatW; ++i)
            {
                m_floatPixels[static_cast<size_t>(j) * m_floatW + i] = doc().context.GetPixel(doc().selX + i, doc().selY + j);
            }
        }
        ClearRegion(doc().selX, doc().selY, doc().selW, doc().selH);
    }

    void MainWindow::StampFloating(int32_t atX, int32_t atY)
    {
        for (int32_t j = 0; j < m_floatH; ++j)
        {
            for (int32_t i = 0; i < m_floatW; ++i)
            {
                const winrt::Windows::UI::Color c = m_floatPixels[static_cast<size_t>(j) * m_floatW + i];
                if (c.A > 0)
                {
                    doc().context.SetPixel(atX + i, atY + j, c);
                }
            }
        }
    }

    void MainWindow::OnSelectAll(IInspectable const&, RoutedEventArgs const&)
    {
        if (doc().context == nullptr)
        {
            return;
        }
        doc().hasSelection = true;
        doc().selX = 0;
        doc().selY = 0;
        doc().selW = doc().context.PixelWidth();
        doc().selH = doc().context.PixelHeight();
        Render();
    }

    void MainWindow::OnDeselect(IInspectable const&, RoutedEventArgs const&)
    {
        doc().hasSelection = false;
        Render();
    }

    void MainWindow::OnCopy(IInspectable const&, RoutedEventArgs const&)
    {
        if (!doc().hasSelection)
        {
            return;
        }
        m_clipW = doc().selW;
        m_clipH = doc().selH;
        m_clipPixels.assign(static_cast<size_t>(m_clipW) * m_clipH, kTransparent);
        for (int32_t j = 0; j < m_clipH; ++j)
        {
            for (int32_t i = 0; i < m_clipW; ++i)
            {
                m_clipPixels[static_cast<size_t>(j) * m_clipW + i] = doc().context.GetPixel(doc().selX + i, doc().selY + j);
            }
        }
        m_hasClip = true;
    }

    void MainWindow::OnCut(IInspectable const& sender, RoutedEventArgs const& args)
    {
        if (!doc().hasSelection)
        {
            return;
        }
        OnCopy(sender, args);
        PushUndo();
        ClearRegion(doc().selX, doc().selY, doc().selW, doc().selH);
        Render();
    }

    void MainWindow::OnPaste(IInspectable const&, RoutedEventArgs const&)
    {
        if (!m_hasClip)
        {
            return;
        }
        PushUndo();
        const int32_t tx = doc().hasSelection ? doc().selX : 0;
        const int32_t ty = doc().hasSelection ? doc().selY : 0;
        for (int32_t j = 0; j < m_clipH; ++j)
        {
            for (int32_t i = 0; i < m_clipW; ++i)
            {
                const winrt::Windows::UI::Color c = m_clipPixels[static_cast<size_t>(j) * m_clipW + i];
                if (c.A > 0)
                {
                    doc().context.SetPixel(tx + i, ty + j, c);
                }
            }
        }
        doc().hasSelection = true;
        doc().selX = tx;
        doc().selY = ty;
        doc().selW = m_clipW;
        doc().selH = m_clipH;
        Render();
    }

    void MainWindow::OnDelete(IInspectable const&, RoutedEventArgs const&)
    {
        if (!doc().hasSelection)
        {
            return;
        }
        PushUndo();
        ClearRegion(doc().selX, doc().selY, doc().selW, doc().selH);
        Render();
    }

    // ---- File ---------------------------------------------------------------

    void MainWindow::ResetTransient()
    {
        m_selecting = false;
        m_moving = false;
        m_shapeActive = false;
        m_floatPixels.clear();
    }

    void MainWindow::AddDocument(winrt::IconMaster::DrawingContext const& context, winrt::hstring const& title, int32_t zoom)
    {
        m_updatingTabs = true;

        Document d;
        d.context = context;
        d.zoom = std::clamp(zoom, k_minZoom, k_maxZoom);
        d.title = title;
        m_docs.push_back(std::move(d));

        auto item = winrt::Microsoft::UI::Xaml::Controls::TabViewItem();
        item.Header(winrt::box_value(title));
        item.IsClosable(true);
        Tabs().TabItems().Append(item);

        m_active = m_docs.size() - 1;
        Tabs().SelectedIndex(static_cast<int32_t>(m_active));

        m_updatingTabs = false;

        ResetTransient();
        RebuildDisplay();
    }

    void MainWindow::NewDocument()
    {
        auto context = winrt::IconMaster::DrawingContext(k_canvasSize, k_canvasSize);
        context.Color(ColorPickerControl().Color());
        m_docCounter += 1;
        AddDocument(context, L"Icon " + winrt::to_hstring(m_docCounter), 16);
        StatusText().Text(L"New 32 x 32 icon.");
    }

    void MainWindow::OnNew(IInspectable const&, RoutedEventArgs const&)
    {
        NewDocument();
    }

    winrt::fire_and_forget MainWindow::OnSave(IInspectable const&, RoutedEventArgs const&)
    {
        auto lifetime = get_strong();
        if (doc().context == nullptr)
        {
            co_return;
        }

        winrt::Windows::Storage::Pickers::FileSavePicker picker;
        {
            auto windowNative = this->try_as<::IWindowNative>();
            HWND hwnd{};
            winrt::check_hresult(windowNative->get_WindowHandle(&hwnd));
            auto initWithWindow = picker.as<::IInitializeWithWindow>();
            winrt::check_hresult(initWithWindow->Initialize(hwnd));
        }
        picker.SuggestedStartLocation(winrt::Windows::Storage::Pickers::PickerLocationId::PicturesLibrary);
        picker.SuggestedFileName(L"icon");
        picker.FileTypeChoices().Insert(L"PNG image", winrt::single_threaded_vector<winrt::hstring>({ L".png" }));

        auto file = co_await picker.PickSaveFileAsync();
        if (file == nullptr)
        {
            co_return;
        }

        const int32_t w = doc().context.PixelWidth();
        const int32_t h = doc().context.PixelHeight();
        std::vector<uint8_t> bytes(static_cast<size_t>(w) * h * 4);
        for (int32_t y = 0; y < h; ++y)
        {
            for (int32_t x = 0; x < w; ++x)
            {
                const auto c = doc().context.GetPixel(x, y);
                const size_t i = (static_cast<size_t>(y) * w + x) * 4;
                bytes[i + 0] = c.B;
                bytes[i + 1] = c.G;
                bytes[i + 2] = c.R;
                bytes[i + 3] = c.A;
            }
        }

        auto stream = co_await file.OpenAsync(winrt::Windows::Storage::FileAccessMode::ReadWrite);
        auto encoder = co_await winrt::Windows::Graphics::Imaging::BitmapEncoder::CreateAsync(
            winrt::Windows::Graphics::Imaging::BitmapEncoder::PngEncoderId(), stream);
        encoder.SetPixelData(
            winrt::Windows::Graphics::Imaging::BitmapPixelFormat::Bgra8,
            winrt::Windows::Graphics::Imaging::BitmapAlphaMode::Straight,
            static_cast<uint32_t>(w), static_cast<uint32_t>(h),
            96.0, 96.0, bytes);
        co_await encoder.FlushAsync();

        StatusText().Text(L"Saved " + file.Name());
    }

    winrt::fire_and_forget MainWindow::OnOpen(IInspectable const&, RoutedEventArgs const&)
    {
        auto lifetime = get_strong();

        winrt::Windows::Storage::Pickers::FileOpenPicker picker;
        {
            auto windowNative = this->try_as<::IWindowNative>();
            HWND hwnd{};
            winrt::check_hresult(windowNative->get_WindowHandle(&hwnd));
            auto initWithWindow = picker.as<::IInitializeWithWindow>();
            winrt::check_hresult(initWithWindow->Initialize(hwnd));
        }
        picker.SuggestedStartLocation(winrt::Windows::Storage::Pickers::PickerLocationId::PicturesLibrary);
        picker.ViewMode(winrt::Windows::Storage::Pickers::PickerViewMode::Thumbnail);
        picker.FileTypeFilter().Append(L".png");

        auto file = co_await picker.PickSingleFileAsync();
        if (file == nullptr)
        {
            co_return;
        }

        auto stream = co_await file.OpenAsync(winrt::Windows::Storage::FileAccessMode::Read);
        auto decoder = co_await winrt::Windows::Graphics::Imaging::BitmapDecoder::CreateAsync(stream);
        const uint32_t w = decoder.PixelWidth();
        const uint32_t h = decoder.PixelHeight();
        if (w == 0 || h == 0 || w > 256 || h > 256)
        {
            StatusText().Text(L"Image must be between 1x1 and 256x256.");
            co_return;
        }

        auto provider = co_await decoder.GetPixelDataAsync(
            winrt::Windows::Graphics::Imaging::BitmapPixelFormat::Bgra8,
            winrt::Windows::Graphics::Imaging::BitmapAlphaMode::Straight,
            winrt::Windows::Graphics::Imaging::BitmapTransform(),
            winrt::Windows::Graphics::Imaging::ExifOrientationMode::IgnoreExifOrientation,
            winrt::Windows::Graphics::Imaging::ColorManagementMode::DoNotColorManage);
        auto bytes = provider.DetachPixelData();

        auto context = winrt::IconMaster::DrawingContext(static_cast<int32_t>(w), static_cast<int32_t>(h));
        context.Color(ColorPickerControl().Color());
        for (uint32_t y = 0; y < h; ++y)
        {
            for (uint32_t x = 0; x < w; ++x)
            {
                const size_t i = (static_cast<size_t>(y) * w + x) * 4;
                const winrt::Windows::UI::Color c{ bytes[i + 3], bytes[i + 2], bytes[i + 1], bytes[i + 0] };
                context.SetPixel(static_cast<int32_t>(x), static_cast<int32_t>(y), c);
            }
        }

        const int32_t fit = static_cast<int32_t>(512u / std::max(w, h));
        AddDocument(context, file.Name(), fit);
        StatusText().Text(L"Opened " + file.Name());
    }

    std::vector<uint8_t> MainWindow::ScaleCanvas(int32_t target)
    {
        const int32_t w = doc().context.PixelWidth();
        const int32_t h = doc().context.PixelHeight();
        std::vector<uint8_t> out(static_cast<size_t>(target) * target * 4);
        for (int32_t y = 0; y < target; ++y)
        {
            for (int32_t x = 0; x < target; ++x)
            {
                const int32_t sx = x * w / target; // nearest-neighbour
                const int32_t sy = y * h / target;
                const auto c = doc().context.GetPixel(sx, sy);
                const size_t i = (static_cast<size_t>(y) * target + x) * 4;
                out[i + 0] = c.B;
                out[i + 1] = c.G;
                out[i + 2] = c.R;
                out[i + 3] = c.A;
            }
        }
        return out;
    }

    winrt::fire_and_forget MainWindow::OnExportIco(IInspectable const&, RoutedEventArgs const&)
    {
        auto lifetime = get_strong();
        if (doc().context == nullptr)
        {
            co_return;
        }

        winrt::Windows::Storage::Pickers::FileSavePicker picker;
        {
            auto windowNative = this->try_as<::IWindowNative>();
            HWND hwnd{};
            winrt::check_hresult(windowNative->get_WindowHandle(&hwnd));
            auto initWithWindow = picker.as<::IInitializeWithWindow>();
            winrt::check_hresult(initWithWindow->Initialize(hwnd));
        }
        picker.SuggestedStartLocation(winrt::Windows::Storage::Pickers::PickerLocationId::PicturesLibrary);
        picker.SuggestedFileName(L"icon");
        picker.FileTypeChoices().Insert(L"Windows icon", winrt::single_threaded_vector<winrt::hstring>({ L".ico" }));

        auto file = co_await picker.PickSaveFileAsync();
        if (file == nullptr)
        {
            co_return;
        }

        // Render each icon size to a PNG blob (ICO may embed PNG-compressed images).
        const int32_t sizes[] = { 16, 32, 48, 256 };
        std::vector<std::vector<uint8_t>> pngs;
        for (int32_t s : sizes)
        {
            const std::vector<uint8_t> bytes = ScaleCanvas(s);

            winrt::Windows::Storage::Streams::InMemoryRandomAccessStream mem;
            auto encoder = co_await winrt::Windows::Graphics::Imaging::BitmapEncoder::CreateAsync(
                winrt::Windows::Graphics::Imaging::BitmapEncoder::PngEncoderId(), mem);
            encoder.SetPixelData(
                winrt::Windows::Graphics::Imaging::BitmapPixelFormat::Bgra8,
                winrt::Windows::Graphics::Imaging::BitmapAlphaMode::Straight,
                static_cast<uint32_t>(s), static_cast<uint32_t>(s),
                96.0, 96.0, bytes);
            co_await encoder.FlushAsync();

            const uint32_t len = static_cast<uint32_t>(mem.Size());
            winrt::Windows::Storage::Streams::DataReader reader(mem.GetInputStreamAt(0));
            co_await reader.LoadAsync(len);
            std::vector<uint8_t> png(len);
            reader.ReadBytes(png);
            pngs.push_back(std::move(png));
        }

        const auto putU16 = [](std::vector<uint8_t>& v, uint16_t x)
        {
            v.push_back(static_cast<uint8_t>(x & 0xFF));
            v.push_back(static_cast<uint8_t>((x >> 8) & 0xFF));
        };
        const auto putU32 = [](std::vector<uint8_t>& v, uint32_t x)
        {
            v.push_back(static_cast<uint8_t>(x & 0xFF));
            v.push_back(static_cast<uint8_t>((x >> 8) & 0xFF));
            v.push_back(static_cast<uint8_t>((x >> 16) & 0xFF));
            v.push_back(static_cast<uint8_t>((x >> 24) & 0xFF));
        };

        const uint16_t count = static_cast<uint16_t>(sizeof(sizes) / sizeof(sizes[0]));
        std::vector<uint8_t> ico;

        // ICONDIR
        putU16(ico, 0); // reserved
        putU16(ico, 1); // type = icon
        putU16(ico, count);

        // ICONDIRENTRY[] — image data starts after the header + all entries.
        uint32_t offset = 6u + 16u * count;
        for (size_t k = 0; k < pngs.size(); ++k)
        {
            const int32_t s = sizes[k];
            ico.push_back(static_cast<uint8_t>(s >= 256 ? 0 : s)); // width (0 => 256)
            ico.push_back(static_cast<uint8_t>(s >= 256 ? 0 : s)); // height
            ico.push_back(0);  // colour count
            ico.push_back(0);  // reserved
            putU16(ico, 1);    // colour planes
            putU16(ico, 32);   // bits per pixel
            putU32(ico, static_cast<uint32_t>(pngs[k].size()));
            putU32(ico, offset);
            offset += static_cast<uint32_t>(pngs[k].size());
        }

        for (auto const& png : pngs)
        {
            ico.insert(ico.end(), png.begin(), png.end());
        }

        co_await winrt::Windows::Storage::FileIO::WriteBytesAsync(file, ico);
        StatusText().Text(L"Exported " + file.Name() + L" (16, 32, 48, 256)");
    }

    // ---- History ------------------------------------------------------------

    MainWindow::Snapshot MainWindow::CaptureSnapshot()
    {
        const int32_t w = doc().context.PixelWidth();
        const int32_t h = doc().context.PixelHeight();
        Snapshot s;
        s.w = w;
        s.h = h;
        s.pixels.resize(static_cast<size_t>(w) * h);
        for (int32_t y = 0; y < h; ++y)
        {
            for (int32_t x = 0; x < w; ++x)
            {
                s.pixels[static_cast<size_t>(y) * w + x] = doc().context.GetPixel(x, y);
            }
        }
        return s;
    }

    void MainWindow::RestoreSnapshot(Snapshot const& snap)
    {
        if (snap.w != doc().context.PixelWidth() || snap.h != doc().context.PixelHeight())
        {
            auto context = winrt::IconMaster::DrawingContext(snap.w, snap.h);
            context.Color(doc().context.Color());
            doc().context = context;
        }
        for (int32_t y = 0; y < snap.h; ++y)
        {
            for (int32_t x = 0; x < snap.w; ++x)
            {
                doc().context.SetPixel(x, y, snap.pixels[static_cast<size_t>(y) * snap.w + x]);
            }
        }
        // The selection may reference pixels that no longer match; clear it.
        doc().hasSelection = false;
        m_selecting = false;
        m_moving = false;
        m_shapeActive = false;
        m_floatPixels.clear();
    }

    void MainWindow::PushUndo()
    {
        doc().undo.push_back(CaptureSnapshot());
        if (doc().undo.size() > k_maxHistory)
        {
            doc().undo.erase(doc().undo.begin());
        }
        doc().redo.clear();
    }

    void MainWindow::ClearHistory()
    {
        doc().undo.clear();
        doc().redo.clear();
    }

    void MainWindow::OnUndo(IInspectable const&, RoutedEventArgs const&)
    {
        if (doc().undo.empty())
        {
            StatusText().Text(L"Nothing to undo.");
            return;
        }
        doc().redo.push_back(CaptureSnapshot());
        Snapshot snap = std::move(doc().undo.back());
        doc().undo.pop_back();
        const bool resized = (snap.w != doc().context.PixelWidth() || snap.h != doc().context.PixelHeight());
        RestoreSnapshot(snap);
        if (resized) { RebuildDisplay(); } else { Render(); }
        StatusText().Text(L"Undo.");
    }

    void MainWindow::OnRedo(IInspectable const&, RoutedEventArgs const&)
    {
        if (doc().redo.empty())
        {
            StatusText().Text(L"Nothing to redo.");
            return;
        }
        doc().undo.push_back(CaptureSnapshot());
        Snapshot snap = std::move(doc().redo.back());
        doc().redo.pop_back();
        const bool resized = (snap.w != doc().context.PixelWidth() || snap.h != doc().context.PixelHeight());
        RestoreSnapshot(snap);
        if (resized) { RebuildDisplay(); } else { Render(); }
        StatusText().Text(L"Redo.");
    }

    // ---- Tabs ---------------------------------------------------------------

    void MainWindow::OnTabSelectionChanged(IInspectable const&, winrt::Microsoft::UI::Xaml::Controls::SelectionChangedEventArgs const&)
    {
        if (m_updatingTabs)
        {
            return;
        }
        const int32_t idx = Tabs().SelectedIndex();
        if (idx < 0 || static_cast<size_t>(idx) >= m_docs.size())
        {
            return;
        }
        m_active = static_cast<size_t>(idx);
        ResetTransient();
        RebuildDisplay();
    }

    void MainWindow::OnAddTab(winrt::Microsoft::UI::Xaml::Controls::TabView const&, IInspectable const&)
    {
        NewDocument();
    }

    void MainWindow::OnTabCloseRequested(winrt::Microsoft::UI::Xaml::Controls::TabView const&, winrt::Microsoft::UI::Xaml::Controls::TabViewTabCloseRequestedEventArgs const& args)
    {
        if (m_docs.size() <= 1)
        {
            return; // always keep at least one document open
        }

        uint32_t index = 0;
        if (!Tabs().TabItems().IndexOf(args.Tab(), index))
        {
            return;
        }

        m_updatingTabs = true;
        m_docs.erase(m_docs.begin() + index);
        Tabs().TabItems().RemoveAt(index);

        if (m_active >= m_docs.size())
        {
            m_active = m_docs.size() - 1;
        }
        else if (index < m_active)
        {
            m_active -= 1;
        }
        Tabs().SelectedIndex(static_cast<int32_t>(m_active));
        m_updatingTabs = false;

        ResetTransient();
        RebuildDisplay();
    }

    // ---- Rendering ----------------------------------------------------------

    void MainWindow::RebuildDisplay()
    {
        if (doc().context == nullptr)
        {
            return;
        }

        // One extra pixel so the closing grid line on the right/bottom is drawn.
        const int32_t dw = doc().context.PixelWidth() * doc().zoom + 1;
        const int32_t dh = doc().context.PixelHeight() * doc().zoom + 1;

        m_display = WriteableBitmap(dw, dh);
        CanvasImage().Source(m_display);
        Render();

        ZoomText().Text(winrt::to_hstring(doc().zoom * 100) + L"%");
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
        if ((dx % doc().zoom == 0) || (dy % doc().zoom == 0))
        {
            b = g = r = 0xA0; // grid line
        }
        else
        {
            const int32_t lx = dx / doc().zoom;
            const int32_t ly = dy / doc().zoom;
            const winrt::Windows::UI::Color c = doc().context.GetPixel(lx, ly);
            if (c.A == 0)
            {
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

    void MainWindow::PaintPreviewBlock(uint8_t* data, int32_t displayWidth, int32_t displayHeight, int32_t lx, int32_t ly, winrt::Windows::UI::Color const& color)
    {
        const int32_t x0 = lx * doc().zoom;
        const int32_t y0 = ly * doc().zoom;

        for (int32_t dy = y0 + 1; dy < y0 + doc().zoom && dy < displayHeight; ++dy)
        {
            for (int32_t dx = x0 + 1; dx < x0 + doc().zoom && dx < displayWidth; ++dx)
            {
                const size_t i = (static_cast<size_t>(dy) * displayWidth + dx) * 4;
                data[i + 0] = color.B;
                data[i + 1] = color.G;
                data[i + 2] = color.R;
                data[i + 3] = 0xFF;
            }
        }
    }

    void MainWindow::RenderBase(uint8_t* data, int32_t dw, int32_t dh)
    {
        for (int32_t dy = 0; dy < dh; ++dy)
        {
            for (int32_t dx = 0; dx < dw; ++dx)
            {
                WriteDisplayPixel(data, dw, dx, dy);
            }
        }
    }

    void MainWindow::OverlayShapePreview(uint8_t* data, int32_t dw, int32_t dh)
    {
        if (m_currentShape == nullptr)
        {
            return;
        }
        const winrt::Windows::UI::Color color = doc().context.Color();
        if (color.A == 0)
        {
            return;
        }
        auto points = m_currentShape.Rasterize(m_shapeStartX, m_shapeStartY, m_shapeCurX, m_shapeCurY);
        for (auto const& p : points)
        {
            PaintPreviewBlock(data, dw, dh, p.X, p.Y, color);
        }
    }

    void MainWindow::OverlayFloating(uint8_t* data, int32_t dw, int32_t dh)
    {
        const int32_t w = doc().context.PixelWidth();
        const int32_t h = doc().context.PixelHeight();
        for (int32_t j = 0; j < m_floatH; ++j)
        {
            for (int32_t i = 0; i < m_floatW; ++i)
            {
                const winrt::Windows::UI::Color c = m_floatPixels[static_cast<size_t>(j) * m_floatW + i];
                if (c.A == 0)
                {
                    continue;
                }
                const int32_t tx = doc().selX + m_moveDX + i;
                const int32_t ty = doc().selY + m_moveDY + j;
                if (tx < 0 || tx >= w || ty < 0 || ty >= h)
                {
                    continue;
                }
                PaintPreviewBlock(data, dw, dh, tx, ty, c);
            }
        }
    }

    void MainWindow::OverlaySelectionBorder(uint8_t* data, int32_t dw, int32_t dh)
    {
        const int32_t sx = doc().selX + (m_moving ? m_moveDX : 0);
        const int32_t sy = doc().selY + (m_moving ? m_moveDY : 0);
        const int32_t x0 = sx * doc().zoom;
        const int32_t y0 = sy * doc().zoom;
        const int32_t x1 = (sx + doc().selW) * doc().zoom;
        const int32_t y1 = (sy + doc().selH) * doc().zoom;

        auto put = [&](int32_t dx, int32_t dy)
        {
            if (dx < 0 || dx >= dw || dy < 0 || dy >= dh)
            {
                return;
            }
            const size_t i = (static_cast<size_t>(dy) * dw + dx) * 4;
            const bool black = ((((dx + dy) / 4) & 1) == 0);
            const uint8_t v = black ? 0x00 : 0xFF;
            data[i + 0] = v;
            data[i + 1] = v;
            data[i + 2] = v;
            data[i + 3] = 0xFF;
        };

        for (int32_t dx = x0; dx <= x1; ++dx) { put(dx, y0); put(dx, y1); }
        for (int32_t dy = y0; dy <= y1; ++dy) { put(x0, dy); put(x1, dy); }
    }

    void MainWindow::Render()
    {
        if (m_display == nullptr)
        {
            return;
        }

        const int32_t dw = m_display.PixelWidth();
        const int32_t dh = m_display.PixelHeight();
        uint8_t* data = DisplayData();

        RenderBase(data, dw, dh);
        if (m_shapeActive)   { OverlayShapePreview(data, dw, dh); }
        if (m_moving)        { OverlayFloating(data, dw, dh); }
        if (doc().hasSelection)  { OverlaySelectionBorder(data, dw, dh); }

        m_display.Invalidate();
    }
}
