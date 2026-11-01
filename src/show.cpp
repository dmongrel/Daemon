// show.cpp — timeline orchestration and teardown/payload rendering.
#include "show.h"
#include <cmath>

// ---- phase boundaries (absolute seconds from show start) -------------------
namespace {
constexpr double HOLD_END   = 1.50;
constexpr double CLOSE_END  = 2.05;
constexpr double TASK_BEG   = 2.20;
constexpr double TASK_END   = 4.40;
constexpr double ICON_BEG   = 4.60;
constexpr double ICON_END   = 6.50;
constexpr double PICON_BEG  = 6.60;   // payload icon fades in
constexpr double PICON_END  = 7.10;
constexpr double MOVE_END   = 7.95;   // cursor glides to icon
constexpr double CLICK_END  = 8.45;   // double-click
constexpr double LAUNCH_END = 8.95;   // "starting" cursor, then console

// ---- CRT switch-off timing (seconds, measured from crtBeg()) ---------------
constexpr double CRT_SQUASH = 0.30;   // frame collapses to a white band
constexpr double CRT_BEAM   = 0.45;   // band recedes to a point
constexpr double CRT_DOT    = CRT_SQUASH + CRT_BEAM;   // the dot has formed
constexpr double CRT_HOLD   = 3.00;   // phosphor dot lingers, lit
constexpr double CRT_FADE   = 5.00;   // then decays to black
constexpr double CRT_TOTAL  = CRT_DOT + CRT_HOLD + CRT_FADE;   // whole switch-off
constexpr double CRT_BLACK  = 2.00;   // brief black before the loop
}

// ---- DissolveField ---------------------------------------------------------
void DissolveField::build(const RECT& area, int bMin, int bMax,
                          double sp, double startFrac) {
    span = sp;
    int w = RW(area), h = RH(area);
    if (w <= 0 || h <= 0) return;
    for (int y = area.top; y < area.bottom; ) {
        int bh = RInt(bMin, bMax); if (y + bh > area.bottom) bh = area.bottom - y;
        for (int x = area.left; x < area.right; ) {
            int bw = RInt(bMin, bMax); if (x + bw > area.right) bw = area.right - x;
            double fx = double(x - area.left) / max(1, w);
            double tGo = sp * (startFrac + (1 - startFrac) * (fx + RFlt(-0.08, 0.08)));
            blocks.push_back({ RectF(x, y, x + bw, y + bh), Clamp01(tGo / sp) * sp,
                               RFlt(0, 1) });
            x += bw;
        }
        y += bh;
    }
}

void DissolveField::buildFromRects(const std::vector<RECT>& rects, int bMin, int bMax,
                                   double sp, bool sweep) {
    span = sp;
    std::vector<int> order(rects.size());
    for (size_t i = 0; i < rects.size(); ++i) order[i] = (int)i;
    if (!sweep) RShuffle(order);
    for (size_t oi = 0; oi < order.size(); ++oi) {
        const RECT& r = rects[order[oi]];
        double base = rects.empty() ? 0 : sp * (double(oi) / rects.size());
        for (int y = r.top; y < r.bottom; ) {
            int bh = RInt(bMin, bMax); if (y + bh > r.bottom) bh = r.bottom - y;
            for (int x = r.left; x < r.right; ) {
                int bw = RInt(bMin, bMax); if (x + bw > r.right) bw = r.right - x;
                double tGo = base + RFlt(0.0, sp / max<size_t>(1, rects.size()) * 0.9);
                blocks.push_back({ RectF(x, y, x + bw, y + bh), tGo, RFlt(0, 1) });
                x += bw;
            }
            y += bh;
        }
    }
}

// ---- Show ------------------------------------------------------------------
void Show::init(const Capture* cap) {
    cap_ = cap;
    t_ = 0; finishedAt_ = -1;
    wchar_t b[8];
    crtOnly_ = GetEnvironmentVariableW(L"DAEMON_CRT_NOW", b, 8) > 0;  // dev: skip to the CRT
    buildRun();
    if (crtOnly_) { finishedAt_ = 0.0; t_ = crtBeg(); }              // jump into the switch-off
}

void Show::buildRun() {
    // Taskbar shatter field.
    taskField_ = DissolveField{};
    iconField_ = DissolveField{};
    std::vector<RECT> tb;
    for (int i = 0; i < cap_->taskbarCount; ++i) tb.push_back(cap_->taskbars[i]);
    if (!tb.empty()) taskField_.buildFromRects(tb, 18, 44, TASK_END - TASK_BEG, true);
    if (!cap_->icons.empty())
        iconField_.buildFromRects(cap_->icons, 14, 30, ICON_END - ICON_BEG, false);

    // Payload icon cell — first desktop slot (top-left of primary monitor).
    RECT prim{ 0, 0, cap_->vw, cap_->vh };
    for (auto& m : cap_->monitors) if (m.primary)
        prim = { m.rc.left - cap_->vx, m.rc.top - cap_->vy,
                 m.rc.right - cap_->vx, m.rc.bottom - cap_->vy };
    double cell = 92;
    pownRect_ = RectF(prim.left + 26, prim.top + 22,
                      prim.left + 26 + 78, prim.top + 22 + cell);
    cursorEnd_ = D2D1::Point2F((pownRect_.left + pownRect_.right) / 2,
                               pownRect_.top + 30);
    cursorStart_ = D2D1::Point2F((prim.left + prim.right) / 2.0f,
                                 (prim.top + prim.bottom) / 2.0f);

    // Console script (fresh randomisation each run).
    ctx_ = ScriptContext{};
    ctx_.build();
    script_ = Script{};
    std::wstring path = ResolveScriptPath();
    if (path.empty() || !LoadScriptFile(path, ctx_, script_))
        script_ = BuiltinDemoScript(ctx_);
    console_.init(cap_, &ctx_, &script_);
    attack_.init(cap_);
    lastVis_.clear(); attackOpen_ = false; attackClock_ = 0;
}

double Show::crtBeg() const {
    return finishedAt_ >= 0 ? finishedAt_ + 2.5 : 1e18;   // tail after the script ends
}

void Show::restart() {
    LARGE_INTEGER q; QueryPerformanceCounter(&q);
    g_rng.seed((unsigned)(q.QuadPart ^ (uint64_t)(loopCount_ * 2654435761u) ^ GetTickCount()));
    ++loopCount_;
    finishedAt_ = -1;
    t_ = 0;
    buildRun();
    if (crtOnly_) { finishedAt_ = 0.0; t_ = crtBeg(); }   // loop straight back to the CRT
}

void Show::releaseBitmaps() {
    SafeRelease(&bmpShot_); SafeRelease(&bmpWall_); SafeRelease(&bmpDesk_);
}

void Show::onDevice(Renderer& r) {
    releaseBitmaps();
    bmpShot_ = r.createBitmap(cap_->screenshot);
    bmpWall_ = r.createBitmap(cap_->wallpaper);
    bmpDesk_ = r.createBitmap(cap_->desktop);
    console_.onDevice(r);
    attack_.onDevice(r);
}

void Show::update(double dt) {
    t_ += dt;
    totalT_ += dt;                     // never reset: lets a dev capture target loop N
    if (t_ >= LAUNCH_END) {
        console_.update(dt);
        const std::wstring& v = console_.currentVis();
        if (!v.empty() && v != lastVis_) {
            lastVis_ = v;
            attack_.setVis(v, console_.currentVisParams(), ctx_.currentTarget);
            if (!attackOpen_) { attackOpen_ = true; attackClock_ = 0; }
        }
        if (attackOpen_) { attackClock_ += dt; attack_.update(dt); }
        if (console_.finished() && finishedAt_ < 0) { finishedAt_ = t_; LogLine("finishedAt=%.3f", t_); }
    }
    // CRT switch-off (squash -> beam -> dot hold+fade) -> brief black -> loop.
    if (finishedAt_ >= 0 && t_ >= crtBeg() + CRT_TOTAL + CRT_BLACK) {
        LogLine("restart at t=%.3f", t_); restart();
    }
}

void Show::applyWorldTransform(Renderer& r) {
    // Map virtual-pixel space (0..vw,0..vh) into the window, centred and scaled.
    double sx = double(r.pxW) / cap_->vw, sy = double(r.pxH) / cap_->vh;
    double s = min(sx, sy);
    double ox = (r.pxW - cap_->vw * s) / 2.0;
    double oy = (r.pxH - cap_->vh * s) / 2.0;
    r.rt->SetTransform(D2D1::Matrix3x2F::Scale((float)s, (float)s) *
                       D2D1::Matrix3x2F::Translation((float)ox, (float)oy));
}

static void DrawCrop(Renderer& r, ID2D1Bitmap* bmp, const D2D1_RECT_F& rc,
                     float op, const D2D1_RECT_F* src) {
    if (bmp) r.rt->DrawBitmap(bmp, rc, op,
        D2D1_BITMAP_INTERPOLATION_MODE_LINEAR, src);
}

void Show::drawTeardown(Renderer& r) {
    const D2D1_RECT_F full = RectF(0, 0, cap_->vw, cap_->vh);

    auto drawDeskPlusTaskbar = [&]() {
        DrawCrop(r, bmpDesk_, full, 1.0f, nullptr);
        for (int i = 0; i < cap_->taskbarCount; ++i) {
            D2D1_RECT_F tr = ToF(cap_->taskbars[i]);
            DrawCrop(r, bmpShot_, tr, 1.0f, &tr);
        }
    };

    if (t_ < HOLD_END) {
        DrawCrop(r, bmpShot_, full, 1.0f, nullptr);
        return;
    }
    if (t_ < CLOSE_END) {                      // windows close together
        drawDeskPlusTaskbar();
        double p = EaseIn((t_ - HOLD_END) / (CLOSE_END - HOLD_END));
        float op = float(1 - p);
        float sc = float(Lerp(1.0, 0.955, p));
        for (auto& w : cap_->windows) {
            float cx = (w.left + w.right) / 2.0f, cy = (w.top + w.bottom) / 2.0f;
            D2D1_RECT_F src = ToF(w);
            D2D1_RECT_F dst = D2D1::RectF(cx + (w.left - cx) * sc, cy + (w.top - cy) * sc,
                                          cx + (w.right - cx) * sc, cy + (w.bottom - cy) * sc);
            DrawCrop(r, bmpShot_, dst, op, &src);
        }
        return;
    }

    // Taskbar deletion.
    if (t_ < TASK_END + 0.05) {
        DrawCrop(r, bmpDesk_, full, 1.0f, nullptr);          // base = wallpaper+icons
        double lt = t_ - TASK_BEG;
        for (auto& b : taskField_.blocks) {
            if (lt >= b.tGo) continue;
            D2D1_RECT_F rc = b.rc;
            if (b.tGo - lt < 0.10) rc.top += (b.jitter > 0.5 ? 1.0f : -1.0f);
            DrawCrop(r, bmpShot_, rc, 1.0f, &b.rc);
        }
        if (t_ < TASK_END) return;
    }

    // Icon deletion and beyond: base is bare wallpaper.
    DrawCrop(r, bmpWall_, full, 1.0f, nullptr);

    if (t_ < ICON_END + 0.05) {
        double lt = t_ - ICON_BEG;
        for (auto& b : iconField_.blocks) {
            if (lt >= b.tGo) continue;
            D2D1_RECT_F rc = b.rc;
            if (b.tGo - lt < 0.09) rc.left += (b.jitter > 0.5 ? 1.0f : -1.0f);
            DrawCrop(r, bmpDesk_, rc, 1.0f, &b.rc);          // icons come from desktop
        }
    }

    // Payload icon + cursor.
    if (t_ >= PICON_BEG) {
        float iconOp = (float)Clamp01((t_ - PICON_BEG) / (PICON_END - PICON_BEG));
        drawPayloadIcon(r, iconOp);
    }
    if (t_ >= PICON_END && t_ < LAUNCH_END) {
        D2D1_POINT_2F cp;
        int kind = 0;
        if (t_ < MOVE_END) {
            double p = EaseInOut((t_ - PICON_END) / (MOVE_END - PICON_END));
            // slight curve via a control offset
            float mx = (cursorStart_.x + cursorEnd_.x) / 2 + 60;
            float my = (cursorStart_.y + cursorEnd_.y) / 2 - 40;
            float u = (float)p, iu = 1 - u;
            cp.x = iu * iu * cursorStart_.x + 2 * iu * u * mx + u * u * cursorEnd_.x;
            cp.y = iu * iu * cursorStart_.y + 2 * iu * u * my + u * u * cursorEnd_.y;
        } else {
            cp = cursorEnd_;
            if (t_ >= LAUNCH_END - 0.0 && t_ < LAUNCH_END) kind = 1;
            if (t_ >= CLICK_END) kind = 1;
        }
        // Selection highlight on the icon during/after click.
        if (t_ >= MOVE_END) {
            bool down = false;
            double ct = t_ - MOVE_END;
            if ((ct > 0.05 && ct < 0.15) || (ct > 0.27 && ct < 0.37)) down = true;
            if (t_ >= CLICK_END) down = true;
            if (down) {
                r.rt->FillRectangle(D2D1::RectF(pownRect_.left - 2, pownRect_.top - 2,
                                                pownRect_.right + 2, pownRect_.bottom + 2),
                                    r.tmpBrush(0x3366CCFF, 0.35f));
            }
        }
        drawCursor(r, cp, kind);
    }

    // Console opens after the launch.
    if (t_ >= LAUNCH_END) {
        double openAmt = Clamp01((t_ - LAUNCH_END) / 0.30);
        console_.draw(r, (float)openAmt);
    }
    // Attack view opens on the first @vis cue.
    if (attackOpen_) {
        double openAmt = Clamp01(attackClock_ / 0.40);
        attack_.draw(r, (float)openAmt);
    }
}

void Show::drawPayloadIcon(Renderer& r, float op) {
    if (op <= 0) return;
    // Icon tile: a dark terminal window with a green skull, label below.
    float cx = (pownRect_.left + pownRect_.right) / 2;
    float iconTop = pownRect_.top;
    D2D1_RECT_F win = D2D1::RectF(cx - 22, iconTop, cx + 22, iconTop + 44);
    D2D1_ROUNDED_RECT rr = D2D1::RoundedRect(win, 4, 4);
    r.rt->FillRoundedRectangle(rr, r.tmpBrush(0x121820, op));
    r.rt->FillRectangle(D2D1::RectF(win.left, win.top, win.right, win.top + 8),
                        r.tmpBrush(0x2A3340, op));
    // skull: two eyes, nose, teeth in phosphor green
    auto* green = r.tmpBrush(0x33FF88, op);
    r.rt->FillEllipse(D2D1::Ellipse(D2D1::Point2F(cx - 7, iconTop + 24), 4, 5), green);
    r.rt->FillEllipse(D2D1::Ellipse(D2D1::Point2F(cx + 7, iconTop + 24), 4, 5), green);
    green = r.tmpBrush(0x33FF88, op);
    r.rt->FillRectangle(D2D1::RectF(cx - 1.5f, iconTop + 29, cx + 1.5f, iconTop + 33), green);
    for (int i = -2; i <= 2; ++i)
        r.rt->FillRectangle(D2D1::RectF(cx + i * 3 - 1, iconTop + 35,
                                        cx + i * 3 + 1, iconTop + 39), green);

    static IDWriteTextFormat* fmt = nullptr;
    if (!fmt) {
        fmt = MakeTextFormat(r.dwrite, L"Segoe UI", 12.5f);
        if (fmt) { fmt->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_CENTER);
                   fmt->SetWordWrapping(DWRITE_WORD_WRAPPING_WRAP); }
    }
    if (fmt) {
        const wchar_t* label = L"U R P0WNED.exe";
        D2D1_RECT_F lr = D2D1::RectF(pownRect_.left - 6, iconTop + 48,
                                     pownRect_.right + 6, iconTop + 90);
        // drop shadow then white
        r.rt->DrawTextW(label, (UINT32)wcslen(label), fmt,
                        D2D1::RectF(lr.left + 1, lr.top + 1, lr.right + 1, lr.bottom + 1),
                        r.tmpBrush(0x000000, op * 0.7f));
        r.rt->DrawTextW(label, (UINT32)wcslen(label), fmt, lr,
                        r.tmpBrush(0xFFFFFF, op));
    }
}

void Show::drawCursor(Renderer& r, D2D1_POINT_2F p, int kind) {
    // A simple arrow cursor built as a filled path with a dark outline.
    ID2D1PathGeometry* geo = nullptr;
    r.factory->CreatePathGeometry(&geo);
    if (!geo) return;
    ID2D1GeometrySink* sink = nullptr;
    geo->Open(&sink);
    const float S = 1.5f;
    D2D1_POINT_2F pts[] = {
        {0,0}, {0,16}, {4.2f,12.2f}, {7.0f,18.2f}, {9.2f,17.2f},
        {6.4f,11.2f}, {11.2f,11.2f}
    };
    sink->BeginFigure(D2D1::Point2F(p.x, p.y), D2D1_FIGURE_BEGIN_FILLED);
    for (int i = 1; i < 7; ++i)
        sink->AddLine(D2D1::Point2F(p.x + pts[i].x * S, p.y + pts[i].y * S));
    sink->EndFigure(D2D1_FIGURE_END_CLOSED);
    sink->Close();
    SafeRelease(&sink);
    r.rt->FillGeometry(geo, r.tmpBrush(0xFFFFFF, 1.0f));
    r.rt->DrawGeometry(geo, r.tmpBrush(0x000000, 1.0f), 1.0f);
    SafeRelease(&geo);
    if (kind == 1) {   // "starting" ring beside the arrow
        r.rt->DrawEllipse(D2D1::Ellipse(D2D1::Point2F(p.x + 26, p.y + 10), 7, 7),
                          r.tmpBrush(0x8FE3FF, 0.9f), 2.0f);
    }
}

// e is seconds since the switch-off began (crtBeg()).
void Show::drawCRT(Renderer& r, double e) {
    r.rt->SetTransform(D2D1::Matrix3x2F::Identity());
    float W = (float)r.pxW, H = (float)r.pxH, cx = W / 2, cy = H / 2;
    if (e < CRT_SQUASH) {                     // vertical squash into a bright white band
        double q = EaseIn(e / CRT_SQUASH);
        float hh = (float)Lerp(H, 3.0, q);
        D2D1_RECT_F dst = D2D1::RectF(0, cy - hh / 2, W, cy + hh / 2);
        r.rt->FillRectangle(dst, r.tmpBrush(0xFFFFFF, 1.0f));   // solid bright white
    } else if (e < CRT_DOT) {                 // white band recedes to a point
        double q = EaseIn((e - CRT_SQUASH) / CRT_BEAM);
        float ww = (float)Lerp(W, 4.0, q);
        float gh = (float)Lerp(3.0, 6.0, q);
        // soft white glow around the beam
        r.rt->FillRectangle(D2D1::RectF(cx - ww / 2, cy - gh, cx + ww / 2, cy + gh),
                            r.tmpBrush(0xFFFFFF, 0.16f + 0.24f * (float)q));
        // solid white beam, fully white (no content tint)
        r.rt->FillRectangle(D2D1::RectF(cx - ww / 2, cy - 2.0f, cx + ww / 2, cy + 2.0f),
                            r.tmpBrush(0xFFFFFF, 1.0f));
    } else {                                  // the phosphor dot: hold lit, then decay
        double since = e - CRT_DOT;
        float al;
        if (since < CRT_HOLD) {               // held, with a faint live shimmer
            al = 0.90f + 0.10f * (float)sin(since * 6.5);
        } else {                              // bright, then a long lingering tail
            double f = Clamp01((since - CRT_HOLD) / CRT_FADE);
            al = (float)pow(1.0 - f, 1.6);
        }
        // concentric glow + bright core, all scaled by the dot's current brightness
        r.rt->FillEllipse(D2D1::Ellipse(D2D1::Point2F(cx, cy), 17.0f, 17.0f),
                          r.tmpBrush(0xFFFFFF, al * 0.10f));
        r.rt->FillEllipse(D2D1::Ellipse(D2D1::Point2F(cx, cy), 8.0f, 8.0f),
                          r.tmpBrush(0xFFFFFF, al * 0.28f));
        r.rt->FillEllipse(D2D1::Ellipse(D2D1::Point2F(cx, cy), 3.2f, 3.2f),
                          r.tmpBrush(0xFFFFFF, al));
    }
}

void Show::draw(Renderer& r) {
    double cb = crtBeg();
    bool collapse = finishedAt_ >= 0 && t_ >= cb && t_ < cb + CRT_TOTAL;
    bool blackout = finishedAt_ >= 0 && t_ >= cb + CRT_TOTAL;

    r.rt->BeginDraw();
    r.rt->Clear(D2D1::ColorF(0, 0, 0));
    if (collapse) {
        drawCRT(r, t_ - cb);
    } else if (blackout) {
        // pure black
    } else {
        applyWorldTransform(r);
        drawTeardown(r);
        r.rt->SetTransform(D2D1::Matrix3x2F::Identity());
    }
    HRESULT hr = r.rt->EndDraw();
    if (hr == D2DERR_RECREATE_TARGET) {
        releaseBitmaps();
        SafeRelease(&r.brush);
        r.hwndRt = nullptr;
        SafeRelease(&r.rt);
    }
}
