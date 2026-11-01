// script.h — the console screenplay: a randomised placeholder context, the
// on-disk script format, and the parsed event stream the player runs.
#pragma once
#include "common.h"

// ---- placeholder context ---------------------------------------------------
// Generated fresh each run (per seed). Substitutes {TOKENS} in script text.
// Everything here is invented set-dressing: fictional hosts, companies, numbers.
struct ScriptContext {
    std::vector<std::wstring> targetPool;  // shuffled list of fictional orgs
    size_t targetIdx = 0;
    std::wstring currentTarget = L"the target";

    std::wstring promptUser = L"root";
    std::wstring promptHost = L"daemon";
    std::wstring promptPath = L"/opt/daemon";

    void build();                          // fill the pool and prompt identity
    void nextTarget();                     // advance currentTarget from the pool
    std::wstring sub(const std::wstring& line) const;  // replace {TOKENS}
};

// ---- parsed events ---------------------------------------------------------
enum class Pace { Slow, Normal, Fast, Flood };

enum class EvKind { Prompt, Type, Output, Pause, Pace_, Vis, Wait };

struct ScriptEvent {
    EvKind kind;
    std::wstring text;                     // Type command / Output line text
    uint32_t color = 0xC8C8C8;             // Output colour
    double seconds = 0;                    // Pause duration
    Pace pace = Pace::Normal;              // Pace value
    std::wstring visName;                  // Vis cue name
    std::vector<std::pair<std::wstring, std::wstring>> visParams;
};

struct Script {
    std::vector<ScriptEvent> events;
    bool empty() const { return events.empty(); }
};

// Load a script file, substituting tokens against ctx (which is mutated as
// @target directives advance). Returns false if the file can't be read.
bool LoadScriptFile(const std::wstring& path, ScriptContext& ctx, Script& out);

// A short, benign built-in script so the console works with no file present.
Script BuiltinDemoScript(ScriptContext& ctx);

// Resolve the script path: DAEMON_SCRIPT, else daemon.script next to the exe
// or in the project docs/, else empty (caller falls back to the demo).
std::wstring ResolveScriptPath();
