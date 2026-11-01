// console.cpp — fake Git Bash window chrome and the typing/printing player.
#include "console.h"

namespace {
double outPer(Pace p) {
    switch (p) { case Pace::Slow: return 0.013; case Pace::Fast: return 0.0035;
                 case Pace::Flood: return 0.0; default: return 0.006; }
}
double outGap(Pace p) {
    switch (p) { case Pace::Slow: return 0.06; case Pace::Fast: return 0.008;
                 case Pace::Flood: return 0.004; default: return 0.02; }
}
constexpr double kTypePer = 0.052;   // per keystroke
constexpr double kEnter   = 0.30;    // pause after a command, before output
constexpr double kWaitDur = 1.4;     // @wait hold if the attack view isn't wired
}

void Console::init(const Capture* cap, ScriptContext* ctx, const Script* script) {
    cap_ = cap; ctx_ = ctx; script_ = script;

    // Reset playback state so a loop restart replays the script from scratch.
    lines_.clear();
    ev_ = 0; evClock_ = 0; typed_ = 0;
    cmdLine_ = SIZE_MAX; outLine_ = SIZE_MAX;
    promptOpen_ = false; done_ = false;
    pace_ = Pace::Normal; blink_ = 0;
    vis_.clear(); visParams_.clear();

    RECT prim{ 0, 0, cap_->vw, cap_->vh };
    for (auto& m : cap_->monitors) if (m.primary)
        prim = { m.rc.left - cap_->vx, m.rc.top - cap_->vy,
                 m.rc.right - cap_->vx, m.rc.bottom - cap_->vy };
    double pw = RW(prim), ph = RH(prim);

    double w = pw * 0.46, h = ph * 0.50;
    double cx = prim.left + pw * 0.25;         // centre of the left half
    double cy = prim.top + ph * 0.50;
    win_ = RectF(cx - w / 2, cy - h / 2, cx + w / 2, cy + h / 2);

    fontPx_ = (float)max(14.0, ph / 68.0);
    titleH_ = fontPx_ * 1.6f;
    pad_    = fontPx_ * 0.7f;
}

void Console::onDevice(Renderer& r) {
    SafeRelease(&fmt_); SafeRelease(&ui_);
    fmt_ = MakeTextFormat(r.dwrite, L"Consolas", fontPx_);
    if (!fmt_) fmt_ = MakeTextFormat(r.dwrite, L"Lucida Console", fontPx_);
    ui_  = MakeTextFormat(r.dwrite, L"Segoe UI", fontPx_ * 0.8f);

    // Measure the monospace cell.
    if (fmt_) {
        IDWriteTextLayout* lay = nullptr;
        const wchar_t* probe = L"MMMMMMMMMMMMMMMMMMMM";
        r.dwrite->CreateTextLayout(probe, 20, fmt_, 4000, 200, &lay);
        if (lay) {
            DWRITE_TEXT_METRICS tm{};
            lay->GetMetrics(&tm);
            cellW_ = tm.widthIncludingTrailingWhitespace / 20.0f;
            lineH_ = tm.height;
            lay->Release();
        }
        if (lineH_ < 4) lineH_ = fontPx_ * 1.25f;
    }
}

void Console::appendPrompt() {
    TLine a;
    a.push_back({ ctx_->promptUser + L"@" + ctx_->promptHost + L" ", 0x33D17A });
    a.push_back({ L"MINGW64 ", 0xC678DD });
    a.push_back({ ctx_->promptPath, 0xE5C07B });
    lines_.push_back(a);
    TLine b;
    b.push_back({ L"$ ", 0xD8D8D8 });
    b.push_back({ L"", 0xF0F0F0 });              // command run
    lines_.push_back(b);
    cmdLine_ = lines_.size() - 1;
    promptOpen_ = true;
}

void Console::ensureCmdLine() {
    if (promptOpen_ && cmdLine_ < lines_.size()) return;
    TLine b;
    b.push_back({ L"$ ", 0xD8D8D8 });
    b.push_back({ L"", 0xF0F0F0 });
    lines_.push_back(b);
    cmdLine_ = lines_.size() - 1;
    promptOpen_ = true;
}

void Console::setCmd(const std::wstring& t) {
    if (cmdLine_ < lines_.size() && !lines_[cmdLine_].empty())
        lines_[cmdLine_].back().text = t;
}

void Console::startOutputLine(uint32_t color) {
    lines_.push_back(TLine{ { L"", color } });
    outLine_ = lines_.size() - 1;
}

void Console::setOutput(const std::wstring& t) {
    if (outLine_ < lines_.size() && !lines_[outLine_].empty())
        lines_[outLine_][0].text = t;
}

void Console::update(double dt) {
    blink_ += dt;
    if (done_ || !script_) return;
    evClock_ += dt;

    int guard = 0;
    while (ev_ < script_->events.size() && guard++ < 20000) {
        const ScriptEvent& e = script_->events[ev_];
        switch (e.kind) {
        case EvKind::Prompt:
            appendPrompt(); ++ev_; typed_ = 0; continue;
        case EvKind::Pace_:
            pace_ = e.pace; ++ev_; continue;
        case EvKind::Vis:
            vis_ = e.visName; visParams_ = e.visParams; ++ev_; continue;
        case EvKind::Wait:
            if (evClock_ >= kWaitDur) { evClock_ -= kWaitDur; ++ev_; continue; }
            return;
        case EvKind::Pause:
            if (evClock_ >= e.seconds) { evClock_ -= e.seconds; ++ev_; continue; }
            return;
        case EvKind::Type: {
            ensureCmdLine();
            size_t len = e.text.size();
            double full = len * kTypePer + kEnter;
            if (evClock_ >= full) {
                setCmd(e.text); promptOpen_ = false;
                evClock_ -= full; typed_ = 0; ++ev_; continue;
            }
            size_t n = (size_t)min((double)len, evClock_ / kTypePer);
            setCmd(e.text.substr(0, n));
            return;
        }
        case EvKind::Output: {
            if (typed_ == 0) startOutputLine(e.color);
            size_t len = e.text.size();
            double per = outPer(pace_), gap = outGap(pace_);
            double full = (pace_ == Pace::Flood ? 0.0 : len * per) + gap;
            if (evClock_ >= full) {
                setOutput(e.text);
                evClock_ -= full; typed_ = 0; ++ev_; continue;
            }
            size_t n = (pace_ == Pace::Flood) ? len : (size_t)min((double)len, evClock_ / per);
            typed_ = n ? n : 1;                   // mark the line as started
            setOutput(e.text.substr(0, n));
            return;
        }
        }
    }
    if (ev_ >= script_->events.size()) done_ = true;
}

void Console::drawTextRun(Renderer& r, const std::wstring& s, float x, float y,
                          uint32_t color, float alpha) {
    if (s.empty()) return;
    r.rt->DrawTextW(s.c_str(), (UINT32)s.size(), fmt_,
                    D2D1::RectF(x, y, x + 100000, y + lineH_ + 2),
                    r.tmpBrush(color, alpha));
}

void Console::draw(Renderer& r, float openAmt) {
    if (openAmt <= 0 || !fmt_) return;
    float a = Clamp01(openAmt);

    // window reveal: slight grow from centre
    float cx = (win_.left + win_.right) / 2, cy = (win_.top + win_.bottom) / 2;
    float sc = Lerp(0.95, 1.0, EaseOut(a));
    D2D1_RECT_F w = D2D1::RectF(cx + (win_.left - cx) * sc, cy + (win_.top - cy) * sc,
                               cx + (win_.right - cx) * sc, cy + (win_.bottom - cy) * sc);

    // drop shadow
    r.rt->FillRoundedRectangle(
        D2D1::RoundedRect(D2D1::RectF(w.left + 6, w.top + 8, w.right + 6, w.bottom + 8), 7, 7),
        r.tmpBrush(0x000000, 0.35f * a));
    // body
    r.rt->FillRoundedRectangle(D2D1::RoundedRect(w, 6, 6), r.tmpBrush(0x0C0C0C, a));
    // title bar
    D2D1_RECT_F tb = D2D1::RectF(w.left, w.top, w.right, w.top + titleH_);
    r.rt->FillRectangle(tb, r.tmpBrush(0x2B2B2B, a));
    if (ui_) {
        std::wstring title = L"MINGW64:" + ctx_->promptPath;
        r.rt->DrawTextW(title.c_str(), (UINT32)title.size(), ui_,
            D2D1::RectF(w.left + 10, w.top + titleH_ * 0.18f, w.right - 70, w.top + titleH_),
            r.tmpBrush(0xD0D0D0, a));
    }
    // window buttons (— ▢ ✕)
    float by = w.top + titleH_ / 2;
    r.rt->DrawLine(D2D1::Point2F(w.right - 54, by), D2D1::Point2F(w.right - 46, by),
                   r.tmpBrush(0xC0C0C0, a), 1.4f);
    r.rt->DrawRectangle(D2D1::RectF(w.right - 38, by - 4, w.right - 30, by + 4),
                        r.tmpBrush(0xC0C0C0, a), 1.2f);
    r.rt->DrawLine(D2D1::Point2F(w.right - 22, by - 4), D2D1::Point2F(w.right - 14, by + 4),
                   r.tmpBrush(0xE06C75, a), 1.6f);
    r.rt->DrawLine(D2D1::Point2F(w.right - 22, by + 4), D2D1::Point2F(w.right - 14, by - 4),
                   r.tmpBrush(0xE06C75, a), 1.6f);

    // body text (clipped), tail-scrolled
    D2D1_RECT_F body = D2D1::RectF(w.left + pad_, w.top + titleH_ + pad_ * 0.5f,
                                   w.right - pad_, w.bottom - pad_ * 0.5f);
    r.rt->PushAxisAlignedClip(body, D2D1_ANTIALIAS_MODE_ALIASED);
    int maxVis = (int)((body.bottom - body.top) / lineH_);
    if (maxVis < 1) maxVis = 1;
    int total = (int)lines_.size();
    int first = max(0, total - maxVis);
    float y = body.top;
    for (int i = first; i < total; ++i) {
        float x = body.left;
        for (auto& run : lines_[i]) {
            drawTextRun(r, run.text, x, y, run.color, a);
            x += run.text.size() * cellW_;
        }
        // blinking block cursor on the active line
        bool activeLine = (!done_ && i == total - 1);
        if (activeLine && fmod(blink_, 1.0) < 0.5) {
            r.rt->FillRectangle(D2D1::RectF(x + 1, y + 2, x + 1 + cellW_ * 0.9f, y + lineH_ - 2),
                                r.tmpBrush(0xCFE8CF, a));
        }
        y += lineH_;
    }
    r.rt->PopAxisAlignedClip();
}
