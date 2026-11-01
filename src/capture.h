// capture.h — grab the real desktop before the saver paints over it.
#pragma once
#include "common.h"

struct MonitorRec {
    RECT rc;        // virtual-screen coords
    bool primary;
};

struct Capture {
    int vx = 0, vy = 0, vw = 0, vh = 0;      // virtual screen origin + size

    Image screenshot;   // the whole desktop as it is now (windows and all)
    Image wallpaper;    // the background picture laid out, NO icons/windows
    Image desktop;      // wallpaper + icons, no windows (== wallpaper if icons failed)

    std::vector<RECT> windows;   // top-level window frames, back-to-front, virtual coords
    std::vector<RECT> icons;     // desktop icon rects (icon+label), virtual coords
    RECT taskbars[4]{};          // primary + secondary taskbar rects
    int  taskbarCount = 0;

    std::vector<MonitorRec> monitors;

    // Map a virtual-screen rect/point into image pixel space.
    RECT toImg(const RECT& r) const { return { r.left - vx, r.top - vy, r.right - vx, r.bottom - vy }; }
};

// Capture everything. Call once, before creating the saver window.
// Returns false only on catastrophic failure (no screenshot); partial data
// (e.g. no icons) still returns true.
bool DoCapture(Capture& out);
