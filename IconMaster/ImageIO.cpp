#include "pch.h"
#include "ImageIO.h"

#include <winrt/Windows.Graphics.Imaging.h>
#include <shcore.h>
#include <wincodec.h>
#include <array>

#pragma comment(lib, "windowscodecs.lib")
#pragma comment(lib, "shcore.lib")
#pragma comment(lib, "ole32.lib")

namespace WGI = winrt::Windows::Graphics::Imaging;
namespace WSS = winrt::Windows::Storage::Streams;

namespace
{
    // Read a bitmap's declared colour depth via WIC and map it to a DrawingContext
    // colour mode (1/4/8/24/32). Returns 32 if the format can't be determined.
    int32_t DetectColorModeFromStream(WSS::IRandomAccessStream const& ras)
    {
        winrt::com_ptr<IStream> stm;
        if (FAILED(CreateStreamOverRandomAccessStream(winrt::get_unknown(ras), __uuidof(IStream), stm.put_void())))
        {
            return 32;
        }
        winrt::com_ptr<IWICImagingFactory> factory;
        if (FAILED(CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER,
                                    __uuidof(IWICImagingFactory), factory.put_void())))
        {
            return 32;
        }
        winrt::com_ptr<IWICBitmapDecoder> dec;
        if (FAILED(factory->CreateDecoderFromStream(stm.get(), nullptr, WICDecodeMetadataCacheOnDemand, dec.put())))
        {
            return 32;
        }
        winrt::com_ptr<IWICBitmapFrameDecode> frame;
        if (FAILED(dec->GetFrame(0, frame.put())))
        {
            return 32;
        }
        WICPixelFormatGUID fmt{};
        if (FAILED(frame->GetPixelFormat(&fmt)))
        {
            return 32;
        }
        winrt::com_ptr<IWICComponentInfo> ci;
        if (FAILED(factory->CreateComponentInfo(fmt, ci.put())))
        {
            return 32;
        }
        auto pfi = ci.try_as<IWICPixelFormatInfo2>();
        if (!pfi)
        {
            return 32;
        }
        UINT bpp = 0;
        pfi->GetBitsPerPixel(&bpp);
        BOOL transparency = FALSE;
        pfi->SupportsTransparency(&transparency);

        if (bpp <= 1) { return 1; }
        if (bpp <= 4) { return 4; }
        if (bpp == 8) { return 8; }
        return transparency ? 32 : 24; // truecolour, with or without an alpha channel
    }

    // Nearest-neighbour scale of a BGRA8 image to target x target.
    std::vector<uint8_t> ScaleBgra(std::vector<uint8_t> const& src, uint32_t w, uint32_t h, int32_t target)
    {
        std::vector<uint8_t> out(static_cast<size_t>(target) * target * 4);
        for (int32_t y = 0; y < target; ++y)
        {
            for (int32_t x = 0; x < target; ++x)
            {
                const uint32_t sx = static_cast<uint32_t>(x) * w / target;
                const uint32_t sy = static_cast<uint32_t>(y) * h / target;
                const size_t si = (static_cast<size_t>(sy) * w + sx) * 4;
                const size_t di = (static_cast<size_t>(y) * target + x) * 4;
                out[di + 0] = src[si + 0];
                out[di + 1] = src[si + 1];
                out[di + 2] = src[si + 2];
                out[di + 3] = src[si + 3];
            }
        }
        return out;
    }
}

namespace IconMaster
{
    winrt::Windows::Foundation::IAsyncAction ImageIO::LoadAsync(
        winrt::Windows::Storage::StorageFile file, std::shared_ptr<LoadedImage> out)
    {
        auto stream = co_await file.OpenAsync(winrt::Windows::Storage::FileAccessMode::Read);
        auto decoder = co_await WGI::BitmapDecoder::CreateAsync(stream);
        out->width = decoder.PixelWidth();
        out->height = decoder.PixelHeight();

        auto provider = co_await decoder.GetPixelDataAsync(
            WGI::BitmapPixelFormat::Bgra8,
            WGI::BitmapAlphaMode::Straight,
            WGI::BitmapTransform(),
            WGI::ExifOrientationMode::IgnoreExifOrientation,
            WGI::ColorManagementMode::DoNotColorManage);
        auto data = provider.DetachPixelData();
        out->bgra.assign(data.begin(), data.end());

        // A fresh stream keeps the WIC depth read independent of the decoder above.
        auto detectStream = co_await file.OpenAsync(winrt::Windows::Storage::FileAccessMode::Read);
        out->colorMode = DetectColorModeFromStream(detectStream);
    }

    winrt::Windows::Foundation::IAsyncAction ImageIO::SaveImageAsync(
        winrt::Windows::Storage::StorageFile file, winrt::guid encoderId,
        std::vector<uint8_t> bgra, uint32_t width, uint32_t height)
    {
        auto stream = co_await file.OpenAsync(winrt::Windows::Storage::FileAccessMode::ReadWrite);
        stream.Size(0); // truncate any previous content when overwriting
        auto encoder = co_await WGI::BitmapEncoder::CreateAsync(encoderId, stream);
        encoder.SetPixelData(
            WGI::BitmapPixelFormat::Bgra8,
            WGI::BitmapAlphaMode::Straight,
            width, height, 96.0, 96.0, bgra);
        co_await encoder.FlushAsync();
    }

    winrt::Windows::Foundation::IAsyncAction ImageIO::SaveIcoAsync(
        winrt::Windows::Storage::StorageFile file,
        std::vector<uint8_t> bgra, uint32_t width, uint32_t height)
    {
        // Render each icon size to a PNG blob (ICO may embed PNG-compressed images).
        constexpr std::array<int32_t, 4> sizes{ 16, 32, 48, 256 };
        std::vector<std::vector<uint8_t>> pngs;
        for (int32_t s : sizes)
        {
            const std::vector<uint8_t> bytes = ScaleBgra(bgra, width, height, s);

            WSS::InMemoryRandomAccessStream mem;
            auto encoder = co_await WGI::BitmapEncoder::CreateAsync(WGI::BitmapEncoder::PngEncoderId(), mem);
            encoder.SetPixelData(
                WGI::BitmapPixelFormat::Bgra8,
                WGI::BitmapAlphaMode::Straight,
                static_cast<uint32_t>(s), static_cast<uint32_t>(s),
                96.0, 96.0, bytes);
            co_await encoder.FlushAsync();

            const auto len = static_cast<uint32_t>(mem.Size());
            WSS::DataReader reader(mem.GetInputStreamAt(0));
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

        const auto count = static_cast<uint16_t>(std::size(sizes));
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
}
