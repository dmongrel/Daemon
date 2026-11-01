// main.cpp — entry point, argument parsing, window and message loop.
#include "common.h"
#include "capture.h"
#include "renderer.h"
#include "show.h"
#include <objbase.h>
#include <shellapi.h>

static Renderer g_r;
static Show     g_show;
static Capture  g_cap;
static Clock    g_clock;
static bool     g_captured = false;
static bool     g_moveSeen = false;
static POINT    g_moveOrigin{};

// ---- argument parsing ------------------------------------------------------
static void ParseArgs() {
    int argc = 0;
    PWSTR* argv = CommandLineToArgvW(GetCommandLineW(), &argc);
    g_opt.mode = Mode::FullScreen;
    if (argv && argc > 1) {
        PWSTR a = argv[1];
        if (*a == L'/' || *a == L'-') ++a;
        wchar_t c = (wchar_t)towlower(*a);
        if (c == L's') {
            g_opt.mode = Mode::FullScreen;
        } else if (c == L'w') {
            g_opt.mode = Mode::Windowed;
        } else if (c == L'p') {
            g_opt.mode = Mode::Preview;
            ++a; if (*a == L':') ++a;
            PWSTR h = (*a) ? a : (argc > 2 ? argv[2] : nullptr);
            if (h) g_opt.parent = (HWND)(UINT_PTR)wcstoull(h, nullptr, 10);
            if (!g_opt.parent || !IsWindow(g_opt.parent)) { LocalFree(argv); ExitProcess(0); }
        } else if (c == L'c') {
            g_opt.mode = Mode::Configure;
            ++a; if (*a == L':') ++a;
            PWSTR h = (*a) ? a : (argc > 2 ? argv[2] : nullptr);
            if (h) g_opt.parent = (HWND)(UINT_PTR)wcstoull(h, nullptr, 10);
        }
    }
    if (argv) LocalFree(argv);

    // Dev capture hooks.
    wchar_t buf[MAX_PATH];
    if (GetEnvironmentVariableW(L"DAEMON_CAPTURE", buf, MAX_PATH)) g_opt.capturePath = buf;
    if (GetEnvironmentVariableW(L"DAEMON_CAPTURE_AT", buf, 64))    g_opt.captureAt = _wtof(buf);
}

// ---- capture-to-BMP (dev) --------------------------------------------------
static void CaptureFrame() {
    if (g_captured || g_opt.capturePath.empty()) return;
    // The frame was drawn into an offscreen WIC bitmap (Renderer::captureMode), so the
    // grab is deterministic and needs no DWM composition of the window.
    g_r.saveCapture(g_opt.capturePath.c_str());
    g_captured = true;
    LogLine("captured frame at t=%.3f", g_show.elapsed());
    PostQuitMessage(0);
}

// ---- window procedure ------------------------------------------------------
static LRESULT CALLBACK WndProc(HWND h, UINT m, WPARAM wp, LPARAM lp) {
    const bool full = (g_opt.mode == Mode::FullScreen);
    switch (m) {
    case WM_TIMER: {
        double dt = g_opt.capturePath.empty() ? g_clock.Now() : (1.0 / 60.0);
        static double last = 0;
        if (g_opt.capturePath.empty()) { dt = g_clock.Now() - last; last = g_clock.Now(); }
        g_show.update(dt);
        InvalidateRect(h, nullptr, FALSE);
        return 0;
    }
    case WM_PAINT: {
        PAINTSTRUCT ps; BeginPaint(h, &ps);
        if (g_r.ensureTarget()) {
            if (g_r.recreated) g_show.onDevice(g_r);
            g_show.draw(g_r);
            if (g_opt.captureAt >= 0 && g_show.totalElapsed() >= g_opt.captureAt)
                CaptureFrame();
        }
        EndPaint(h, &ps);
        return 0;
    }
    case WM_SIZE:
        g_r.resize(LOWORD(lp), HIWORD(lp));
        return 0;
    case WM_ERASEBKGND:
        return 1;
    case WM_SETCURSOR:
        if (full) { SetCursor(nullptr); return TRUE; }
        return DefWindowProc(h, m, wp, lp);
    case WM_MOUSEMOVE:
        if (full) {
            int x = GET_X_LPARAM(lp), y = GET_Y_LPARAM(lp);
            if (!g_moveSeen) { g_moveSeen = true; g_moveOrigin = { x, y }; return 0; }
            if (abs(x - g_moveOrigin.x) > 6 || abs(y - g_moveOrigin.y) > 6) DestroyWindow(h);
        }
        return 0;
    case WM_LBUTTONDOWN: case WM_RBUTTONDOWN: case WM_MBUTTONDOWN:
    case WM_KEYDOWN: case WM_SYSKEYDOWN:
        if (full) DestroyWindow(h);
        return 0;
    case WM_ACTIVATEAPP:
        if (full && !wp) DestroyWindow(h);
        return 0;
    case WM_SYSCOMMAND:
        if (full && (wp & 0xFFF0) == SC_SCREENSAVE) return 0;
        return DefWindowProc(h, m, wp, lp);
    case WM_DESTROY:
        KillTimer(h, 1);
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProc(h, m, wp, lp);
}

// ---- run -------------------------------------------------------------------
static int RunSaver() {
    WNDCLASSW wc{};
    wc.lpfnWndProc = WndProc;
    wc.hInstance = GetModuleHandle(nullptr);
    wc.lpszClassName = L"DaemonClass";
    wc.hCursor = nullptr;
    wc.hbrBackground = nullptr;
    wc.style = CS_HREDRAW | CS_VREDRAW;
    RegisterClassW(&wc);

    HWND hwnd = nullptr;
    if (g_opt.mode == Mode::FullScreen) {
        hwnd = CreateWindowExW(WS_EX_TOPMOST, L"DaemonClass", L"Daemon", WS_POPUP,
            g_cap.vx, g_cap.vy, g_cap.vw, g_cap.vh,
            nullptr, nullptr, wc.hInstance, nullptr);
    } else if (g_opt.mode == Mode::Preview) {
        RECT rc; GetClientRect(g_opt.parent, &rc);
        hwnd = CreateWindowExW(0, L"DaemonClass", nullptr, WS_CHILD | WS_VISIBLE,
            0, 0, rc.right, rc.bottom, g_opt.parent, nullptr, wc.hInstance, nullptr);
    } else { // Windowed
        RECT rc{ 0, 0, g_opt.winW, g_opt.winH };
        AdjustWindowRect(&rc, WS_OVERLAPPEDWINDOW, FALSE);
        hwnd = CreateWindowExW(0, L"DaemonClass", L"Daemon (test)", WS_OVERLAPPEDWINDOW,
            CW_USEDEFAULT, CW_USEDEFAULT, rc.right - rc.left, rc.bottom - rc.top,
            nullptr, nullptr, wc.hInstance, nullptr);
    }
    if (!hwnd) return 0;

    g_r.init(hwnd);
    g_r.captureMode = !g_opt.capturePath.empty();
    g_show.init(&g_cap);
    g_clock.Reset();
    SetTimer(hwnd, 1, 16, nullptr);
    ShowWindow(hwnd, SW_SHOW);
    UpdateWindow(hwnd);
    if (g_opt.mode == Mode::FullScreen) SetForegroundWindow(hwnd);

    MSG msg;
    while (GetMessageW(&msg, nullptr, 0, 0) > 0) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }
    g_r.shutdown();
    return 0;
}

int WINAPI wWinMain(HINSTANCE, HINSTANCE, PWSTR, int) {
    LogInit();
    SeedRng();
    ParseArgs();
    CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);

    if (g_opt.mode == Mode::Configure) {
        MessageBoxW(g_opt.parent, L"Daemon runs a fake \"the machine is being used "
            L"to hack the world\" sequence, then loops.\n\nNo settings yet.",
            L"Daemon", MB_OK | MB_ICONINFORMATION);
        CoUninitialize();
        return 0;
    }

    if (!DoCapture(g_cap)) { CoUninitialize(); return 0; }
    int rc = RunSaver();
    CoUninitialize();
    return rc;
}
