#pragma once

#include "MainWindow.g.h"
#include <winrt/Microsoft.UI.Xaml.Media.Imaging.h>

namespace winrt::IconMaster::implementation
{
    struct MainWindow : MainWindowT<MainWindow>
    {
        MainWindow();

        void OnToolChanged(winrt::Windows::Foundation::IInspectable const& sender, winrt::Microsoft::UI::Xaml::RoutedEventArgs const& args);
        void OnColorClick(winrt::Windows::Foundation::IInspectable const& sender, winrt::Microsoft::UI::Xaml::RoutedEventArgs const& args);
        void OnZoomIn(winrt::Windows::Foundation::IInspectable const& sender, winrt::Microsoft::UI::Xaml::RoutedEventArgs const& args);
        void OnZoomOut(winrt::Windows::Foundation::IInspectable const& sender, winrt::Microsoft::UI::Xaml::RoutedEventArgs const& args);
        void OnCanvasPointerPressed(winrt::Windows::Foundation::IInspectable const& sender, winrt::Microsoft::UI::Xaml::Input::PointerRoutedEventArgs const& args);
        void OnCanvasPointerMoved(winrt::Windows::Foundation::IInspectable const& sender, winrt::Microsoft::UI::Xaml::Input::PointerRoutedEventArgs const& args);

    private:
        void RebuildDisplay();
        void RenderAll();
        void RenderPixel(int32_t lx, int32_t ly);
        void WriteDisplayPixel(uint8_t* data, int32_t displayWidth, int32_t dx, int32_t dy);
        uint8_t* DisplayData();
        void SetZoom(int32_t zoom);
        void DrawFromPointer(winrt::Microsoft::UI::Xaml::Input::PointerRoutedEventArgs const& args);

        static constexpr int32_t k_canvasSize = 32;
        static constexpr int32_t k_minZoom = 4;
        static constexpr int32_t k_maxZoom = 48;
        static constexpr int32_t k_checkerCell = 8; // transparency checker cell, in display pixels

        int32_t m_zoom{ 16 };

        winrt::IconMaster::DrawingContext m_context{ nullptr };
        winrt::IconMaster::Pen m_pen{ nullptr };
        winrt::IconMaster::Eraser m_eraser{ nullptr };
        winrt::IconMaster::ITool m_currentTool{ nullptr };
        winrt::Microsoft::UI::Xaml::Media::Imaging::WriteableBitmap m_display{ nullptr };
    };
}

namespace winrt::IconMaster::factory_implementation
{
    struct MainWindow : MainWindowT<MainWindow, implementation::MainWindow>
    {
    };
}
