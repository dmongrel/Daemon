// show.h — the saver's timeline: capture -> teardown -> payload -> (console).
#pragma once
#include "common.h"
#include "capture.h"
#include "renderer.h"
#include "script.h"
#include "console.h"
#include "attack.h"

// One shatter block: a sub-rect that disappears (revealing wallpaper) at tGo.
struct Block {
    D2D1_RECT_F rc;     // virtual-pixel coords
    double tGo;         // phase-local time it vanishes
    double jitter;      // small pre-vanish wobble seed
};

struct DissolveField {
    std::vector<Block> blocks;
    double span = 1.0;  // total time to clear the field
    void build(const RECT& area, int blockMin, int blockMax, double span, double startFrac);
    void buildFromRects(const std::vector<RECT>& rects, int blockMin, int blockMax,
                        double span, bool sweep);
};

class Show {
public:
    void init(const Capture* cap);
    void onDevice(Renderer& r);     // (re)create bitmaps
    void update(double dt);         // advance clock
    void draw(Renderer& r);         // render current frame
    double elapsed() const { return t_; }
    double totalElapsed() const { return totalT_; }   // monotonic across loops
    bool   finished() const { return false; }  // loops internally

private:
    const Capture* cap_ = nullptr;
    double t_ = 0;                  // seconds since this run's start (resets on loop)
    double totalT_ = 0;             // seconds since the very first run (never resets)

    ID2D1Bitmap* bmpShot_ = nullptr;
    ID2D1Bitmap* bmpWall_ = nullptr;
    ID2D1Bitmap* bmpDesk_ = nullptr;

    DissolveField taskField_;
    DissolveField iconField_;
    ScriptContext ctx_;
    Script        script_;
    Console       console_;
    AttackView    attack_;
    std::wstring  lastVis_;
    bool          attackOpen_ = false;
    double        attackClock_ = 0;

    // finale + loop
    double        finishedAt_ = -1;     // when the console script ended
    int           loopCount_ = 0;
    bool          crtOnly_ = false;     // dev: jump straight to the CRT each loop

    double crtBeg() const;
    void   buildRun();                  // per-run setup (fields, script, windows)
    void   restart();                   // loop back to the start with a new seed
    void   drawCRT(Renderer& r, double p);
    D2D1_RECT_F   pownRect_{};      // payload icon cell
    D2D1_POINT_2F cursorStart_{}, cursorEnd_{};

    void applyWorldTransform(Renderer& r);
    void drawTeardown(Renderer& r);
    void drawPayloadIcon(Renderer& r, float opacity);
    void drawCursor(Renderer& r, D2D1_POINT_2F p, int kind); // 0 arrow,1 busy
    void releaseBitmaps();
};
