// capture.cpp — grab the screenshot, wallpaper, icon geometry and window list.
#include "capture.h"

#include <objidl.h>
#include <shellapi.h>
#include <shlobj.h>
#include <shobjidl.h>
#include <dwmapi.h>
#include <commctrl.h>

// GDI+ uses unqualified min/max; NOMINMAX removed the macros, so bring in the
// std versions at global scope before the header, which resolves them.
#include <algorithm>
using std::min;
using std::max;
#include <gdiplus.h>

// ---- a top-down 32bpp DIB with its own memory DC ---------------------------
namespace {

struct Dib {
    HBITMAP bmp = nullptr;
    HDC     dc  = nullptr;
    void*   bits = nullptr;
    int     w = 0, h = 0;

    bool create(int ww, int hh) {
        destroy();
        BITMAPINFO bi{};
        bi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
        bi.bmiHeader.biWidth = ww;
        bi.bmiHeader.biHeight = -hh;              // top-down
        bi.bmiHeader.biPlanes = 1;
        bi.bmiHeader.biBitCount = 32;
        bi.bmiHeader.biCompression = BI_RGB;
        HDC screen = GetDC(nullptr);
        bmp = CreateDIBSection(screen, &bi, DIB_RGB_COLORS, &bits, nullptr, 0);
        ReleaseDC(nullptr, screen);
        if (!bmp) return false;
        dc = CreateCompatibleDC(nullptr);
        SelectObject(dc, bmp);
        w = ww; h = hh;
        return true;
    }
    void destroy() {
        if (dc)  { DeleteDC(dc); dc = nullptr; }
        if (bmp) { DeleteObject(bmp); bmp = nullptr; }
        bits = nullptr; w = h = 0;
    }
    ~Dib() { destroy(); }
    uint32_t* row(int y) { return reinterpret_cast<uint32_t*>(bits) + size_t(y) * w; }

    void toImage(Image& img) const {
        img.alloc(w, h);
        memcpy(img.px.data(), bits, size_t(w) * h * 4);
    }
};

// ---- monitors --------------------------------------------------------------
BOOL CALLBACK monProc(HMONITOR hm, HDC, LPRECT, LPARAM lp) {
    auto* v = reinterpret_cast<std::vector<MonitorRec>*>(lp);
    MONITORINFO mi{}; mi.cbSize = sizeof(mi);
    if (GetMonitorInfo(hm, &mi)) {
        v->push_back({ mi.rcMonitor, (mi.dwFlags & MONITORINFOF_PRIMARY) != 0 });
    }
    return TRUE;
}

// ---- wallpaper layout ------------------------------------------------------
enum class WPos { Center, Tile, Stretch, Fit, Fill, Span };

void drawLayout(Gdiplus::Graphics& g, Gdiplus::Bitmap* img,
                const RECT& dest, WPos pos, int imgW, int imgH) {
    using namespace Gdiplus;
    const double dw = RW(dest), dh = RH(dest);
    const double iw = imgW, ih = imgH;
    if (iw <= 0 || ih <= 0) return;

    if (pos == WPos::Tile) {
        for (double y = dest.top; y < dest.bottom; y += ih)
            for (double x = dest.left; x < dest.right; x += iw)
                g.DrawImage(img, (REAL)x, (REAL)y, (REAL)iw, (REAL)ih);
        return;
    }
    double tw = dw, th = dh, tx = dest.left, ty = dest.top;
    if (pos == WPos::Center) {
        tw = iw; th = ih;
    } else if (pos == WPos::Fit) {
        double s = min(dw / iw, dh / ih); tw = iw * s; th = ih * s;
    } else if (pos == WPos::Fill || pos == WPos::Span) {
        double s = max(dw / iw, dh / ih); tw = iw * s; th = ih * s;
    } // Stretch: tw=dw, th=dh
    tx = dest.left + (dw - tw) / 2.0;
    ty = dest.top  + (dh - th) / 2.0;
    g.DrawImage(img, (REAL)tx, (REAL)ty, (REAL)tw, (REAL)th);
}

WPos posFromDWPOS(int p) {
    switch (p) {
        case DWPOS_CENTER:  return WPos::Center;
        case DWPOS_TILE:    return WPos::Tile;
        case DWPOS_STRETCH: return WPos::Stretch;
        case DWPOS_FIT:     return WPos::Fit;
        case DWPOS_FILL:    return WPos::Fill;
        case DWPOS_SPAN:    return WPos::Span;
    }
    return WPos::Fill;
}

void fillRect(Dib& dib, const RECT& r, uint32_t argb) {
    int x0 = max(0, (int)r.left),  y0 = max(0, (int)r.top);
    int x1 = min(dib.w, (int)r.right), y1 = min(dib.h, (int)r.bottom);
    for (int y = y0; y < y1; ++y) {
        uint32_t* p = dib.row(y);
        for (int x = x0; x < x1; ++x) p[x] = argb;
    }
}

void buildWallpaper(Dib& dib, const Capture& cap) {
    using namespace Gdiplus;
    // Default: dark grey everywhere, in case nothing loads.
    for (int y = 0; y < dib.h; ++y) {
        uint32_t* p = dib.row(y);
        for (int x = 0; x < dib.w; ++x) p[x] = 0xFF101014;
    }
    Graphics g(dib.dc);
    g.SetInterpolationMode(InterpolationModeHighQualityBicubic);
    g.SetPixelOffsetMode(PixelOffsetModeHalf);

    ComPtr<IDesktopWallpaper> dw;
    bool used = false;
    if (SUCCEEDED(CoCreateInstance(CLSID_DesktopWallpaper, nullptr, CLSCTX_ALL,
                                   IID_PPV_ARGS(&dw))) && dw.valid()) {
        DESKTOP_WALLPAPER_POSITION pos = DWPOS_FILL;
        dw->GetPosition(&pos);
        COLORREF bg = 0;
        if (SUCCEEDED(dw->GetBackgroundColor(&bg))) {
            uint32_t argb = 0xFF000000 | (GetRValue(bg) << 16) |
                            (GetGValue(bg) << 8) | GetBValue(bg);
            RECT all{ 0, 0, dib.w, dib.h };
            fillRect(dib, all, argb);
        }
        UINT cnt = 0;
        if (SUCCEEDED(dw->GetMonitorDevicePathCount(&cnt)) && cnt > 0) {
            // SPAN: one image across the whole virtual desktop.
            if (pos == DWPOS_SPAN) {
                LPWSTR path = nullptr;
                LPWSTR id = nullptr;
                dw->GetMonitorDevicePathAt(0, &id);
                if (id && SUCCEEDED(dw->GetWallpaper(id, &path)) && path && *path) {
                    Bitmap img(path);
                    if (img.GetLastStatus() == Ok) {
                        RECT all{ 0, 0, dib.w, dib.h };
                        drawLayout(g, &img, all, WPos::Span, img.GetWidth(), img.GetHeight());
                        used = true;
                    }
                }
                if (path) CoTaskMemFree(path);
                if (id) CoTaskMemFree(id);
            } else {
                for (UINT i = 0; i < cnt; ++i) {
                    LPWSTR id = nullptr, path = nullptr;
                    if (FAILED(dw->GetMonitorDevicePathAt(i, &id)) || !id) continue;
                    RECT mr{};
                    HRESULT hrR = dw->GetMonitorRECT(id, &mr);
                    if (SUCCEEDED(dw->GetWallpaper(id, &path)) && path && *path &&
                        SUCCEEDED(hrR)) {
                        Bitmap img(path);
                        if (img.GetLastStatus() == Ok) {
                            RECT d{ mr.left - cap.vx, mr.top - cap.vy,
                                    mr.right - cap.vx, mr.bottom - cap.vy };
                            drawLayout(g, &img, d, posFromDWPOS(pos),
                                       img.GetWidth(), img.GetHeight());
                            used = true;
                        }
                    }
                    if (path) CoTaskMemFree(path);
                    if (id) CoTaskMemFree(id);
                }
            }
        }
    }

    if (!used) {
        // Fallback: SPI path + registry style, applied to every monitor.
        wchar_t path[MAX_PATH]{};
        if (SystemParametersInfoW(SPI_GETDESKWALLPAPER, MAX_PATH, path, 0) && path[0]) {
            WPos wp = WPos::Fill; bool tile = false;
            HKEY k;
            if (RegOpenKeyExW(HKEY_CURRENT_USER, L"Control Panel\\Desktop", 0,
                              KEY_QUERY_VALUE, &k) == ERROR_SUCCESS) {
                wchar_t v[16]; DWORD sz = sizeof(v), ty = 0;
                if (RegQueryValueExW(k, L"WallpaperStyle", nullptr, &ty,
                                     (LPBYTE)v, &sz) == ERROR_SUCCESS) {
                    int s = _wtoi(v);
                    if (s == 0) wp = WPos::Center;
                    else if (s == 2) wp = WPos::Stretch;
                    else if (s == 6) wp = WPos::Fit;
                    else if (s == 10) wp = WPos::Fill;
                    else if (s == 22) wp = WPos::Span;
                }
                sz = sizeof(v);
                if (RegQueryValueExW(k, L"TileWallpaper", nullptr, &ty,
                                     (LPBYTE)v, &sz) == ERROR_SUCCESS && _wtoi(v))
                    tile = true;
                RegCloseKey(k);
            }
            if (tile) wp = WPos::Tile;
            Bitmap img(path);
            if (img.GetLastStatus() == Ok) {
                if (wp == WPos::Span) {
                    RECT all{ 0, 0, dib.w, dib.h };
                    drawLayout(g, &img, all, WPos::Span, img.GetWidth(), img.GetHeight());
                } else {
                    for (auto& m : cap.monitors) {
                        RECT d{ m.rc.left - cap.vx, m.rc.top - cap.vy,
                                m.rc.right - cap.vx, m.rc.bottom - cap.vy };
                        drawLayout(g, &img, d, wp, img.GetWidth(), img.GetHeight());
                    }
                }
            }
        }
    }
    g.Flush();
    // DIB alpha from GDI+ is unreliable; force opaque.
    for (int y = 0; y < dib.h; ++y) {
        uint32_t* p = dib.row(y);
        for (int x = 0; x < dib.w; ++x) p[x] |= 0xFF000000;
    }
}

// ---- desktop icon view -----------------------------------------------------
HWND findDefView() {
    HWND progman = FindWindowW(L"Progman", nullptr);
    HWND defview = FindWindowExW(progman, nullptr, L"SHELLDLL_DefView", nullptr);
    if (defview) return defview;
    // Wallpaper slideshow puts the icons under a WorkerW.
    HWND worker = nullptr;
    while ((worker = FindWindowExW(nullptr, worker, L"WorkerW", nullptr)) != nullptr) {
        HWND dv = FindWindowExW(worker, nullptr, L"SHELLDLL_DefView", nullptr);
        if (dv) return dv;
    }
    return nullptr;
}

// Cross-process query of the desktop listview's item rectangles.
void getIconRects(HWND defview, const Capture& cap, std::vector<RECT>& out) {
    HWND lv = FindWindowExW(defview, nullptr, L"SysListView32", nullptr);
    if (!lv) return;
    DWORD pid = 0; GetWindowThreadProcessId(lv, &pid);
    HANDLE hp = OpenProcess(PROCESS_VM_OPERATION | PROCESS_VM_READ |
                            PROCESS_VM_WRITE, FALSE, pid);
    if (!hp) return;
    int count = (int)SendMessageW(lv, LVM_GETITEMCOUNT, 0, 0);
    if (count <= 0 || count > 4096) { CloseHandle(hp); return; }
    LPVOID remote = VirtualAllocEx(hp, nullptr, sizeof(RECT), MEM_COMMIT, PAGE_READWRITE);
    if (!remote) { CloseHandle(hp); return; }

    POINT clientOrg{ 0, 0 };
    ClientToScreen(lv, &clientOrg);               // listview client -> screen

    for (int i = 0; i < count; ++i) {
        RECT r{}; r.left = LVIR_BOUNDS;           // request kind in .left
        SIZE_T nw = 0;
        WriteProcessMemory(hp, remote, &r, sizeof(r), &nw);
        if (!SendMessageW(lv, LVM_GETITEMRECT, i, (LPARAM)remote)) continue;
        ReadProcessMemory(hp, remote, &r, sizeof(r), &nw);
        RECT scr{ r.left + clientOrg.x, r.top + clientOrg.y,
                  r.right + clientOrg.x, r.bottom + clientOrg.y };
        out.push_back({ scr.left - cap.vx, scr.top - cap.vy,
                        scr.right - cap.vx, scr.bottom - cap.vy });
    }
    VirtualFreeEx(hp, remote, 0, MEM_RELEASE);
    CloseHandle(hp);
}

// Composite the icon view (icons on black) onto the wallpaper by keying black
// out: bright pixels (icon art, labels) win, dark pixels stay as wallpaper.
void keyIcons(Dib& desk, const Dib& view, int originX, int originY) {
    for (int y = 0; y < view.h; ++y) {
        int dy = y + originY;
        if (dy < 0 || dy >= desk.h) continue;
        const uint32_t* s = reinterpret_cast<const uint32_t*>(view.bits) + size_t(y) * view.w;
        uint32_t* d = desk.row(dy);
        for (int x = 0; x < view.w; ++x) {
            int dx = x + originX;
            if (dx < 0 || dx >= desk.w) continue;
            uint32_t sp = s[x];
            int b = sp & 0xFF, gg = (sp >> 8) & 0xFF, r = (sp >> 16) & 0xFF;
            int lum = (r * 54 + gg * 183 + b * 19) >> 8;      // 0..255
            if (lum < 10) continue;                            // black -> wallpaper
            double a = lum < 40 ? lum / 40.0 : 1.0;            // soften edges
            uint32_t dp = d[dx];
            int db = dp & 0xFF, dg2 = (dp >> 8) & 0xFF, dr = (dp >> 16) & 0xFF;
            int ob = int(b * a + db * (1 - a));
            int og = int(gg * a + dg2 * (1 - a));
            int orr = int(r * a + dr * (1 - a));
            d[dx] = 0xFF000000 | (orr << 16) | (og << 8) | ob;
        }
    }
}

void buildDesktop(Dib& desk, const Dib& wall, const Capture& cap, HWND defview) {
    memcpy(desk.bits, wall.bits, size_t(wall.w) * wall.h * 4);
    if (!defview) return;
    RECT cr{}; GetClientRect(defview, &cr);
    int vw = RW(cr), vh = RH(cr);
    if (vw <= 0 || vh <= 0) return;
    POINT org{ 0, 0 }; ClientToScreen(defview, &org);
    Dib view;
    if (!view.create(vw, vh)) return;
    // PW_RENDERFULLCONTENT = 2; draws even when covered by other windows.
    if (!PrintWindow(defview, view.dc, 2)) {
        // older path: without the flag it still usually renders the icons
        PrintWindow(defview, view.dc, 0);
    }
    keyIcons(desk, view, org.x - cap.vx, org.y - cap.vy);
}

// ---- window list -----------------------------------------------------------
bool isCloaked(HWND h) {
    int cloaked = 0;
    DwmGetWindowAttribute(h, DWMWA_CLOAKED, &cloaked, sizeof(cloaked));
    return cloaked != 0;
}
RECT frameBounds(HWND h) {
    RECT r{};
    if (FAILED(DwmGetWindowAttribute(h, DWMWA_EXTENDED_FRAME_BOUNDS, &r, sizeof(r))))
        GetWindowRect(h, &r);
    return r;
}

BOOL CALLBACK winProc(HWND h, LPARAM lp) {
    auto* cap = reinterpret_cast<Capture*>(lp);
    if (!IsWindowVisible(h) || IsIconic(h)) return TRUE;
    if (isCloaked(h)) return TRUE;

    wchar_t cls[64]{};
    GetClassNameW(h, cls, 63);
    if (!wcscmp(cls, L"Progman") || !wcscmp(cls, L"WorkerW") ||
        !wcscmp(cls, L"Shell_TrayWnd") || !wcscmp(cls, L"Shell_SecondaryTrayWnd") ||
        !wcscmp(cls, L"Windows.UI.Core.CoreWindow") || !wcscmp(cls, L"DaemonClass"))
        return TRUE;

    LONG ex = GetWindowLong(h, GWL_EXSTYLE);
    if ((ex & WS_EX_TOOLWINDOW) && !(ex & WS_EX_APPWINDOW)) return TRUE;

    RECT r = frameBounds(h);
    if (RW(r) < 40 || RH(r) < 40) return TRUE;
    RECT v{ cap->vx, cap->vy, cap->vx + cap->vw, cap->vy + cap->vh }, inter;
    if (!IntersectRect(&inter, &r, &v)) return TRUE;

    cap->windows.push_back({ r.left - cap->vx, r.top - cap->vy,
                             r.right - cap->vx, r.bottom - cap->vy });
    return TRUE;
}

void getTaskbars(Capture& cap) {
    APPBARDATA ab{}; ab.cbSize = sizeof(ab);
    if (SHAppBarMessage(ABM_GETTASKBARPOS, &ab) && cap.taskbarCount < 4) {
        cap.taskbars[cap.taskbarCount++] = RECT{ ab.rc.left - cap.vx, ab.rc.top - cap.vy,
                                                 ab.rc.right - cap.vx, ab.rc.bottom - cap.vy };
    }
    HWND sec = nullptr;
    while ((sec = FindWindowExW(nullptr, sec, L"Shell_SecondaryTrayWnd", nullptr)) &&
           cap.taskbarCount < 4) {
        RECT r; if (GetWindowRect(sec, &r))
            cap.taskbars[cap.taskbarCount++] = RECT{ r.left - cap.vx, r.top - cap.vy,
                                                     r.right - cap.vx, r.bottom - cap.vy };
    }
}

} // namespace

// ---- public entry ----------------------------------------------------------
bool DoCapture(Capture& cap) {
    ULONG_PTR gpToken = 0;
    Gdiplus::GdiplusStartupInput gpIn;
    Gdiplus::GdiplusStartup(&gpToken, &gpIn, nullptr);

    cap.vx = GetSystemMetrics(SM_XVIRTUALSCREEN);
    cap.vy = GetSystemMetrics(SM_YVIRTUALSCREEN);
    cap.vw = GetSystemMetrics(SM_CXVIRTUALSCREEN);
    cap.vh = GetSystemMetrics(SM_CYVIRTUALSCREEN);
    if (cap.vw <= 0 || cap.vh <= 0) {
        Gdiplus::GdiplusShutdown(gpToken);
        return false;
    }
    LogLine("virtual screen %d,%d %dx%d", cap.vx, cap.vy, cap.vw, cap.vh);

    EnumDisplayMonitors(nullptr, nullptr, monProc, (LPARAM)&cap.monitors);

    // Screenshot.
    Dib shot;
    if (!shot.create(cap.vw, cap.vh)) { Gdiplus::GdiplusShutdown(gpToken); return false; }
    {
        HDC screen = GetDC(nullptr);
        BitBlt(shot.dc, 0, 0, cap.vw, cap.vh, screen, cap.vx, cap.vy,
               SRCCOPY | CAPTUREBLT);
        ReleaseDC(nullptr, screen);
        for (int y = 0; y < shot.h; ++y) {       // force opaque
            uint32_t* p = shot.row(y);
            for (int x = 0; x < shot.w; ++x) p[x] |= 0xFF000000;
        }
    }
    shot.toImage(cap.screenshot);

    // Wallpaper.
    Dib wall;
    if (wall.create(cap.vw, cap.vh)) {
        buildWallpaper(wall, cap);
        wall.toImage(cap.wallpaper);
    } else {
        cap.wallpaper = cap.screenshot;           // degrade gracefully
    }

    // Icons + clean desktop.
    HWND defview = findDefView();
    if (defview) getIconRects(defview, cap, cap.icons);
    LogLine("icons=%zu windows(pre)", cap.icons.size());

    Dib desk;
    if (wall.bmp && desk.create(cap.vw, cap.vh)) {
        buildDesktop(desk, wall, cap, defview);
        desk.toImage(cap.desktop);
    } else {
        cap.desktop = cap.wallpaper;
    }

    // Windows (topmost-first from EnumWindows; store reversed = back-to-front).
    EnumWindows(winProc, (LPARAM)&cap);
    std::reverse(cap.windows.begin(), cap.windows.end());
    LogLine("windows=%zu", cap.windows.size());

    getTaskbars(cap);
    LogLine("taskbars=%d", cap.taskbarCount);

    Gdiplus::GdiplusShutdown(gpToken);
    return true;
}
