#pragma once

#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Storage.h>
#include <winrt/Windows.Storage.Streams.h>
#include <cstdint>
#include <memory>
#include <vector>

// File I/O helpers, kept out of MainWindow: decode/encode images, detect a file's
// colour depth, and assemble multi-size ICOs. Callers supply/receive straight
// BGRA8 pixel buffers (row-major), so this stays independent of the editor model.
namespace IconMaster
{
    struct LoadedImage
    {
        uint32_t width{ 0 };
        uint32_t height{ 0 };
        std::vector<uint8_t> bgra;   // straight BGRA8, row-major
        int32_t colorMode{ 32 };     // depth declared by the file (1/4/8/24/32)
    };

    struct ImageIO
    {
        // Decode file into out (pixels + dimensions) and detect its colour depth.
        static winrt::Windows::Foundation::IAsyncAction LoadAsync(
            winrt::Windows::Storage::StorageFile file,
            std::shared_ptr<LoadedImage> out);

        // Encode a single BGRA8 image with the given WIC encoder (PNG/BMP/JPEG/...).
        static winrt::Windows::Foundation::IAsyncAction SaveImageAsync(
            winrt::Windows::Storage::StorageFile file,
            winrt::guid encoderId,
            std::vector<uint8_t> bgra,
            uint32_t width,
            uint32_t height);

        // Assemble a multi-size ICO (16/32/48/256) from one full-resolution BGRA8 image.
        // pngCompress: PNG-compressed frames (smaller, Win7+) vs uncompressed BMP frames.
        static winrt::Windows::Foundation::IAsyncAction SaveIcoAsync(
            winrt::Windows::Storage::StorageFile file,
            std::vector<uint8_t> bgra,
            uint32_t width,
            uint32_t height,
            bool pngCompress);

        // Windows cursor: an ICO-family container (type 2) with a hotspot, one BMP frame.
        static winrt::Windows::Foundation::IAsyncAction SaveCurAsync(
            winrt::Windows::Storage::StorageFile file,
            std::vector<uint8_t> bgra,
            uint32_t width,
            uint32_t height,
            uint16_t hotspotX,
            uint16_t hotspotY);

        // Apple icon set: 'icns' container with PNG entries at several sizes.
        static winrt::Windows::Foundation::IAsyncAction SaveIcnsAsync(
            winrt::Windows::Storage::StorageFile file,
            std::vector<uint8_t> bgra,
            uint32_t width,
            uint32_t height);

        // X PixMap: a C source array with a colour table ("None" for transparency).
        static winrt::Windows::Foundation::IAsyncAction SaveXpmAsync(
            winrt::Windows::Storage::StorageFile file,
            std::vector<uint8_t> bgra,
            uint32_t width,
            uint32_t height,
            winrt::hstring name);

        // X Bitmap: a 1-bit C source array (dark opaque pixels set).
        static winrt::Windows::Foundation::IAsyncAction SaveXbmAsync(
            winrt::Windows::Storage::StorageFile file,
            std::vector<uint8_t> bgra,
            uint32_t width,
            uint32_t height,
            winrt::hstring name);

        // WAP bitmap: 1-bit monochrome, type 0.
        static winrt::Windows::Foundation::IAsyncAction SaveWbmpAsync(
            winrt::Windows::Storage::StorageFile file,
            std::vector<uint8_t> bgra,
            uint32_t width,
            uint32_t height);
    };
}
