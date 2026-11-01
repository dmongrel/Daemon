// renderer.h — Direct2D/DirectWrite device and helpers bound to the saver window.
#pragma once
#include "common.h"
#include <wincodec.h>

struct Renderer {
    ComPtr<ID2D1Factory>    factory;
    ComPtr<IDWriteFactory>  dwrite;
    ComPtr<IWICImagingFactory> wic;           // only used for offscreen capture
    ID2D1RenderTarget*      rt = nullptr;      // Hwnd target, or a WIC target in capture mode
    ID2D1HwndRenderTarget*  hwndRt = nullptr;  // non-null only on the on-screen path
    IWICBitmap*             wicBmp = nullptr;  // backing bitmap when capturing
    HWND hwnd = nullptr;
    UINT pxW = 0, pxH = 0;
    bool recreated = false;                   // set true when rt was (re)made
    bool captureMode = false;                 // render offscreen for a deterministic grab

    bool init(HWND h);
    void shutdown();
    bool ensureTarget();                      // create rt if needed
    void resize(UINT w, UINT h);
    bool saveCapture(const wchar_t* path);    // write the offscreen frame to a BMP

    ID2D1Bitmap* createBitmap(const Image& img);  // caller releases

    // Shared solid brush; set colour per use.
    ID2D1SolidColorBrush* brush = nullptr;
    void setBrush(uint32_t argb, float a = 1.0f);

    ID2D1SolidColorBrush* tmpBrush(uint32_t argb, float a = 1.0f) { setBrush(argb, a); return brush; }
};

// Create a DirectWrite text format. Caller releases.
IDWriteTextFormat* MakeTextFormat(IDWriteFactory* dw, const wchar_t* family,
                                  float sizePx, bool bold = false);
