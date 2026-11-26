#include "image_decoder.h"

#include <Windows.h>
#include <wincodec.h>
#include <wrl/client.h>
#include <algorithm>

using Microsoft::WRL::ComPtr;

namespace menu::model
{
    bool decode_image(const std::uint8_t* data, std::size_t size, std::uint32_t max_size, image& out)
    {
        ComPtr<IWICImagingFactory> factory;
        if (FAILED(CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&factory))))
            return false;

        ComPtr<IWICStream> stream;
        if (FAILED(factory->CreateStream(&stream)))
            return false;
        if (FAILED(stream->InitializeFromMemory(const_cast<BYTE*>(data), static_cast<DWORD>(size))))
            return false;

        ComPtr<IWICBitmapDecoder> decoder;
        if (FAILED(factory->CreateDecoderFromStream(stream.Get(), nullptr, WICDecodeMetadataCacheOnDemand, &decoder)))
            return false;

        ComPtr<IWICBitmapFrameDecode> frame;
        if (FAILED(decoder->GetFrame(0, &frame)))
            return false;

        ComPtr<IWICFormatConverter> converter;
        if (FAILED(factory->CreateFormatConverter(&converter)))
            return false;
        if (FAILED(converter->Initialize(frame.Get(), GUID_WICPixelFormat32bppRGBA, WICBitmapDitherTypeNone, nullptr, 0.0, WICBitmapPaletteTypeCustom)))
            return false;

        UINT width = 0;
        UINT height = 0;
        converter->GetSize(&width, &height);
        if (width == 0 || height == 0)
            return false;

        ComPtr<IWICBitmapSource> source = converter;
        const UINT largest = (std::max)(width, height);
        if (largest > max_size)
        {
            const double scale = static_cast<double>(max_size) / static_cast<double>(largest);
            const UINT scaled_width = (std::max)(1u, static_cast<UINT>(width * scale));
            const UINT scaled_height = (std::max)(1u, static_cast<UINT>(height * scale));

            ComPtr<IWICBitmapScaler> scaler;
            if (FAILED(factory->CreateBitmapScaler(&scaler)))
                return false;
            if (FAILED(scaler->Initialize(converter.Get(), scaled_width, scaled_height, WICBitmapInterpolationModeHighQualityCubic)))
                return false;

            source = scaler;
            width = scaled_width;
            height = scaled_height;
        }

        out.width = width;
        out.height = height;
        out.pixels.resize(static_cast<std::size_t>(width) * height * 4);
        if (FAILED(source->CopyPixels(nullptr, width * 4, static_cast<UINT>(out.pixels.size()), out.pixels.data())))
            return false;

        const auto* texels = reinterpret_cast<const std::uint32_t*>(out.pixels.data());
        const std::size_t count = static_cast<std::size_t>(width) * height;
        if (std::all_of(texels, texels + count, [first = texels[0]](std::uint32_t t) { return t == first; }))
        {
            out.width = 1;
            out.height = 1;
            out.pixels.resize(4);
            out.pixels.shrink_to_fit();
        }
        return true;
    }
}
