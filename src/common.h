// common.h — shared types, helpers, logging, timing and RNG for Daemon.
#pragma once

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <windowsx.h>
#include <d2d1.h>
#include <d2d1helper.h>
#include <dwrite.h>

#include <cstdint>
#include <cstdio>
#include <cmath>
#include <vector>
#include <string>
#include <random>
#include <algorithm>
using std::min;
using std::max;

// ---- COM / HRESULT helpers -------------------------------------------------

template <class T>
inline void SafeRelease(T** pp) {
    if (*pp) { (*pp)->Release(); *pp = nullptr; }
}

// A COM pointer that releases itself. Minimal; enough for our use.
template <class T>
struct ComPtr {
    T* p = nullptr;
    ComPtr() = default;
    ~ComPtr() { if (p) p->Release(); }
    ComPtr(const ComPtr&) = delete;
    ComPtr& operator=(const ComPtr&) = delete;
    ComPtr(ComPtr&& o) noexcept : p(o.p) { o.p = nullptr; }
    ComPtr& operator=(ComPtr&& o) noexcept {
        if (this != &o) { if (p) p->Release(); p = o.p; o.p = nullptr; }
        return *this;
    }
    T* operator->() const { return p; }
    T** operator&() { return &p; }   // for CreateXxx(&ptr)
    operator T*() const { return p; }
    void reset() { if (p) { p->Release(); p = nullptr; } }
    bool valid() const { return p != nullptr; }
};

// ---- Logging ---------------------------------------------------------------
// Set env DAEMON_LOG=path to get a trace. Cheap no-op otherwise.
void LogInit();
void LogLine(const char* fmt, ...);

// ---- Timing ----------------------------------------------------------------
struct Clock {
    LARGE_INTEGER freq{}, start{};
    void Reset() {
        QueryPerformanceFrequency(&freq);
        QueryPerformanceCounter(&start);
    }
    // Seconds since Reset().
    double Now() const {
        LARGE_INTEGER n; QueryPerformanceCounter(&n);
        return double(n.QuadPart - start.QuadPart) / double(freq.QuadPart);
    }
};

// ---- RNG -------------------------------------------------------------------
// One global stream seeded per run (QPC, or DAEMON_SEED for reproducibility).
extern std::mt19937 g_rng;
void SeedRng();                       // reads DAEMON_SEED if present
inline int   RInt(int lo, int hi)     { return std::uniform_int_distribution<int>(lo, hi)(g_rng); }
inline double RFlt(double lo, double hi){ return std::uniform_real_distribution<double>(lo, hi)(g_rng); }
inline bool  RChance(double p)        { return std::uniform_real_distribution<double>(0.0, 1.0)(g_rng) < p; }
template <class T> void RShuffle(std::vector<T>& v) { std::shuffle(v.begin(), v.end(), g_rng); }

// ---- CPU-side image (top-down BGRA, premultiplied where it matters) --------
struct Image {
    int w = 0, h = 0;
    std::vector<uint32_t> px;         // 0xAARRGGBB in memory as BGRA little-endian
    bool valid() const { return w > 0 && h > 0 && (int)px.size() == w * h; }
    void alloc(int ww, int hh) { w = ww; h = hh; px.assign(size_t(ww) * hh, 0); }
    uint32_t  get(int x, int y) const { return px[size_t(y) * w + x]; }
    uint32_t& at(int x, int y)        { return px[size_t(y) * w + x]; }
};

// ---- Easing ----------------------------------------------------------------
inline double Clamp01(double t) { return t < 0 ? 0 : (t > 1 ? 1 : t); }
inline double EaseInOut(double t) { t = Clamp01(t); return t * t * (3 - 2 * t); }
inline double EaseOut(double t)   { t = Clamp01(t); return 1 - (1 - t) * (1 - t); }
inline double EaseIn(double t)    { t = Clamp01(t); return t * t; }
inline double Lerp(double a, double b, double t) { return a + (b - a) * t; }

// ---- Geometry helpers ------------------------------------------------------
inline D2D1_RECT_F RectF(double l, double t, double r, double b) {
    return D2D1::RectF(float(l), float(t), float(r), float(b));
}
inline D2D1_RECT_F ToF(const RECT& r) {
    return D2D1::RectF(float(r.left), float(r.top), float(r.right), float(r.bottom));
}
inline int RW(const RECT& r) { return r.right - r.left; }
inline int RH(const RECT& r) { return r.bottom - r.top; }

// ---- Run options (parsed from the command line) ----------------------------
enum class Mode { FullScreen, Preview, Configure, Windowed };
struct Options {
    Mode mode = Mode::FullScreen;
    HWND parent = nullptr;            // preview/configure parent
    int  winW = 1280, winH = 720;     // windowed test size
    // Dev capture hooks:
    std::wstring capturePath;         // DAEMON_CAPTURE=path.bmp
    double captureAt = -1;            // DAEMON_CAPTURE_AT=seconds (then quit)
};
extern Options g_opt;

// ---- BMP writer (dev capture) ----------------------------------------------
bool SaveBmp(const wchar_t* path, const Image& img);
