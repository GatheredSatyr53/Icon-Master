#include "pch.h"
#include "App.xaml.h"
#include "MainWindow.xaml.h"

using namespace winrt;
using namespace winrt::Windows::Foundation;
using namespace winrt::Microsoft::UI::Xaml;

// To learn more about WinUI, the WinUI project structure,
// and more about our project templates, see: http://aka.ms/winui-project-info.

namespace winrt::IconMaster::implementation
{
    // Initializes the singleton application object. This is the first line of
    // authored code executed, and as such is the logical equivalent of main()
    // or WinMain().
    App::App()
    {
        // Xaml objects should not call InitializeComponent during construction.
        // See https://github.com/microsoft/cppwinrt/tree/master/nuget#initializecomponent

#if defined _DEBUG && !defined DISABLE_XAML_GENERATED_BREAK_ON_UNHANDLED_EXCEPTION
        UnhandledException([](IInspectable const&, UnhandledExceptionEventArgs const& e)
        {
            if (IsDebuggerPresent())
            {
                auto errorMessage = e.Message();
                __debugbreak();
            }
        });
#endif
    }

    // Invoked when the application is launched.
    // param e: Details about the launch request and process.
    void App::OnLaunched(LaunchActivatedEventArgs const& e)
    {
        auto mainWindow = make<MainWindow>();
        window = mainWindow;
        window.Activate();

        // A taskbar jump-list entry launches the app with an "open:<token>"
        // argument; hand it to the window so it reopens that recent file.
        const auto arguments = e.Arguments();
        if (!arguments.empty())
        {
            winrt::get_self<MainWindow>(mainWindow)->OpenFromArgument(arguments);
        }
    }
}
