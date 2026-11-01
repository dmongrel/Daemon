// console.h — the fake Git Bash window and the script-typing player.
#pragma once
#include "common.h"
#include "capture.h"
#include "renderer.h"
#include "script.h"

struct Run { std::wstring text; uint32_t color; };
using  TLine = std::vector<Run>;

class Console {
public:
    void init(const Capture* cap, ScriptContext* ctx, const Script* script);
    void onDevice(Renderer& r);
    void update(double dt);
    void draw(Renderer& r, float openAmt);     // openAmt 0..1 window reveal

    bool finished() const { return done_; }
    const std::wstring& currentVis() const { return vis_; }
    const std::vector<std::pair<std::wstring, std::wstring>>& currentVisParams() const { return visParams_; }
    D2D1_RECT_F windowRect() const { return win_; }

private:
    const Capture* cap_ = nullptr;
    ScriptContext* ctx_ = nullptr;
    const Script*  script_ = nullptr;

    D2D1_RECT_F win_{};        // whole window (virtual coords)
    float titleH_ = 26, pad_ = 10;
    float fontPx_ = 18, cellW_ = 10, lineH_ = 20;

    IDWriteTextFormat* fmt_ = nullptr;      // monospace body
    IDWriteTextFormat* ui_  = nullptr;      // title text

    // scrollback
    std::vector<TLine> lines_;
    size_t ev_ = 0;
    double evClock_ = 0;
    size_t cmdLine_ = SIZE_MAX;
    size_t outLine_ = SIZE_MAX;
    size_t typed_ = 0;
    bool   promptOpen_ = false;
    bool   done_ = false;
    Pace   pace_ = Pace::Normal;
    double blink_ = 0;
    std::wstring vis_;
    std::vector<std::pair<std::wstring, std::wstring>> visParams_;

    void appendPrompt();
    void ensureCmdLine();
    void setCmd(const std::wstring& t);
    void startOutputLine(uint32_t color);
    void setOutput(const std::wstring& t);
    void drawTextRun(Renderer& r, const std::wstring& s, float x, float y,
                     uint32_t color, float alpha);
};
