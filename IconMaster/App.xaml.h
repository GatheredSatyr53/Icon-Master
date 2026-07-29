#pragma once

#include "App.xaml.g.h"
#include "StringFormatConverter.h"
#include "WrapLayout.h"        // used as a custom ItemsRepeater layout in MainWindow.xaml

namespace winrt::IconMaster::implementation
{
    struct App : AppT<App>
    {
        App();

        void OnLaunched(winrt::Microsoft::UI::Xaml::LaunchActivatedEventArgs const& e);

    private:
        winrt::Microsoft::UI::Xaml::Window window{ nullptr };
    };
}
