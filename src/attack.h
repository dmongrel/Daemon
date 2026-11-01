// attack.h — the right-hand tactical window: animated "hacking" scenes driven
// by the script's @vis cues. All imagery is invented movie-prop set-dressing.
#pragma once
#include "common.h"
#include "capture.h"
#include "renderer.h"

class AttackView {
public:
    void init(const Capture* cap);
    void onDevice(Renderer& r);
    void setVis(const std::wstring& name,
                const std::vector<std::pair<std::wstring, std::wstring>>& params,
                const std::wstring& fallbackTarget);
    void update(double dt);
    void draw(Renderer& r, float openAmt);

    D2D1_RECT_F windowRect() const { return win_; }

private:
    const Capture* cap_ = nullptr;
    D2D1_RECT_F win_{};
    float titleH_ = 30;

    IDWriteTextFormat* fBig_ = nullptr;
    IDWriteTextFormat* fMed_ = nullptr;
    IDWriteTextFormat* fSml_ = nullptr;
    IDWriteTextFormat* fMono_ = nullptr;

    std::wstring vis_ = L"idle";
    std::wstring target_ = L"UNKNOWN TARGET";
    std::wstring source_ = L"DAEMON-NODE";
    double clock_ = 0;         // since current scene started
    double total_ = 0;         // absolute, for background shimmer
    double trans_ = 1;         // 0..1 fade-in of current scene
    unsigned sceneSeed_ = 1;   // stable randomness per scene instance

    // helpers
    void drawFrame(Renderer& r, float a);
    void drawHUDBrackets(Renderer& r, D2D1_RECT_F rc, uint32_t c, float a);
    void drawScanlines(Renderer& r, D2D1_RECT_F rc, float a);
    void label(Renderer& r, IDWriteTextFormat* f, const std::wstring& s,
               float x, float y, uint32_t c, float a, bool center = false);

    D2D1_RECT_F body() const;
    // scenes
    void sceneIdle(Renderer& r, D2D1_RECT_F b, float a);
    void sceneScan(Renderer& r, D2D1_RECT_F b, float a);
    void sceneMap(Renderer& r, D2D1_RECT_F b, float a);
    void sceneBreach(Renderer& r, D2D1_RECT_F b, float a);
    void sceneExfil(Renderer& r, D2D1_RECT_F b, float a);
    void sceneCrack(Renderer& r, D2D1_RECT_F b, float a);
    void sceneTakeover(Renderer& r, D2D1_RECT_F b, float a);
    void sceneImpersonate(Renderer& r, D2D1_RECT_F b, float a);
};
