#include "pch.h"
#include "ImageIO.h"

#include <winrt/Windows.Graphics.Imaging.h>
#include <shcore.h>
#include <wincodec.h>
#include <algorithm>
#include <array>
#include <string>
#include <unordered_map>

#pragma comment(lib, "windowscodecs.lib")
#pragma comment(lib, "shcore.lib")
#pragma comment(lib, "ole32.lib")

namespace WGI = winrt::Windows::Graphics::Imaging;
namespace WSS = winrt::Windows::Storage::Streams;

namespace
{
    // Read a bitmap frame's declared colour depth via WIC and map it to a
    // DrawingContext colour mode (1/4/8/24/32). Returns 32 if it can't be read.
    int32_t DetectColorModeFromStream(WSS::IRandomAccessStream const& ras, uint32_t frameIndex)
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
        if (FAILED(dec->GetFrame(frameIndex, frame.put())))
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

    // Encode one ICO frame as an uncompressed 32bpp BMP/DIB: a BITMAPINFOHEADER
    // (double height for the XOR image + AND mask), the BGRA pixels bottom-up,
    // then an all-zero AND mask (transparency comes from the alpha channel).
    std::vector<uint8_t> EncodeBmpIcoFrame(std::vector<uint8_t> const& bgra, int32_t w, int32_t h)
    {
        const uint32_t xorSize = static_cast<uint32_t>(w) * static_cast<uint32_t>(h) * 4u;
        const uint32_t andRow = ((static_cast<uint32_t>(w) + 31u) / 32u) * 4u; // 1bpp, 4-byte aligned
        const uint32_t andSize = andRow * static_cast<uint32_t>(h);

        std::vector<uint8_t> out;
        out.reserve(40u + xorSize + andSize);

        const auto putU16 = [&out](uint16_t x)
        {
            out.push_back(static_cast<uint8_t>(x & 0xFF));
            out.push_back(static_cast<uint8_t>((x >> 8) & 0xFF));
        };
        const auto putU32 = [&out](uint32_t x)
        {
            out.push_back(static_cast<uint8_t>(x & 0xFF));
            out.push_back(static_cast<uint8_t>((x >> 8) & 0xFF));
            out.push_back(static_cast<uint8_t>((x >> 16) & 0xFF));
            out.push_back(static_cast<uint8_t>((x >> 24) & 0xFF));
        };

        // BITMAPINFOHEADER
        putU32(40);                                       // biSize
        putU32(static_cast<uint32_t>(w));                 // biWidth
        putU32(static_cast<uint32_t>(h) * 2u);            // biHeight (XOR + AND)
        putU16(1);                                        // biPlanes
        putU16(32);                                       // biBitCount
        putU32(0);                                        // biCompression = BI_RGB
        putU32(0);                                        // biSizeImage
        putU32(0);                                        // biXPelsPerMeter
        putU32(0);                                        // biYPelsPerMeter
        putU32(0);                                        // biClrUsed
        putU32(0);                                        // biClrImportant

        // XOR bitmap: BGRA rows, bottom-up.
        for (int32_t y = h - 1; y >= 0; --y)
        {
            const size_t row = static_cast<size_t>(y) * static_cast<size_t>(w) * 4u;
            out.insert(out.end(), bgra.begin() + row, bgra.begin() + row + static_cast<size_t>(w) * 4u);
        }

        // AND mask: all opaque (zero), bottom-up, row-padded.
        out.insert(out.end(), andSize, 0);
        return out;
    }

    // A pixel counts as "ink" for the monochrome formats when it is opaque and dark.
    bool IsDarkOpaque(uint8_t b, uint8_t g, uint8_t r, uint8_t a)
    {
        if (a < 128) { return false; }
        const uint32_t luma = (static_cast<uint32_t>(r) * 299u + static_cast<uint32_t>(g) * 587u + static_cast<uint32_t>(b) * 114u) / 1000u;
        return luma < 128u;
    }

    // Little-endian byte pushers (ICO/CUR are little-endian).
    void PutU16(std::vector<uint8_t>& v, uint16_t x)
    {
        v.push_back(static_cast<uint8_t>(x & 0xFF));
        v.push_back(static_cast<uint8_t>((x >> 8) & 0xFF));
    }
    void PutU32(std::vector<uint8_t>& v, uint32_t x)
    {
        v.push_back(static_cast<uint8_t>(x & 0xFF));
        v.push_back(static_cast<uint8_t>((x >> 8) & 0xFF));
        v.push_back(static_cast<uint8_t>((x >> 16) & 0xFF));
        v.push_back(static_cast<uint8_t>((x >> 24) & 0xFF));
    }

    // Append a hex byte ("0xAB, ") to a C array literal.
    void AppendHexByte(std::string& s, uint8_t x)
    {
        static const char* hex = "0123456789abcdef";
        s += "0x";
        s += hex[(x >> 4) & 0xF];
        s += hex[x & 0xF];
        s += ", ";
    }
}

namespace IconMaster
{
    winrt::Windows::Foundation::IAsyncAction ImageIO::LoadAsync(
        winrt::Windows::Storage::StorageFile file, std::shared_ptr<LoadedImage> out)
    {
        auto stream = co_await file.OpenAsync(winrt::Windows::Storage::FileAccessMode::Read);
        auto decoder = co_await WGI::BitmapDecoder::CreateAsync(stream);

        // Multi-frame containers (ICO) hold several sizes; pick the largest frame
        // that still fits the editor's 256px limit. Single-frame formats leave
        // this at frame 0.
        uint32_t best = 0;
        uint32_t bestW = 0;
        const uint32_t frameCount = decoder.FrameCount();
        for (uint32_t i = 0; i < frameCount; ++i)
        {
            auto f = co_await decoder.GetFrameAsync(i);
            const uint32_t fw = f.PixelWidth();
            const uint32_t fh = f.PixelHeight();
            if (fw <= 256 && fh <= 256 && fw > bestW)
            {
                bestW = fw;
                best = i;
            }
        }

        auto frame = co_await decoder.GetFrameAsync(best);
        out->width = frame.PixelWidth();
        out->height = frame.PixelHeight();

        auto provider = co_await frame.GetPixelDataAsync(
            WGI::BitmapPixelFormat::Bgra8,
            WGI::BitmapAlphaMode::Straight,
            WGI::BitmapTransform(),
            WGI::ExifOrientationMode::IgnoreExifOrientation,
            WGI::ColorManagementMode::DoNotColorManage);
        auto data = provider.DetachPixelData();
        out->bgra.assign(data.begin(), data.end());

        // A fresh stream keeps the WIC depth read independent of the decoder above.
        auto detectStream = co_await file.OpenAsync(winrt::Windows::Storage::FileAccessMode::Read);
        out->colorMode = DetectColorModeFromStream(detectStream, best);
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
        std::vector<uint8_t> bgra, uint32_t width, uint32_t height, bool pngCompress)
    {
        // Each icon size becomes one frame blob: PNG-compressed (smaller, Win7+) or
        // an uncompressed 32bpp BMP/DIB (broadest compatibility).
        constexpr std::array<int32_t, 4> sizes{ 16, 32, 48, 256 };
        std::vector<std::vector<uint8_t>> frames;
        for (int32_t s : sizes)
        {
            const std::vector<uint8_t> bytes = ScaleBgra(bgra, width, height, s);

            if (!pngCompress)
            {
                frames.push_back(EncodeBmpIcoFrame(bytes, s, s));
                continue;
            }

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
            frames.push_back(std::move(png));
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
        for (size_t k = 0; k < frames.size(); ++k)
        {
            const int32_t s = sizes[k];
            ico.push_back(static_cast<uint8_t>(s >= 256 ? 0 : s)); // width (0 => 256)
            ico.push_back(static_cast<uint8_t>(s >= 256 ? 0 : s)); // height
            ico.push_back(0);  // colour count
            ico.push_back(0);  // reserved
            putU16(ico, 1);    // colour planes
            putU16(ico, 32);   // bits per pixel
            putU32(ico, static_cast<uint32_t>(frames[k].size()));
            putU32(ico, offset);
            offset += static_cast<uint32_t>(frames[k].size());
        }

        for (auto const& frame : frames)
        {
            ico.insert(ico.end(), frame.begin(), frame.end());
        }

        co_await winrt::Windows::Storage::FileIO::WriteBytesAsync(file, ico);
    }

    winrt::Windows::Foundation::IAsyncAction ImageIO::SaveCurAsync(
        winrt::Windows::Storage::StorageFile file,
        std::vector<uint8_t> bgra, uint32_t width, uint32_t height,
        uint16_t hotspotX, uint16_t hotspotY)
    {
        // Cursors are capped at 256x256; scale to a square that fits.
        const int32_t s = std::clamp<int32_t>(static_cast<int32_t>(std::max(width, height)), 1, 256);
        const std::vector<uint8_t> scaled = ScaleBgra(bgra, width, height, s);
        const std::vector<uint8_t> frame = EncodeBmpIcoFrame(scaled, s, s);

        std::vector<uint8_t> cur;
        PutU16(cur, 0); // reserved
        PutU16(cur, 2); // type = cursor
        PutU16(cur, 1); // one image

        // CURSORDIRENTRY: the planes/bitcount fields carry the hotspot instead.
        cur.push_back(static_cast<uint8_t>(s >= 256 ? 0 : s)); // width (0 => 256)
        cur.push_back(static_cast<uint8_t>(s >= 256 ? 0 : s)); // height
        cur.push_back(0); // colour count
        cur.push_back(0); // reserved
        PutU16(cur, hotspotX);
        PutU16(cur, hotspotY);
        PutU32(cur, static_cast<uint32_t>(frame.size()));
        PutU32(cur, 6u + 16u); // image data follows the header + single entry

        cur.insert(cur.end(), frame.begin(), frame.end());
        co_await winrt::Windows::Storage::FileIO::WriteBytesAsync(file, cur);
    }

    winrt::Windows::Foundation::IAsyncAction ImageIO::SaveIcnsAsync(
        winrt::Windows::Storage::StorageFile file,
        std::vector<uint8_t> bgra, uint32_t width, uint32_t height)
    {
        // Modern icns entries hold PNG data keyed by an OSType per size.
        struct Entry { char type[4]; int32_t size; };
        constexpr Entry kEntries[] = {
            { {'i','c','1','1'}, 32 },
            { {'i','c','1','2'}, 64 },
            { {'i','c','0','7'}, 128 },
            { {'i','c','0','8'}, 256 },
            { {'i','c','0','9'}, 512 },
        };

        // 4-byte big-endian length (icns is big-endian).
        const auto beU32 = [](std::vector<uint8_t>& v, uint32_t x)
        {
            v.push_back(static_cast<uint8_t>((x >> 24) & 0xFF));
            v.push_back(static_cast<uint8_t>((x >> 16) & 0xFF));
            v.push_back(static_cast<uint8_t>((x >> 8) & 0xFF));
            v.push_back(static_cast<uint8_t>(x & 0xFF));
        };

        std::vector<uint8_t> body;
        for (auto const& e : kEntries)
        {
            const std::vector<uint8_t> scaled = ScaleBgra(bgra, width, height, e.size);

            WSS::InMemoryRandomAccessStream mem;
            auto encoder = co_await WGI::BitmapEncoder::CreateAsync(WGI::BitmapEncoder::PngEncoderId(), mem);
            encoder.SetPixelData(
                WGI::BitmapPixelFormat::Bgra8,
                WGI::BitmapAlphaMode::Straight,
                static_cast<uint32_t>(e.size), static_cast<uint32_t>(e.size),
                96.0, 96.0, scaled);
            co_await encoder.FlushAsync();

            const auto len = static_cast<uint32_t>(mem.Size());
            WSS::DataReader reader(mem.GetInputStreamAt(0));
            co_await reader.LoadAsync(len);
            std::vector<uint8_t> png(len);
            reader.ReadBytes(png);

            body.insert(body.end(), e.type, e.type + 4);
            beU32(body, 8u + static_cast<uint32_t>(png.size())); // element length includes its 8-byte header
            body.insert(body.end(), png.begin(), png.end());
        }

        std::vector<uint8_t> icns;
        const char magic[4] = { 'i','c','n','s' };
        icns.insert(icns.end(), magic, magic + 4);
        beU32(icns, 8u + static_cast<uint32_t>(body.size())); // total file length
        icns.insert(icns.end(), body.begin(), body.end());

        co_await winrt::Windows::Storage::FileIO::WriteBytesAsync(file, icns);
    }

    winrt::Windows::Foundation::IAsyncAction ImageIO::SaveXpmAsync(
        winrt::Windows::Storage::StorageFile file,
        std::vector<uint8_t> bgra, uint32_t width, uint32_t height, winrt::hstring name)
    {
        // Printable symbol alphabet, excluding '"' and '\\' so the C strings stay valid.
        static const std::string cs =
            ".#abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789@%&*=+-<>[]{}()/?";

        // Map each pixel to a colour index; transparent pixels share one slot.
        std::unordered_map<uint32_t, int32_t> lookup;
        std::vector<uint32_t> colors; // packed 0x00RRGGBB
        std::vector<int32_t> idx(static_cast<size_t>(width) * height);
        bool hasTransparent = false;
        for (uint32_t p = 0; p < width * height; ++p)
        {
            const size_t i = static_cast<size_t>(p) * 4;
            const uint8_t b = bgra[i + 0], g = bgra[i + 1], r = bgra[i + 2], a = bgra[i + 3];
            if (a < 128)
            {
                hasTransparent = true;
                idx[p] = -1;
                continue;
            }
            const uint32_t rgb = (static_cast<uint32_t>(r) << 16) | (static_cast<uint32_t>(g) << 8) | b;
            auto it = lookup.find(rgb);
            if (it == lookup.end())
            {
                const int32_t n = static_cast<int32_t>(colors.size());
                lookup.emplace(rgb, n);
                colors.push_back(rgb);
                idx[p] = n;
            }
            else
            {
                idx[p] = it->second;
            }
        }

        const int32_t transparentSlot = static_cast<int32_t>(colors.size());
        const int32_t nColors = transparentSlot + (hasTransparent ? 1 : 0);

        // Characters per pixel: enough symbols to cover every colour.
        int32_t cpp = 1;
        size_t capacity = cs.size();
        while (capacity < static_cast<size_t>(nColors)) { ++cpp; capacity *= cs.size(); }

        const auto symbol = [&](int32_t k)
        {
            std::string s(static_cast<size_t>(cpp), cs[0]);
            for (int32_t pos = cpp - 1; pos >= 0; --pos)
            {
                s[static_cast<size_t>(pos)] = cs[static_cast<size_t>(k) % cs.size()];
                k /= static_cast<int32_t>(cs.size());
            }
            return s;
        };
        const auto hex2 = [](uint32_t x)
        {
            static const char* h = "0123456789ABCDEF";
            std::string s;
            s += h[(x >> 4) & 0xF];
            s += h[x & 0xF];
            return s;
        };

        std::string out;
        out += "/* XPM */\n";
        out += "static char * " + winrt::to_string(name) + "_xpm[] = {\n";
        out += "\"" + std::to_string(width) + " " + std::to_string(height) + " " +
               std::to_string(nColors) + " " + std::to_string(cpp) + "\",\n";

        for (int32_t k = 0; k < transparentSlot; ++k)
        {
            const uint32_t rgb = colors[static_cast<size_t>(k)];
            out += "\"" + symbol(k) + " c #" + hex2(rgb >> 16) + hex2(rgb >> 8) + hex2(rgb) + "\",\n";
        }
        if (hasTransparent)
        {
            out += "\"" + symbol(transparentSlot) + " c None\",\n";
        }

        for (uint32_t y = 0; y < height; ++y)
        {
            out += "\"";
            for (uint32_t x = 0; x < width; ++x)
            {
                const int32_t k = idx[static_cast<size_t>(y) * width + x];
                out += symbol(k < 0 ? transparentSlot : k);
            }
            out += (y + 1 == height) ? "\"\n" : "\",\n";
        }
        out += "};\n";

        co_await winrt::Windows::Storage::FileIO::WriteTextAsync(file, winrt::to_hstring(out));
    }

    winrt::Windows::Foundation::IAsyncAction ImageIO::SaveXbmAsync(
        winrt::Windows::Storage::StorageFile file,
        std::vector<uint8_t> bgra, uint32_t width, uint32_t height, winrt::hstring name)
    {
        const uint32_t stride = (width + 7u) / 8u;
        std::vector<uint8_t> bits(static_cast<size_t>(stride) * height, 0);
        for (uint32_t y = 0; y < height; ++y)
        {
            for (uint32_t x = 0; x < width; ++x)
            {
                const size_t i = (static_cast<size_t>(y) * width + x) * 4;
                if (IsDarkOpaque(bgra[i + 0], bgra[i + 1], bgra[i + 2], bgra[i + 3]))
                {
                    bits[static_cast<size_t>(y) * stride + x / 8] |= static_cast<uint8_t>(1u << (x % 8)); // XBM is LSB-first
                }
            }
        }

        const std::string id = winrt::to_string(name);
        std::string out;
        out += "#define " + id + "_width " + std::to_string(width) + "\n";
        out += "#define " + id + "_height " + std::to_string(height) + "\n";
        out += "static unsigned char " + id + "_bits[] = {\n";
        for (size_t i = 0; i < bits.size(); ++i)
        {
            if (i % 12 == 0) { out += "  "; }
            AppendHexByte(out, bits[i]);
            if (i % 12 == 11) { out += "\n"; }
        }
        if (bits.size() % 12 != 0) { out += "\n"; }
        out += "};\n";

        co_await winrt::Windows::Storage::FileIO::WriteTextAsync(file, winrt::to_hstring(out));
    }

    winrt::Windows::Foundation::IAsyncAction ImageIO::SaveWbmpAsync(
        winrt::Windows::Storage::StorageFile file,
        std::vector<uint8_t> bgra, uint32_t width, uint32_t height)
    {
        std::vector<uint8_t> out;
        out.push_back(0); // TypeField = 0 (mono, no compression)
        out.push_back(0); // FixHeaderField

        // Multi-byte integer: 7 bits per byte, most-significant first, continuation bit on all but the last.
        const auto putMultiByte = [&out](uint32_t n)
        {
            uint8_t groups[5];
            int32_t count = 0;
            do { groups[count++] = static_cast<uint8_t>(n & 0x7F); n >>= 7; } while (n != 0);
            for (int32_t i = count - 1; i >= 0; --i)
            {
                out.push_back(static_cast<uint8_t>(groups[i] | (i > 0 ? 0x80 : 0x00)));
            }
        };
        putMultiByte(width);
        putMultiByte(height);

        // 1bpp rows, MSB-first, padded to a byte. WBMP: 0 = black, 1 = white.
        for (uint32_t y = 0; y < height; ++y)
        {
            uint8_t acc = 0;
            int32_t filled = 0;
            for (uint32_t x = 0; x < width; ++x)
            {
                const size_t i = (static_cast<size_t>(y) * width + x) * 4;
                const bool white = !IsDarkOpaque(bgra[i + 0], bgra[i + 1], bgra[i + 2], bgra[i + 3]);
                acc = static_cast<uint8_t>((acc << 1) | (white ? 1u : 0u));
                if (++filled == 8) { out.push_back(acc); acc = 0; filled = 0; }
            }
            if (filled != 0) { out.push_back(static_cast<uint8_t>(acc << (8 - filled))); }
        }

        co_await winrt::Windows::Storage::FileIO::WriteBytesAsync(file, out);
    }
}
