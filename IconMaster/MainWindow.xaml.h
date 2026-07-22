#pragma once

#include "MainWindow.g.h"
#include <winrt/Microsoft.UI.Xaml.Controls.h>
#include <winrt/Microsoft.UI.Xaml.Media.Imaging.h>
#include <vector>

namespace winrt::IconMaster::implementation
{
    struct MainWindow : MainWindowT<MainWindow>
    {
        MainWindow();

        void OnToolSelected(winrt::Windows::Foundation::IInspectable const& sender, winrt::Microsoft::UI::Xaml::RoutedEventArgs const& args);
        void OnSwatchClick(winrt::Windows::Foundation::IInspectable const& sender, winrt::Microsoft::UI::Xaml::RoutedEventArgs const& args);
        void OnColorChanged(winrt::Microsoft::UI::Xaml::Controls::ColorPicker const& sender, winrt::Microsoft::UI::Xaml::Controls::ColorChangedEventArgs const& args);
        void OnZoomIn(winrt::Windows::Foundation::IInspectable const& sender, winrt::Microsoft::UI::Xaml::RoutedEventArgs const& args);
        void OnZoomOut(winrt::Windows::Foundation::IInspectable const& sender, winrt::Microsoft::UI::Xaml::RoutedEventArgs const& args);
        void OnCanvasPointerPressed(winrt::Windows::Foundation::IInspectable const& sender, winrt::Microsoft::UI::Xaml::Input::PointerRoutedEventArgs const& args);
        void OnCanvasPointerMoved(winrt::Windows::Foundation::IInspectable const& sender, winrt::Microsoft::UI::Xaml::Input::PointerRoutedEventArgs const& args);
        void OnCanvasPointerReleased(winrt::Windows::Foundation::IInspectable const& sender, winrt::Microsoft::UI::Xaml::Input::PointerRoutedEventArgs const& args);

        void OnSelectAll(winrt::Windows::Foundation::IInspectable const& sender, winrt::Microsoft::UI::Xaml::RoutedEventArgs const& args);
        void OnDeselect(winrt::Windows::Foundation::IInspectable const& sender, winrt::Microsoft::UI::Xaml::RoutedEventArgs const& args);
        void OnCopy(winrt::Windows::Foundation::IInspectable const& sender, winrt::Microsoft::UI::Xaml::RoutedEventArgs const& args);
        void OnCut(winrt::Windows::Foundation::IInspectable const& sender, winrt::Microsoft::UI::Xaml::RoutedEventArgs const& args);
        void OnPaste(winrt::Windows::Foundation::IInspectable const& sender, winrt::Microsoft::UI::Xaml::RoutedEventArgs const& args);
        void OnDelete(winrt::Windows::Foundation::IInspectable const& sender, winrt::Microsoft::UI::Xaml::RoutedEventArgs const& args);

        void OnNew(winrt::Windows::Foundation::IInspectable const& sender, winrt::Microsoft::UI::Xaml::RoutedEventArgs const& args);
        winrt::fire_and_forget OnOpen(winrt::Windows::Foundation::IInspectable const& sender, winrt::Microsoft::UI::Xaml::RoutedEventArgs const& args);
        winrt::fire_and_forget OnSave(winrt::Windows::Foundation::IInspectable const& sender, winrt::Microsoft::UI::Xaml::RoutedEventArgs const& args);
        winrt::fire_and_forget OnExportIco(winrt::Windows::Foundation::IInspectable const& sender, winrt::Microsoft::UI::Xaml::RoutedEventArgs const& args);

        void OnUndo(winrt::Windows::Foundation::IInspectable const& sender, winrt::Microsoft::UI::Xaml::RoutedEventArgs const& args);
        void OnRedo(winrt::Windows::Foundation::IInspectable const& sender, winrt::Microsoft::UI::Xaml::RoutedEventArgs const& args);

    private:
        void LoadContext(winrt::IconMaster::DrawingContext const& context, int32_t fitZoom);
        std::vector<uint8_t> ScaleCanvas(int32_t target); // nearest-neighbour, BGRA8

        // Undo/redo via full-canvas snapshots.
        struct Snapshot { int32_t w; int32_t h; std::vector<winrt::Windows::UI::Color> pixels; };
        Snapshot CaptureSnapshot();
        void RestoreSnapshot(Snapshot const& snap);
        void PushUndo();
        void ClearHistory();
        enum class ToolKind { Pen, Eraser, Fill, Eyedropper, Line, Rectangle, Ellipse, Select };

        // Rendering.
        void RebuildDisplay();
        void Render();
        void RenderBase(uint8_t* data, int32_t dw, int32_t dh);
        void WriteDisplayPixel(uint8_t* data, int32_t displayWidth, int32_t dx, int32_t dy);
        void PaintPreviewBlock(uint8_t* data, int32_t displayWidth, int32_t displayHeight, int32_t lx, int32_t ly, winrt::Windows::UI::Color const& color);
        void OverlayShapePreview(uint8_t* data, int32_t dw, int32_t dh);
        void OverlayFloating(uint8_t* data, int32_t dw, int32_t dh);
        void OverlaySelectionBorder(uint8_t* data, int32_t dw, int32_t dh);
        uint8_t* DisplayData();
        void SetZoom(int32_t zoom);

        // Tools.
        winrt::IconMaster::ITool ToolForKind(ToolKind kind);
        static bool IsShapeTool(ToolKind kind);
        winrt::IconMaster::IShapeTool ShapeToolForKind(ToolKind kind);
        void PointerToPixelClamped(winrt::Microsoft::UI::Xaml::Input::PointerRoutedEventArgs const& args, int32_t& lx, int32_t& ly);
        void DrawFromPointer(winrt::Microsoft::UI::Xaml::Input::PointerRoutedEventArgs const& args);
        void CommitShape(int32_t x1, int32_t y1);

        // Selection / clipboard.
        bool InsideSelection(int32_t px, int32_t py) const;
        void SetSelectionFromPoints(int32_t ax, int32_t ay, int32_t bx, int32_t by);
        void LiftSelection();
        void StampFloating(int32_t atX, int32_t atY);
        void ClearRegion(int32_t x, int32_t y, int32_t w, int32_t h);

        static constexpr int32_t k_canvasSize = 32;
        static constexpr int32_t k_minZoom = 4;
        static constexpr int32_t k_maxZoom = 48;
        static constexpr int32_t k_checkerCell = 8; // transparency checker cell, in display pixels

        int32_t m_zoom{ 16 };
        ToolKind m_toolKind{ ToolKind::Pen };
        bool m_suppressColorSync{ false };

        // In-progress shape drag (line/rectangle/ellipse).
        bool m_shapeActive{ false };
        int32_t m_shapeStartX{ 0 };
        int32_t m_shapeStartY{ 0 };
        int32_t m_shapeCurX{ 0 };
        int32_t m_shapeCurY{ 0 };

        // Selection state (pixel-space rectangle).
        bool m_hasSelection{ false };
        int32_t m_selX{ 0 };
        int32_t m_selY{ 0 };
        int32_t m_selW{ 0 };
        int32_t m_selH{ 0 };

        // Defining a selection by dragging.
        bool m_selecting{ false };
        int32_t m_selAnchorX{ 0 };
        int32_t m_selAnchorY{ 0 };

        // Moving the selected pixels (floating).
        bool m_moving{ false };
        int32_t m_moveAnchorX{ 0 };
        int32_t m_moveAnchorY{ 0 };
        int32_t m_moveDX{ 0 };
        int32_t m_moveDY{ 0 };
        int32_t m_floatW{ 0 };
        int32_t m_floatH{ 0 };
        std::vector<winrt::Windows::UI::Color> m_floatPixels;

        // Clipboard.
        bool m_hasClip{ false };
        int32_t m_clipW{ 0 };
        int32_t m_clipH{ 0 };
        std::vector<winrt::Windows::UI::Color> m_clipPixels;

        // History.
        static constexpr size_t k_maxHistory = 64;
        std::vector<Snapshot> m_undo;
        std::vector<Snapshot> m_redo;

        winrt::IconMaster::DrawingContext m_context{ nullptr };
        winrt::IconMaster::Pen m_pen{ nullptr };
        winrt::IconMaster::Eraser m_eraser{ nullptr };
        winrt::IconMaster::Fill m_fill{ nullptr };
        winrt::IconMaster::Eyedropper m_eyedropper{ nullptr };
        winrt::IconMaster::LineTool m_line{ nullptr };
        winrt::IconMaster::RectangleTool m_rectangle{ nullptr };
        winrt::IconMaster::EllipseTool m_ellipse{ nullptr };
        winrt::IconMaster::ITool m_currentTool{ nullptr };
        winrt::IconMaster::IShapeTool m_currentShape{ nullptr };
        winrt::Microsoft::UI::Xaml::Media::Imaging::WriteableBitmap m_display{ nullptr };
    };
}

namespace winrt::IconMaster::factory_implementation
{
    struct MainWindow : MainWindowT<MainWindow, implementation::MainWindow>
    {
    };
}
