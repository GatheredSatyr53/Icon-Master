# Icon Master

Pixel icon editor — a rewrite of the original WPF/C# prototype
([Icon-Master-Legacy](https://github.com/GatheredSatyr53/Icon-Master-Legacy))
on **WinUI 3** and **C++/WinRT**.

> **Status:** project scaffolding only. This is a minimal, known-good WinUI 3
> desktop skeleton (blank window). Editor functionality — canvas, pixel grid,
> zoom, Pen/Eraser tools, colour selection, file open/save — is being ported
> from the legacy project in follow-up steps.

## Requirements

- Windows 10 version 1809 (10.0.17763) or later
- Visual Studio 2022 with the workloads:
  - **Desktop development with C++**
  - **Windows application development** (includes the Windows App SDK / WinUI 3
    C++ templates and the "C++ (v143) Universal Windows Platform tools"
    individual component)
- Windows 11 SDK (10.0.22621.0)

Everything else (Windows App SDK 1.6, C++/WinRT, SDK build tools) is restored
automatically from NuGet on first build.

## Building

1. Open `IconMaster.sln` in Visual Studio 2022.
2. Let NuGet restore the packages listed in `IconMaster/packages.config`.
3. Pick a configuration/platform, e.g. `Debug | x64`.
4. Set **IconMaster** as the startup project and press **F5**.

The app is packaged (MSIX) and deploys to the local machine on run. To build
from the command line:

```
msbuild IconMaster.sln /restore /p:Configuration=Debug /p:Platform=x64
```

## Project layout

```
IconMaster.sln
IconMaster/
├─ App.xaml / App.xaml.h / App.xaml.cpp      Application entry point
├─ MainWindow.xaml / .idl / .h / .cpp         Main window (blank scaffold)
├─ pch.h / pch.cpp                            Precompiled header
├─ app.manifest                               DPI awareness / supported OS
├─ Package.appxmanifest                       MSIX package manifest
├─ packages.config                            NuGet dependencies
└─ Assets/                                    Placeholder logo images
```

> The images in `Assets/` are solid-colour placeholders so the package builds;
> replace them with real artwork before shipping.

## Roadmap

- [ ] Pixel canvas with a visible grid and zoom
- [ ] Tool system (Pen, Eraser) wired to the UI
- [ ] Colour selection
- [ ] Multiple documents (tabs)
- [ ] File open / save (PNG, ICO)
- [ ] Undo / redo
