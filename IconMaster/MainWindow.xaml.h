#pragma once

#include "MainWindow.g.h"
#include <winrt/Microsoft.UI.Xaml.Controls.h>
#include <winrt/Microsoft.UI.Xaml.Media.Imaging.h>

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

    private:
        enum class ToolKind { Pen, Eraser, Fill, Eyedropper, Line, Rectangle, Ellipse };

        void RebuildDisplay();
        void RenderAll();
        void RenderPixel(int32_t lx, int32_t ly);
        void WriteDisplayPixel(uint8_t* data, int32_t displayWidth, int32_t dx, int32_t dy);
        uint8_t* DisplayData();
        void SetZoom(int32_t zoom);
        void DrawFromPointer(winrt::Microsoft::UI::Xaml::Input::PointerRoutedEventArgs const& args);
        winrt::IconMaster::ITool ToolForKind(ToolKind kind);

        static bool IsShapeTool(ToolKind kind);
        winrt::IconMaster::IShapeTool ShapeToolForKind(ToolKind kind);
        void PointerToPixelClamped(winrt::Microsoft::UI::Xaml::Input::PointerRoutedEventArgs const& args, int32_t& lx, int32_t& ly);
        void OverlayShapePreview(int32_t x1, int32_t y1);
        void PaintPreviewBlock(uint8_t* data, int32_t displayWidth, int32_t displayHeight, int32_t lx, int32_t ly, winrt::Windows::UI::Color const& color);
        void CommitShape(int32_t x1, int32_t y1);

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
