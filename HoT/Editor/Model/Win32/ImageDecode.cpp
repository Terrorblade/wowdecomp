#include "HoT/Editor/Model/ImageDecode.h"
#include <cstdio>
#include <windows.h>
#include <wincodec.h>

namespace HoT::Editor::Model::TextureImport {

template <class T>
static void SafeRelease(T*& object) {
    if (object) {
        object->Release();
        object = nullptr;
    }
}

bool ReadDiskFile(const std::wstring& path, std::vector<uint8_t>& data) {
    FILE* file = _wfopen(path.c_str(), L"rb");

    if (!file) {
        return false;
    }

    fseek(file, 0, SEEK_END);
    long size = ftell(file);
    fseek(file, 0, SEEK_SET);
    bool ok = size >= 0 && size < 0x10000000;

    if (ok) {
        data.resize(static_cast<size_t>(size));
        ok = !size || fread(data.data(), 1, data.size(), file) == data.size();
    }

    fclose(file);
    return ok;
}

bool DecodeImageFile(const std::wstring& path, Image& image, std::string& error) {
    HRESULT init = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    bool uninitialize = SUCCEEDED(init);
    IWICImagingFactory* factory = nullptr;
    IWICBitmapDecoder* decoder = nullptr;
    IWICBitmapFrameDecode* frame = nullptr;
    IWICFormatConverter* converter = nullptr;
    bool ok = false;

    HRESULT hr = CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&factory));

    if (SUCCEEDED(hr)) {
        hr = factory->CreateDecoderFromFilename(path.c_str(), nullptr, GENERIC_READ, WICDecodeMetadataCacheOnDemand, &decoder);
    }

    if (SUCCEEDED(hr)) {
        hr = decoder->GetFrame(0, &frame);
    }

    if (SUCCEEDED(hr)) {
        hr = factory->CreateFormatConverter(&converter);
    }

    if (SUCCEEDED(hr)) {
        hr = converter->Initialize(frame, GUID_WICPixelFormat32bppBGRA, WICBitmapDitherTypeNone, nullptr, 0.0, WICBitmapPaletteTypeCustom);
    }

    UINT width = 0;
    UINT height = 0;

    if (SUCCEEDED(hr)) {
        hr = converter->GetSize(&width, &height);
    }

    if (SUCCEEDED(hr)) {
        if (!width || !height || width > MAX_SOURCE || height > MAX_SOURCE) {
            error = "image size is not supported";
        } else {
            image.width = width;
            image.height = height;
            image.bgra.resize(static_cast<size_t>(width) * height * 4);
            hr = converter->CopyPixels(nullptr, width * 4, static_cast<UINT>(image.bgra.size()), image.bgra.data());
            ok = SUCCEEDED(hr);
        }
    }

    if (!ok && error.empty()) {
        error = "the image could not be decoded";
    }

    SafeRelease(converter);
    SafeRelease(frame);
    SafeRelease(decoder);
    SafeRelease(factory);

    if (uninitialize) {
        CoUninitialize();
    }

    return ok;
}

}
