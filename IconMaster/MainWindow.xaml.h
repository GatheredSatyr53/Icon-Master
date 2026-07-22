#pragma once

#include "MainWindow.g.h"

namespace winrt::IconMaster::implementation
{
    struct MainWindow : MainWindowT<MainWindow>
    {
        MainWindow();

        void OnToolChanged(winrt::Windows::Foundation::IInspectable const& sender, winrt::Microsoft::UI::Xaml::RoutedEventArgs const& args);
        void OnColorClick(winrt::Windows::Foundation::IInspectable const& sender, winrt::Microsoft::UI::Xaml::RoutedEventArgs const& args);
        void OnCanvasPointerPressed(winrt::Windows::Foundation::IInspectable const& sender, winrt::Microsoft::UI::Xaml::Input::PointerRoutedEventArgs const& args);
        void OnCanvasPointerMoved(winrt::Windows::Foundation::IInspectable const& sender, winrt::Microsoft::UI::Xaml::Input::PointerRoutedEventArgs const& args);

    private:
        void DrawFromPointer(winrt::Microsoft::UI::Xaml::Input::PointerRoutedEventArgs const& args);

        static constexpr int32_t k_canvasSize = 32;

        winrt::IconMaster::DrawingContext m_context{ nullptr };
        winrt::IconMaster::Pen m_pen{ nullptr };
        winrt::IconMaster::Eraser m_eraser{ nullptr };
        winrt::IconMaster::ITool m_currentTool{ nullptr };
    };
}

namespace winrt::IconMaster::factory_implementation
{
    struct MainWindow : MainWindowT<MainWindow, implementation::MainWindow>
    {
    };
}
