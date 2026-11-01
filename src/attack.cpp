// attack.cpp — tactical "hacking" window and its animated scenes.
// Every number, host and target here is invented movie-prop set-dressing.
#include "attack.h"
#include <random>

namespace {
constexpr uint32_t BG    = 0x080B10;
constexpr uint32_t BORD  = 0x24424F;
constexpr uint32_t CYAN  = 0x33E0FF;
constexpr uint32_t GREEN = 0x4DFF9E;
constexpr uint32_t AMBER = 0xFFC14D;
constexpr uint32_t RED   = 0xFF5566;
constexpr uint32_t DIM   = 0x5A6B7A;
constexpr uint32_t WHITE = 0xE8F0F5;
constexpr uint32_t GRID  = 0x14303C;

std::wstring commas(long long v) {
    std::wstring s = std::to_wstring(v);
    for (int i = (int)s.size() - 3; i > 0; i -= 3) s.insert(i, L",");
    return s;
}
}

void AttackView::init(const Capture* cap) {
    cap_ = cap;
    RECT prim{ 0, 0, cap_->vw, cap_->vh };
    for (auto& m : cap_->monitors) if (m.primary)
        prim = { m.rc.left - cap_->vx, m.rc.top - cap_->vy,
                 m.rc.right - cap_->vx, m.rc.bottom - cap_->vy };
    double pw = RW(prim), ph = RH(prim);
    double w = pw * 0.40, h = ph * 0.75;
    double cx = prim.left + pw * 0.75;
    double cy = prim.top + ph * 0.50;
    win_ = RectF(cx - w / 2, cy - h / 2, cx + w / 2, cy + h / 2);
    titleH_ = (float)(ph / 55.0);

    // Reset scene state so a loop restart starts clean.
    vis_ = L"idle";
    target_ = L"UNKNOWN TARGET";
    source_ = L"DAEMON-NODE";
    clock_ = 0; total_ = 0; trans_ = 1; sceneSeed_ = 1;
}

void AttackView::onDevice(Renderer& r) {
    SafeRelease(&fBig_); SafeRelease(&fMed_); SafeRelease(&fSml_); SafeRelease(&fMono_);
    float wh = win_.bottom - win_.top;
    fBig_  = MakeTextFormat(r.dwrite, L"Bahnschrift", wh * 0.045f, true);
    if (!fBig_) fBig_ = MakeTextFormat(r.dwrite, L"Segoe UI", wh * 0.045f, true);
    fMed_  = MakeTextFormat(r.dwrite, L"Segoe UI", wh * 0.027f, true);
    fSml_  = MakeTextFormat(r.dwrite, L"Segoe UI", wh * 0.019f);
    fMono_ = MakeTextFormat(r.dwrite, L"Consolas", wh * 0.024f);
    if (!fMono_) fMono_ = MakeTextFormat(r.dwrite, L"Lucida Console", wh * 0.024f);
}

void AttackView::setVis(const std::wstring& name,
                        const std::vector<std::pair<std::wstring, std::wstring>>& params,
                        const std::wstring& fallbackTarget) {
    vis_ = name;
    clock_ = 0; trans_ = 0;
    sceneSeed_ = (unsigned)(std::hash<std::wstring>{}(name) ^ (unsigned)(total_ * 1000) ^ 0x9E3779B9u);
    target_ = fallbackTarget.empty() ? L"UNKNOWN TARGET" : fallbackTarget;
    for (auto& kv : params) if (kv.first == L"target" && !kv.second.empty()) target_ = kv.second;
    // upper-case-ish display
}

void AttackView::update(double dt) {
    clock_ += dt; total_ += dt;
    trans_ = Clamp01(clock_ / 0.35);
}

void AttackView::label(Renderer& r, IDWriteTextFormat* f, const std::wstring& s,
                       float x, float y, uint32_t c, float a, bool center) {
    if (!f || s.empty()) return;
    f->SetTextAlignment(center ? DWRITE_TEXT_ALIGNMENT_CENTER : DWRITE_TEXT_ALIGNMENT_LEADING);
    D2D1_RECT_F rc = center ? D2D1::RectF(x - 800, y, x + 800, y + 400)
                            : D2D1::RectF(x, y, x + 3000, y + 400);
    r.rt->DrawTextW(s.c_str(), (UINT32)s.size(), f, rc, r.tmpBrush(c, a));
}

D2D1_RECT_F AttackView::body() const {
    float wh = win_.bottom - win_.top;
    float header = titleH_ + wh * 0.075f;
    float footer = wh * 0.05f;
    float pad = (win_.right - win_.left) * 0.04f;
    return D2D1::RectF(win_.left + pad, win_.top + header,
                       win_.right - pad, win_.bottom - footer);
}

void AttackView::drawHUDBrackets(Renderer& r, D2D1_RECT_F rc, uint32_t c, float a) {
    float L = min(28.0f, (rc.right - rc.left) * 0.08f);
    auto* b = r.tmpBrush(c, a);
    struct P { float x, y, dx, dy; } cs[] = {
        {rc.left, rc.top, 1, 1}, {rc.right, rc.top, -1, 1},
        {rc.left, rc.bottom, 1, -1}, {rc.right, rc.bottom, -1, -1} };
    for (auto& p : cs) {
        r.rt->DrawLine(D2D1::Point2F(p.x, p.y), D2D1::Point2F(p.x + p.dx * L, p.y), b, 1.6f);
        r.rt->DrawLine(D2D1::Point2F(p.x, p.y), D2D1::Point2F(p.x, p.y + p.dy * L), b, 1.6f);
    }
}

void AttackView::drawScanlines(Renderer& r, D2D1_RECT_F rc, float a) {
    auto* b = r.tmpBrush(0x000000, 0.10f * a);
    for (float y = rc.top; y < rc.bottom; y += 3.0f)
        r.rt->DrawLine(D2D1::Point2F(rc.left, y), D2D1::Point2F(rc.right, y), b, 1.0f);
}

void AttackView::drawFrame(Renderer& r, float a) {
    r.rt->FillRoundedRectangle(
        D2D1::RoundedRect(D2D1::RectF(win_.left + 7, win_.top + 9, win_.right + 7, win_.bottom + 9), 6, 6),
        r.tmpBrush(0x000000, 0.35f * a));
    r.rt->FillRoundedRectangle(D2D1::RoundedRect(win_, 5, 5), r.tmpBrush(BG, a));
    r.rt->DrawRoundedRectangle(D2D1::RoundedRect(win_, 5, 5), r.tmpBrush(BORD, a), 1.3f);
    drawHUDBrackets(r, D2D1::RectF(win_.left + 5, win_.top + 5, win_.right - 5, win_.bottom - 5), CYAN, a * 0.9f);

    float wh = win_.bottom - win_.top;
    // title
    label(r, fMed_, L"TARGET ACQUISITION  //  DAEMON", win_.left + 14, win_.top + titleH_ * 0.15f, CYAN, a);
    // header SOURCE / TARGET
    float hy = win_.top + titleH_ + wh * 0.012f;
    label(r, fSml_, L"SOURCE", win_.left + 16, hy, DIM, a);
    label(r, fMed_, source_, win_.left + 16, hy + wh * 0.022f, GREEN, a);
    // target right-aligned
    if (fMed_) {
        fMed_->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_TRAILING);
        r.rt->DrawTextW(target_.c_str(), (UINT32)target_.size(), fMed_,
            D2D1::RectF(win_.left + 100, hy + wh * 0.022f, win_.right - 16, hy + wh * 0.09f),
            r.tmpBrush(AMBER, a));
        fSml_->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_TRAILING);
        r.rt->DrawTextW(L"TARGET", 6, fSml_,
            D2D1::RectF(win_.left + 100, hy, win_.right - 16, hy + wh * 0.02f), r.tmpBrush(DIM, a));
    }
    float dvy = win_.top + titleH_ + wh * 0.07f;
    r.rt->DrawLine(D2D1::Point2F(win_.left + 14, dvy), D2D1::Point2F(win_.right - 14, dvy),
                   r.tmpBrush(BORD, a), 1.0f);
    // footer
    bool rec = fmod(total_, 1.0) < 0.5;
    float fy = win_.bottom - wh * 0.042f;
    r.rt->DrawLine(D2D1::Point2F(win_.left + 14, fy - wh * 0.01f), D2D1::Point2F(win_.right - 14, fy - wh * 0.01f),
                   r.tmpBrush(BORD, a), 1.0f);
    if (rec) r.rt->FillEllipse(D2D1::Ellipse(D2D1::Point2F(win_.left + 22, fy + wh * 0.012f), 4, 4), r.tmpBrush(RED, a));
    label(r, fSml_, L"LINK SECURE   INTRUSION ACTIVE", win_.left + 34, fy, DIM, a);
    if (fSml_) {
        fSml_->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_TRAILING);
        std::wstring mode = L"MODE: " + vis_;
        r.rt->DrawTextW(mode.c_str(), (UINT32)mode.size(), fSml_,
            D2D1::RectF(win_.left + 100, fy, win_.right - 16, fy + wh * 0.03f), r.tmpBrush(CYAN, a));
    }
}

void AttackView::draw(Renderer& r, float openAmt) {
    if (openAmt <= 0 || !fMed_) return;
    float a = Clamp01(openAmt);
    drawFrame(r, a);
    float sa = a * (float)trans_;   // scene alpha incl. transition fade-in
    D2D1_RECT_F b = body();
    r.rt->PushAxisAlignedClip(b, D2D1_ANTIALIAS_MODE_ALIASED);
    if      (vis_ == L"scan")        sceneScan(r, b, sa);
    else if (vis_ == L"map")         sceneMap(r, b, sa);
    else if (vis_ == L"breach")      sceneBreach(r, b, sa);
    else if (vis_ == L"exfil")       sceneExfil(r, b, sa);
    else if (vis_ == L"crack")       sceneCrack(r, b, sa);
    else if (vis_ == L"takeover")    sceneTakeover(r, b, sa);
    else if (vis_ == L"impersonate") sceneImpersonate(r, b, sa);
    else                             sceneIdle(r, b, sa);
    r.rt->PopAxisAlignedClip();
    drawScanlines(r, D2D1::RectF(win_.left + 4, win_.top + 4, win_.right - 4, win_.bottom - 4), a);
}

// ---- scenes ----------------------------------------------------------------
void AttackView::sceneIdle(Renderer& r, D2D1_RECT_F b, float a) {
    float cx = (b.left + b.right) / 2, cy = (b.top + b.bottom) / 2;
    float rad = min(b.right - b.left, b.bottom - b.top) * 0.33f;
    for (int i = -3; i <= 3; ++i)
        r.rt->DrawEllipse(D2D1::Ellipse(D2D1::Point2F(cx, cy), rad, rad * (0.2f + fabsf(i) * 0.12f)),
                          r.tmpBrush(GRID, a), 1.0f);
    float sweep = (float)fmod(clock_ * 0.5, 1.0) * 2 * 3.14159f;
    r.rt->DrawLine(D2D1::Point2F(cx, cy),
                   D2D1::Point2F(cx + cosf(sweep) * rad, cy + sinf(sweep) * rad),
                   r.tmpBrush(CYAN, a), 1.6f);
    if (fmod(total_, 1.0) < 0.6)
        label(r, fMed_, L"ACQUIRING TARGETS...", cx, b.bottom - 40, CYAN, a, true);
}

void AttackView::sceneScan(Renderer& r, D2D1_RECT_F b, float a) {
    float cy = (b.top + b.bottom) / 2;
    D2D1_POINT_2F src{ b.left + 36, cy }, tgt{ b.right - 70, cy };
    r.rt->DrawLine(src, tgt, r.tmpBrush(GRID, a), 1.0f);
    // radar arcs on target
    for (int i = 0; i < 3; ++i) {
        float ph = (float)fmod(clock_ * 0.7 + i / 3.0, 1.0);
        float rad = 20 + ph * 120;
        r.rt->DrawEllipse(D2D1::Ellipse(tgt, rad, rad), r.tmpBrush(CYAN, a * (1 - ph) * 0.8f), 1.4f);
    }
    r.rt->FillEllipse(D2D1::Ellipse(src, 7, 7), r.tmpBrush(GREEN, a));
    r.rt->FillEllipse(D2D1::Ellipse(tgt, 10, 10), r.tmpBrush(AMBER, a));
    label(r, fSml_, L"SOURCE", src.x - 20, cy + 14, DIM, a);
    label(r, fSml_, L"TARGET", tgt.x - 24, cy + 18, DIM, a);
    // sweep line
    float sx = b.left + (float)fmod(clock_ * 0.55, 1.0) * (b.right - b.left);
    for (int k = 0; k < 6; ++k)
        r.rt->DrawLine(D2D1::Point2F(sx - k * 3, b.top), D2D1::Point2F(sx - k * 3, b.bottom),
                       r.tmpBrush(CYAN, a * (0.5f - k * 0.08f)), 1.2f);
    // discovered-services readout, revealed over time
    std::mt19937 rng(sceneSeed_);
    auto ri = [&](int lo, int hi) { return std::uniform_int_distribution<int>(lo, hi)(rng); };
    const wchar_t* svc[] = { L"ssh", L"https", L"rdp", L"smb", L"psql", L"redis", L"ldap", L"vpn" };
    float sy = cy + 58, rowX = b.left + 20;
    label(r, fSml_, L"TARGET SERVICES", rowX, cy + 40, DIM, a);
    float ry = sy;
    for (int i = 0; i < 8; ++i) {
        int port = ri(20, 9000);
        bool open = ri(0, 3) > 0;
        if (clock_ * 2.2 > i + 1) {
            wchar_t line[72];
            swprintf(line, 72, L"%-6ls :%-5d  [%ls]", svc[i], port, open ? L"OPEN" : L"FILTERED");
            label(r, fMono_, line, rowX, ry, open ? GREEN : DIM, a);
        }
        ry += (b.bottom - sy - 24) / 8;
    }
    label(r, fBig_, L"PORT SWEEP", b.left + 20, b.top + 6, CYAN, a * 0.9f);
}

void AttackView::sceneMap(Renderer& r, D2D1_RECT_F b, float a) {
    float cx = (b.left + b.right) / 2, cy = (b.top + b.bottom) / 2;
    float R = min(b.right - b.left, b.bottom - b.top) * 0.40f;
    r.rt->DrawEllipse(D2D1::Ellipse(D2D1::Point2F(cx, cy), R, R), r.tmpBrush(GRID, a), 1.2f);
    for (int i = 1; i <= 3; ++i)
        r.rt->DrawEllipse(D2D1::Ellipse(D2D1::Point2F(cx, cy), R, R * (i / 4.0f)), r.tmpBrush(GRID, a), 1.0f);
    for (int i = 1; i <= 3; ++i)
        r.rt->DrawEllipse(D2D1::Ellipse(D2D1::Point2F(cx, cy), R * (i / 4.0f), R), r.tmpBrush(GRID, a), 1.0f);
    D2D1_POINT_2F src{ b.left + 40, b.bottom - 30 };
    r.rt->FillEllipse(D2D1::Ellipse(src, 6, 6), r.tmpBrush(GREEN, a));
    label(r, fSml_, L"SOURCE", src.x + 8, src.y - 22, GREEN, a);
    std::mt19937 rng(sceneSeed_);
    auto rf = [&](double lo, double hi) { return std::uniform_real_distribution<double>(lo, hi)(rng); };
    for (int i = 0; i < 6; ++i) {
        float ang = (float)rf(-3.0, 3.0), rr = (float)rf(0.35, 0.95) * R;
        D2D1_POINT_2F t{ cx + cosf(ang) * rr, cy + sinf(ang) * rr * 0.9f };
        D2D1_POINT_2F ctrl{ (src.x + t.x) / 2, min(src.y, t.y) - R * 0.5f };
        // pulse travels TARGET -> SOURCE (loot converges on the thief)
        float pr = (float)fmod(clock_ * 0.6 + i * 0.16, 1.0);
        float iu = 1 - pr;
        D2D1_POINT_2F pp{ iu * iu * t.x + 2 * iu * pr * ctrl.x + pr * pr * src.x,
                          iu * iu * t.y + 2 * iu * pr * ctrl.y + pr * pr * src.y };
        r.rt->DrawEllipse(D2D1::Ellipse(t, 5, 5), r.tmpBrush(AMBER, a), 1.4f);
        r.rt->FillEllipse(D2D1::Ellipse(pp, 2.5f, 2.5f), r.tmpBrush(pr > 0.96 ? GREEN : CYAN, a));
    }
    label(r, fBig_, L"DATA ROUTES  TARGET → SOURCE", b.left + 16, b.top + 6, CYAN, a * 0.9f);
}

void AttackView::sceneBreach(Renderer& r, D2D1_RECT_F b, float a) {
    float cx = (b.left + b.right) / 2, cy = (b.top + b.bottom) / 2 + 10;
    float R = min(b.right - b.left, b.bottom - b.top) * 0.40f;
    const wchar_t* names[] = { L"PERIMETER", L"FIREWALL", L"IDS", L"AUTH", L"KERNEL" };
    int n = 5;
    float prog = (float)Clamp01(clock_ / 5.0);     // 0..1 drill
    int bypassed = (int)(prog * n + 0.0001f);
    for (int i = 0; i < n; ++i) {
        float rr = R * (1.0f - i / (float)n);
        bool done = i < bypassed;
        uint32_t c = done ? GREEN : (i == bypassed ? AMBER : DIM);
        r.rt->DrawEllipse(D2D1::Ellipse(D2D1::Point2F(cx, cy), rr, rr), r.tmpBrush(c, a), i == bypassed ? 2.2f : 1.3f);
        label(r, fSml_, done ? (std::wstring(names[i]) + L"  BYPASSED") : names[i],
              cx - rr + 6, cy - rr - 2, c, a);
    }
    // probe spiraling in
    float ang = (float)clock_ * 3.2f;
    float pr = R * (1.0f - prog);
    D2D1_POINT_2F probe{ cx + cosf(ang) * pr, cy + sinf(ang) * pr };
    r.rt->FillEllipse(D2D1::Ellipse(probe, 4, 4), r.tmpBrush(CYAN, a));
    r.rt->DrawLine(D2D1::Point2F(cx, cy), probe, r.tmpBrush(CYAN, a * 0.5f), 1.0f);
    r.rt->FillEllipse(D2D1::Ellipse(D2D1::Point2F(cx, cy), 7, 7), r.tmpBrush(RED, a));
    label(r, fBig_, L"BREACHING", b.left + 16, b.top + 6, CYAN, a * 0.9f);
    if (prog >= 1.0f && fmod(total_, 0.8) < 0.5)
        label(r, fBig_, L"ACCESS GRANTED", cx, b.bottom - 44, GREEN, a, true);
}

void AttackView::sceneExfil(Renderer& r, D2D1_RECT_F b, float a) {
    float cy = (b.top + b.bottom) / 2 - 10;
    D2D1_POINT_2F src{ b.left + 40, cy }, tgt{ b.right - 40, cy };
    r.rt->DrawLine(D2D1::Point2F(src.x, cy - 10), D2D1::Point2F(tgt.x, cy - 10), r.tmpBrush(GRID, a), 1.0f);
    r.rt->DrawLine(D2D1::Point2F(src.x, cy + 10), D2D1::Point2F(tgt.x, cy + 10), r.tmpBrush(GRID, a), 1.0f);
    // packets flow target -> source (leftward)
    for (int i = 0; i < 14; ++i) {
        float t = (float)fmod(clock_ * 0.5 + i / 14.0, 1.0);
        float x = Lerp(tgt.x, src.x, t);
        r.rt->FillRectangle(D2D1::RectF(x - 4, cy - 4, x + 4, cy + 4), r.tmpBrush(CYAN, a * (0.4f + 0.6f * (1 - t))));
    }
    // chevrons along the pipe pointing at SOURCE (leftward flow)
    for (int k = 0; k < 5; ++k) {
        float ax = Lerp(tgt.x - 20, src.x + 20, (k + 0.5f) / 5.0f);
        r.rt->DrawLine(D2D1::Point2F(ax + 7, cy - 6), D2D1::Point2F(ax, cy), r.tmpBrush(CYAN, a * 0.8f), 1.5f);
        r.rt->DrawLine(D2D1::Point2F(ax + 7, cy + 6), D2D1::Point2F(ax, cy), r.tmpBrush(CYAN, a * 0.8f), 1.5f);
    }
    r.rt->FillEllipse(D2D1::Ellipse(src, 8, 8), r.tmpBrush(GREEN, a));
    r.rt->FillEllipse(D2D1::Ellipse(tgt, 8, 8), r.tmpBrush(AMBER, a));
    label(r, fSml_, L"SOURCE", src.x - 20, cy + 16, GREEN, a);
    label(r, fSml_, L"TARGET", tgt.x - 24, cy + 16, AMBER, a);
    label(r, fSml_, L"TARGET → SOURCE", (b.left + b.right) / 2, cy - 34, CYAN, a, true);
    // counters
    std::mt19937 rng(sceneSeed_);
    auto ri = [&](int lo, int hi) { return std::uniform_int_distribution<int>(lo, hi)(rng); };
    long long recFinal = (long long)ri(1, 9) * 1000000000LL + (long long)ri(0, 999) * 1000000LL;
    long long amtFinal = (long long)ri(1, 9) * 1000000000LL + (long long)ri(0, 999) * 1000000LL;
    float e = (float)EaseOut(Clamp01(clock_ / 6.0));
    label(r, fSml_, L"RECORDS EXFILTRATED", b.left + 10, b.bottom - 92, DIM, a);
    label(r, fBig_, commas((long long)(recFinal * e)), b.left + 10, b.bottom - 74, GREEN, a);
    label(r, fSml_, L"FUNDS DIVERTED (USD)", b.right - 10, b.bottom - 92, DIM, a, false);
    if (fBig_) {
        fBig_->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_TRAILING);
        std::wstring amt = L"$" + commas((long long)(amtFinal * e));
        r.rt->DrawTextW(amt.c_str(), (UINT32)amt.size(), fBig_,
            D2D1::RectF(b.left + 100, b.bottom - 74, b.right - 10, b.bottom - 30), r.tmpBrush(AMBER, a));
        fSml_->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_TRAILING);
        r.rt->DrawTextW(L"FUNDS DIVERTED (USD)", 20, fSml_,
            D2D1::RectF(b.left + 100, b.bottom - 92, b.right - 10, b.bottom - 74), r.tmpBrush(DIM, a));
    }
    // progress bar
    float by = b.bottom - 16;
    r.rt->DrawRectangle(D2D1::RectF(b.left + 10, by, b.right - 10, by + 8), r.tmpBrush(BORD, a), 1.0f);
    r.rt->FillRectangle(D2D1::RectF(b.left + 11, by + 1, b.left + 11 + (b.right - b.left - 22) * e, by + 7), r.tmpBrush(CYAN, a));
    label(r, fBig_, L"EXFILTRATION", b.left + 16, b.top + 6, CYAN, a * 0.9f);
}

void AttackView::sceneCrack(Renderer& r, D2D1_RECT_F b, float a) {
    std::mt19937 rng(sceneSeed_ ^ (unsigned)(total_ * 20));  // flicker
    auto hx = [&]() { const wchar_t* h = L"0123456789abcdef";
        return std::wstring(1, h[std::uniform_int_distribution<int>(0, 15)(rng)]); };
    int cols = 24, rows = 6;
    float gw = (b.right - b.left - 20) / cols, gh = (b.bottom - b.top - 70) / rows;
    float prog = (float)Clamp01(clock_ / 5.0);
    int locked = (int)(prog * cols * rows);
    std::mt19937 keyRng(sceneSeed_);
    for (int y = 0; y < rows; ++y)
        for (int x = 0; x < cols; ++x) {
            int idx = y * cols + x;
            bool lk = idx < locked;
            std::wstring ch = lk ? std::wstring(1, L"0123456789abcdef"[std::uniform_int_distribution<int>(0, 15)(keyRng)]) : hx();
            label(r, fMono_, ch, b.left + 12 + x * gw, b.top + 40 + y * gh, lk ? GREEN : DIM, a);
        }
    label(r, fBig_, L"KEY RECOVERY", b.left + 16, b.top + 6, CYAN, a * 0.9f);
    wchar_t pl[48]; swprintf(pl, 48, L"BRUTE FORCE  %d%%", (int)(prog * 100));
    label(r, fSml_, pl, b.left + 16, b.bottom - 30, AMBER, a);
    if (prog >= 1.0f && fmod(total_, 0.8) < 0.5)
        label(r, fBig_, L"KEY RECOVERED", (b.left + b.right) / 2, b.bottom - 60, GREEN, a, true);
}

void AttackView::sceneTakeover(Renderer& r, D2D1_RECT_F b, float a) {
    int cols = 8, rows = 5, total = cols * rows;
    std::vector<int> order(total);
    for (int i = 0; i < total; ++i) order[i] = i;
    std::mt19937 rng(sceneSeed_);
    std::shuffle(order.begin(), order.end(), rng);
    float prog = (float)Clamp01(clock_ / 5.5);
    int owned = (int)(prog * total);
    std::vector<bool> own(total, false);
    for (int i = 0; i < owned; ++i) own[order[i]] = true;
    float gw = (b.right - b.left - 20) / cols, gh = (b.bottom - b.top - 70) / rows;
    for (int y = 0; y < rows; ++y)
        for (int x = 0; x < cols; ++x) {
            int idx = y * cols + x;
            D2D1_RECT_F t = D2D1::RectF(b.left + 12 + x * gw, b.top + 40 + y * gh,
                                        b.left + 12 + x * gw + gw - 6, b.top + 40 + y * gh + gh - 6);
            r.rt->FillRectangle(t, r.tmpBrush(own[idx] ? GREEN : RED, a * 0.18f));
            r.rt->DrawRectangle(t, r.tmpBrush(own[idx] ? GREEN : RED, a), 1.1f);
        }
    label(r, fBig_, L"SYSTEM TAKEOVER", b.left + 16, b.top + 6, CYAN, a * 0.9f);
    wchar_t pl[48]; swprintf(pl, 48, L"OWNED  %d / %d NODES", owned, total);
    label(r, fMed_, pl, b.left + 16, b.bottom - 34, owned >= total ? GREEN : AMBER, a);
}

void AttackView::sceneImpersonate(Renderer& r, D2D1_RECT_F b, float a) {
    float cy = (b.top + b.bottom) / 2;
    float cardW = (b.right - b.left) * 0.30f, cardH = (b.bottom - b.top) * 0.42f;
    D2D1_RECT_F tgt = D2D1::RectF(b.right - 20 - cardW, cy - cardH / 2, b.right - 20, cy + cardH / 2);
    D2D1_RECT_F src = D2D1::RectF(b.left + 20, cy - cardH / 2, b.left + 20 + cardW, cy + cardH / 2);
    float prog = (float)Clamp01(clock_ / 4.0);
    std::mt19937 rng(sceneSeed_);
    auto ri = [&](int lo, int hi) { return std::uniform_int_distribution<int>(lo, hi)(rng); };
    const wchar_t* nm[] = { L"A. OKAFOR", L"M. REYES", L"S. PETROV", L"D. WEBER" };
    std::wstring name = nm[ri(0, 3)];
    wchar_t tok[20]; swprintf(tok, 20, L"%08X", (unsigned)ri(0, 0x7FFFFFFF));
    for (auto* card : { &tgt, &src }) {
        bool isSrc = (card == &src);
        r.rt->DrawRectangle(*card, r.tmpBrush(isSrc ? GREEN : AMBER, a), 1.3f);
        label(r, fSml_, isSrc ? L"SOURCE IDENTITY" : L"TARGET IDENTITY", card->left + 8, card->top + 6,
              isSrc ? GREEN : AMBER, a);
        bool show = isSrc ? (prog > 0.5f) : true;
        if (show) {
            label(r, fMono_, L"USER: " + name, card->left + 8, card->top + cardH * 0.32f, WHITE, a);
            label(r, fMono_, std::wstring(L"TOKEN: ") + tok, card->left + 8, card->top + cardH * 0.52f, WHITE, a);
            label(r, fMono_, L"ROLE: ADMIN", card->left + 8, card->top + cardH * 0.72f, isSrc ? GREEN : DIM, a);
        }
    }
    // copy beam
    float bx = Lerp(tgt.left, src.right, prog);
    r.rt->FillEllipse(D2D1::Ellipse(D2D1::Point2F(bx, cy), 5, 5), r.tmpBrush(CYAN, a));
    r.rt->DrawLine(D2D1::Point2F(tgt.left, cy), D2D1::Point2F(src.right, cy), r.tmpBrush(GRID, a), 1.0f);
    label(r, fBig_, L"IDENTITY CLONE", b.left + 16, b.top + 6, CYAN, a * 0.9f);
    if (prog >= 1.0f && fmod(total_, 0.8) < 0.5)
        label(r, fMed_, L"IMPERSONATION ACTIVE", (b.left + b.right) / 2, b.bottom - 30, GREEN, a, true);
}
