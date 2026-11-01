// common.cpp — logging, RNG seeding, BMP capture, global options.
#include "common.h"
#include <cstdarg>

std::mt19937 g_rng;
Options      g_opt;

static FILE* g_log = nullptr;

void LogInit() {
    wchar_t buf[MAX_PATH];
    DWORD n = GetEnvironmentVariableW(L"DAEMON_LOG", buf, MAX_PATH);
    if (n > 0 && n < MAX_PATH) {
        g_log = _wfopen(buf, L"w");
    }
}

void LogLine(const char* fmt, ...) {
    if (!g_log) return;
    va_list ap; va_start(ap, fmt);
    vfprintf(g_log, fmt, ap);
    va_end(ap);
    fputc('\n', g_log);
    fflush(g_log);
}

void SeedRng() {
    wchar_t buf[64];
    DWORD n = GetEnvironmentVariableW(L"DAEMON_SEED", buf, 64);
    unsigned seed;
    if (n > 0 && n < 64) {
        seed = (unsigned)wcstoul(buf, nullptr, 10);
    } else {
        LARGE_INTEGER q; QueryPerformanceCounter(&q);
        seed = (unsigned)(q.QuadPart ^ (q.QuadPart >> 32) ^ GetTickCount());
    }
    g_rng.seed(seed);
    LogLine("seed=%u", seed);
}

// 32-bit top-down BGRA -> 24-bit BMP (bottom-up). Dev-only; small and simple.
bool SaveBmp(const wchar_t* path, const Image& img) {
    if (!img.valid()) return false;
    const int w = img.w, h = img.h;
    const int rowbytes = (w * 3 + 3) & ~3;
    const uint32_t dataSize = uint32_t(rowbytes) * h;

#pragma pack(push, 1)
    struct { uint16_t bfType; uint32_t bfSize; uint16_t r1, r2; uint32_t bfOff; } fh{};
    struct { uint32_t sz; int32_t w, h; uint16_t planes, bpp; uint32_t comp, img;
             int32_t xppm, yppm; uint32_t clr, imp; } ih{};
#pragma pack(pop)
    fh.bfType = 0x4D42;
    fh.bfOff  = sizeof(fh) + sizeof(ih);
    fh.bfSize = fh.bfOff + dataSize;
    ih.sz = sizeof(ih); ih.w = w; ih.h = h; ih.planes = 1; ih.bpp = 24;

    FILE* f = _wfopen(path, L"wb");
    if (!f) return false;
    fwrite(&fh, sizeof(fh), 1, f);
    fwrite(&ih, sizeof(ih), 1, f);
    std::vector<uint8_t> row(rowbytes, 0);
    for (int y = h - 1; y >= 0; --y) {              // bottom-up
        for (int x = 0; x < w; ++x) {
            uint32_t p = img.get(x, y);             // 0xAARRGGBB
            row[x * 3 + 0] = uint8_t(p & 0xFF);         // B
            row[x * 3 + 1] = uint8_t((p >> 8) & 0xFF);  // G
            row[x * 3 + 2] = uint8_t((p >> 16) & 0xFF); // R
        }
        fwrite(row.data(), rowbytes, 1, f);
    }
    fclose(f);
    return true;
}
