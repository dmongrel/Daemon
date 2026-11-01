// script.cpp — placeholder generation and the script-file parser.
// All generated values are fictional set-dressing for the visualisation.
#include "script.h"
#include <string>

// ---- fictional data pools --------------------------------------------------
namespace {

const wchar_t* kOrgs[] = {
    L"Meridian Data Vaults",      L"Helios Clearing House",
    L"Northwind Exchange",        L"Atlas Sovereign Bank",
    L"Cindermark Crypto",         L"Verex Logistics Grid",
    L"Orillion Health Network",   L"Brightpar Capital",
    L"Kestrel National Registry", L"Pallas Energy Authority",
    L"Sable & Roe Holdings",      L"Quillon Payments",
    L"Drayton Federal Reserve",   L" Icarus Freight Systems",
    L"Vantor Telecom",            L"Grey Harbor Mutual",
    L"Thornfield Defense Works",   L"Lumen Identity Bureau",
    L"Castellan Trust",           L"Obsidian Mining Collective",
};
const wchar_t* kHostPrefix[] = { L"edge", L"core", L"vault", L"bastion", L"relay",
    L"gw", L"db", L"auth", L"mail", L"san", L"node", L"proxy", L"dc" };
const wchar_t* kTld[] = { L"corp", L"internal", L"net", L"sys", L"local", L"gov" };
const wchar_t* kNames[] = { L"a.okafor", L"m.reyes", L"s.petrov", L"j.lindqvist",
    L"r.haddad", L"t.nguyen", L"e.santos", L"d.weber", L"admin", L"svc_backup",
    L"l.calderon", L"operator" };
const wchar_t* kCC[] = { L"US", L"DE", L"SG", L"BR", L"NL", L"JP", L"ZA", L"AE", L"KR", L"GB" };

std::wstring itow(long long v) { return std::to_wstring(v); }

std::wstring genIP() {
    static const int firsts[] = { 41, 77, 91, 103, 185, 196, 201, 212, 45, 160 };
    wchar_t b[32];
    swprintf(b, 32, L"%d.%d.%d.%d", firsts[RInt(0, 9)], RInt(0, 255), RInt(0, 255), RInt(1, 254));
    return b;
}
std::wstring genHost() {
    std::wstring s = kHostPrefix[RInt(0, 12)];
    s += L"-" + itow(RInt(1, 48));
    s += L".";
    s += kTld[RInt(0, 5)];
    return s;
}
std::wstring genPort() {
    static const int ports[] = { 22, 443, 3389, 8443, 5432, 1433, 445, 8080, 9200, 6379 };
    return itow(ports[RInt(0, 9)]);
}
std::wstring genHash(int n) {
    std::wstring s; const wchar_t* h = L"0123456789abcdef";
    for (int i = 0; i < n; ++i) s += h[RInt(0, 15)];
    return s;
}
std::wstring genAmount() {
    wchar_t b[32]; swprintf(b, 32, L"$%d.%d billion", RInt(1, 9), RInt(0, 9));
    return b;
}
std::wstring genRecords() {
    wchar_t b[40]; swprintf(b, 40, L"%d.%d billion records", RInt(1, 9), RInt(0, 9));
    return b;
}

// case-insensitive replace of {TOKEN} using a generator
void replaceToken(std::wstring& s, const wchar_t* tok, std::wstring(*gen)()) {
    std::wstring t = tok;
    size_t pos;
    while ((pos = s.find(t)) != std::wstring::npos)
        s.replace(pos, t.size(), gen());
}

std::wstring g_demoTargetHolder;   // used by the {TARGET} closure indirectly

} // namespace

// ---- ScriptContext ---------------------------------------------------------
void ScriptContext::build() {
    for (auto* o : kOrgs) targetPool.push_back(o);
    RShuffle(targetPool);
    promptUser = L"root";
    promptHost = L"daemon";
    promptPath = L"/opt/daemon";
    if (!targetPool.empty()) currentTarget = targetPool[0];
}

void ScriptContext::nextTarget() {
    if (targetPool.empty()) return;
    targetIdx = (targetIdx + 1) % targetPool.size();
    currentTarget = targetPool[targetIdx];
}

std::wstring ScriptContext::sub(const std::wstring& line) const {
    std::wstring s = line;
    size_t pos;
    // {TARGET} is sticky (chapter-scoped).
    while ((pos = s.find(L"{TARGET}")) != std::wstring::npos)
        s.replace(pos, 8, currentTarget);
    replaceToken(s, L"{IP}", genIP);
    replaceToken(s, L"{HOST}", genHost);
    replaceToken(s, L"{PORT}", genPort);
    replaceToken(s, L"{AMOUNT}", genAmount);
    replaceToken(s, L"{RECORDS}", genRecords);
    replaceToken(s, L"{USER}", []() -> std::wstring { return kNames[RInt(0, 11)]; });
    replaceToken(s, L"{CC}", []() -> std::wstring { return kCC[RInt(0, 9)]; });
    replaceToken(s, L"{HASH}", []() -> std::wstring { return genHash(RInt(10, 16)); });
    replaceToken(s, L"{ASN}", []() -> std::wstring { return L"AS" + itow(RInt(1000, 65000)); });
    replaceToken(s, L"{PCT}", []() -> std::wstring { return itow(RInt(1, 100)); });
    replaceToken(s, L"{N}", []() -> std::wstring { return itow(RInt(2, 9999)); });
    replaceToken(s, L"{MS}", []() -> std::wstring { return itow(RInt(3, 900)); });
    return s;
}

// ---- parser ----------------------------------------------------------------
namespace {

uint32_t parseHexColor(const std::wstring& w) {
    std::wstring h = w;
    if (!h.empty() && h[0] == L'#') h = h.substr(1);
    return (uint32_t)wcstoul(h.c_str(), nullptr, 16) & 0xFFFFFF;
}

Pace parsePace(const std::wstring& w) {
    if (w == L"slow") return Pace::Slow;
    if (w == L"fast") return Pace::Fast;
    if (w == L"flood") return Pace::Flood;
    return Pace::Normal;
}

void splitLines(const std::wstring& all, std::vector<std::wstring>& out) {
    std::wstring cur;
    for (wchar_t c : all) {
        if (c == L'\n') { if (!cur.empty() && cur.back() == L'\r') cur.pop_back();
                          out.push_back(cur); cur.clear(); }
        else cur += c;
    }
    out.push_back(cur);
}

bool readFileUtf8(const std::wstring& path, std::wstring& out) {
    FILE* f = _wfopen(path.c_str(), L"rb");
    if (!f) return false;
    fseek(f, 0, SEEK_END); long n = ftell(f); fseek(f, 0, SEEK_SET);
    if (n <= 0) { fclose(f); return false; }
    std::string bytes((size_t)n, 0);
    fread(bytes.data(), 1, n, f);
    fclose(f);
    // strip UTF-8 BOM
    size_t off = (bytes.size() >= 3 && (unsigned char)bytes[0] == 0xEF &&
                  (unsigned char)bytes[1] == 0xBB && (unsigned char)bytes[2] == 0xBF) ? 3 : 0;
    int wn = MultiByteToWideChar(CP_UTF8, 0, bytes.data() + off, (int)(bytes.size() - off), nullptr, 0);
    out.assign(wn, 0);
    MultiByteToWideChar(CP_UTF8, 0, bytes.data() + off, (int)(bytes.size() - off), out.data(), wn);
    return true;
}

std::wstring firstWord(const std::wstring& s, size_t start, size_t& after) {
    size_t i = start;
    while (i < s.size() && iswspace(s[i])) ++i;
    size_t b = i;
    while (i < s.size() && !iswspace(s[i])) ++i;
    after = i;
    return s.substr(b, i - b);
}

} // namespace

bool LoadScriptFile(const std::wstring& path, ScriptContext& ctx, Script& out) {
    std::wstring all;
    if (!readFileUtf8(path, all)) return false;
    std::vector<std::wstring> lines;
    splitLines(all, lines);

    uint32_t curColor = 0xC8C8C8;
    for (auto& raw : lines) {
        std::wstring line = raw;
        if (!line.empty() && line.rfind(L"#", 0) == 0) continue;     // comment

        if (!line.empty() && line[0] == L'@') {
            size_t after;
            std::wstring dir = firstWord(line, 1, after);
            std::wstring rest = line.substr(min(after, line.size()));
            // trim leading space of rest
            size_t s0 = rest.find_first_not_of(L" \t");
            rest = (s0 == std::wstring::npos) ? L"" : rest.substr(s0);

            if (dir == L"prompt") {
                out.events.push_back({ EvKind::Prompt });
            } else if (dir == L"pace") {
                ScriptEvent e{ EvKind::Pace_ }; e.pace = parsePace(rest);
                out.events.push_back(e);
            } else if (dir == L"pause") {
                ScriptEvent e{ EvKind::Pause }; e.seconds = _wtof(rest.c_str());
                out.events.push_back(e);
            } else if (dir == L"wait") {
                out.events.push_back({ EvKind::Wait });
            } else if (dir == L"vis") {
                ScriptEvent e{ EvKind::Vis };
                size_t a2; e.visName = firstWord(rest, 0, a2);
                // remaining key=val pairs
                std::wstring kv = rest.substr(min(a2, rest.size()));
                size_t i = 0;
                while (i < kv.size()) {
                    while (i < kv.size() && iswspace(kv[i])) ++i;
                    size_t b = i;
                    while (i < kv.size() && !iswspace(kv[i])) ++i;
                    std::wstring tok = kv.substr(b, i - b);
                    size_t eq = tok.find(L'=');
                    if (eq != std::wstring::npos)
                        e.visParams.push_back({ tok.substr(0, eq), ctx.sub(tok.substr(eq + 1)) });
                }
                out.events.push_back(e);
            } else if (dir == L"color") {
                curColor = (rest == L"off" || rest.empty()) ? 0xC8C8C8 : parseHexColor(rest);
            } else if (dir == L"target") {
                if (rest.empty()) ctx.nextTarget();
                else ctx.currentTarget = rest;
            }
            continue;
        }

        if (line.rfind(L"$ ", 0) == 0 || line == L"$") {
            ScriptEvent e{ EvKind::Type };
            e.text = ctx.sub(line.size() > 2 ? line.substr(2) : L"");
            out.events.push_back(e);
        } else {
            ScriptEvent e{ EvKind::Output };
            e.text = ctx.sub(line);
            e.color = curColor;
            out.events.push_back(e);
        }
    }
    return !out.events.empty();
}

// ---- builtin demo ----------------------------------------------------------
Script BuiltinDemoScript(ScriptContext& ctx) {
    Script s;
    auto prompt = [&] { s.events.push_back({ EvKind::Prompt }); };
    auto type   = [&](const wchar_t* t) {
        ScriptEvent e{ EvKind::Type }; e.text = ctx.sub(t); s.events.push_back(e); };
    auto out    = [&](const wchar_t* t, uint32_t c = 0xC8C8C8) {
        ScriptEvent e{ EvKind::Output }; e.text = ctx.sub(t); e.color = c; s.events.push_back(e); };
    auto pause  = [&](double sec) { ScriptEvent e{ EvKind::Pause }; e.seconds = sec; s.events.push_back(e); };

    auto vis = [&](const wchar_t* name) {
        ScriptEvent e{ EvKind::Vis }; e.visName = name;
        e.visParams.push_back({ L"target", ctx.currentTarget });
        s.events.push_back(e); };

    prompt(); type(L"whoami");
    out(L"root");
    pause(0.4);
    prompt(); type(L"./daemon --init --targets=all");
    out(L"[ok] loading target set", 0x6FE26F);
    out(L"[ok] relay chain established via {HOST} ({IP})", 0x6FE26F);
    out(L"[**] locking onto SOURCE 0.0.0.0", 0xFFC14D);
    pause(0.6);

    prompt(); type(L"scan --source daemon --target \"{TARGET}\"");
    vis(L"scan");
    out(L"[scan] mapping TARGET {HOST} from SOURCE", 0x8FD0FF);
    out(L"[scan] {PORT}/tcp open   {PORT}/tcp open   {PORT}/tcp filtered", 0xC8C8C8);
    pause(3.0);

    prompt(); type(L"route --build --target \"{TARGET}\"");
    vis(L"map");
    out(L"[route] SOURCE -> relay -> TARGET  ({CC})", 0x8FD0FF);
    pause(2.6);

    prompt(); type(L"breach --target \"{TARGET}\" --chain auto");
    vis(L"breach");
    out(L"[breach] SOURCE bypassing TARGET perimeter", 0xFFC14D);
    out(L"[breach] firewall down :: auth bypassed :: kernel {HASH}", 0xFFC14D);
    out(L"[ok] ACCESS GRANTED on {TARGET}", 0x6FE26F);
    pause(3.2);

    prompt(); type(L"crack --target \"{TARGET}\" --vault master.key");
    vis(L"crack");
    out(L"[crack] recovering TARGET master key...", 0x8FD0FF);
    out(L"[ok] KEY RECOVERED :: {HASH}", 0x6FE26F);
    pause(3.0);

    prompt(); type(L"impersonate --target \"{TARGET}\" --as admin");
    vis(L"impersonate");
    out(L"[id] cloning TARGET identity {USER} to SOURCE", 0x8FD0FF);
    out(L"[ok] IMPERSONATION ACTIVE :: role=admin", 0x6FE26F);
    pause(2.8);

    prompt(); type(L"exfil --target \"{TARGET}\" --all");
    vis(L"exfil");
    out(L"[exfil] draining TARGET -> SOURCE", 0x8FD0FF);
    out(L"[ok] {RECORDS} // {AMOUNT} moved to SOURCE", 0x6FE26F);
    pause(3.4);

    prompt(); type(L"takeover --target \"{TARGET}\" --persist");
    vis(L"takeover");
    out(L"[own] flipping TARGET nodes to SOURCE control", 0xFFC14D);
    out(L"[ok] TARGET {TARGET} fully owned", 0x6FE26F);
    pause(3.2);
    return s;
}

// ---- path resolution -------------------------------------------------------
std::wstring ResolveScriptPath() {
    wchar_t buf[MAX_PATH];
    if (GetEnvironmentVariableW(L"DAEMON_SCRIPT", buf, MAX_PATH) && buf[0]) {
        if (GetFileAttributesW(buf) != INVALID_FILE_ATTRIBUTES) return buf;
    }
    // next to the executable
    wchar_t exe[MAX_PATH];
    if (GetModuleFileNameW(nullptr, exe, MAX_PATH)) {
        std::wstring p = exe;
        size_t sl = p.find_last_of(L"\\/");
        if (sl != std::wstring::npos) {
            std::wstring d = p.substr(0, sl + 1) + L"daemon.script";
            if (GetFileAttributesW(d.c_str()) != INVALID_FILE_ATTRIBUTES) return d;
        }
    }
    // project docs
    const wchar_t* fixed = L"C:\\_C_Projects\\Daemon\\docs\\daemon.script";
    if (GetFileAttributesW(fixed) != INVALID_FILE_ATTRIBUTES) return fixed;
    return L"";
}
