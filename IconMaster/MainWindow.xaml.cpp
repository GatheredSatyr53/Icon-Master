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
#include <winrt/Windows.Storage.AccessCache.h>
#include <winrt/Windows.Storage.Pickers.h>
#include <winrt/Windows.Storage.Streams.h>
#include <winrt/Windows.UI.StartScreen.h>
#include <microsoft.ui.xaml.window.h>
#include <shobjidl_core.h>
#include <robuffer.h>
#include <algorithm>
#include <cmath>
#include <string>
#include <utility>
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

    // Walk up from a tapped element to the swatch Border that carries the
    // "#AARRGGBB" hex in its Tag. Returns an empty string if the tap did not
    // land on a swatch (e.g. empty space inside the ItemsRepeater).
    winrt::hstring SwatchHexFromSource(winrt::Windows::Foundation::IInspectable const& source)
    {
        auto element = source.try_as<winrt::Microsoft::UI::Xaml::FrameworkElement>();
        while (element)
        {
            const auto hex = winrt::unbox_value_or<winrt::hstring>(element.Tag(), L"");
            if (!hex.empty()) { return hex; }
            element = winrt::Microsoft::UI::Xaml::Media::VisualTreeHelper::GetParent(element)
                          .try_as<winrt::Microsoft::UI::Xaml::FrameworkElement>();
        }
        return L"";
    }

    // Source-over composite of src (scaled by coverage) onto dst.
    winrt::Windows::UI::Color OverBlend(winrt::Windows::UI::Color const& src, double coverage, winrt::Windows::UI::Color const& dst)
    {
        const double sa = (src.A / 255.0) * std::clamp(coverage, 0.0, 1.0);
        if (sa <= 0.0) { return dst; }
        const double da = dst.A / 255.0;
        const double outA = sa + da * (1.0 - sa);
        if (outA <= 0.0) { return kTransparent; }
        auto ch = [&](uint8_t s, uint8_t d) -> uint8_t
        {
            const double v = (s / 255.0 * sa + d / 255.0 * da * (1.0 - sa)) / outA;
            return static_cast<uint8_t>(std::clamp(static_cast<int>(std::lround(v * 255.0)), 0, 255));
        };
        return winrt::Windows::UI::Color{
            static_cast<uint8_t>(std::clamp(static_cast<int>(std::lround(outA * 255.0)), 0, 255)),
            ch(src.R, dst.R), ch(src.G, dst.G), ch(src.B, dst.B) };
    }

    // Soft erase: reduce the destination alpha by coverage.
    winrt::Windows::UI::Color EraseBlend(winrt::Windows::UI::Color const& dst, double coverage)
    {
        const double da = (dst.A / 255.0) * (1.0 - std::clamp(coverage, 0.0, 1.0));
        return winrt::Windows::UI::Color{
            static_cast<uint8_t>(std::clamp(static_cast<int>(std::lround(da * 255.0)), 0, 255)),
            dst.R, dst.G, dst.B };
    }
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
            appWindow.SetIcon(L"Assets/App.ico"); // title-bar / taskbar icon
        }

        {
            auto ctx = winrt::IconMaster::DrawingContext(k_canvasSize, k_canvasSize);
            doc().layers.push_back(Layer{ ctx, winrt::hstring{ L"Layer 1" }, true, 100 });
            doc().activeLayer = 0;
            doc().layerCounter = 1;
            doc().context = ctx;
        }
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

        // Seed the fixed preset palette (same repeater/style as the custom one).
        for (auto const& hex : {
                L"#FF000000", L"#FF808080", L"#FFFFFFFF", L"#FFE81123",
                L"#FFFF8C00", L"#FFFFF100", L"#FF107C10", L"#FF0078D7",
                L"#FF00B7C3", L"#FF881798", L"#FF8E562E", L"#00000000" })
        {
            m_standardItems.Append(winrt::box_value(winrt::hstring{ hex }));
        }
        StandardPaletteRepeater().ItemsSource(m_standardItems);

        LoadPalette();       // restore the custom palette from the previous session
        PaletteRepeater().ItemsSource(m_paletteItems);
        RebuildPaletteUI();

        RebuildDisplay();
        RebuildLayersUI();
        RebuildRecentMenu(); // populate from the persisted most-recently-used list
        UpdateJumpListAsync(); // refresh the taskbar jump list from the same list
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
        else if (tag == L"wand")       { m_toolKind = ToolKind::Wand; }
        else                           { m_toolKind = ToolKind::Pen; }

        m_currentShape = ShapeToolForKind(m_toolKind);
        if (!IsShapeTool(m_toolKind) && m_toolKind != ToolKind::Select && m_toolKind != ToolKind::Wand)
        {
            m_currentTool = ToolForKind(m_toolKind);
        }
    }

    // ---- Colour -------------------------------------------------------------

    void MainWindow::OnColorChanged(ColorPicker const&, ColorChangedEventArgs const& args)
    {
        if (doc().context == nullptr || m_suppressColorSync)
        {
            return;
        }
        doc().context.Color(args.NewColor());
    }

    // ---- Custom palette -----------------------------------------------------

    winrt::hstring MainWindow::ColorToHex(winrt::Windows::UI::Color const& c)
    {
        wchar_t buffer[10];
        swprintf_s(buffer, L"#%02X%02X%02X%02X", c.A, c.R, c.G, c.B);
        return winrt::hstring{ buffer };
    }

    winrt::Windows::UI::Color MainWindow::HexToColor(std::wstring_view hex)
    {
        std::wstring text{ hex };
        if (!text.empty() && text.front() == L'#')
        {
            text.erase(0, 1);
        }
        const uint32_t argb = static_cast<uint32_t>(std::wcstoul(text.c_str(), nullptr, 16));
        return winrt::Windows::UI::Color{
            static_cast<uint8_t>((argb >> 24) & 0xFF),
            static_cast<uint8_t>((argb >> 16) & 0xFF),
            static_cast<uint8_t>((argb >> 8) & 0xFF),
            static_cast<uint8_t>(argb & 0xFF) };
    }

    void MainWindow::AddPaletteColor(winrt::Windows::UI::Color const& color)
    {
        // Skip exact duplicates so the palette stays a set of distinct swatches.
        for (auto const& existing : m_palette)
        {
            if (existing.A == color.A && existing.R == color.R &&
                existing.G == color.G && existing.B == color.B)
            {
                return;
            }
        }
        if (m_palette.size() >= k_maxPalette)
        {
            return;
        }
        m_palette.push_back(color);
    }

    void MainWindow::RebuildPaletteUI()
    {
        // The swatches are laid out by a WrapLayout inside PaletteRepeater; we only
        // have to keep the bound collection in sync with m_palette. ElementPrepared
        // fills in each realized swatch's colour on demand.
        m_paletteItems.Clear();
        for (auto const& color : m_palette)
        {
            m_paletteItems.Append(winrt::box_value(ColorToHex(color)));
        }

        PaletteEmptyHint().Visibility(m_palette.empty()
            ? winrt::Microsoft::UI::Xaml::Visibility::Visible
            : winrt::Microsoft::UI::Xaml::Visibility::Collapsed);
    }

    void MainWindow::OnPaletteElementPrepared(winrt::Microsoft::UI::Xaml::Controls::ItemsRepeater const& sender, winrt::Microsoft::UI::Xaml::Controls::ItemsRepeaterElementPreparedEventArgs const& args)
    {
        auto swatch = args.Element().try_as<Border>();
        if (!swatch) { return; }

        // Read the swatch colour from whichever repeater raised the event (standard
        // or custom), so both share one appearance and one set of handlers.
        auto source = sender.ItemsSourceView();
        const auto index = args.Index();
        if (source == nullptr || index < 0 || index >= source.Count()) { return; }

        const auto hex = winrt::unbox_value_or<winrt::hstring>(source.GetAt(index), L"");
        const auto color = HexToColor(std::wstring_view{ hex });

        swatch.Tag(winrt::box_value(hex));
        ToolTipService::SetToolTip(swatch, winrt::box_value(hex));

        // A uniform 1px border keeps light and transparent swatches visible.
        swatch.BorderBrush(winrt::Microsoft::UI::Xaml::Media::SolidColorBrush{
            winrt::Windows::UI::Color{ 0x60, 0x80, 0x80, 0x80 } });
        swatch.BorderThickness(winrt::Microsoft::UI::Xaml::ThicknessHelper::FromUniformLength(1));

        if (color.A == 0)
        {
            // Fully transparent: show a 2x2 checkerboard, matching how the canvas
            // renders transparency, instead of a fill.
            swatch.Background(winrt::Microsoft::UI::Xaml::Media::SolidColorBrush{
                winrt::Windows::UI::Color{ 0xFF, 0xFF, 0xFF, 0xFF } });

            Grid checker;
            checker.ColumnDefinitions().Append(winrt::Microsoft::UI::Xaml::Controls::ColumnDefinition{});
            checker.ColumnDefinitions().Append(winrt::Microsoft::UI::Xaml::Controls::ColumnDefinition{});
            checker.RowDefinitions().Append(winrt::Microsoft::UI::Xaml::Controls::RowDefinition{});
            checker.RowDefinitions().Append(winrt::Microsoft::UI::Xaml::Controls::RowDefinition{});

            auto grayCell = [](int32_t row, int32_t col)
            {
                Border cell;
                cell.Background(winrt::Microsoft::UI::Xaml::Media::SolidColorBrush{
                    winrt::Windows::UI::Color{ 0xFF, 0xC0, 0xC0, 0xC0 } });
                Grid::SetRow(cell, row);
                Grid::SetColumn(cell, col);
                return cell;
            };
            checker.Children().Append(grayCell(0, 0));
            checker.Children().Append(grayCell(1, 1));
            swatch.Child(checker);
        }
        else
        {
            swatch.Child(nullptr);
            swatch.Background(winrt::Microsoft::UI::Xaml::Media::SolidColorBrush{ color });
        }
    }

    void MainWindow::OnPaletteSwatchTapped(IInspectable const&, winrt::Microsoft::UI::Xaml::Input::TappedRoutedEventArgs const& args)
    {
        const auto hex = SwatchHexFromSource(args.OriginalSource());
        if (hex.empty()) { return; }
        ColorPickerControl().Color(HexToColor(std::wstring_view{ hex }));
    }

    void MainWindow::OnPaletteSwatchRightTapped(IInspectable const&, winrt::Microsoft::UI::Xaml::Input::RightTappedRoutedEventArgs const& args)
    {
        const auto hex = SwatchHexFromSource(args.OriginalSource());
        if (hex.empty()) { return; }
        const auto target = HexToColor(std::wstring_view{ hex });

        for (auto it = m_palette.begin(); it != m_palette.end(); ++it)
        {
            if (it->A == target.A && it->R == target.R && it->G == target.G && it->B == target.B)
            {
                m_palette.erase(it);
                break;
            }
        }
        RebuildPaletteUI();
        SavePalette();
    }

    void MainWindow::OnPaletteAdd(IInspectable const&, RoutedEventArgs const&)
    {
        AddPaletteColor(ColorPickerControl().Color());
        RebuildPaletteUI();
        SavePalette();
    }

    void MainWindow::OnPaletteFromImage(IInspectable const&, RoutedEventArgs const&)
    {
        if (doc().context == nullptr) { return; }

        const int32_t w = doc().context.PixelWidth();
        const int32_t h = doc().context.PixelHeight();

        // Count how often each fully/partly opaque colour occurs, then take the
        // most common ones. Fully transparent pixels carry no colour and are skipped.
        std::vector<std::pair<uint32_t, int32_t>> counts; // packed ARGB -> frequency
        counts.reserve(256);
        for (int32_t y = 0; y < h; ++y)
        {
            for (int32_t x = 0; x < w; ++x)
            {
                const auto c = doc().context.GetPixel(x, y);
                if (c.A == 0) { continue; }
                const uint32_t key = (static_cast<uint32_t>(c.A) << 24) |
                                     (static_cast<uint32_t>(c.R) << 16) |
                                     (static_cast<uint32_t>(c.G) << 8) |
                                      static_cast<uint32_t>(c.B);
                auto found = std::find_if(counts.begin(), counts.end(),
                    [key](auto const& p) { return p.first == key; });
                if (found != counts.end()) { ++found->second; }
                else { counts.emplace_back(key, 1); }
            }
        }

        if (counts.empty()) { return; }

        std::stable_sort(counts.begin(), counts.end(),
            [](auto const& a, auto const& b) { return a.second > b.second; });

        for (auto const& entry : counts)
        {
            if (m_palette.size() >= k_maxPalette) { break; }
            const uint32_t key = entry.first;
            AddPaletteColor(winrt::Windows::UI::Color{
                static_cast<uint8_t>((key >> 24) & 0xFF),
                static_cast<uint8_t>((key >> 16) & 0xFF),
                static_cast<uint8_t>((key >> 8) & 0xFF),
                static_cast<uint8_t>(key & 0xFF) });
        }
        RebuildPaletteUI();
        SavePalette();
    }

    void MainWindow::OnPaletteClear(IInspectable const&, RoutedEventArgs const&)
    {
        m_palette.clear();
        RebuildPaletteUI();
        SavePalette();
    }

    void MainWindow::SavePalette()
    {
        std::wstring joined;
        for (auto const& c : m_palette)
        {
            if (!joined.empty()) { joined += L';'; }
            joined += std::wstring{ ColorToHex(c) };
        }
        auto settings = winrt::Windows::Storage::ApplicationData::Current().LocalSettings();
        settings.Values().Insert(L"CustomPalette", winrt::box_value(winrt::hstring{ joined }));
    }

    void MainWindow::LoadPalette()
    {
        auto settings = winrt::Windows::Storage::ApplicationData::Current().LocalSettings();
        auto value = settings.Values().TryLookup(L"CustomPalette");
        if (!value) { return; }

        const std::wstring joined{ winrt::unbox_value_or<winrt::hstring>(value, L"") };
        m_palette.clear();
        size_t start = 0;
        while (start <= joined.size())
        {
            const size_t sep = joined.find(L';', start);
            const std::wstring token = joined.substr(start, sep == std::wstring::npos ? std::wstring::npos : sep - start);
            if (!token.empty())
            {
                AddPaletteColor(HexToColor(token));
            }
            if (sep == std::wstring::npos) { break; }
            start = sep + 1;
        }
    }

    winrt::fire_and_forget MainWindow::OnPaletteSave(IInspectable const&, RoutedEventArgs const&)
    {
        auto lifetime = get_strong();

        winrt::Windows::Storage::Pickers::FileSavePicker picker;
        {
            auto windowNative = this->try_as<::IWindowNative>();
            HWND hwnd{};
            winrt::check_hresult(windowNative->get_WindowHandle(&hwnd));
            auto initWithWindow = picker.as<::IInitializeWithWindow>();
            winrt::check_hresult(initWithWindow->Initialize(hwnd));
        }

        picker.SuggestedStartLocation(winrt::Windows::Storage::Pickers::PickerLocationId::DocumentsLibrary);
        picker.SuggestedFileName(L"palette");
        picker.FileTypeChoices().Insert(L"Palette", winrt::single_threaded_vector<winrt::hstring>({ L".txt" }));

        auto file = co_await picker.PickSaveFileAsync();
        if (!file) { co_return; }

        std::wstring contents;
        for (auto const& c : m_palette)
        {
            contents += std::wstring{ ColorToHex(c) };
            contents += L"\r\n";
        }
        co_await winrt::Windows::Storage::FileIO::WriteTextAsync(file, winrt::hstring{ contents });
    }

    winrt::fire_and_forget MainWindow::OnPaletteLoad(IInspectable const&, RoutedEventArgs const&)
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

        picker.SuggestedStartLocation(winrt::Windows::Storage::Pickers::PickerLocationId::DocumentsLibrary);
        picker.FileTypeFilter().Append(L".txt");
        picker.FileTypeFilter().Append(L".pal");

        auto file = co_await picker.PickSingleFileAsync();
        if (!file) { co_return; }

        const auto text = co_await winrt::Windows::Storage::FileIO::ReadTextAsync(file);
        const std::wstring body{ text };

        // Accept any whitespace/comma/semicolon separated list of "#AARRGGBB" tokens.
        std::wstring token;
        auto flush = [&]()
        {
            if (!token.empty())
            {
                AddPaletteColor(HexToColor(token));
                token.clear();
            }
        };
        for (wchar_t ch : body)
        {
            if (ch == L'\r' || ch == L'\n' || ch == L' ' || ch == L'\t' || ch == L';' || ch == L',')
            {
                flush();
            }
            else
            {
                token += ch;
            }
        }
        flush();

        RebuildPaletteUI();
        SavePalette();
    }

    // ---- Zoom ---------------------------------------------------------------

    void MainWindow::OnZoomIn(IInspectable const&, RoutedEventArgs const&) { SetZoom(doc().zoom + 4); }
    void MainWindow::OnZoomOut(IInspectable const&, RoutedEventArgs const&) { SetZoom(doc().zoom - 4); }

    void MainWindow::OnToggleGrid(IInspectable const& sender, RoutedEventArgs const&)
    {
        if (auto item = sender.try_as<winrt::Microsoft::UI::Xaml::Controls::ToggleMenuFlyoutItem>())
        {
            m_showGrid = item.IsChecked();
        }
        Render();
    }

    void MainWindow::OnToggleGuides(IInspectable const& sender, RoutedEventArgs const&)
    {
        if (auto item = sender.try_as<winrt::Microsoft::UI::Xaml::Controls::ToggleMenuFlyoutItem>())
        {
            m_showGuides = item.IsChecked();
        }
        Render();
    }

    // ---- Layers -------------------------------------------------------------

    winrt::IconMaster::DrawingContext MainWindow::NewLayerContext()
    {
        const int32_t w = doc().context.PixelWidth();
        const int32_t h = doc().context.PixelHeight();
        return winrt::IconMaster::DrawingContext(w, h);
    }

    void MainWindow::SyncActiveContext()
    {
        if (doc().layers.empty()) { return; }
        doc().activeLayer = std::min(doc().activeLayer, doc().layers.size() - 1);
        doc().context = doc().layers[doc().activeLayer].context;
        // Tools draw with the context's colour; keep it in step with the picker.
        if (doc().context != nullptr)
        {
            doc().context.Color(ColorPickerControl().Color());
        }
    }

    winrt::Windows::UI::Color MainWindow::CompositePixel(int32_t x, int32_t y) const
    {
        winrt::Windows::UI::Color acc = kTransparent;
        for (auto const& layer : doc().layers)
        {
            if (!layer.visible || layer.context == nullptr) { continue; }
            acc = OverBlend(layer.context.GetPixel(x, y), layer.opacity / 100.0, acc);
        }
        return acc;
    }

    void MainWindow::FlattenActive()
    {
        const int32_t w = doc().context.PixelWidth();
        const int32_t h = doc().context.PixelHeight();
        m_flatW = w;
        m_flatH = h;
        m_flat.assign(static_cast<size_t>(w) * h, kTransparent);
        for (auto const& layer : doc().layers)
        {
            if (!layer.visible || layer.context == nullptr) { continue; }
            const double cov = layer.opacity / 100.0;
            for (int32_t y = 0; y < h; ++y)
            {
                for (int32_t x = 0; x < w; ++x)
                {
                    const auto c = layer.context.GetPixel(x, y);
                    if (c.A == 0) { continue; }
                    const size_t idx = static_cast<size_t>(y) * w + x;
                    m_flat[idx] = OverBlend(c, cov, m_flat[idx]);
                }
            }
        }
    }

    void MainWindow::RebuildLayersUI()
    {
        m_updatingLayers = true;

        auto list = LayerList();
        list.Children().Clear();

        // Top of the stack (last index) first, so the panel reads like other editors.
        for (size_t k = doc().layers.size(); k-- > 0; )
        {
            auto const& layer = doc().layers[k];
            const bool active = (k == doc().activeLayer);

            Grid row;
            row.ColumnSpacing(4);
            {
                winrt::Microsoft::UI::Xaml::Controls::ColumnDefinition c0, c1;
                c0.Width(winrt::Microsoft::UI::Xaml::GridLengthHelper::FromValueAndType(0, winrt::Microsoft::UI::Xaml::GridUnitType::Auto));
                c1.Width(winrt::Microsoft::UI::Xaml::GridLengthHelper::FromValueAndType(1, winrt::Microsoft::UI::Xaml::GridUnitType::Star));
                row.ColumnDefinitions().Append(c0);
                row.ColumnDefinitions().Append(c1);
            }

            winrt::Microsoft::UI::Xaml::Controls::Primitives::ToggleButton eye;
            eye.Tag(winrt::box_value(static_cast<int32_t>(k)));
            eye.IsChecked(layer.visible);
            eye.Padding(winrt::Microsoft::UI::Xaml::ThicknessHelper::FromLengths(6, 2, 6, 2));
            eye.MinWidth(0);
            {
                FontIcon icon;
                icon.FontFamily(winrt::Microsoft::UI::Xaml::Media::FontFamily{ L"Segoe MDL2 Assets" });
                icon.Glyph(L""); // eye
                icon.FontSize(14);
                icon.Opacity(layer.visible ? 1.0 : 0.35);
                eye.Content(icon);
            }
            eye.Click({ this, &MainWindow::OnLayerVisibilityToggled });
            ToolTipService::SetToolTip(eye, winrt::box_value(winrt::hstring{ L"Show / hide layer" }));
            Grid::SetColumn(eye, 0);

            Button select;
            select.Tag(winrt::box_value(static_cast<int32_t>(k)));
            select.Content(winrt::box_value(layer.name));
            select.HorizontalAlignment(winrt::Microsoft::UI::Xaml::HorizontalAlignment::Stretch);
            select.HorizontalContentAlignment(winrt::Microsoft::UI::Xaml::HorizontalAlignment::Left);
            select.Padding(winrt::Microsoft::UI::Xaml::ThicknessHelper::FromLengths(8, 4, 8, 4));
            if (active)
            {
                select.Background(winrt::Microsoft::UI::Xaml::Media::SolidColorBrush{
                    winrt::Windows::UI::Color{ 0x40, 0x00, 0x78, 0xD7 } });
            }
            else
            {
                select.Background(nullptr);
            }
            select.Click({ this, &MainWindow::OnLayerSelect });
            Grid::SetColumn(select, 1);

            row.Children().Append(eye);
            row.Children().Append(select);
            list.Children().Append(row);
        }

        if (!doc().layers.empty())
        {
            LayerOpacity().Value(doc().layers[doc().activeLayer].opacity);
        }

        m_updatingLayers = false;
    }

    void MainWindow::OnLayerAdd(IInspectable const&, RoutedEventArgs const&)
    {
        if (doc().context == nullptr) { return; }
        PushUndo();
        doc().layerCounter += 1;
        Layer layer;
        layer.context = NewLayerContext();
        layer.name = L"Layer " + winrt::to_hstring(doc().layerCounter);
        // Insert above the active layer (higher in the z-order) and select it.
        doc().layers.insert(doc().layers.begin() + doc().activeLayer + 1, std::move(layer));
        doc().activeLayer += 1;
        SyncActiveContext();
        RebuildLayersUI();
        Render();
    }

    void MainWindow::OnLayerDelete(IInspectable const&, RoutedEventArgs const&)
    {
        if (doc().layers.size() <= 1) { return; }
        PushUndo();
        doc().layers.erase(doc().layers.begin() + doc().activeLayer);
        if (doc().activeLayer >= doc().layers.size()) { doc().activeLayer = doc().layers.size() - 1; }
        SyncActiveContext();
        RebuildLayersUI();
        Render();
    }

    void MainWindow::OnLayerMoveUp(IInspectable const&, RoutedEventArgs const&)
    {
        if (doc().activeLayer + 1 >= doc().layers.size()) { return; }
        PushUndo();
        std::swap(doc().layers[doc().activeLayer], doc().layers[doc().activeLayer + 1]);
        doc().activeLayer += 1;
        SyncActiveContext();
        RebuildLayersUI();
        Render();
    }

    void MainWindow::OnLayerMoveDown(IInspectable const&, RoutedEventArgs const&)
    {
        if (doc().activeLayer == 0) { return; }
        PushUndo();
        std::swap(doc().layers[doc().activeLayer], doc().layers[doc().activeLayer - 1]);
        doc().activeLayer -= 1;
        SyncActiveContext();
        RebuildLayersUI();
        Render();
    }

    void MainWindow::OnLayerMergeDown(IInspectable const&, RoutedEventArgs const&)
    {
        if (doc().activeLayer == 0) { return; } // nothing below to merge into
        PushUndo();
        const int32_t w = doc().context.PixelWidth();
        const int32_t h = doc().context.PixelHeight();
        auto& upper = doc().layers[doc().activeLayer];
        auto& lower = doc().layers[doc().activeLayer - 1];
        const double cov = upper.opacity / 100.0;
        for (int32_t y = 0; y < h; ++y)
        {
            for (int32_t x = 0; x < w; ++x)
            {
                const auto u = upper.context.GetPixel(x, y);
                if (u.A == 0) { continue; }
                lower.context.SetPixel(x, y, OverBlend(u, cov, lower.context.GetPixel(x, y)));
            }
        }
        doc().layers.erase(doc().layers.begin() + doc().activeLayer);
        doc().activeLayer -= 1;
        SyncActiveContext();
        RebuildLayersUI();
        Render();
    }

    void MainWindow::OnLayerOpacityChanged(IInspectable const&, winrt::Microsoft::UI::Xaml::Controls::Primitives::RangeBaseValueChangedEventArgs const& args)
    {
        if (m_updatingLayers || doc().layers.empty()) { return; }
        doc().layers[doc().activeLayer].opacity = std::clamp(static_cast<int32_t>(args.NewValue()), 0, 100);
        Render();
    }

    void MainWindow::OnLayerVisibilityToggled(IInspectable const& sender, RoutedEventArgs const&)
    {
        if (m_updatingLayers) { return; }
        auto btn = sender.try_as<winrt::Microsoft::UI::Xaml::Controls::Primitives::ToggleButton>();
        if (!btn) { return; }
        const int32_t k = winrt::unbox_value_or<int32_t>(btn.Tag(), -1);
        if (k < 0 || static_cast<size_t>(k) >= doc().layers.size()) { return; }
        const auto state = btn.IsChecked();
        doc().layers[static_cast<size_t>(k)].visible = (state != nullptr && state.Value());
        if (auto icon = btn.Content().try_as<FontIcon>())
        {
            icon.Opacity(doc().layers[static_cast<size_t>(k)].visible ? 1.0 : 0.35);
        }
        Render();
    }

    void MainWindow::OnLayerSelect(IInspectable const& sender, RoutedEventArgs const&)
    {
        auto btn = sender.try_as<Button>();
        if (!btn) { return; }
        const int32_t k = winrt::unbox_value_or<int32_t>(btn.Tag(), -1);
        if (k < 0 || static_cast<size_t>(k) >= doc().layers.size()) { return; }
        doc().activeLayer = static_cast<size_t>(k);
        SyncActiveContext();
        RebuildLayersUI();
    }

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

        m_hoverValid = false; // the hover preview gives way to the actual stroke

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

        if (m_toolKind == ToolKind::Wand)
        {
            if (!left)
            {
                return;
            }
            int32_t px, py;
            PointerToPixelClamped(e, px, py);
            MagicWandSelect(px, py);
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
        if (m_toolKind == ToolKind::Pen || m_toolKind == ToolKind::Eraser)
        {
            const bool erase = (m_toolKind == ToolKind::Eraser) || (right && !left);
            BeginStroke(erase);
            CanvasImage().CapturePointer(e.Pointer());
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

        // Not dragging: update the live brush-footprint preview (Pen/Eraser only),
        // then draw if a button is held. DrawFromPointer renders on a real stroke;
        // otherwise we render here to reflect the moving preview.
        if (doc().context != nullptr)
        {
            auto point = e.GetCurrentPoint(CanvasImage());
            auto props = point.Properties();
            const bool pressed = props.IsLeftButtonPressed() || props.IsRightButtonPressed();

            auto pos = point.Position();
            const int32_t lx = static_cast<int32_t>(pos.X) / doc().zoom;
            const int32_t ly = static_cast<int32_t>(pos.Y) / doc().zoom;
            const bool inBounds = pos.X >= 0 && pos.Y >= 0 &&
                                  lx >= 0 && lx < doc().context.PixelWidth() &&
                                  ly >= 0 && ly < doc().context.PixelHeight();
            // Pen/Eraser and the shape tools all stamp the brush footprint, so
            // preview it on hover (for shapes this shows where the first point lands).
            const bool footprintTool = (m_toolKind == ToolKind::Pen ||
                                        m_toolKind == ToolKind::Eraser ||
                                        IsShapeTool(m_toolKind));

            m_hoverX = lx;
            m_hoverY = ly;
            m_hoverValid = inBounds && footprintTool && !pressed;

            DrawFromPointer(e);
            if (!pressed)
            {
                RenderHover();
            }
            return;
        }

        DrawFromPointer(e);
    }

    void MainWindow::OnCanvasPointerExited(IInspectable const&, PointerRoutedEventArgs const&)
    {
        if (m_hoverValid)
        {
            m_hoverValid = false;
            RenderHover();
        }
    }

    void MainWindow::OnCanvasPointerReleased(IInspectable const&, PointerRoutedEventArgs const& e)
    {
        if (m_strokeActive)
        {
            m_strokeActive = false;
            m_strokeBase.clear();
            m_strokeBase.shrink_to_fit();
            m_strokeCoverage.clear();
            m_strokeCoverage.shrink_to_fit();
            CanvasImage().ReleasePointerCapture(e.Pointer());
            return;
        }

        if (m_moving)
        {
            int32_t px, py;
            PointerToPixelClamped(e, px, py);
            m_moveDX = px - m_moveAnchorX;
            m_moveDY = py - m_moveAnchorY;
            StampFloating(doc().selX + m_moveDX, doc().selY + m_moveDY);
            // Shift the mask so the selection follows the moved pixels.
            {
                const int32_t w = doc().context.PixelWidth();
                const int32_t h = doc().context.PixelHeight();
                std::vector<uint8_t> shifted(static_cast<size_t>(w) * h, 0);
                for (int32_t y = 0; y < h; ++y)
                {
                    for (int32_t x = 0; x < w; ++x)
                    {
                        if (!Selected(x, y)) { continue; }
                        const int32_t nx = x + m_moveDX;
                        const int32_t ny = y + m_moveDY;
                        if (nx >= 0 && nx < w && ny >= 0 && ny < h)
                        {
                            shifted[static_cast<size_t>(ny) * w + nx] = 1;
                        }
                    }
                }
                doc().selMask = std::move(shifted);
            }
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
        else if (kind == ToolKind::Pen || kind == ToolKind::Eraser)
        {
            // The stamp tools honour the brush size and hardness (soft edges);
            // flood-fill and eyedropper do not.
            StampStroke(lx, ly);
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

    // Coverage (0..1) of footprint pixel (i,j) in an s x s brush at the current
    // hardness. 100% hardness => solid square (identical to the old behaviour);
    // lower hardness feathers the edges toward the centre.
    double MainWindow::FootprintCoverage(int32_t i, int32_t j, int32_t s) const
    {
        if (s <= 1) { return 1.0; }
        const double hf = std::clamp(m_hardness / 100.0, 0.0, 1.0);
        if (hf >= 1.0) { return 1.0; }
        const double c = (s - 1) / 2.0;   // footprint centre
        const double r = s / 2.0;         // half-extent
        const double m = std::max(std::abs((i - c) / r), std::abs((j - c) / r));
        if (m <= hf)  { return 1.0; }
        if (m >= 1.0) { return 0.0; }
        return (1.0 - m) / (1.0 - hf);
    }

    void MainWindow::BeginStroke(bool erase)
    {
        const int32_t w = doc().context.PixelWidth();
        const int32_t h = doc().context.PixelHeight();
        m_strokeActive = true;
        m_strokeErase = erase;
        m_strokeColor = doc().context.Color();
        m_strokeBase.assign(static_cast<size_t>(w) * h, kTransparent);
        m_strokeCoverage.assign(static_cast<size_t>(w) * h, 0.0f);
        for (int32_t y = 0; y < h; ++y)
        {
            for (int32_t x = 0; x < w; ++x)
            {
                m_strokeBase[static_cast<size_t>(y) * w + x] = doc().context.GetPixel(x, y);
            }
        }
    }

    // Stamps the brush footprint at (cx,cy), keeping the maximum coverage per pixel
    // over the whole stroke and re-compositing from the pre-stroke pixels, so soft
    // edges do not accumulate where stamps overlap along a drag.
    void MainWindow::StampStroke(int32_t cx, int32_t cy)
    {
        if (!m_strokeActive)
        {
            return;
        }
        const int32_t s = std::clamp(m_brushSize, 1, 64);
        const int32_t start = -(s / 2);
        const int32_t w = doc().context.PixelWidth();
        const int32_t h = doc().context.PixelHeight();
        for (int32_t j = 0; j < s; ++j)
        {
            for (int32_t i = 0; i < s; ++i)
            {
                const int32_t px = cx + start + i;
                const int32_t py = cy + start + j;
                if (px < 0 || px >= w || py < 0 || py >= h)
                {
                    continue;
                }
                const double cov = FootprintCoverage(i, j, s);
                if (cov <= 0.0)
                {
                    continue;
                }
                const size_t idx = static_cast<size_t>(py) * w + px;
                if (static_cast<double>(m_strokeCoverage[idx]) >= cov)
                {
                    continue;
                }
                m_strokeCoverage[idx] = static_cast<float>(cov);
                const winrt::Windows::UI::Color base = m_strokeBase[idx];
                const winrt::Windows::UI::Color out = m_strokeErase
                    ? EraseBlend(base, cov)
                    : OverBlend(m_strokeColor, cov, base);
                doc().context.SetPixel(px, py, out);
            }
        }
    }

    // Fills cov with the maximum brush-footprint coverage along the shape between
    // the two points, honouring the current thickness and hardness.
    void MainWindow::AccumulateStrokeCoverage(std::vector<float>& cov, int32_t x0, int32_t y0, int32_t x1, int32_t y1)
    {
        if (m_currentShape == nullptr)
        {
            return;
        }
        const int32_t w = doc().context.PixelWidth();
        const int32_t h = doc().context.PixelHeight();
        const int32_t s = std::clamp(m_brushSize, 1, 64);
        const int32_t start = -(s / 2);
        auto points = m_currentShape.Rasterize(x0, y0, x1, y1, m_shapeFilled);
        for (auto const& p : points)
        {
            for (int32_t j = 0; j < s; ++j)
            {
                for (int32_t i = 0; i < s; ++i)
                {
                    const int32_t px = p.X + start + i;
                    const int32_t py = p.Y + start + j;
                    if (px < 0 || px >= w || py < 0 || py >= h)
                    {
                        continue;
                    }
                    const double c = FootprintCoverage(i, j, s);
                    const size_t idx = static_cast<size_t>(py) * w + px;
                    if (c > cov[idx])
                    {
                        cov[idx] = static_cast<float>(c);
                    }
                }
            }
        }
    }

    void winrt::IconMaster::implementation::MainWindow::OnBrushSizeChanged(IInspectable const& sender, [[maybe_unused]] winrt::Microsoft::UI::Xaml::Controls::Primitives::RangeBaseValueChangedEventArgs const& args)
    {
        auto slider = sender.try_as<Slider>();
        if (slider == nullptr)
        {
            return;
        }
        m_brushSize = (int32_t) slider.Value();
    }

    void MainWindow::OnHardnessChanged(IInspectable const& sender, winrt::Microsoft::UI::Xaml::Controls::Primitives::RangeBaseValueChangedEventArgs const&)
    {
        auto slider = sender.try_as<Slider>();
        if (slider == nullptr)
        {
            return;
        }
        m_hardness = std::clamp((int32_t) slider.Value(), 0, 100);
    }

    void MainWindow::OnShapeFillModeChanged(IInspectable const& sender, RoutedEventArgs const&)
    {
        auto radio = sender.try_as<winrt::Microsoft::UI::Xaml::Controls::RadioButton>();
        if (radio == nullptr)
        {
            return;
        }
        m_shapeFilled = winrt::unbox_value_or<winrt::hstring>(radio.Tag(), L"outline") == L"filled";
    }

    void MainWindow::CommitShape(int32_t x1, int32_t y1)
    {
        if (m_currentShape == nullptr)
        {
            return;
        }

        const winrt::Windows::UI::Color color = doc().context.Color();
        const int32_t w = doc().context.PixelWidth();
        const int32_t h = doc().context.PixelHeight();

        // Composite the thick, hardness-aware stroke once (max coverage per pixel).
        std::vector<float> cov(static_cast<size_t>(w) * h, 0.0f);
        AccumulateStrokeCoverage(cov, m_shapeStartX, m_shapeStartY, x1, y1);
        for (int32_t y = 0; y < h; ++y)
        {
            for (int32_t x = 0; x < w; ++x)
            {
                const double c = cov[static_cast<size_t>(y) * w + x];
                if (c <= 0.0)
                {
                    continue;
                }
                doc().context.SetPixel(x, y, OverBlend(color, c, doc().context.GetPixel(x, y)));
            }
        }
        Render();
    }

    // ---- Selection / clipboard ---------------------------------------------

    bool MainWindow::Selected(int32_t x, int32_t y) const
    {
        if (!doc().hasSelection)
        {
            return false;
        }
        const int32_t w = doc().context.PixelWidth();
        const int32_t h = doc().context.PixelHeight();
        if (x < 0 || x >= w || y < 0 || y >= h)
        {
            return false;
        }
        const size_t idx = static_cast<size_t>(y) * w + x;
        return idx < doc().selMask.size() && doc().selMask[idx] != 0;
    }

    bool MainWindow::InsideSelection(int32_t px, int32_t py) const
    {
        return Selected(px, py);
    }

    void MainWindow::FillSelectionRect()
    {
        const int32_t w = doc().context.PixelWidth();
        const int32_t h = doc().context.PixelHeight();
        doc().selMask.assign(static_cast<size_t>(w) * h, 0);
        for (int32_t y = doc().selY; y < doc().selY + doc().selH; ++y)
        {
            for (int32_t x = doc().selX; x < doc().selX + doc().selW; ++x)
            {
                if (x >= 0 && x < w && y >= 0 && y < h)
                {
                    doc().selMask[static_cast<size_t>(y) * w + x] = 1;
                }
            }
        }
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
        FillSelectionRect();
    }

    // Flood the contiguous region of pixels matching the clicked colour into the
    // selection mask; selX/Y/W/H becomes the region's bounding box.
    void MainWindow::MagicWandSelect(int32_t sx, int32_t sy)
    {
        const int32_t w = doc().context.PixelWidth();
        const int32_t h = doc().context.PixelHeight();
        if (sx < 0 || sx >= w || sy < 0 || sy >= h)
        {
            return;
        }

        const winrt::Windows::UI::Color target = doc().context.GetPixel(sx, sy);
        // Tolerance rides on the Hardness slider: hard (100) = exact match, and
        // lowering it widens the allowed per-channel colour distance (0..255).
        const int32_t tol = ((100 - std::clamp(m_hardness, 0, 100)) * 255) / 100;
        auto same = [tol](winrt::Windows::UI::Color const& a, winrt::Windows::UI::Color const& b)
        {
            auto ad = [](uint8_t p, uint8_t q) { return p > q ? p - q : q - p; };
            const int32_t d = std::max(std::max(ad(a.A, b.A), ad(a.R, b.R)),
                                       std::max(ad(a.G, b.G), ad(a.B, b.B)));
            return d <= tol;
        };

        std::vector<uint8_t> mask(static_cast<size_t>(w) * h, 0);
        std::vector<int32_t> stack;                 // pixel indices (y*w + x)
        stack.push_back(sy * w + sx);
        mask[static_cast<size_t>(sy) * w + sx] = 1;

        int32_t minx = sx, miny = sy, maxx = sx, maxy = sy;
        while (!stack.empty())
        {
            const int32_t idx = stack.back();
            stack.pop_back();
            const int32_t cx = idx % w;
            const int32_t cy = idx / w;
            const int32_t nb[4][2] = { { cx + 1, cy }, { cx - 1, cy }, { cx, cy + 1 }, { cx, cy - 1 } };
            for (auto const& n : nb)
            {
                const int32_t nx = n[0];
                const int32_t ny = n[1];
                if (nx < 0 || nx >= w || ny < 0 || ny >= h)
                {
                    continue;
                }
                const size_t nidx = static_cast<size_t>(ny) * w + nx;
                if (mask[nidx] || !same(doc().context.GetPixel(nx, ny), target))
                {
                    continue;
                }
                mask[nidx] = 1;
                stack.push_back(ny * w + nx);
                minx = std::min(minx, nx);
                miny = std::min(miny, ny);
                maxx = std::max(maxx, nx);
                maxy = std::max(maxy, ny);
            }
        }

        doc().selMask = std::move(mask);
        doc().selX = minx;
        doc().selY = miny;
        doc().selW = maxx - minx + 1;
        doc().selH = maxy - miny + 1;
        doc().hasSelection = true;
    }

    void MainWindow::ClearSelectedPixels()
    {
        const int32_t w = doc().context.PixelWidth();
        const int32_t h = doc().context.PixelHeight();
        for (int32_t y = 0; y < h; ++y)
        {
            for (int32_t x = 0; x < w; ++x)
            {
                if (Selected(x, y))
                {
                    doc().context.SetPixel(x, y, kTransparent);
                }
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
                const int32_t gx = doc().selX + i;
                const int32_t gy = doc().selY + j;
                if (Selected(gx, gy))
                {
                    m_floatPixels[static_cast<size_t>(j) * m_floatW + i] = doc().context.GetPixel(gx, gy);
                    doc().context.SetPixel(gx, gy, kTransparent);
                }
            }
        }
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
        FillSelectionRect();
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
                const int32_t gx = doc().selX + i;
                const int32_t gy = doc().selY + j;
                if (Selected(gx, gy))
                {
                    m_clipPixels[static_cast<size_t>(j) * m_clipW + i] = doc().context.GetPixel(gx, gy);
                }
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
        ClearSelectedPixels();
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
        FillSelectionRect();
        Render();
    }

    void MainWindow::OnDelete(IInspectable const&, RoutedEventArgs const&)
    {
        if (!doc().hasSelection)
        {
            return;
        }
        PushUndo();
        ClearSelectedPixels();
        Render();
    }

    // ---- File ---------------------------------------------------------------

    void MainWindow::ResetTransient()
    {
        m_selecting = false;
        m_moving = false;
        m_shapeActive = false;
        m_hoverValid = false;
        m_floatPixels.clear();
    }

    void MainWindow::AddDocument(winrt::IconMaster::DrawingContext const& context, winrt::hstring const& title, int32_t zoom)
    {
        m_updatingTabs = true;

        Document d;
        d.layers.push_back(Layer{ context, winrt::hstring{ L"Layer 1" }, true, 100 });
        d.activeLayer = 0;
        d.layerCounter = 1;
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
        RebuildLayersUI();
    }

    int32_t MainWindow::FitZoom(int32_t maxDim)
    {
        if (maxDim <= 0)
        {
            return k_minZoom;
        }
        return std::clamp(512 / maxDim, k_minZoom, k_maxZoom);
    }

    void MainWindow::NewDocument(int32_t w, int32_t h)
    {
        w = std::clamp(w, 1, 1024);
        h = std::clamp(h, 1, 1024);
        auto context = winrt::IconMaster::DrawingContext(w, h);
        context.Color(ColorPickerControl().Color());
        m_docCounter += 1;
        AddDocument(context, L"Icon " + winrt::to_hstring(m_docCounter), FitZoom(std::max(w, h)));
        StatusText().Text(L"New " + winrt::to_hstring(w) + L" x " + winrt::to_hstring(h) + L" icon.");
    }

    void MainWindow::OnNewSizePreset(IInspectable const& sender, RoutedEventArgs const&)
    {
        const auto tag = winrt::unbox_value_or<winrt::hstring>(sender.as<FrameworkElement>().Tag(), L"");
        const int32_t val = static_cast<int32_t>(std::wcstol(tag.c_str(), nullptr, 10));
        if (val <= 0) { return; }
        NewDims().SetSize(val, val);
    }

    winrt::fire_and_forget MainWindow::OnNew(IInspectable const&, RoutedEventArgs const&)
    {
        auto lifetime = get_strong();

        int32_t w = m_newW;
        int32_t h = m_newH;

        if (m_askOnNew)
        {
            // Seed the shared size control and select the matching preset (or Other).
            NewDims().SetSize(m_newW, m_newH);
            NewDontAsk().IsChecked(false);

            winrt::Microsoft::UI::Xaml::Controls::RadioButton preset{ nullptr };
            if (m_newW == m_newH)
            {
                switch (m_newW)
                {
                case 16:   preset = Size16();   break;
                case 24:   preset = Size24();   break;
                case 32:   preset = Size32();   break;
                case 48:   preset = Size48();   break;
                case 256:  preset = Size256();  break;
                case 512:  preset = Size512();  break;
                case 1024: preset = Size1024(); break;
                default:   break;
                }
            }
            if (preset != nullptr) { preset.IsChecked(true); }
            else                   { SizeOther().IsChecked(true); }

            if (NewIconDialog().XamlRoot() == nullptr)
            {
                NewIconDialog().XamlRoot(this->Content().XamlRoot());
            }
            if (co_await NewIconDialog().ShowAsync() != ContentDialogResult::Primary)
            {
                co_return;
            }

            w = NewDims().SelectedWidth();
            h = NewDims().SelectedHeight();
            m_newW = w;
            m_newH = h;

            auto ask = NewDontAsk().IsChecked();
            if (ask && ask.Value())
            {
                m_askOnNew = false;
            }
        }

        NewDocument(w, h);
    }

    void MainWindow::ResizeCanvas(int32_t newW, int32_t newH)
    {
        if (doc().context == nullptr || newW <= 0 || newH <= 0)
        {
            return;
        }
        const int32_t oldW = doc().context.PixelWidth();
        const int32_t oldH = doc().context.PixelHeight();
        if (newW == oldW && newH == oldH)
        {
            return;
        }

        // Snapshot the old canvas so the resize is undoable (dimensions included).
        PushUndo();

        // Copy the overlapping region of every layer, anchored at the top-left.
        const int32_t copyW = std::min(oldW, newW);
        const int32_t copyH = std::min(oldH, newH);
        for (auto& layer : doc().layers)
        {
            auto resized = winrt::IconMaster::DrawingContext(newW, newH);
            resized.Color(layer.context.Color());
            for (int32_t y = 0; y < copyH; ++y)
            {
                for (int32_t x = 0; x < copyW; ++x)
                {
                    resized.SetPixel(x, y, layer.context.GetPixel(x, y));
                }
            }
            layer.context = resized;
        }

        SyncActiveContext();
        doc().hasSelection = false;
        ResetTransient();
        doc().zoom = FitZoom(std::max(newW, newH));
        RebuildDisplay();
    }

    void MainWindow::FlipHorizontal()
    {
        if (doc().context == nullptr)
        {
            return;
        }
        PushUndo();
        const int32_t w = doc().context.PixelWidth();
        const int32_t h = doc().context.PixelHeight();
        for (auto& layer : doc().layers)
        {
            for (int32_t y = 0; y < h; ++y)
            {
                for (int32_t x = 0; x < w / 2; ++x)
                {
                    const auto a = layer.context.GetPixel(x, y);
                    const auto b = layer.context.GetPixel(w - 1 - x, y);
                    layer.context.SetPixel(x, y, b);
                    layer.context.SetPixel(w - 1 - x, y, a);
                }
            }
        }
        doc().hasSelection = false;
        ResetTransient();
        Render();
    }

    void MainWindow::FlipVertical()
    {
        if (doc().context == nullptr)
        {
            return;
        }
        PushUndo();
        const int32_t w = doc().context.PixelWidth();
        const int32_t h = doc().context.PixelHeight();
        for (auto& layer : doc().layers)
        {
            for (int32_t y = 0; y < h / 2; ++y)
            {
                for (int32_t x = 0; x < w; ++x)
                {
                    const auto a = layer.context.GetPixel(x, y);
                    const auto b = layer.context.GetPixel(x, h - 1 - y);
                    layer.context.SetPixel(x, y, b);
                    layer.context.SetPixel(x, h - 1 - y, a);
                }
            }
        }
        doc().hasSelection = false;
        ResetTransient();
        Render();
    }

    void MainWindow::Rotate90(bool clockwise)
    {
        if (doc().context == nullptr)
        {
            return;
        }
        PushUndo();
        const int32_t w = doc().context.PixelWidth();
        const int32_t h = doc().context.PixelHeight();

        // A quarter turn swaps the dimensions (w x h -> h x w), for every layer.
        for (auto& layer : doc().layers)
        {
            auto rotated = winrt::IconMaster::DrawingContext(h, w);
            rotated.Color(layer.context.Color());
            for (int32_t y = 0; y < h; ++y)
            {
                for (int32_t x = 0; x < w; ++x)
                {
                    const auto c = layer.context.GetPixel(x, y);
                    if (clockwise)
                    {
                        rotated.SetPixel(h - 1 - y, x, c);
                    }
                    else
                    {
                        rotated.SetPixel(y, w - 1 - x, c);
                    }
                }
            }
            layer.context = rotated;
        }

        SyncActiveContext();
        doc().hasSelection = false;
        ResetTransient();
        RebuildDisplay(); // keep the current zoom
    }

    void MainWindow::RotateArbitrary(double degrees, bool keepSize, double px, double py)
    {
        if (doc().context == nullptr)
        {
            return;
        }
        const double norm = std::fmod(degrees, 360.0);
        if (norm == 0.0)
        {
            return;
        }
        PushUndo();

        const int32_t w = doc().context.PixelWidth();
        const int32_t h = doc().context.PixelHeight();
        const double a = norm * 3.14159265358979323846 / 180.0;
        const double ca = std::cos(a);
        const double sa = std::sin(a);

        // Where the source pivot lands in the destination, and the new size.
        int32_t nw, nh;
        double dpx, dpy;
        if (keepSize)
        {
            // Same canvas; the pivot stays put and anything rotated off-canvas clips.
            nw = w;
            nh = h;
            dpx = px;
            dpy = py;
        }
        else
        {
            // Grow to the bounding box of the forward-rotated corners (positive = CW).
            const double corners[4][2] = { {0.0, 0.0}, {static_cast<double>(w), 0.0},
                                           {0.0, static_cast<double>(h)}, {static_cast<double>(w), static_cast<double>(h)} };
            double minX = 0.0, minY = 0.0, maxX = 0.0, maxY = 0.0;
            for (int32_t k = 0; k < 4; ++k)
            {
                const double rx = corners[k][0] - px;
                const double ry = corners[k][1] - py;
                const double fx = ca * rx - sa * ry;
                const double fy = sa * rx + ca * ry;
                if (k == 0 || fx < minX) { minX = fx; }
                if (k == 0 || fx > maxX) { maxX = fx; }
                if (k == 0 || fy < minY) { minY = fy; }
                if (k == 0 || fy > maxY) { maxY = fy; }
            }
            nw = std::max(1, static_cast<int32_t>(std::ceil(maxX - minX)));
            nh = std::max(1, static_cast<int32_t>(std::ceil(maxY - minY)));
            dpx = -minX;
            dpy = -minY;
        }
        nw = std::clamp(nw, 1, 1024);
        nh = std::clamp(nh, 1, 1024);

        for (auto& layer : doc().layers)
        {
            auto rotated = winrt::IconMaster::DrawingContext(nw, nh);
            rotated.Color(layer.context.Color());
            for (int32_t dy = 0; dy < nh; ++dy)
            {
                for (int32_t dx = 0; dx < nw; ++dx)
                {
                    // Inverse-rotate the destination pixel centre about the pivot and
                    // sample the nearest source pixel.
                    const double rx = (dx + 0.5) - dpx;
                    const double ry = (dy + 0.5) - dpy;
                    const double srcX = px + (ca * rx + sa * ry);
                    const double srcY = py + (-sa * rx + ca * ry);
                    const int32_t sx = static_cast<int32_t>(std::floor(srcX));
                    const int32_t sy = static_cast<int32_t>(std::floor(srcY));
                    if (sx >= 0 && sx < w && sy >= 0 && sy < h)
                    {
                        rotated.SetPixel(dx, dy, layer.context.GetPixel(sx, sy));
                    }
                }
            }
            layer.context = rotated;
        }

        SyncActiveContext();
        doc().hasSelection = false;
        ResetTransient();
        RebuildDisplay(); // keep the current zoom
    }

    void MainWindow::OnFlipHorizontal(IInspectable const&, RoutedEventArgs const&)
    {
        FlipHorizontal();
        if (auto s = StatusText()) { s.Text(L"Flipped horizontally."); }
    }

    void MainWindow::OnFlipVertical(IInspectable const&, RoutedEventArgs const&)
    {
        FlipVertical();
        if (auto s = StatusText()) { s.Text(L"Flipped vertically."); }
    }

    void MainWindow::OnRotateCW(IInspectable const&, RoutedEventArgs const&)
    {
        Rotate90(true);
        if (auto s = StatusText()) { s.Text(L"Rotated a quarter turn right."); }
    }

    void MainWindow::OnRotateCCW(IInspectable const&, RoutedEventArgs const&)
    {
        Rotate90(false);
        if (auto s = StatusText()) { s.Text(L"Rotated a quarter turn left."); }
    }

    winrt::fire_and_forget MainWindow::OnRotateArbitrary(IInspectable const&, RoutedEventArgs const&)
    {
        auto lifetime = get_strong();
        if (doc().context == nullptr)
        {
            co_return;
        }

        // Seed the pivot to the canvas centre each time the dialog opens. The pivot
        // may sit outside the image (any point is a valid centre of rotation), so the
        // range is left symmetric rather than clamped to the image bounds.
        const int32_t w = doc().context.PixelWidth();
        const int32_t h = doc().context.PixelHeight();
        RotatePivotX().Value(w / 2.0);
        RotatePivotY().Value(h / 2.0);

        if (RotateDialog().XamlRoot() == nullptr)
        {
            RotateDialog().XamlRoot(this->Content().XamlRoot());
        }
        if (co_await RotateDialog().ShowAsync() != ContentDialogResult::Primary)
        {
            co_return;
        }

        const double deg = RotateAngle().Value();
        if (std::isnan(deg))
        {
            co_return;
        }
        auto kc = RotateKeep().IsChecked();
        const bool keepSize = kc && kc.Value();
        double px = RotatePivotX().Value();
        double py = RotatePivotY().Value();
        if (std::isnan(px)) { px = w / 2.0; }
        if (std::isnan(py)) { py = h / 2.0; }

        RotateArbitrary(deg, keepSize, px, py);
        if (auto s = StatusText())
        {
            s.Text(L"Rotated " + winrt::to_hstring(static_cast<int32_t>(std::lround(deg))) + L" degrees.");
        }
    }

    winrt::fire_and_forget MainWindow::OnSaveAs(IInspectable const&, RoutedEventArgs const&)
    {
        namespace WGI = winrt::Windows::Graphics::Imaging;

        auto lifetime = get_strong();
        if (doc().context == nullptr)
        {
            co_return;
        }

        // Ask the user which image format to save. There are more raster formats
        // than PNG (BMP, JPEG, GIF, TIFF), plus the multi-size Windows ICO.
        if (SaveDialog().XamlRoot() == nullptr)
        {
            SaveDialog().XamlRoot(this->Content().XamlRoot());
        }
        if (co_await SaveDialog().ShowAsync() != ContentDialogResult::Primary)
        {
            co_return;
        }

        winrt::hstring typeName;
        winrt::hstring ext;
        winrt::guid encoderId{};
        bool isIco = false;
        switch (SaveFormatCombo().SelectedIndex())
        {
        case 1:  typeName = L"BMP image";    ext = L".bmp";  encoderId = WGI::BitmapEncoder::BmpEncoderId();  break;
        case 2:  typeName = L"JPEG image";   ext = L".jpg";  encoderId = WGI::BitmapEncoder::JpegEncoderId(); break;
        case 3:  typeName = L"GIF image";    ext = L".gif";  encoderId = WGI::BitmapEncoder::GifEncoderId();  break;
        case 4:  typeName = L"TIFF image";   ext = L".tiff"; encoderId = WGI::BitmapEncoder::TiffEncoderId(); break;
        case 5:  typeName = L"Windows icon"; ext = L".ico";  isIco = true;                                    break;
        default: typeName = L"PNG image";    ext = L".png";  encoderId = WGI::BitmapEncoder::PngEncoderId();  break;
        }

        auto file = co_await PickSaveFileAsync(typeName, ext);
        if (file == nullptr)
        {
            co_return;
        }

        if (isIco)
        {
            co_await WriteIcoAsync(file);
        }
        else
        {
            co_await WriteSingleLayerImageAsync(file, encoderId);
        }

        winrt::hstring filePath = file.Path();
        doc().associatedFile.path = filePath;
        doc().associatedFile.typeName = filePath;
        doc().associatedFile.extension = ext;
        doc().associatedFile.encoder = encoderId;
        doc().associatedFile.isIco = isIco;
        Tabs().TabItems().GetAt(static_cast<uint32_t>(m_active)).as<TabViewItem>().Header(winrt::box_value(file.Name()));
        StatusText().Text(L"Saved " + filePath);
        AddToRecent(file);
    }

    winrt::fire_and_forget MainWindow::OnSaveCopy(IInspectable const& sender, RoutedEventArgs const& args)
    {
        AssociatedFile assoc = doc().associatedFile;
        if (assoc.path.empty()) {
            OnSaveAs(sender, args);
        }
        else {
            auto file = co_await PickSaveFileAsync(assoc.typeName, assoc.extension);
            if (file == nullptr)
            {
                co_return;
            }

            if (assoc.isIco)
            {
                co_await WriteIcoAsync(file);
            }
            else
            {
                co_await WriteSingleLayerImageAsync(file, assoc.encoder);
            }
            StatusText().Text(L"Saved copy " + file.Path());
        }
    }

    winrt::fire_and_forget MainWindow::OnResizeImage(winrt::Windows::Foundation::IInspectable const&, winrt::Microsoft::UI::Xaml::RoutedEventArgs const&)
    {
        auto lifetime = get_strong();
        if (doc().context == nullptr)
        {
            co_return;
        }

        ResizeDims().SetSize(doc().context.PixelWidth(), doc().context.PixelHeight());

        if (ResizeImageDialog().XamlRoot() == nullptr)
        {
            ResizeImageDialog().XamlRoot(this->Content().XamlRoot());
        }

        if (co_await ResizeImageDialog().ShowAsync() != ContentDialogResult::Primary)
        {
            co_return;
        }

        const int32_t newW = ResizeDims().SelectedWidth();
        const int32_t newH = ResizeDims().SelectedHeight();
        ResizeCanvas(newW, newH);
        auto statusBar = StatusText();
        if (statusBar != nullptr)
        {
            statusBar.Text(L"Resized to " + winrt::to_hstring(newW) + L" x " + winrt::to_hstring(newH) + L".");
        }
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

        co_await LoadImageFileAsync(file);
    }

    winrt::Windows::Foundation::IAsyncAction MainWindow::LoadImageFileAsync(winrt::Windows::Storage::StorageFile file)
    {
        auto lifetime = get_strong();

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
                const winrt::array_view<uint8_t>::size_type i = (static_cast<winrt::array_view<uint8_t>::size_type>(y) * w + x) * 4;
                const winrt::Windows::UI::Color c{ bytes[i + 3], bytes[i + 2], bytes[i + 1], bytes[i + 0] };
                context.SetPixel(static_cast<int32_t>(x), static_cast<int32_t>(y), c);
            }
        }

        const int32_t fit = static_cast<int32_t>(512u / std::max(w, h));
        AddDocument(context, file.Name(), fit);
        StatusText().Text(L"Opened " + file.Name());
        AddToRecent(file);
    }

    void MainWindow::AddToRecent(winrt::Windows::Storage::StorageFile const& file)
    {
        namespace AC = winrt::Windows::Storage::AccessCache;
        auto mru = AC::StorageApplicationPermissions::MostRecentlyUsedList();
        const winrt::hstring path = file.Path();

        // De-duplicate by path so re-opening a file just bumps its recency.
        if (!path.empty())
        {
            std::vector<winrt::hstring> dup;
            for (auto const& e : mru.Entries())
            {
                if (e.Metadata == path) { dup.push_back(e.Token); }
            }
            for (auto const& t : dup) { mru.Remove(t); }
        }
        mru.Add(file, path);
        RebuildRecentMenu();
        UpdateJumpListAsync();
    }

    void MainWindow::RebuildRecentMenu()
    {
        namespace AC = winrt::Windows::Storage::AccessCache;
        auto items = RecentMenu().Items();
        items.Clear();

        auto entries = AC::StorageApplicationPermissions::MostRecentlyUsedList().Entries();
        if (entries.Size() == 0)
        {
            MenuFlyoutItem empty;
            empty.Text(L"(No recent files)");
            empty.IsEnabled(false);
            items.Append(empty);
            return;
        }

        uint32_t shown = 0;
        for (auto const& e : entries)
        {
            if (shown++ >= 12) { break; }
            const winrt::hstring token = e.Token;
            MenuFlyoutItem item;
            item.Text(e.Metadata);
            item.Click([this, token](IInspectable const&, RoutedEventArgs const&) { OpenRecentByTokenAsync(token); });
            items.Append(item);
        }
    }

    winrt::fire_and_forget MainWindow::OpenRecentByTokenAsync(winrt::hstring token)
    {
        auto lifetime = get_strong();
        namespace AC = winrt::Windows::Storage::AccessCache;
        auto mru = AC::StorageApplicationPermissions::MostRecentlyUsedList();

        winrt::Windows::Storage::StorageFile file{ nullptr };
        try
        {
            file = co_await mru.GetFileAsync(token);
        }
        catch (winrt::hresult_error const&)
        {
            mru.Remove(token);
            RebuildRecentMenu();
            StatusText().Text(L"That file is no longer available.");
            co_return;
        }
        if (file == nullptr)
        {
            co_return;
        }
        co_await LoadImageFileAsync(file);
    }

    void MainWindow::OpenFromArgument(winrt::hstring const& argument)
    {
        // Jump-list entries pass "open:<mru-token>".
        std::wstring a{ argument };
        const std::wstring prefix = L"open:";
        if (a.rfind(prefix, 0) == 0)
        {
            OpenRecentByTokenAsync(winrt::hstring{ a.substr(prefix.size()) });
        }
    }

    winrt::fire_and_forget MainWindow::UpdateJumpListAsync()
    {
        namespace SS = winrt::Windows::UI::StartScreen;
        namespace AC = winrt::Windows::Storage::AccessCache;
        auto lifetime = get_strong();

        if (!SS::JumpList::IsSupported())
        {
            co_return;
        }
        auto jumpList = co_await SS::JumpList::LoadCurrentAsync();
        jumpList.SystemGroupKind(SS::JumpListSystemGroupKind::None);
        jumpList.Items().Clear();

        auto entries = AC::StorageApplicationPermissions::MostRecentlyUsedList().Entries();
        uint32_t shown = 0;
        for (auto const& e : entries)
        {
            if (shown++ >= 10) { break; }
            const winrt::hstring path = e.Metadata;
            std::wstring p{ path };
            const size_t slash = p.find_last_of(L"\\/");
            const winrt::hstring name = (slash == std::wstring::npos) ? path : winrt::hstring{ p.substr(slash + 1) };

            auto item = SS::JumpListItem::CreateWithArguments(winrt::hstring{ L"open:" } + e.Token, name.empty() ? path : name);
            item.Description(path);
            item.GroupName(L"Recent");
            jumpList.Items().Append(item);
        }
        co_await jumpList.SaveAsync();
    }

    winrt::fire_and_forget MainWindow::OnSave(IInspectable const& sender, RoutedEventArgs const& args)
    {
        winrt::hstring filePath = doc().associatedFile.path;
        if (filePath.empty()) {
            OnSaveAs(sender, args);
        }
        else {
            auto file = co_await winrt::Windows::Storage::StorageFile::GetFileFromPathAsync(filePath);
            if (doc().associatedFile.isIco)
            {
                co_await WriteIcoAsync(file);
            }
            else
            {
                co_await WriteSingleLayerImageAsync(file, doc().associatedFile.encoder);
            }
            StatusText().Text(L"Saved " + filePath);
        }
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
                const auto c = CompositePixel(sx, sy);
                const size_t i = (static_cast<size_t>(y) * target + x) * 4;
                out[i + 0] = c.B;
                out[i + 1] = c.G;
                out[i + 2] = c.R;
                out[i + 3] = c.A;
            }
        }
        return out;
    }

    winrt::Windows::Foundation::IAsyncOperation<winrt::Windows::Storage::StorageFile> winrt::IconMaster::implementation::MainWindow::PickSaveFileAsync(winrt::hstring const& typeName, winrt::hstring const& extension)
    {
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
        picker.FileTypeChoices().Insert(typeName, winrt::single_threaded_vector<winrt::hstring>({ extension }));

        return picker.PickSaveFileAsync();
    }

    winrt::Windows::Foundation::IAsyncAction winrt::IconMaster::implementation::MainWindow::WriteSingleLayerImageAsync(winrt::Windows::Storage::StorageFile file, winrt::guid encoderId)
    {
        namespace WGI = winrt::Windows::Graphics::Imaging;

        const int32_t w = doc().context.PixelWidth();
        const int32_t h = doc().context.PixelHeight();
        std::vector<uint8_t> bytes(static_cast<size_t>(w) * h * 4);
        for (int32_t y = 0; y < h; ++y)
        {
            for (int32_t x = 0; x < w; ++x)
            {
                const auto c = CompositePixel(x, y);
                const size_t i = (static_cast<size_t>(y) * w + x) * 4;
                bytes[i + 0] = c.B;
                bytes[i + 1] = c.G;
                bytes[i + 2] = c.R;
                bytes[i + 3] = c.A;
            }
        }

        auto stream = co_await file.OpenAsync(winrt::Windows::Storage::FileAccessMode::ReadWrite);
        stream.Size(0); // truncate any previous content when overwriting
        auto encoder = co_await WGI::BitmapEncoder::CreateAsync(encoderId, stream);
        encoder.SetPixelData(
            WGI::BitmapPixelFormat::Bgra8,
            WGI::BitmapAlphaMode::Straight,
            static_cast<uint32_t>(w), static_cast<uint32_t>(h),
            96.0, 96.0, bytes);
        co_await encoder.FlushAsync();
    }

    winrt::Windows::Foundation::IAsyncAction MainWindow::WriteIcoAsync(winrt::Windows::Storage::StorageFile file)
    {
        auto lifetime = get_strong();

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
    }

    // ---- History ------------------------------------------------------------

    MainWindow::Snapshot MainWindow::CaptureSnapshot()
    {
        const int32_t w = doc().context.PixelWidth();
        const int32_t h = doc().context.PixelHeight();
        Snapshot s;
        s.w = w;
        s.h = h;
        s.active = doc().activeLayer;
        s.layers.reserve(doc().layers.size());
        for (auto const& layer : doc().layers)
        {
            LayerSnapshot ls;
            ls.name = layer.name;
            ls.visible = layer.visible;
            ls.opacity = layer.opacity;
            ls.pixels.resize(static_cast<size_t>(w) * h);
            for (int32_t y = 0; y < h; ++y)
            {
                for (int32_t x = 0; x < w; ++x)
                {
                    ls.pixels[static_cast<size_t>(y) * w + x] = layer.context.GetPixel(x, y);
                }
            }
            s.layers.push_back(std::move(ls));
        }
        return s;
    }

    void MainWindow::RestoreSnapshot(Snapshot const& snap)
    {
        const winrt::Windows::UI::Color color =
            (doc().context != nullptr) ? doc().context.Color() : winrt::Windows::UI::Color{ 0xFF, 0x00, 0x00, 0x00 };
        if (snap.w != doc().context.PixelWidth() || snap.h != doc().context.PixelHeight())
        {
            doc().zoom = FitZoom(std::max(snap.w, snap.h));
        }

        // Rebuild the entire layer stack from the snapshot.
        doc().layers.clear();
        for (auto const& ls : snap.layers)
        {
            Layer layer;
            layer.context = winrt::IconMaster::DrawingContext(snap.w, snap.h);
            layer.context.Color(color);
            layer.name = ls.name;
            layer.visible = ls.visible;
            layer.opacity = ls.opacity;
            for (int32_t y = 0; y < snap.h; ++y)
            {
                for (int32_t x = 0; x < snap.w; ++x)
                {
                    layer.context.SetPixel(x, y, ls.pixels[static_cast<size_t>(y) * snap.w + x]);
                }
            }
            doc().layers.push_back(std::move(layer));
        }
        doc().activeLayer = doc().layers.empty() ? 0 : std::min(snap.active, doc().layers.size() - 1);
        SyncActiveContext();

        // The selection may reference pixels that no longer match; clear it.
        doc().hasSelection = false;
        m_selecting = false;
        m_moving = false;
        m_shapeActive = false;
        m_floatPixels.clear();
        RebuildLayersUI();
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
        SyncActiveContext();
        ResetTransient();
        RebuildDisplay();
        RebuildLayersUI();
    }

    void MainWindow::OnAddTab(winrt::Microsoft::UI::Xaml::Controls::TabView const&, IInspectable const&)
    {
        // The tab "+" button is a quick add: reuse the last chosen size without a prompt.
        NewDocument(m_newW, m_newH);
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
        if (m_showGrid && ((dx % doc().zoom == 0) || (dy % doc().zoom == 0)))
        {
            b = g = r = 0xA0; // grid line
        }
        else
        {
            // Map to a logical pixel, clamping the extra +1 border row/column that
            // exists only to close the grid (it falls outside the canvas). The pixel
            // comes from m_flat, the pre-composited stack of visible layers.
            const int32_t lx = std::min(dx / doc().zoom, m_flatW - 1);
            const int32_t ly = std::min(dy / doc().zoom, m_flatH - 1);
            const winrt::Windows::UI::Color c = (m_flatW > 0 && m_flatH > 0)
                ? m_flat[static_cast<size_t>(ly) * m_flatW + lx]
                : kTransparent;
            // Composite the (possibly semi-transparent) pixel over the checkerboard
            // so partial alpha - e.g. soft brush edges - is actually visible.
            const bool light = ((((dx / k_checkerCell) + (dy / k_checkerCell)) & 1) == 0);
            const double bg = light ? 255.0 : 200.0; // 0xFF / 0xC8
            const double a = c.A / 255.0;
            b = static_cast<uint8_t>(std::lround(c.B * a + bg * (1.0 - a)));
            g = static_cast<uint8_t>(std::lround(c.G * a + bg * (1.0 - a)));
            r = static_cast<uint8_t>(std::lround(c.R * a + bg * (1.0 - a)));
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

        const double a = color.A / 255.0;
        for (int32_t dy = y0 + 1; dy < y0 + doc().zoom && dy < displayHeight; ++dy)
        {
            for (int32_t dx = x0 + 1; dx < x0 + doc().zoom && dx < displayWidth; ++dx)
            {
                const size_t i = (static_cast<size_t>(dy) * displayWidth + dx) * 4;
                data[i + 0] = static_cast<uint8_t>(std::lround(color.B * a + data[i + 0] * (1.0 - a)));
                data[i + 1] = static_cast<uint8_t>(std::lround(color.G * a + data[i + 1] * (1.0 - a)));
                data[i + 2] = static_cast<uint8_t>(std::lround(color.R * a + data[i + 2] * (1.0 - a)));
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

    // Cross through the canvas centre, for aligning symmetric artwork. Drawn as an
    // alternating black/white dash so it stays visible over any colour underneath.
    void MainWindow::OverlayGuides(uint8_t* data, int32_t dw, int32_t dh)
    {
        if (!m_showGuides)
        {
            return;
        }
        const int32_t gx = (doc().context.PixelWidth() * doc().zoom) / 2;
        const int32_t gy = (doc().context.PixelHeight() * doc().zoom) / 2;

        auto put = [&](int32_t dx, int32_t dy)
        {
            if (dx < 0 || dx >= dw || dy < 0 || dy >= dh) { return; }
            const size_t i = (static_cast<size_t>(dy) * dw + dx) * 4;
            const uint8_t v = ((((dx + dy) / 4) & 1) == 0) ? 0x00 : 0xFF;
            data[i + 0] = v;
            data[i + 1] = v;
            data[i + 2] = v;
            data[i + 3] = 0xFF;
        };

        for (int32_t dy = 0; dy < dh; ++dy) { put(gx, dy); }
        for (int32_t dx = 0; dx < dw; ++dx) { put(dx, gy); }
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
        const int32_t w = doc().context.PixelWidth();
        const int32_t h = doc().context.PixelHeight();
        std::vector<float> cov(static_cast<size_t>(w) * h, 0.0f);
        AccumulateStrokeCoverage(cov, m_shapeStartX, m_shapeStartY, m_shapeCurX, m_shapeCurY);
        for (int32_t y = 0; y < h; ++y)
        {
            for (int32_t x = 0; x < w; ++x)
            {
                const double c = cov[static_cast<size_t>(y) * w + x];
                if (c <= 0.0)
                {
                    continue;
                }
                winrt::Windows::UI::Color src = color;
                src.A = static_cast<uint8_t>(std::clamp(static_cast<int>(std::lround(color.A * c)), 0, 255));
                PaintPreviewBlock(data, dw, dh, x, y, src);
            }
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
        const int32_t z = doc().zoom;
        const int32_t offX = (m_moving ? m_moveDX : 0);
        const int32_t offY = (m_moving ? m_moveDY : 0);
        const int32_t w = doc().context.PixelWidth();
        const int32_t h = doc().context.PixelHeight();
        auto const& mask = doc().selMask;
        if (static_cast<int32_t>(mask.size()) != w * h)
        {
            return;
        }

        // Read the mask directly (no cross-ABI call per pixel); out-of-bounds is unselected.
        auto sel = [&](int32_t x, int32_t y) -> bool
        {
            return x >= 0 && x < w && y >= 0 && y < h && mask[static_cast<size_t>(y) * w + x] != 0;
        };

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

        // Trace the mask outline: draw an edge wherever a selected pixel borders an
        // unselected one. For a rectangular selection this is just its outline.
        for (int32_t y = 0; y < h; ++y)
        {
            for (int32_t x = 0; x < w; ++x)
            {
                if (!sel(x, y))
                {
                    continue;
                }
                const int32_t px = (x + offX) * z;
                const int32_t py = (y + offY) * z;
                if (!sel(x, y - 1)) { for (int32_t d = 0; d <= z; ++d) { put(px + d, py); } }
                if (!sel(x, y + 1)) { for (int32_t d = 0; d <= z; ++d) { put(px + d, py + z); } }
                if (!sel(x - 1, y)) { for (int32_t d = 0; d <= z; ++d) { put(px, py + d); } }
                if (!sel(x + 1, y)) { for (int32_t d = 0; d <= z; ++d) { put(px + z, py + d); } }
            }
        }
    }

    void MainWindow::OverlayBrushPreview(uint8_t* data, int32_t dw, int32_t dh)
    {
        const int32_t s = std::clamp(m_brushSize, 1, 64);
        const int32_t startX = m_hoverX - (s / 2); // same centring as the brush footprint
        const int32_t startY = m_hoverY - (s / 2);
        const int32_t x0 = startX * doc().zoom;
        const int32_t y0 = startY * doc().zoom;
        const int32_t x1 = (startX + s) * doc().zoom;
        const int32_t y1 = (startY + s) * doc().zoom;

        // Invert the outline so it stays visible over any pixels/checker/grid.
        auto invert = [&](int32_t dx, int32_t dy)
        {
            if (dx < 0 || dx >= dw || dy < 0 || dy >= dh)
            {
                return;
            }
            const size_t i = (static_cast<size_t>(dy) * dw + dx) * 4;
            data[i + 0] = static_cast<uint8_t>(255 - data[i + 0]);
            data[i + 1] = static_cast<uint8_t>(255 - data[i + 1]);
            data[i + 2] = static_cast<uint8_t>(255 - data[i + 2]);
            data[i + 3] = 0xFF;
        };

        for (int32_t dx = x0; dx <= x1; ++dx) { invert(dx, y0); invert(dx, y1); }
        for (int32_t dy = y0; dy <= y1; ++dy) { invert(x0, dy); invert(x1, dy); }
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

        // Composite the visible layers once into m_flat; RenderBase then reads that
        // instead of doing a cross-ABI GetPixel per display pixel. The result is
        // cached (minus the hover preview) so RenderHover() can blit it.
        FlattenActive();
        RenderBase(data, dw, dh);
        OverlayGuides(data, dw, dh);
        if (m_shapeActive)   { OverlayShapePreview(data, dw, dh); }
        if (m_moving)        { OverlayFloating(data, dw, dh); }
        if (doc().hasSelection)  { OverlaySelectionBorder(data, dw, dh); }

        const size_t bytes = static_cast<size_t>(dw) * dh * 4;
        m_baseCache.assign(data, data + bytes);

        if (m_hoverValid)    { OverlayBrushPreview(data, dw, dh); }

        m_display.Invalidate();
    }

    // Fast path for cursor moves: restore the cached base (no per-pixel work) and
    // redraw only the small hover outline, so the preview tracks the cursor snappily.
    void MainWindow::RenderHover()
    {
        if (m_display == nullptr)
        {
            return;
        }

        const int32_t dw = m_display.PixelWidth();
        const int32_t dh = m_display.PixelHeight();
        const size_t bytes = static_cast<size_t>(dw) * dh * 4;
        if (m_baseCache.size() != bytes)
        {
            Render(); // cache is missing or stale (e.g. after a resize) - rebuild it
            return;
        }

        uint8_t* data = DisplayData();
        std::copy(m_baseCache.begin(), m_baseCache.end(), data);
        if (m_hoverValid)    { OverlayBrushPreview(data, dw, dh); }

        m_display.Invalidate();
    }
}
