# Icon Master

[![build](https://github.com/GatheredSatyr53/Icon-Master/actions/workflows/build.yml/badge.svg)](https://github.com/GatheredSatyr53/Icon-Master/actions/workflows/build.yml)

Pixel icon editor — a rewrite of the original WPF/C# prototype
([Icon-Master-Legacy](https://github.com/GatheredSatyr53/Icon-Master-Legacy))
on **WinUI 3** and **C++/WinRT**.

> **Status:** early work in progress. The app builds and runs: draw on a zoomable
> pixel canvas with a crisp grid and a transparency checkerboard using the Pen,
> Eraser, Fill, and Eyedropper tools, and choose a colour from swatches or a full
> colour picker. Shape tools, tabs, and file open/save are still to come.

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
├─ MainWindow.xaml / .idl / .h / .cpp         Main window: canvas + toolbar
├─ Editor.idl                                 WinRT contracts for the editor core
├─ DrawingContext.h / .cpp                    Pixel surface (WriteableBitmap + colour)
├─ Pen.h / .cpp, Eraser.h / .cpp             ITool implementations
├─ pch.h / pch.cpp                            Precompiled header
├─ app.manifest                               DPI awareness / supported OS
├─ Package.appxmanifest                       MSIX package manifest
├─ packages.config                            NuGet dependencies
└─ Assets/                                    Placeholder logo images
```

The editor core is modelled as Windows Runtime classes (declared in
`Editor.idl`): `DrawingContext` owns the logical pixel grid and current colour,
and the `ITool` interface is implemented by `Pen`, `Eraser`, `Fill`, and
`Eyedropper`. `MainWindow`
holds a `DrawingContext` and the active tool, renders the grid into a scaled
`WriteableBitmap` (each logical pixel becomes a `zoom`×`zoom` block over a
transparency checkerboard, with grid lines, drawn at 1:1 so pixels stay crisp),
and routes pointer input to the active tool.

> The images in `Assets/` are solid-colour placeholders so the package builds;
> replace them with real artwork before shipping.

## Roadmap

- [x] Pixel canvas with a crisp grid, transparency checkerboard, and zoom
- [x] Tools: Pen, Eraser, Fill (flood), Eyedropper
- [x] Colour selection: swatches plus a full HSV/alpha colour picker
- [ ] Shape tools (line, rectangle, ellipse) with a drag preview
- [ ] Multiple documents (tabs)
- [ ] File open / save (PNG, ICO)
- [ ] Undo / redo
