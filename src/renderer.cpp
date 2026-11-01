// renderer.cpp — Direct2D device lifecycle and small helpers.
#include "renderer.h"
#include <dxgiformat.h>

bool Renderer::init(HWND h) {
    hwnd = h;
    if (FAILED(D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED, &factory)))
        return false;
    if (FAILED(DWriteCreateFactory(DWRITE_FACTORY_TYPE_SHARED,
                                   __uuidof(IDWriteFactory),
                                   reinterpret_cast<IUnknown**>(&dwrite))))
        return false;
    return true;
}

void Renderer::shutdown() {
    SafeRelease(&brush);
    SafeRelease(&wicBmp);
    hwndRt = nullptr;
    SafeRelease(&rt);
}

bool Renderer::ensureTarget() {
    recreated = false;
    if (rt) return true;
    RECT rc; GetClientRect(hwnd, &rc);
    pxW = max(1L, rc.right - rc.left);
    pxH = max(1L, rc.bottom - rc.top);
    D2D1_RENDER_TARGET_PROPERTIES props = D2D1::RenderTargetProperties(
        D2D1_RENDER_TARGET_TYPE_DEFAULT,
        D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_PREMULTIPLIED),
        96.0f, 96.0f);                         // 1 DIP == 1 pixel

    if (captureMode) {
        // Offscreen WIC target: a frame grabbed from this is deterministic and does
        // not depend on the window being composited by the DWM.
        if (!wic && FAILED(CoCreateInstance(CLSID_WICImagingFactory, nullptr,
                CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&wic))))
            return false;
        SafeRelease(&wicBmp);
        if (FAILED(wic->CreateBitmap(pxW, pxH, GUID_WICPixelFormat32bppPBGRA,
                WICBitmapCacheOnLoad, &wicBmp)))
            return false;
        if (FAILED(factory->CreateWicBitmapRenderTarget(wicBmp, props, &rt))) {
            rt = nullptr;
            return false;
        }
    } else {
        D2D1_HWND_RENDER_TARGET_PROPERTIES hprops = D2D1::HwndRenderTargetProperties(
            hwnd, D2D1::SizeU(pxW, pxH), D2D1_PRESENT_OPTIONS_IMMEDIATELY);
        if (FAILED(factory->CreateHwndRenderTarget(props, hprops, &hwndRt))) {
            hwndRt = nullptr;
            return false;
        }
        rt = hwndRt;
    }
    rt->CreateSolidColorBrush(D2D1::ColorF(0, 0, 0), &brush);
    recreated = true;
    return true;
}

void Renderer::resize(UINT w, UINT h) {
    pxW = max(1u, w); pxH = max(1u, h);
    if (hwndRt) hwndRt->Resize(D2D1::SizeU(pxW, pxH));
}

bool Renderer::saveCapture(const wchar_t* path) {
    if (!wicBmp) return false;
    WICRect lock{ 0, 0, (INT)pxW, (INT)pxH };
    IWICBitmapLock* bl = nullptr;
    if (FAILED(wicBmp->Lock(&lock, WICBitmapLockRead, &bl)) || !bl) return false;
    UINT cb = 0; BYTE* data = nullptr; UINT stride = 0;
    bl->GetStride(&stride);
    bl->GetDataPointer(&cb, &data);
    Image img; img.alloc(pxW, pxH);
    for (UINT y = 0; y < pxH; ++y) {
        const uint32_t* src = (const uint32_t*)(data + size_t(y) * stride);
        for (UINT x = 0; x < pxW; ++x) img.px[size_t(y) * pxW + x] = src[x] | 0xFF000000u;
    }
    bl->Release();
    SaveBmp(path, img);
    return true;
}

ID2D1Bitmap* Renderer::createBitmap(const Image& img) {
    if (!rt || !img.valid()) return nullptr;
    D2D1_BITMAP_PROPERTIES bp = D2D1::BitmapProperties(
        D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_PREMULTIPLIED),
        96.0f, 96.0f);
    ID2D1Bitmap* bmp = nullptr;
    rt->CreateBitmap(D2D1::SizeU(img.w, img.h), img.px.data(),
                     img.w * 4, bp, &bmp);
    return bmp;
}

void Renderer::setBrush(uint32_t argb, float a) {
    if (!brush) return;
    float r = ((argb >> 16) & 0xFF) / 255.0f;
    float g = ((argb >> 8) & 0xFF) / 255.0f;
    float b = (argb & 0xFF) / 255.0f;
    float base = ((argb >> 24) & 0xFF) / 255.0f;
    if (((argb >> 24) & 0xFF) == 0) base = 1.0f;  // 0xRRGGBB == opaque; `a` is the opacity
    brush->SetColor(D2D1::ColorF(r, g, b, base * a));
}

IDWriteTextFormat* MakeTextFormat(IDWriteFactory* dw, const wchar_t* family,
                                  float sizePx, bool bold) {
    IDWriteTextFormat* fmt = nullptr;
    dw->CreateTextFormat(family, nullptr,
        bold ? DWRITE_FONT_WEIGHT_BOLD : DWRITE_FONT_WEIGHT_NORMAL,
        DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL,
        sizePx, L"", &fmt);
    return fmt;
}
