// OptiCaptureLauncher - the front end of OptiCaptureHost.
//
// Pick the running game, switch VSync, change the shortcuts, press Start. The launcher starts OptiCaptureHost.exe
// (with OptiScaler next to it) for the chosen window and keeps writing OptiCaptureHost.ini, which the host re-reads
// while it runs, so VSync and the shortcuts change live.
//
// It deliberately uses no Direct3D / DXGI at all (plain GDI+): OptiScaler loads as dxgi.dll next to the exes, and the
// launcher must not pull it in.
//
// by MK-JH - https://t.me/MKJH_APPS

#ifndef NOMINMAX
#define NOMINMAX
#endif

#include <windows.h>
#include <windowsx.h>
#include <objidl.h>
#include <shellapi.h>
#include <dwmapi.h>
#include <tlhelp32.h>

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <cwchar>
#include <format>
#include <memory>
#include <string>
#include <vector>

// gdiplus.h uses unqualified min / max; with NOMINMAX they have to come from here.
using std::max;
using std::min;
#include <gdiplus.h>

#include "settings.h"
#include "version.h"
#include "ui_strings.h"

#pragma comment(lib, "advapi32.lib")

namespace gp = Gdiplus;
namespace ui = oc::ui;

namespace
{

constexpr const wchar_t* kClassName = L"OptiCaptureLauncher";
constexpr const wchar_t* kTelegramUrl = L"https://t.me/MKJH_APPS";
constexpr const wchar_t* kTelegramJHUrl = L"https://t.me/PersianPatchDeveloper";
constexpr const wchar_t* kTelegramAppsUrl = L"https://t.me/MKJH_APPS";
constexpr const wchar_t* kTelegramArchiveUrl = L"https://t.me/MKJH_ARCHIVE";
constexpr const wchar_t* kGithubUrl = L"https://github.com/tegraultra/OptiCaptureHost";
constexpr const wchar_t* kTelegramText = L"t.me/MKJH_APPS";
constexpr const wchar_t* kHostExe = L"OptiCaptureHost.exe";
constexpr const wchar_t* kOverlayClass = L"OptiCaptureHostOverlay"; // the host's overlay window class

constexpr const wchar_t* kVersion = L"" OC_VER_STR;

constexpr float kW = 796.0f; // client size in logical pixels (scaled by the DPI)
constexpr float kH = 812.0f;

constexpr UINT_PTR kTimerList = 1;
constexpr UINT_PTR kTimerHost = 2;
constexpr UINT_PTR kTimerFront = 3;
constexpr int kHotkeyShow = 1;
constexpr int kIdFrameGenSelect = 6;
constexpr int kIdFrameGenApply = 7;
constexpr int kIdUpscalerSelect = 11;
constexpr int kIdUpscalerApply = 12;
constexpr int kIdLinkJH = 8;
constexpr int kIdLinkApps = 9;
constexpr int kIdLinkArchive = 10;
constexpr int kIdLinkGithub = 50;
constexpr int kIdD5Enable = 70;   // DLSS 5 side window: enable / disable
constexpr int kIdHudFix = 73;     // tick box: [OptiFG] HUDFix
constexpr int kIdAsyncFg = 74;    // tick box: [FSRFG] AllowAsync
constexpr int kIdD5Toggle = 75;   // header button: opens / closes the DLSS 5 window
constexpr int kIdDepthToggle = 76;  // header button: opens / closes the Depth window
constexpr int kIdDepthFlat = 77;    // Depth window: flat depth (as always)
constexpr int kIdDepthReshade = 78; // Depth window: the game's depth through the ReShade add-on
constexpr int kIdDepthFolder = 79;  // Depth window: open the folder with the add-on files
constexpr int kIdDepthInstall = 80; // Depth window: copy ready ReShade files next to the selected game's exe
constexpr int kIdD5Download = 72; // DLSS 5 side window: download link for nvngx_dlssnr.dll
constexpr const wchar_t* kPanelClass = L"OptiCaptureLauncherDlss5";
constexpr const wchar_t* kDlssnrFile = L"nvngx_dlssnr.dll";
// Where users download nvngx_dlssnr.dll (to be filled in; empty = the window says the link is not set yet).
constexpr const wchar_t* kDlssnrUrl = L"https://t.me/MKJH_ARCHIVE/352";
constexpr int kIdOptiFps = 51; // OptiScaler FPS limit = monitor refresh rate
constexpr int kIdMfg0 = 60; // 60..64: multi frame generation 2x / 3x / 4x / 5x / 6x
constexpr int kFgMenuBase = 5000;
constexpr int kUpscalerMenuBase = 5100;
constexpr int kIdBoostSelect = 13;
constexpr int kBoostMenuBase = 5200;
constexpr int kIdDlgCheck = 20;  // warning dialog: "don't show again"
constexpr int kIdDlgOk = 21;
constexpr int kIdDlgCancel = 22;
constexpr int kIdWarnReset = 23; // show the hidden warnings again
constexpr int kIdPresentSelect = 14;
constexpr int kPresentMenuBase = 5300;
constexpr int kIdLimitTarget = 15;
constexpr int kIdLimitFps = 16;
constexpr int kIdLimitApply = 17;
constexpr int kIdDelaySelect = 18;
constexpr int kIdLimitMode = 19;
constexpr int kIdLimitAuto = 24;
constexpr int kIdLimitOff = 25;
constexpr int kIdBorderless = 26;
constexpr int kIdPictureOn = 27;
constexpr int kIdPictureOff = 28;
constexpr int kIdProfileSave = 29;
constexpr int kIdProfileAuto = 30;
constexpr int kIdUpSelect = 31;
constexpr int kIdUpResize = 32;
constexpr int kIdUpSlider = 33;
constexpr int kIdProfSlot0 = 34; // 34..37: profile slots 1..4
constexpr int kIdProfileLoad = 38;
constexpr int kIdPreset0 = 40; // 40..46: quick modes NO FG, x2 / x3 / x4 (new GPU: Intel MFG), x2 / x3 / x4 (old GPU: Artur)
constexpr int kUpMenuBase = 5900;
const wchar_t* const kFgNames[11] = {
    ui::IntelMfgXellNewGpu,         // 0
    ui::ArturMfgOldNewGpu,          // 1
    ui::NoFg,                            // 2
    ui::IntelMfgXellNewGpu,         // 3 (old "FSR Frame Generation" entry, removed: shown as 0)
    ui::IntelFgXellNewGpu,            // 4 (folder "intel FG")
    ui::ComboMfgOldNewGpu,          // 5
    ui::NukemFgOldNewGpu,           // 6
    ui::Fsr34FgOldNewGpu,          // 7
    ui::NvidiaMfgRtxOnly,            // 8
    ui::FsrFgAntilag2Rdna234,      // 9  folder "FSR FG AL2"
    ui::FsrFgLatencyFlexOldNewGpu // 10 folder "FSR FG LF"
};
// menu order -> mode
const int kFgMenuOrder[10] = { 0, 1, 5, 9, 10, 6, 7, 8, 4, 2 };
const wchar_t* const kProfilesDir = L"Game Profiles"; // next to the launcher: one folder per game
const wchar_t* const kReshadeBundleDir = L"ReShade depth"; // folder next to the launcher containing ready ReShade files
constexpr int kLimitModeMenuBase = 5800;
constexpr int kLimitTargetMenuBase = 5500;
constexpr int kLimitFpsMenuBase = 5600;
constexpr int kDelayMenuBase = 5700;
constexpr int kVsyncMenuBase = 5400;

// ---------------------------------------------------------------------------------------------------------------
// Look
// ---------------------------------------------------------------------------------------------------------------

namespace col
{
inline gp::Color Bg() { return gp::Color(255, 13, 15, 20); }
inline gp::Color Card() { return gp::Color(255, 21, 24, 32); }
inline gp::Color Row() { return gp::Color(255, 27, 31, 42); }
inline gp::Color RowHot() { return gp::Color(255, 34, 39, 53); }
inline gp::Color RowSel() { return gp::Color(255, 38, 45, 66); }
inline gp::Color Line() { return gp::Color(255, 40, 45, 58); }
inline gp::Color Text() { return gp::Color(255, 233, 236, 243); }
inline gp::Color Muted() { return gp::Color(255, 135, 143, 163); }
inline gp::Color Accent() { return gp::Color(255, 108, 140, 255); }
inline gp::Color Accent2() { return gp::Color(255, 56, 214, 220); }
inline gp::Color Green() { return gp::Color(255, 61, 220, 151); }
inline gp::Color Red() { return gp::Color(255, 255, 92, 112); }
inline gp::Color Amber() { return gp::Color(255, 255, 191, 71); }
inline gp::Color White() { return gp::Color(255, 255, 255, 255); }
inline gp::Color Alpha(const gp::Color& c, BYTE a) { return gp::Color(a, c.GetR(), c.GetG(), c.GetB()); }
} // namespace col

void RoundPath(gp::GraphicsPath& p, const gp::RectF& r, float rad)
{
    rad = min(rad, min(r.Width, r.Height) / 2.0f);
    const float d = rad * 2.0f;
    p.AddArc(r.X, r.Y, d, d, 180.0f, 90.0f);
    p.AddArc(r.X + r.Width - d, r.Y, d, d, 270.0f, 90.0f);
    p.AddArc(r.X + r.Width - d, r.Y + r.Height - d, d, d, 0.0f, 90.0f);
    p.AddArc(r.X, r.Y + r.Height - d, d, d, 90.0f, 90.0f);
    p.CloseFigure();
}

void FillRound(gp::Graphics& g, const gp::RectF& r, float rad, const gp::Color& c)
{
    gp::GraphicsPath p;
    RoundPath(p, r, rad);
    gp::SolidBrush b(c);
    g.FillPath(&b, &p);
}

void StrokeRound(gp::Graphics& g, const gp::RectF& r, float rad, const gp::Color& c, float w)
{
    const gp::RectF in(r.X + w / 2.0f, r.Y + w / 2.0f, r.Width - w, r.Height - w);
    gp::GraphicsPath p;
    RoundPath(p, in, max(0.0f, rad - w / 2.0f));
    gp::Pen pen(c, w);
    g.DrawPath(&pen, &p);
}

void GradRound(gp::Graphics& g, const gp::RectF& r, float rad, const gp::Color& a, const gp::Color& b)
{
    gp::GraphicsPath p;
    RoundPath(p, r, rad);
    gp::LinearGradientBrush br(r, a, b, gp::LinearGradientModeForwardDiagonal);
    g.FillPath(&br, &p);
}

// The app's mark: a gradient tile with four capture-frame corners and a dot.
void DrawLogo(gp::Graphics& g, const gp::RectF& r)
{
    GradRound(g, r, r.Width * 0.28f, col::Accent(), col::Accent2());

    const float m = r.Width * 0.25f;
    const float l = r.Width * 0.17f;
    const float x0 = r.X + m;
    const float y0 = r.Y + m;
    const float x1 = r.X + r.Width - m;
    const float y1 = r.Y + r.Height - m;

    gp::Pen pen(col::White(), max(1.5f, r.Width * 0.065f));
    pen.SetStartCap(gp::LineCapRound);
    pen.SetEndCap(gp::LineCapRound);
    pen.SetLineJoin(gp::LineJoinRound);

    gp::PointF a[3];
    a[0] = gp::PointF(x0, y0 + l);
    a[1] = gp::PointF(x0, y0);
    a[2] = gp::PointF(x0 + l, y0);
    g.DrawLines(&pen, a, 3);

    a[0] = gp::PointF(x1 - l, y0);
    a[1] = gp::PointF(x1, y0);
    a[2] = gp::PointF(x1, y0 + l);
    g.DrawLines(&pen, a, 3);

    a[0] = gp::PointF(x1, y1 - l);
    a[1] = gp::PointF(x1, y1);
    a[2] = gp::PointF(x1 - l, y1);
    g.DrawLines(&pen, a, 3);

    a[0] = gp::PointF(x0 + l, y1);
    a[1] = gp::PointF(x0, y1);
    a[2] = gp::PointF(x0, y1 - l);
    g.DrawLines(&pen, a, 3);

    gp::SolidBrush w(col::White());
    const float d = r.Width * 0.16f;
    g.FillEllipse(&w, r.X + (r.Width - d) / 2.0f, r.Y + (r.Height - d) / 2.0f, d, d);
}

// A small paper plane in a round badge (the link to the Telegram channel).
void DrawPlane(gp::Graphics& g, const gp::RectF& r)
{
    GradRound(g, r, r.Width / 2.0f, col::Accent(), col::Accent2());

    auto P = [&](float x, float y) { return gp::PointF(r.X + r.Width * x, r.Y + r.Height * y); };
    gp::PointF pts[4] = { P(0.20f, 0.50f), P(0.80f, 0.24f), P(0.62f, 0.78f), P(0.47f, 0.60f) };
    gp::SolidBrush w(col::White());
    g.FillPolygon(&w, pts, 4);

    gp::Pen fold(gp::Color(255, 60, 120, 200), max(1.0f, r.Width * 0.05f));
    g.DrawLine(&fold, P(0.47f, 0.60f), P(0.80f, 0.24f));
}

// The GitHub mark (Octicons "mark-github", MIT, (c) GitHub Inc.), drawn as one filled outline in the given colour.
// The shape is the 16 x 16 path of the official SVG, as 25 cubic Bezier segments.
void DrawGithubMark(gp::Graphics& g, const gp::RectF& r, const gp::Color& color)
{
    static const gp::PointF kPts[] = { { 8.0f, 0.0f },{ 3.58f, 0.0f },{ 0.0f, 3.58f },{ 0.0f, 8.0f },{ 0.0f, 11.54f },{ 2.29f, 14.53f },{ 5.47f, 15.59f },{ 5.87f, 15.66f },{ 6.02f, 15.42f },{ 6.02f, 15.21f },{ 6.02f, 15.02f },{ 6.01f, 14.39f },{ 6.01f, 13.72f },{ 4.0f, 14.09f },{ 3.48f, 13.23f },{ 3.32f, 12.78f },{ 3.23f, 12.55f },{ 2.84f, 11.84f },{ 2.5f, 11.65f },{ 2.22f, 11.5f },{ 1.82f, 11.13f },{ 2.49f, 11.12f },{ 3.12f, 11.11f },{ 3.57f, 11.7f },{ 3.72f, 11.94f },{ 4.44f, 13.15f },{ 5.59f, 12.81f },{ 6.05f, 12.6f },{ 6.12f, 12.08f },{ 6.33f, 11.73f },{ 6.56f, 11.53f },{ 4.78f, 11.33f },{ 2.92f, 10.64f },{ 2.92f, 7.58f },{ 2.92f, 6.71f },{ 3.23f, 5.99f },{ 3.74f, 5.43f },{ 3.66f, 5.23f },{ 3.38f, 4.41f },{ 3.82f, 3.31f },{ 3.82f, 3.31f },{ 4.49f, 3.1f },{ 6.02f, 4.13f },{ 6.66f, 3.95f },{ 7.34f, 3.86f },{ 8.02f, 3.86f },{ 8.7f, 3.86f },{ 9.38f, 3.95f },{ 10.02f, 4.13f },{ 11.55f, 3.09f },{ 12.22f, 3.31f },{ 12.22f, 3.31f },{ 12.66f, 4.41f },{ 12.38f, 5.23f },{ 12.3f, 5.43f },{ 12.81f, 5.99f },{ 13.12f, 6.7f },{ 13.12f, 7.58f },{ 13.12f, 10.65f },{ 11.25f, 11.33f },{ 9.47f, 11.53f },{ 9.76f, 11.78f },{ 10.01f, 12.26f },{ 10.01f, 13.01f },{ 10.01f, 14.08f },{ 10.0f, 14.94f },{ 10.0f, 15.21f },{ 10.0f, 15.42f },{ 10.15f, 15.67f },{ 10.55f, 15.59f },{ 13.71f, 14.53f },{ 16.0f, 11.53f },{ 16.0f, 8.0f },{ 16.0f, 3.58f },{ 12.42f, 0.0f },{ 8.0f, 0.0f } };
    constexpr int kCount = static_cast<int>(sizeof(kPts) / sizeof(kPts[0]));
    gp::PointF p[kCount];
    for (int i = 0; i < kCount; ++i)
        p[i] = gp::PointF(r.X + kPts[i].X / 16.0f * r.Width, r.Y + kPts[i].Y / 16.0f * r.Height);
    gp::GraphicsPath path;
    path.AddBeziers(p, kCount);
    path.CloseFigure();
    gp::SolidBrush b(color);
    g.FillPath(&b, &path);
}

// ---------------------------------------------------------------------------------------------------------------
// Key names
// ---------------------------------------------------------------------------------------------------------------

std::wstring KeyName(UINT vk)
{
    if (vk >= VK_F1 && vk <= VK_F24)
        return L"F" + std::to_wstring(vk - VK_F1 + 1);
    if ((vk >= '0' && vk <= '9') || (vk >= 'A' && vk <= 'Z'))
        return std::wstring(1, static_cast<wchar_t>(vk));
    if (vk >= VK_NUMPAD0 && vk <= VK_NUMPAD9)
        return L"Num " + std::to_wstring(vk - VK_NUMPAD0);

    switch (vk)
    {
    case VK_INSERT: return L"Insert";
    case VK_DELETE: return L"Delete";
    case VK_HOME: return L"Home";
    case VK_END: return L"End";
    case VK_PRIOR: return L"Page Up";
    case VK_NEXT: return L"Page Down";
    case VK_LEFT: return L"Left";
    case VK_RIGHT: return L"Right";
    case VK_UP: return L"Up";
    case VK_DOWN: return L"Down";
    case VK_PAUSE: return L"Pause";
    case VK_SNAPSHOT: return L"Print Screen";
    case VK_SCROLL: return L"Scroll Lock";
    case VK_BACK: return L"Backspace";
    case VK_TAB: return L"Tab";
    case VK_RETURN: return L"Enter";
    case VK_SPACE: return L"Space";
    case VK_MULTIPLY: return L"Num *";
    case VK_ADD: return L"Num +";
    case VK_SUBTRACT: return L"Num -";
    case VK_DIVIDE: return L"Num /";
    case VK_DECIMAL: return L"Num .";
    default: break;
    }

    LONG lp = static_cast<LONG>(MapVirtualKeyW(vk, MAPVK_VK_TO_VSC) << 16);
    wchar_t buf[64] = {};
    if (GetKeyNameTextW(lp, buf, 64) > 0)
        return buf;
    return L"Key " + std::to_wstring(vk);
}

std::wstring HotkeyText(const oc::Hotkey& h)
{
    std::wstring s;
    if (h.mods & MOD_CONTROL)
        s += L"Ctrl + ";
    if (h.mods & MOD_ALT)
        s += L"Alt + ";
    if (h.mods & MOD_SHIFT)
        s += L"Shift + ";
    if (h.mods & MOD_WIN)
        s += L"Win + ";
    return s + KeyName(h.vk);
}

bool IsModifierVk(UINT vk)
{
    return vk == VK_SHIFT || vk == VK_CONTROL || vk == VK_MENU || vk == VK_LSHIFT || vk == VK_RSHIFT || vk == VK_LCONTROL ||
           vk == VK_RCONTROL || vk == VK_LMENU || vk == VK_RMENU || vk == VK_LWIN || vk == VK_RWIN;
}

// ---------------------------------------------------------------------------------------------------------------
// Data
// ---------------------------------------------------------------------------------------------------------------

struct GameItem
{
    HWND hwnd = nullptr;
    std::wstring title;
    std::wstring exe;
    std::wstring fullExe;
    int w = 0;
    int h = 0;
    bool minimized = false;
    bool fills = false; // covers a whole monitor: borderless or fullscreen-sized, the usual look of a game
    HICON icon = nullptr; // owned by the window, never destroyed here
};

// The OptiScaler.ini lines the DLSS 5 window writes. Filled in as the settings are decided:
// { section, key, value }. Nothing is written for an empty list.
struct Hit
{
    gp::RectF r;
    int id = 0;
};

enum class Note
{
    Info,
    Ok,
    Error
};

// Ids of the clickable things.
constexpr int kIdNone = 0;
constexpr int kIdRefresh = 1;
constexpr int kIdVsync = 2;
constexpr int kIdStart = 3;
constexpr int kIdReset = 4;
constexpr int kIdLink = 5;
constexpr int kIdShortcut0 = 100; // + shortcut number
constexpr int kIdRow0 = 1000;     // + row number

enum Shortcut
{
    kScMenuKey,
    kScFpsKey,
    kScFpsCycleKey,
    kScFgKey,
    kScStop,     // Ctrl/Shift/Alt + key (handled by the host)
    kScLauncher, // Ctrl/Shift/Alt + key (handled by this window)
    kScCount
};

struct ShortcutDef
{
    const wchar_t* label;
    const wchar_t* hint;
};

const ShortcutDef kShortcutDefs[kScCount] = {
    { ui::OptiscalerMenu, ui::OpenCloseTheMenu },
    { ui::FpsOverlay, ui::ShowHideIt },
    { ui::OverlayType, ui::ChangeWhatItShows },
    { ui::FrameGeneration, ui::TurnItOnOff },
    { ui::StopCapture, ui::CloseTheCaptureHost },
    { ui::ThisWindow, ui::BringTheLauncherOnTop },
};

struct EnumCtx
{
    std::vector<GameItem>* out;
    const std::vector<GameItem>* old;
    DWORD selfPid;
    DWORD hostPid;
};

// OptiScaler reads its own hotkeys from OptiScaler.ini ([Menu]); keep that file in step with the launcher.
void WriteOptiScalerKeys(const oc::Settings& s)
{
    const std::wstring ini = oc::ExeDir() + L"OptiScaler.ini";
    if (GetFileAttributesW(ini.c_str()) == INVALID_FILE_ATTRIBUTES)
        return;

    auto put = [&](const wchar_t* key, UINT vk)
    {
        wchar_t buf[16] = {};
        swprintf_s(buf, L"0x%02X", vk);
        WritePrivateProfileStringW(L"Menu", key, buf, ini.c_str());
    };
    put(L"ShortcutKey", s.menuKey);
    put(L"FpsShortcutKey", s.fpsKey);
    put(L"FpsCycleShortcutKey", s.fpsCycleKey);
    put(L"FGShortcutKey", s.fgKey);
}


// ---- OptiScaler.ini: ForceVsync (auto / true / false) ----
// The key is edited in place (whatever section it lives in); the rest of the file is left byte for byte as it was.
bool IsIniKeyLine(const std::string& line, const char* key, size_t& valueStart, size_t& valueEnd)
{
    size_t i = 0;
    while (i < line.size() && (line[i] == ' ' || line[i] == '\t')) ++i;
    if (i >= line.size() || line[i] == ';' || line[i] == '#') return false;
    const size_t kl = strlen(key);
    if (line.size() < i + kl || _strnicmp(line.c_str() + i, key, kl) != 0) return false;
    size_t j = i + kl;
    while (j < line.size() && (line[j] == ' ' || line[j] == '\t')) ++j;
    if (j >= line.size() || line[j] != '=') return false;
    ++j;
    valueStart = j;
    valueEnd = line.size();
    while (valueEnd > valueStart && (line[valueEnd - 1] == '\r' || line[valueEnd - 1] == '\n')) --valueEnd;
    return true;
}

bool ReadFileBytes(const std::wstring& path, std::string& out)
{
    HANDLE h = CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_EXISTING,
                           FILE_ATTRIBUTE_NORMAL, nullptr);
    if (h == INVALID_HANDLE_VALUE) return false;
    LARGE_INTEGER sz{};
    GetFileSizeEx(h, &sz);
    out.assign(static_cast<size_t>(sz.QuadPart), '\0');
    DWORD got = 0;
    const bool ok = out.empty() || ReadFile(h, out.data(), static_cast<DWORD>(out.size()), &got, nullptr);
    CloseHandle(h);
    return ok;
}

// Returns 1 for true, 0 for false, -1 for auto / missing.
int ReadForceVsync()
{
    std::string data;
    if (!ReadFileBytes(oc::ExeDir() + L"OptiScaler.ini", data)) return -1;
    size_t pos = 0;
    while (pos < data.size())
    {
        size_t eol = data.find('\n', pos);
        if (eol == std::string::npos) eol = data.size();
        const std::string line = data.substr(pos, eol - pos);
        size_t vs = 0, ve = 0;
        if (IsIniKeyLine(line, "ForceVsync", vs, ve))
        {
            std::string v = line.substr(vs, ve - vs);
            const size_t semi = v.find(';');
            if (semi != std::string::npos) v.resize(semi);
            while (!v.empty() && (v.back() == ' ' || v.back() == '\t')) v.pop_back();
            size_t a = 0;
            while (a < v.size() && (v[a] == ' ' || v[a] == '\t')) ++a;
            v = v.substr(a);
            if (_stricmp(v.c_str(), "true") == 0) return 1;
            if (_stricmp(v.c_str(), "false") == 0) return 0;
            return -1;
        }
        pos = eol + 1;
    }
    return -1;
}

// Sets "key = value" in OptiScaler.ini in place (the file is otherwise left as it was). A missing key is added to the
// given section through the INI API.
bool SetIniValue(const char* key, const char* value, const wchar_t* section)
{
    const std::wstring ini = oc::ExeDir() + L"OptiScaler.ini";
    std::string data;
    if (!ReadFileBytes(ini, data)) return false;

    std::string out;
    bool done = false;
    size_t pos = 0;
    while (pos < data.size())
    {
        size_t eol = data.find('\n', pos);
        const bool hasEol = eol != std::string::npos;
        if (!hasEol) eol = data.size();
        std::string line = data.substr(pos, eol - pos);
        size_t vs = 0, ve = 0;
        if (!done && IsIniKeyLine(line, key, vs, ve))
        {
            std::string tail;
            const size_t semi = line.find(';', vs);
            if (semi != std::string::npos) tail = " " + line.substr(semi);
            else if (line.size() > ve) tail = line.substr(ve);
            line = line.substr(0, vs) + " " + value + tail;
            done = true;
        }
        out += line;
        if (hasEol) out += '\n';
        pos = eol + 1;
    }
    if (!done)
    {
        const std::wstring wk(key, key + strlen(key));
        const std::wstring wv(value, value + strlen(value));
        return WritePrivateProfileStringW(section, wk.c_str(), wv.c_str(), ini.c_str()) != FALSE;
    }

    HANDLE h = CreateFileW(ini.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (h == INVALID_HANDLE_VALUE) return false;
    DWORD wrote = 0;
    const bool ok = WriteFile(h, out.data(), static_cast<DWORD>(out.size()), &wrote, nullptr) && wrote == out.size();
    CloseHandle(h);
    return ok;
}

// OptiScaler only looks at ForceVsync when [V-Sync] OverrideVsync is on, so both are written together.
bool WriteForceVsync(int mode) // -1 auto, 0 off (ForceVsync=false), 1 on / SyncInterval 1, 2 on / SyncInterval 0
{
    const char* force = mode < 0 ? "auto" : (mode == 0 ? "false" : "true");
    const char* sync = mode < 0 ? "auto" : ((mode == 1) ? "1" : "0");
    bool ok = SetIniValue("OverrideVsync", mode < 0 ? "auto" : "true", L"V-Sync");
    ok = SetIniValue("ForceVsync", force, L"V-Sync") && ok;
    ok = SetIniValue("SyncInterval", sync, L"V-Sync") && ok;
    return ok;
}

HICON IconOf(HWND h)
{
    DWORD_PTR res = 0;
    if (SendMessageTimeoutW(h, WM_GETICON, ICON_BIG, 0, SMTO_ABORTIFHUNG | SMTO_NORMAL, 40, &res) && res)
        return reinterpret_cast<HICON>(res);
    if (SendMessageTimeoutW(h, WM_GETICON, ICON_SMALL2, 0, SMTO_ABORTIFHUNG | SMTO_NORMAL, 40, &res) && res)
        return reinterpret_cast<HICON>(res);

    HICON ic = reinterpret_cast<HICON>(GetClassLongPtrW(h, GCLP_HICON));
    if (!ic)
        ic = reinterpret_cast<HICON>(GetClassLongPtrW(h, GCLP_HICONSM));
    return ic;
}

BOOL CALLBACK EnumWindowsProc(HWND h, LPARAM lp)
{
    auto* c = reinterpret_cast<EnumCtx*>(lp);

    if (!IsWindowVisible(h))
        return TRUE;

    wchar_t title[256] = {};
    if (GetWindowTextW(h, title, 256) <= 0)
        return TRUE;

    const LONG_PTR ex = GetWindowLongPtrW(h, GWL_EXSTYLE);
    if (ex & WS_EX_TOOLWINDOW)
        return TRUE;
    if (GetWindow(h, GW_OWNER) != nullptr && !(ex & WS_EX_APPWINDOW))
        return TRUE;

    DWORD cloaked = 0; // DWMWA_CLOAKED: windows of suspended store apps and other virtual desktops
    if (SUCCEEDED(DwmGetWindowAttribute(h, 14, &cloaked, sizeof(cloaked))) && cloaked != 0)
        return TRUE;

    DWORD pid = 0;
    GetWindowThreadProcessId(h, &pid);
    if (pid == c->selfPid || (c->hostPid != 0 && pid == c->hostPid))
        return TRUE;

    wchar_t cls[64] = {};
    GetClassNameW(h, cls, 64);
    if (!wcscmp(cls, L"Progman") || !wcscmp(cls, L"WorkerW") || !wcscmp(cls, L"Shell_TrayWnd") ||
        !wcscmp(cls, kOverlayClass))
        return TRUE;

    GameItem it;
    it.hwnd = h;
    it.title = title;
    it.minimized = IsIconic(h) != FALSE;

    RECT r{};
    if (it.minimized)
    {
        WINDOWPLACEMENT wp{};
        wp.length = sizeof(wp);
        GetWindowPlacement(h, &wp);
        r = wp.rcNormalPosition;
    }
    else
    {
        GetWindowRect(h, &r);
    }
    it.w = r.right - r.left;
    it.h = r.bottom - r.top;
    if (it.w < 200 || it.h < 120)
        return TRUE;

    if (!it.minimized)
    {
        MONITORINFO mi{};
        mi.cbSize = sizeof(mi);
        if (GetMonitorInfoW(MonitorFromWindow(h, MONITOR_DEFAULTTONEAREST), &mi))
        {
            const RECT& m = mi.rcMonitor;
            it.fills = r.left <= m.left + 2 && r.top <= m.top + 2 && r.right >= m.right - 2 && r.bottom >= m.bottom - 2;
        }
    }

    HANDLE p = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);
    if (p)
    {
        wchar_t path[MAX_PATH] = {};
        DWORD len = MAX_PATH;
        if (QueryFullProcessImageNameW(p, 0, path, &len))
        {
            std::wstring s = path;
            it.fullExe = s;
            const size_t cut = s.find_last_of(L"\\/");
            it.exe = cut == std::wstring::npos ? s : s.substr(cut + 1);
        }
        CloseHandle(p);
    }

    // Reuse the icon found last time (asking a window for it can take a moment).
    if (c->old)
    {
        for (const auto& o : *c->old)
        {
            if (o.hwnd == h)
            {
                it.icon = o.icon;
                break;
            }
        }
    }
    if (!it.icon)
        it.icon = IconOf(h);

    c->out->push_back(std::move(it));
    return TRUE;
}

BOOL CALLBACK CloseOverlayProc(HWND h, LPARAM lp)
{
    DWORD pid = 0;
    GetWindowThreadProcessId(h, &pid);
    if (pid != static_cast<DWORD>(lp))
        return TRUE;

    wchar_t cls[64] = {};
    GetClassNameW(h, cls, 64);
    if (!wcscmp(cls, kOverlayClass))
        PostMessageW(h, WM_CLOSE, 0, 0);
    return TRUE;
}

// Windows only lets the foreground process give the foreground away; attaching to its input thread gets around that
// for a window the user just asked for.
void ForceForeground(HWND h)
{
    HWND fg = GetForegroundWindow();
    const DWORD fgThread = fg ? GetWindowThreadProcessId(fg, nullptr) : 0;
    const DWORD me = GetCurrentThreadId();

    bool attached = false;
    if (fgThread != 0 && fgThread != me)
        attached = AttachThreadInput(me, fgThread, TRUE) != FALSE;

    keybd_event(VK_MENU, 0, 0, 0);
    keybd_event(VK_MENU, 0, KEYEVENTF_KEYUP, 0);

    BringWindowToTop(h);
    SetForegroundWindow(h);

    if (attached)
        AttachThreadInput(me, fgThread, FALSE);
}

HICON MakeAppIcon(int px)
{
    gp::Bitmap bmp(px, px, PixelFormat32bppARGB);
    gp::Graphics g(&bmp);
    g.SetSmoothingMode(gp::SmoothingModeAntiAlias);
    DrawLogo(g, gp::RectF(1.0f, 1.0f, static_cast<float>(px) - 2.0f, static_cast<float>(px) - 2.0f));

    HICON ic = nullptr;
    bmp.GetHICON(&ic);
    return ic;
}

bool FileExists(const std::wstring& f)
{
    return GetFileAttributesW(f.c_str()) != INVALID_FILE_ATTRIBUTES;
}

bool IsGameReshadeInstalled(const std::wstring& gameExePath)
{
    if (gameExePath.empty())
        return false;
    const size_t slash = gameExePath.find_last_of(L"\\/");
    if (slash == std::wstring::npos)
        return false;
    const std::wstring gameDir = gameExePath.substr(0, slash + 1);
    const std::wstring f1 = gameDir + L"OptiCapture.ini";
    const std::wstring f2 = gameDir + L"OptiCaptureDepth.addon64";
    const std::wstring f3 = gameDir + L"reshade-shaders\\Shaders\\OptiCaptureDepth.fx";
    return FileExists(f1) && FileExists(f2) && FileExists(f3);
}

bool IsProcessRunningByExe(const std::wstring& exeName)
{
    if (exeName.empty())
        return false;
    HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snap == INVALID_HANDLE_VALUE)
        return false;
    PROCESSENTRY32W pe{};
    pe.dwSize = sizeof(pe);
    bool found = false;
    if (Process32FirstW(snap, &pe))
    {
        do
        {
            if (_wcsicmp(pe.szExeFile, exeName.c_str()) == 0)
            {
                found = true;
                break;
            }
        } while (Process32NextW(snap, &pe));
    }
    CloseHandle(snap);
    return found;
}

// ---------------------------------------------------------------------------------------------------------------
// The window
// ---------------------------------------------------------------------------------------------------------------

// ---------------------------------------------------------------------------------------------------------------
// Warnings (things outside this program that limit the frame rate)
// ---------------------------------------------------------------------------------------------------------------

enum Warn
{
    kWarnRtssMissing, // the frame limiter needs RivaTuner Statistics Server (RTSS) to be installed
    kWarnProfile,     // a saved game profile exists for the game that was just picked
    kWarnCount
};

const wchar_t* const kWarnKeys[kWarnCount] = { L"HideRtssMissing", L"HideProfile" }; // OptiCaptureHost.ini [Warnings]
const wchar_t* const kWarnTitles[kWarnCount] = { ui::RivatunerRtssIsNotInstalled, ui::SavedGameProfileFound };

// Folder of the running RivaTuner Statistics Server, with a trailing backslash (empty when it is not running).
std::wstring RtssDir()
{
    std::wstring dir;
    HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snap == INVALID_HANDLE_VALUE)
        return dir;

    PROCESSENTRY32W pe{};
    pe.dwSize = sizeof(pe);
    if (Process32FirstW(snap, &pe))
    {
        do
        {
            if (_wcsicmp(pe.szExeFile, L"RTSS.exe") != 0)
                continue;
            if (HANDLE p = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pe.th32ProcessID))
            {
                wchar_t path[MAX_PATH * 2] = {};
                DWORD n = MAX_PATH * 2;
                if (QueryFullProcessImageNameW(p, 0, path, &n))
                {
                    dir = path;
                    const size_t cut = dir.find_last_of(L"\\/");
                    dir = cut == std::wstring::npos ? std::wstring() : dir.substr(0, cut + 1);
                }
                CloseHandle(p);
            }
            break;
        } while (Process32NextW(snap, &pe));
    }
    CloseHandle(snap);
    return dir;
}

// Install folder of RTSS (trailing backslash) when RTSSHooks64.dll is there, else empty. Not installed = empty.
std::wstring RtssInstallDir()
{
    std::vector<std::wstring> cands;
    cands.push_back(RtssDir());

    const DWORD flags[] = { RRF_RT_REG_SZ | RRF_SUBKEY_WOW6432KEY, RRF_RT_REG_SZ | RRF_SUBKEY_WOW6464KEY };
    for (DWORD f : flags)
    {
        wchar_t buf[MAX_PATH * 2] = {};
        DWORD cb = sizeof(buf) - sizeof(wchar_t);
        if (RegGetValueW(HKEY_LOCAL_MACHINE, L"SOFTWARE\\Unwinder\\RTSS", L"InstallDir", f, nullptr, buf, &cb) == ERROR_SUCCESS)
            cands.push_back(buf);
    }
    for (const wchar_t* var : { L"ProgramFiles(x86)", L"ProgramFiles" })
    {
        wchar_t pf[MAX_PATH] = {};
        if (GetEnvironmentVariableW(var, pf, MAX_PATH) > 0)
            cands.push_back(std::wstring(pf) + L"\\RivaTuner Statistics Server");
    }

    for (std::wstring d : cands)
    {
        if (d.empty())
            continue;
        if (d.back() != L'\\' && d.back() != L'/')
            d += L'\\';
        if (FileExists(d + L"RTSSHooks64.dll"))
            return d;
    }
    return std::wstring();
}

std::string Narrow(const std::wstring& w)
{
    if (w.empty())
        return std::string();
    const int n = WideCharToMultiByte(CP_ACP, 0, w.c_str(), -1, nullptr, 0, nullptr, nullptr);
    std::string r(static_cast<size_t>(n > 0 ? n : 1), '\0');
    if (n > 0)
        WideCharToMultiByte(CP_ACP, 0, w.c_str(), -1, r.data(), n, nullptr, nullptr);
    r.resize(strlen(r.c_str()));
    return r;
}

// Small text log next to the program (rtss_log.txt) so a limiter that does nothing can be diagnosed.
void RtssLog(const std::wstring& line)
{
    const std::wstring path = oc::ExeDir() + L"rtss_log.txt";
    HANDLE f = CreateFileW(path.c_str(), FILE_APPEND_DATA, FILE_SHARE_READ, nullptr, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (f == INVALID_HANDLE_VALUE)
        return;
    SYSTEMTIME t{};
    GetLocalTime(&t);
    wchar_t head[40];
    swprintf(head, 40, L"%02d:%02d:%02d  ", t.wHour, t.wMinute, t.wSecond);
    const std::wstring w = std::wstring(head) + line + L"\r\n";
    const int n = WideCharToMultiByte(CP_UTF8, 0, w.c_str(), -1, nullptr, 0, nullptr, nullptr);
    std::string u(static_cast<size_t>(n > 0 ? n : 1), '\0');
    if (n > 0)
        WideCharToMultiByte(CP_UTF8, 0, w.c_str(), -1, u.data(), n, nullptr, nullptr);
    DWORD wr = 0;
    WriteFile(f, u.c_str(), static_cast<DWORD>(strlen(u.c_str())), &wr, nullptr);
    CloseHandle(f);
}

// The profile functions exported by RTSSHooks64.dll (the same ones RTSS and MSI Afterburner use for their settings).
struct RtssApi
{
    typedef void (*LoadFn)(LPCSTR);
    typedef void (*SaveFn)(LPCSTR);
    typedef BOOL (*GetFn)(LPCSTR, LPBYTE, DWORD);
    typedef BOOL (*SetFn)(LPCSTR, LPBYTE, DWORD);
    typedef void (*UpdateFn)();

    HMODULE mod = nullptr;
    LoadFn load = nullptr;
    SaveFn save = nullptr;
    GetFn get = nullptr;
    SetFn set = nullptr;
    UpdateFn update = nullptr;

    bool Open(const std::wstring& dir)
    {
        if (mod)
            return true;
        mod = LoadLibraryExW((dir + L"RTSSHooks64.dll").c_str(), nullptr, LOAD_WITH_ALTERED_SEARCH_PATH);
        if (!mod)
            return false;
        load = reinterpret_cast<LoadFn>(GetProcAddress(mod, "LoadProfile"));
        save = reinterpret_cast<SaveFn>(GetProcAddress(mod, "SaveProfile"));
        get = reinterpret_cast<GetFn>(GetProcAddress(mod, "GetProfileProperty"));
        set = reinterpret_cast<SetFn>(GetProcAddress(mod, "SetProfileProperty"));
        update = reinterpret_cast<UpdateFn>(GetProcAddress(mod, "UpdateProfiles"));
        if (!load || !save || !get || !set || !update)
        {
            FreeLibrary(mod);
            mod = nullptr;
            return false;
        }
        return true;
    }

    // value of a DWORD property of a profile (profile "" = Global), or def when it does not exist
    int Read(const std::string& profile, const char* prop, int def)
    {
        load(profile.c_str());
        DWORD v = 0;
        if (!get(prop, reinterpret_cast<LPBYTE>(&v), sizeof(v)))
            return def;
        return static_cast<int>(v);
    }

    // sets limit (fps), denominator and limiter mode (0 = async, 1 = front edge sync) of a profile and checks the result
    bool Write(const std::string& profile, int limit, int denominator, int syncLimiter)
    {
        load(profile.c_str());
        DWORD d = static_cast<DWORD>(denominator), l = static_cast<DWORD>(limit), m = static_cast<DWORD>(syncLimiter);
        const BOOL r1 = set("FramerateLimitDenominator", reinterpret_cast<LPBYTE>(&d), sizeof(d));
        const BOOL r2 = set("FramerateLimit", reinterpret_cast<LPBYTE>(&l), sizeof(l));
        const BOOL r3 = set("SyncLimiter", reinterpret_cast<LPBYTE>(&m), sizeof(m));
        save(profile.c_str());
        update();
        const int back = Read(profile, "FramerateLimit", -1);
        const int backDen = Read(profile, "FramerateLimitDenominator", denominator);
        const int backMode = Read(profile, "SyncLimiter", syncLimiter);
        RtssLog(L"write profile \"" + std::wstring(profile.begin(), profile.end()) + L"\": limit " + std::to_wstring(limit) + L" den " +
                std::to_wstring(denominator) + L" mode " + std::to_wstring(syncLimiter) + L" -> set results " + std::to_wstring(r1) +
                std::to_wstring(r2) + std::to_wstring(r3) + L", read back limit " + std::to_wstring(back) + L" den " +
                std::to_wstring(backDen) + L" mode " + std::to_wstring(backMode));
        // all three must have arrived (a change of only the denominator or the mode keeps the same limit number)
        return back == limit && backDen == denominator && backMode == syncLimiter;
    }
};

// Refresh rate of the monitor the window is on (the primary monitor when there is no window).
int RefreshRateOf(HWND wnd)
{
    MONITORINFOEXW mi{};
    mi.cbSize = sizeof(mi);
    HMONITOR mon = MonitorFromWindow(wnd ? wnd : GetDesktopWindow(), MONITOR_DEFAULTTOPRIMARY);
    DEVMODEW dm{};
    dm.dmSize = sizeof(dm);
    if (GetMonitorInfoW(mon, &mi) && EnumDisplaySettingsW(mi.szDevice, ENUM_CURRENT_SETTINGS, &dm) && dm.dmDisplayFrequency > 1)
        return static_cast<int>(dm.dmDisplayFrequency);
    return 60;
}

// Result of one limiter run.
struct LimiterOut
{
    int code = 0;       // 0 done, 1 RTSS did not take it (needs administrator rights / RTSS not ready), 2 RTSS not installed, 3 bad input
    std::wstring info;  // short text under the Apply button
    std::wstring note;  // status line
};

bool IsElevated()
{
    HANDLE tok = nullptr;
    bool r = false;
    if (OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &tok))
    {
        TOKEN_ELEVATION te{};
        DWORD n = 0;
        if (GetTokenInformation(tok, TokenElevation, &te, sizeof(te), &n))
            r = te.TokenIsElevated != 0;
        CloseHandle(tok);
    }
    return r;
}

// Base name of this program (RTSS profile name of the launcher itself).
std::string OwnExeName()
{
    wchar_t path[MAX_PATH * 2] = {};
    GetModuleFileNameW(nullptr, path, MAX_PATH * 2);
    std::wstring w = path;
    const size_t cut = w.find_last_of(L"\\/");
    return Narrow(cut == std::wstring::npos ? w : w.substr(cut + 1));
}

// The RTSS frame limiter. Only one profile is changed at a time; its old values are kept in the .ini and put back when
// the limiter is switched off or moved to another target. hz = refresh rate of the game's monitor.
LimiterOut DoLimiterImpl(RtssApi& api, const oc::Settings& st, const std::wstring& gameExe, int hz, bool off);

// The limiter as asked. When the limiter target is not the host, the host program's own RTSS profile (OptiCaptureHost.exe) is put to limit 0.
LimiterOut DoLimiter(RtssApi& api, const oc::Settings& st, const std::wstring& gameExe, int hz, bool off)
{
    LimiterOut out = DoLimiterImpl(api, st, gameExe, hz, off);
    if (api.mod && st.limitTarget != 2)
    {
        const std::string host = Narrow(kHostExe);
        api.Write(host, 0, 1, api.Read(host, "SyncLimiter", 0));
    }
    return out;
}

LimiterOut DoLimiterImpl(RtssApi& api, const oc::Settings& st, const std::wstring& gameExe, int hz, bool off)
{
    LimiterOut out;
    const std::wstring ini = oc::SettingsPath();
    const std::wstring dir = RtssInstallDir();
    RtssLog(L"apply: RTSS folder \"" + dir + L"\", running folder \"" + RtssDir() + L"\", elevated " + std::to_wstring(IsElevated()));

    if (dir.empty() || !api.Open(dir))
    {
        RtssLog(L"RTSS missing or RTSSHooks64.dll could not be loaded (error " + std::to_wstring(GetLastError()) + L")");
        if (st.limitTarget != 0)
        {
            out.code = 2;
            out.note = ui::RivatunerRtssIsNotInstalled2;
            return out;
        }
        out.info = ui::LimiterOff;
        out.note = ui::FrameLimiterIsOff;
        return out;
    }

    // the launcher itself must not get the RTSS on-screen display (it would draw over the launcher)
    {
        const std::string me = OwnExeName();
        api.load(me.c_str());
        DWORD zero = 0;
        api.set("EnableOSD", reinterpret_cast<LPBYTE>(&zero), sizeof(zero));
        api.save(me.c_str());
        api.update();
        RtssLog(L"launcher profile \"" + std::wstring(me.begin(), me.end()) + L"\": overlay off");
    }

    // Off: set the limit of the profile that was limited, and of the profile the current choice points at, to 0.
    if (off || st.limitTarget == 0)
    {
        std::vector<std::string> names;
        if (GetPrivateProfileIntW(L"Rtss", L"Has", 0, ini.c_str()) != 0)
        {
            wchar_t prof[260] = {};
            GetPrivateProfileStringW(L"Rtss", L"Profile", L"", prof, 260, ini.c_str());
            names.push_back(Narrow(prof));
        }
        if (st.limitTarget == 1 && !gameExe.empty())
            names.push_back(Narrow(gameExe));
        else if (st.limitTarget == 2)
            names.push_back(Narrow(kHostExe));
        else if (st.limitTarget == 3)
            names.push_back(std::string());
        std::sort(names.begin(), names.end());
        names.erase(std::unique(names.begin(), names.end()), names.end());

        bool allOk = true;
        for (const std::string& n : names)
            allOk = api.Write(n, 0, 1, api.Read(n, "SyncLimiter", 0)) && allOk;
        WritePrivateProfileStringW(L"Rtss", L"Has", L"0", ini.c_str());
        out.info = ui::LimiterOff;
        if (allOk)
        {
            out.note = ui::FrameLimiterIsOffLimitSetTo;
        }
        else
        {
            out.code = 1;
            out.note = ui::RtssDidNotTakeTheChange;
        }
        return out;
    }

    // Every Apply first switches the limiter off (limit 0) and waits a moment, then applies the new value. RTSS does not
    // notice a profile that is rewritten twice within a moment, and an Off followed by an Apply always worked.
    if (!off && st.limitTarget != 0)
    {
        const LimiterOut o = DoLimiterImpl(api, st, gameExe, hz, true);
        if (o.code == 1 && !IsElevated())
            return o; // not allowed to write: the caller repeats everything from an elevated copy
        Sleep(1100);
    }

    // Same profile as the last Apply (only the number or the mode changed): overwrite it in one go and keep the values
    // saved from before the first Apply. Restoring and writing again within a moment is not noticed by RTSS.
    bool sameProfile = false;
    if (GetPrivateProfileIntW(L"Rtss", L"Has", 0, ini.c_str()) != 0)
    {
        wchar_t prev[260] = {};
        GetPrivateProfileStringW(L"Rtss", L"Profile", L"", prev, 260, ini.c_str());
        std::wstring want;
        if (st.limitTarget == 1)
            want = gameExe;
        else if (st.limitTarget == 2)
            want = kHostExe;
        sameProfile = st.limitTarget != 0 && _wcsicmp(prev, want.c_str()) == 0;
    }

    // 1) put back whatever an earlier Apply changed (another profile)
    if (!sameProfile && GetPrivateProfileIntW(L"Rtss", L"Has", 0, ini.c_str()) != 0)
    {
        wchar_t prof[260] = {};
        GetPrivateProfileStringW(L"Rtss", L"Profile", L"", prof, 260, ini.c_str());
        const int oLimit = GetPrivateProfileIntW(L"Rtss", L"OrigLimit", 0, ini.c_str());
        const int oDen = GetPrivateProfileIntW(L"Rtss", L"OrigDen", 1, ini.c_str());
        const int oSync = GetPrivateProfileIntW(L"Rtss", L"OrigSync", 0, ini.c_str());
        api.Write(Narrow(prof), oLimit, oDen < 1 ? 1 : oDen, oSync);
        WritePrivateProfileStringW(L"Rtss", L"Has", L"0", ini.c_str());
    }

    if (st.limitTarget == 0)
    {
        out.info = ui::LimiterOff;
        out.note = ui::FrameLimiterIsOff;
        return out;
    }

    // 2) which profile
    std::wstring profile; // empty = Global
    if (st.limitTarget == 1)
    {
        if (gameExe.empty())
        {
            out.code = 3;
            out.note = ui::PickAGameInTheListFirst;
            return out;
        }
        profile = gameExe;
    }
    else if (st.limitTarget == 2)
    {
        profile = kHostExe;
    }

    // 3) which number: limit = num / den (half of 75 Hz is exactly 37.5 = 75 / 2)
    int num = st.limitFps, den = 1;
    if (st.limitFps == 0)
        num = hz;
    else if (st.limitFps <= -2 && st.limitFps >= -4)
    {
        // refresh / 2, / 3 or / 4 as an exact fraction (75 Hz / 2 = 75/2 = 37.5, 75 Hz / 3 = 25)
        num = hz;
        den = -st.limitFps;
        int a = num, b = den;
        while (b != 0)
        {
            const int t = a % b;
            a = b;
            b = t;
        }
        if (a > 1)
        {
            num /= a;
            den /= a;
        }
    }
    if (num < 1)
        num = 30;

    const std::string name = Narrow(profile);
    if (!sameProfile)
    {
        const int oLimit = api.Read(name, "FramerateLimit", 0);
        const int oDen = api.Read(name, "FramerateLimitDenominator", 1);
        const int oSync = api.Read(name, "SyncLimiter", 0);
        WritePrivateProfileStringW(L"Rtss", L"Profile", profile.c_str(), ini.c_str());
        WritePrivateProfileStringW(L"Rtss", L"OrigLimit", std::to_wstring(oLimit).c_str(), ini.c_str());
        WritePrivateProfileStringW(L"Rtss", L"OrigDen", std::to_wstring(oDen).c_str(), ini.c_str());
        WritePrivateProfileStringW(L"Rtss", L"OrigSync", std::to_wstring(oSync).c_str(), ini.c_str());
        WritePrivateProfileStringW(L"Rtss", L"Has", L"1", ini.c_str());
    }

    static const wchar_t* const modeNames[] = { ui::Async, ui::FrontEdgeSync, ui::BackEdgeSync };
    const int mode = (st.limitMode >= 0 && st.limitMode <= 2) ? st.limitMode : 1;
    const std::wstring who = profile.empty() ? std::wstring(ui::Global) : profile;
    if (!api.Write(name, num, den, mode))
    {
        if (!sameProfile)
            WritePrivateProfileStringW(L"Rtss", L"Has", L"0", ini.c_str());
        out.code = 1;
        out.info = ui::RtssDidNotAcceptTheChange;
        out.note = ui::RtssDidNotTakeTheLimit;
        return out;
    }

    std::wstring fpsText;
    {
        wchar_t tb[32];
        swprintf(tb, 32, L"%.2f", static_cast<double>(num) / static_cast<double>(den));
        fpsText = tb;
        while (!fpsText.empty() && fpsText.back() == L'0')
            fpsText.pop_back();
        if (!fpsText.empty() && fpsText.back() == L'.')
            fpsText.pop_back();
    }
    out.info = std::format(ui::LimiterSummary, who, fpsText, modeNames[mode]);
    out.note = std::format(ui::RtssLimitSet, who, fpsText, modeNames[mode]) +
               (RtssDir().empty() ? ui::RtssNotRunning : L"");
    return out;
}

// After the capture: put the limiter's profile back to the values it had before Apply (not just limit 0).
// 0 done (or nothing to do), 1 RTSS did not take it (needs administrator rights), 2 RTSS not installed.
int DoLimitRestore(RtssApi& api)
{
    const std::wstring ini = oc::SettingsPath();
    if (GetPrivateProfileIntW(L"Rtss", L"Has", 0, ini.c_str()) == 0)
        return 0;
    const std::wstring dir = RtssInstallDir();
    if (dir.empty() || !api.Open(dir))
        return 2;
    wchar_t prof[260] = {};
    GetPrivateProfileStringW(L"Rtss", L"Profile", L"", prof, 260, ini.c_str());
    const int oLimit = GetPrivateProfileIntW(L"Rtss", L"OrigLimit", 0, ini.c_str());
    const int oDen = GetPrivateProfileIntW(L"Rtss", L"OrigDen", 1, ini.c_str());
    const int oSync = GetPrivateProfileIntW(L"Rtss", L"OrigSync", 0, ini.c_str());
    RtssLog(L"capture ended: putting the limiter profile \"" + std::wstring(prof) + L"\" back");
    if (!api.Write(Narrow(prof), oLimit, oDen < 1 ? 1 : oDen, oSync))
        return 1;
    WritePrivateProfileStringW(L"Rtss", L"Has", L"0", ini.c_str());
    return 0;
}

int RunLimitRestoreElevated()
{
    wchar_t self[MAX_PATH * 2] = {};
    GetModuleFileNameW(nullptr, self, MAX_PATH * 2);
    SHELLEXECUTEINFOW sei{};
    sei.cbSize = sizeof(sei);
    sei.fMask = SEE_MASK_NOCLOSEPROCESS;
    sei.lpVerb = L"runas";
    sei.lpFile = self;
    sei.lpParameters = L"--rtss-restore";
    sei.nShow = SW_HIDE;
    if (!ShellExecuteExW(&sei) || !sei.hProcess)
        return 1;
    WaitForSingleObject(sei.hProcess, 30000);
    DWORD code = 1;
    GetExitCodeProcess(sei.hProcess, &code);
    CloseHandle(sei.hProcess);
    return static_cast<int>(code);
}

// RTSS on-screen display of the game's profile: off while the host runs (the host's overlay and RTSS's would sit on top of
// each other), put back to what it was afterwards. 0 done, 1 RTSS did not take it (needs administrator rights), 2 not installed.
int DoOsd(RtssApi& api, const std::wstring& exe, bool hide)
{
    if (exe.empty())
        return 0;
    const std::wstring dir = RtssInstallDir();
    if (dir.empty() || !api.Open(dir))
        return 2;
    const std::wstring ini = oc::SettingsPath();
    const std::string prof = Narrow(exe);
    int want = 0;
    wchar_t saved[260] = {};
    GetPrivateProfileStringW(L"Rtss", L"OsdExe", L"", saved, 260, ini.c_str());
    const bool pending = GetPrivateProfileIntW(L"Rtss", L"OsdHas", 0, ini.c_str()) != 0 && _wcsicmp(saved, exe.c_str()) == 0;
    if (hide)
    {
        if (!pending)
        {
            int cur = api.Read(prof, "EnableOSD", -1);
            if (cur < 0)
                cur = 1; // RTSS default; the Global profile is never touched or read
            WritePrivateProfileStringW(L"Rtss", L"OsdExe", exe.c_str(), ini.c_str());
            WritePrivateProfileStringW(L"Rtss", L"OsdOrig", std::to_wstring(cur).c_str(), ini.c_str());
            WritePrivateProfileStringW(L"Rtss", L"OsdHas", L"1", ini.c_str());
        }
        want = 0;
    }
    else
    {
        if (!pending)
            return 0;
        want = GetPrivateProfileIntW(L"Rtss", L"OsdOrig", 1, ini.c_str());
    }
    api.load(prof.c_str());
    DWORD v = static_cast<DWORD>(want);
    api.set("EnableOSD", reinterpret_cast<LPBYTE>(&v), sizeof(v));
    api.save(prof.c_str());
    api.update();
    const int back = api.Read(prof, "EnableOSD", -1);
    RtssLog(L"overlay of \"" + exe + L"\": " + (hide ? L"off" : L"restored to " + std::to_wstring(want)) + L", read back " + std::to_wstring(back));
    if (back != want)
        return 1;
    if (!hide)
        WritePrivateProfileStringW(L"Rtss", L"OsdHas", L"0", ini.c_str());
    return 0;
}

int RunOsdElevated(const std::wstring& exe, bool hide)
{
    wchar_t self[MAX_PATH * 2] = {};
    GetModuleFileNameW(nullptr, self, MAX_PATH * 2);
    const std::wstring args = L"--rtss-osd \"" + exe + L"\" " + (hide ? L"hide" : L"show");
    SHELLEXECUTEINFOW sei{};
    sei.cbSize = sizeof(sei);
    sei.fMask = SEE_MASK_NOCLOSEPROCESS;
    sei.lpVerb = L"runas";
    sei.lpFile = self;
    sei.lpParameters = args.c_str();
    sei.nShow = SW_HIDE;
    if (!ShellExecuteExW(&sei) || !sei.hProcess)
        return 1;
    WaitForSingleObject(sei.hProcess, 30000);
    DWORD code = 1;
    GetExitCodeProcess(sei.hProcess, &code);
    CloseHandle(sei.hProcess);
    return static_cast<int>(code);
}

// Runs this program again, elevated, to write the RTSS profile (RTSS often needs administrator rights for that), so the
// launcher itself can stay a normal program (an elevated launcher breaks the shortcuts of the games it starts).
LimiterOut RunLimiterElevated(const std::wstring& gameExe, int hz, bool off)
{
    LimiterOut out;
    wchar_t self[MAX_PATH * 2] = {};
    GetModuleFileNameW(nullptr, self, MAX_PATH * 2);
    const std::wstring args = L"--rtss-apply \"" + (gameExe.empty() ? std::wstring(L"-") : gameExe) + L"\" " + std::to_wstring(hz) + (off ? L" off" : L"");

    SHELLEXECUTEINFOW sei{};
    sei.cbSize = sizeof(sei);
    sei.fMask = SEE_MASK_NOCLOSEPROCESS;
    sei.lpVerb = L"runas";
    sei.lpFile = self;
    sei.lpParameters = args.c_str();
    sei.nShow = SW_HIDE;
    if (!ShellExecuteExW(&sei) || !sei.hProcess)
    {
        out.code = 1;
        out.info = ui::AdministratorPermissionWasNotGiven;
        out.note = ui::RtssNeedsAdministratorRightsToTakeThe;
        return out;
    }
    WaitForSingleObject(sei.hProcess, 60000);
    DWORD code = 1;
    GetExitCodeProcess(sei.hProcess, &code);
    CloseHandle(sei.hProcess);

    const std::wstring ini = oc::SettingsPath();
    wchar_t buf[512] = {};
    GetPrivateProfileStringW(L"Rtss", L"LastInfo", L"", buf, 512, ini.c_str());
    out.info = buf;
    GetPrivateProfileStringW(L"Rtss", L"LastNote", L"", buf, 512, ini.c_str());
    out.note = buf;
    out.code = static_cast<int>(code);
    if (out.note.empty())
        out.note = ui::RtssDidNotTakeTheLimitIs;
    return out;
}

// Copies a folder with everything in it over another one (files are replaced).
void CopyTree(const std::wstring& from, const std::wstring& to, int& copied, int& failed)
{
    WIN32_FIND_DATAW fd{};
    HANDLE h = FindFirstFileW((from + L"\\*").c_str(), &fd);
    if (h == INVALID_HANDLE_VALUE)
        return;
    do
    {
        const std::wstring name = fd.cFileName;
        if (name == L"." || name == L"..")
            continue;
        const std::wstring src = from + L"\\" + name;
        const std::wstring dst = to + L"\\" + name;
        if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
        {
            CreateDirectoryW(dst.c_str(), nullptr);
            CopyTree(src, dst, copied, failed);
        }
        else if (CopyFileW(src.c_str(), dst.c_str(), FALSE))
            ++copied;
        else
            ++failed;
    } while (FindNextFileW(h, &fd));
    FindClose(h);
}

// ---- Force Borderless ----
struct BorderState
{
    HWND wnd = nullptr;
    LONG_PTR style = 0;
    LONG_PTR exStyle = 0;
    RECT rect{};
    bool wasMaximized = false;
};

// Takes the frame off a window and stretches it over its monitor. False when Windows refused the change.
bool BorderApplyOn(HWND w, const BorderState& b)
{
    if (IsZoomed(w) || IsIconic(w))
        ShowWindow(w, SW_RESTORE);
    MONITORINFO mi{};
    mi.cbSize = sizeof(mi);
    if (!GetMonitorInfoW(MonitorFromWindow(w, MONITOR_DEFAULTTONEAREST), &mi))
        return false;
    LONG_PTR style = b.style & ~static_cast<LONG_PTR>(WS_CAPTION | WS_THICKFRAME | WS_MINIMIZE | WS_MAXIMIZE | WS_SYSMENU);
    style |= WS_POPUP;
    const LONG_PTR ex = b.exStyle & ~static_cast<LONG_PTR>(WS_EX_DLGMODALFRAME | WS_EX_CLIENTEDGE | WS_EX_STATICEDGE | WS_EX_WINDOWEDGE);
    SetLastError(0);
    SetWindowLongPtrW(w, GWL_STYLE, style);
    SetWindowLongPtrW(w, GWL_EXSTYLE, ex);
    if (GetWindowLongPtrW(w, GWL_STYLE) != style)
        return false;
    SetWindowPos(w, nullptr, mi.rcMonitor.left, mi.rcMonitor.top, mi.rcMonitor.right - mi.rcMonitor.left,
                 mi.rcMonitor.bottom - mi.rcMonitor.top, SWP_NOZORDER | SWP_NOACTIVATE | SWP_FRAMECHANGED);
    return true;
}

// Puts the frame, the size and the position back.
bool BorderApplyOff(HWND w, const BorderState& b)
{
    SetWindowLongPtrW(w, GWL_STYLE, b.style);
    SetWindowLongPtrW(w, GWL_EXSTYLE, b.exStyle);
    if (GetWindowLongPtrW(w, GWL_STYLE) != b.style)
        return false;
    SetWindowPos(w, nullptr, b.rect.left, b.rect.top, b.rect.right - b.rect.left, b.rect.bottom - b.rect.top,
                 SWP_NOZORDER | SWP_NOACTIVATE | SWP_FRAMECHANGED);
    if (b.wasMaximized)
        ShowWindow(w, SW_MAXIMIZE);
    return true;
}

// Same change from an elevated copy of this program (needed when the game runs as administrator and the launcher does not).
bool RunBorderElevated(bool on, const BorderState& b)
{
    wchar_t self[MAX_PATH * 2] = {};
    GetModuleFileNameW(nullptr, self, MAX_PATH * 2);
    const std::wstring args = std::wstring(L"--borderless ") + (on ? L"on " : L"off ") +
                              std::to_wstring(reinterpret_cast<ULONG_PTR>(b.wnd)) + L" " + std::to_wstring(static_cast<long long>(b.style)) +
                              L" " + std::to_wstring(static_cast<long long>(b.exStyle)) + L" " + std::to_wstring(b.rect.left) + L" " +
                              std::to_wstring(b.rect.top) + L" " + std::to_wstring(b.rect.right) + L" " + std::to_wstring(b.rect.bottom) +
                              L" " + (b.wasMaximized ? L"1" : L"0");
    SHELLEXECUTEINFOW sei{};
    sei.cbSize = sizeof(sei);
    sei.fMask = SEE_MASK_NOCLOSEPROCESS;
    sei.lpVerb = L"runas";
    sei.lpFile = self;
    sei.lpParameters = args.c_str();
    sei.nShow = SW_HIDE;
    if (!ShellExecuteExW(&sei) || !sei.hProcess)
        return false;
    WaitForSingleObject(sei.hProcess, 30000);
    DWORD code = 1;
    GetExitCodeProcess(sei.hProcess, &code);
    CloseHandle(sei.hProcess);
    return code == 0;
}

class App
{
  public:
    bool Create(HINSTANCE inst);
    int Loop();

  private:
    struct Fonts
    {
        std::unique_ptr<gp::Font> title, h2, body, bodyB, sm, tiny, link, linkN, btn, chip;
    };

    static LRESULT CALLBACK Proc(HWND h, UINT m, WPARAM w, LPARAM l);

    // The DLSS 5 window: a small separate window docked to the right side of the main window.
    HWND panel_ = nullptr;
    std::vector<Hit> hits2_;
    int hover2_ = kIdNone;
    int pressed2_ = kIdNone;
    bool tracking2_ = false;
    static LRESULT CALLBACK PanelProc(HWND h, UINT m, WPARAM w, LPARAM l);
    LRESULT OnPanelMessage(UINT m, WPARAM w, LPARAM l);
    bool CreatePanel();
    bool lastHave_ = false; // the dll was found at the last look (decides the height of the DLSS 5 window)
    int panelKind_ = 0;     // what the side window shows: 0 DLSS 5, 1 Depth
    void DrawDepthPanel(gp::Graphics& g);
    bool panelOpen_ = false; // the DLSS 5 window is shown (header button "DLSS 5")
    void DockPanel();
    void PaintPanel(HDC dc, int w, int h);
    int HitAt2(int x, int y) const;
    void InvalidatePanel() { if (panel_) InvalidateRect(panel_, nullptr, FALSE); }
    bool DlssnrPresent() const { return GetFileAttributesW((oc::ExeDir() + kDlssnrFile).c_str()) != INVALID_FILE_ATTRIBUTES; }
    LRESULT OnMessage(UINT m, WPARAM w, LPARAM l);

    float S(float v) const { return v * scale_ * 0.85f; }
    // DPI scale, made smaller when the window would not fit the height of the screen (taskbar excluded)
    float ScaleFor(UINT dpi, HWND wnd) const
    {
        const float base = static_cast<float>(dpi) / 96.0f;
        MONITORINFO mi{};
        mi.cbSize = sizeof(mi);
        HMONITOR mon = MonitorFromWindow(wnd, MONITOR_DEFAULTTOPRIMARY);
        if (!GetMonitorInfoW(mon, &mi))
            return base;
        RECT rc{ 0, 0, static_cast<LONG>(kW * base), static_cast<LONG>(kH * base) };
        AdjustWindowRectExForDpi(&rc, WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX, FALSE, 0, dpi);
        const float nonClient = static_cast<float>((rc.bottom - rc.top) - static_cast<LONG>(kH * base));
        const float availH = static_cast<float>(mi.rcWork.bottom - mi.rcWork.top) * 0.97f - nonClient;
        const float availW = static_cast<float>(mi.rcWork.right - mi.rcWork.left) * 0.97f;
        float fit = min(availH / (kH * base), availW / (kW * base));
        if (fit > 1.0f) fit = 1.0f;
        if (fit < 0.5f) fit = 0.5f;
        return base * fit;
    }
    gp::RectF R(float x, float y, float w, float h) const { return gp::RectF(S(x), S(y), S(w), S(h)); }

    void MakeFonts();
    void ApplyDpi(UINT dpi, const RECT* suggested);
    void Invalidate() { if (hwnd_) InvalidateRect(hwnd_, nullptr, FALSE); }

    void Paint(HDC dc, int w, int h);
    void Text(gp::Graphics& g, const std::wstring& t, const gp::Font& f, const gp::RectF& r, const gp::Color& c,
              int align = 0, bool wrap = false);
    void AddHit(const gp::RectF& r, int id) { hits_.push_back(Hit{ r, id }); }
    int HitAt(int x, int y) const;
    void Card(gp::Graphics& g, const gp::RectF& r);
    void Chip(gp::Graphics& g, const gp::RectF& r, const std::wstring& text, int id, bool capturing);
    void Button(gp::Graphics& g, const gp::RectF& r, const std::wstring& text, bool disabled, bool danger);

    void Refresh();
    void ClampScroll();
    gp::RectF ListView() const { return R(36, 164, 476, 438); }

    void OnClick(int id);
    void InstallReshadeForSelectedGame();
    LRESULT OnCaptureKey(UINT vk);
    void BeginCapture(int sc);
    UINT* KeyRef(int sc);
    oc::Hotkey* HotkeyRef(int sc);
    std::wstring ChipText(int sc);
    bool RegisterShowHotkey();
    void Save();
    void SetNote(const std::wstring& t, Note k = Note::Info);

    bool Running() const { return host_ != nullptr; }
    void Start();
    void Stop(bool wait);
    void PollHost();
    void ShowOnTop();

    // live status + warnings
    void LoadWarnFlags();
    void ApplyLimiter(bool off = false);
    std::vector<BorderState> border_; // windows made borderless by this launcher (to bring the frame back)
    int BorderIndex(HWND w) const
    {
        for (size_t i = 0; i < border_.size(); ++i)
            if (border_[i].wnd == w)
                return static_cast<int>(i);
        return -1;
    }
    void ToggleBorderless();
    void ResizeGameWindow(bool quiet = false);
    void KeepGameSize();
    int keepW_ = 0, keepH_ = 0, keepTries_ = 0;
    ULONGLONG keepSince_ = 0, keepNext_ = 0;
    std::wstring osdExe_; // game whose RTSS overlay was turned off for this capture
    void SetRtssOsd(bool hide);
    void RestoreLimiter();
    // Quick modes: 0 NO FG, 1 x2, 2 x3, 3 x4: limiter on the refresh rate (or 1/2, 1/3, 1/4 of it, applied at START), Classic + unlimited, and
    // NO FG or Intel MFG.
    static int PresetFg(int k) { return k == 0 ? 2 : (k <= 3 ? 0 : 1); }   // NO FG, Intel MFG (new GPU), Artur (old GPU)
    static int PresetLimit(int k) { return k == 0 ? 0 : (k <= 3 ? -(k + 1) : -(k - 2)); }
    static int PresetMfg(int k) { return k == 0 ? 0 : (k <= 3 ? k : k - 3); } // x2 / x3 / x4 -> 1 / 2 / 3
    bool PresetActive(int k) const
    {
        return st_.frameGenMode == PresetFg(k) && st_.mfgCount == PresetMfg(k) && st_.limitFps == PresetLimit(k) && st_.limitAuto == 1 && st_.limitTarget != 0 &&
               st_.presentMode == 6 && st_.upscale == 0;
    }
    void ApplyPreset(int k)
    {
        st_.frameGenMode = PresetFg(k);
        st_.mfgCount = PresetMfg(k);
        st_.limitFps = PresetLimit(k);
        st_.limitAuto = 1;
        if (st_.limitTarget == 0)
            st_.limitTarget = 1; // the game's own profile
        st_.presentMode = 6;
        st_.optiFps = 1; // "OptiScaler fps = Hz" goes on with every profile
        vsyncMode_ = -1; // VSync goes to Auto
        ApplyVsyncToOptiScaler();
        // every mode: Output scaling off
        st_.upscale = 0;
        Save();
        static const wchar_t* const names[] = { ui::NoFgMod, ui::X2FgModNewGpu, ui::X3FgModNewGpu, ui::X4FgModNewGpu,
                                                ui::X2FgModOldGpu, ui::X3FgModOldGpu, ui::X4FgModOldGpu };
        SetNote(std::format(ui::PresetApplied, names[k]), Note::Ok);
        OnClick(kIdFrameGenApply); // writes OptiScaler.ini for the chosen frame generation (and the picture upscaler when output scaling is on)
    }
    gp::RectF upSliderRect_{};
    bool upSliding_ = false;
    ULONGLONG upSliderSaved_ = 0;
    int UpSharpValue() const { return (st_.upscale >= 3) ? st_.nisSharp : (st_.upscale == 2 ? 100 - st_.upscaleSharp : 0); }
    void SetUpSharpFromX(int x, bool save)
    {
        if (st_.upscale != 2 && st_.upscale < 3)
            return;
        float f = (static_cast<float>(x) - upSliderRect_.X) / upSliderRect_.Width;
        f = f < 0.0f ? 0.0f : (f > 1.0f ? 1.0f : f);
        const int v = static_cast<int>(f * 100.0f + 0.5f);
        if (st_.upscale >= 3)
            st_.nisSharp = v;
        else
            st_.upscaleSharp = 100 - v;
        const ULONGLONG now = GetTickCount64();
        if (save || now - upSliderSaved_ > 150)
        {
            Save();
            upSliderSaved_ = now;
        }
    }
    HWND upWnd_ = nullptr; // game window the launcher shrank for output scaling, and the rect to give back
    RECT upRect_{};
    bool dlgProfile_ = false;     // the dialog is the "apply the saved profile?" question
    std::wstring profileExe_;     // the game that question is about
    int lastOffered_ = -1;
    void SaveProfile();
    bool ProfileFolder(const std::wstring& exe, std::wstring& dir) const;
    void ApplyProfile(const std::wstring& exe, int slot);
    int profileSlot_ = 1;         // 1..4: which of the game's four saved profiles Save / Load use
    int profileApplySlot_ = 1;    // the slot the "apply?" question is about
    static std::wstring SlotFile(int slot)
    {
        return slot <= 1 ? std::wstring(L"OptiScaler.ini") : L"OptiScaler.slot" + std::to_wstring(slot) + L".ini";
    }
    bool SlotExists(const std::wstring& exe, int slot) const
    {
        std::wstring dir;
        return ProfileFolder(exe, dir) && GetFileAttributesW((dir + L"\\" + SlotFile(slot)).c_str()) != INVALID_FILE_ATTRIBUTES;
    }
    int LastSlot(const std::wstring& exe) const
    {
        const int v = GetPrivateProfileIntW(L"ProfileSlot", exe.c_str(), 1, oc::SettingsPath().c_str());
        return v >= 1 && v <= 4 ? v : 1;
    }
    void RememberSlot(const std::wstring& exe, int slot) const
    {
        WritePrivateProfileStringW(L"ProfileSlot", exe.c_str(), std::to_wstring(slot).c_str(), oc::SettingsPath().c_str());
    }
    void OfferProfile();
    // Multi frame generation count: Intel MFG -> [XeFG], Artur / Combo / Nvidia MFG -> [DLSSG]; "auto" or 1..3 (2x..4x).
    static bool MfgMode(int fg) { return fg == 0 || fg == 1 || fg == 5 || fg == 8; }
    void ApplyMfgCount()
    {
        if (!MfgMode(st_.frameGenMode))
            return;
        const std::wstring ini = oc::ExeDir() + L"OptiScaler.ini";
        if (GetFileAttributesW(ini.c_str()) == INVALID_FILE_ATTRIBUTES)
            return;
        const std::wstring v = st_.mfgCount == 0 ? std::wstring(L"auto") : std::to_wstring(st_.mfgCount);
        WritePrivateProfileStringW(st_.frameGenMode == 0 ? L"XeFG" : L"DLSSG", L"InterpolationCount", v.c_str(), ini.c_str());
    }
    // Writes one key of the OptiScaler.ini next to the launcher; false (and an error note) when the file or the write fails.
    bool PutIni(const wchar_t* section, const wchar_t* key, const wchar_t* value)
    {
        const std::wstring ini = oc::ExeDir() + L"OptiScaler.ini";
        if (GetFileAttributesW(ini.c_str()) == INVALID_FILE_ATTRIBUTES)
        {
            SetNote(ui::OptiscalerIniWasNotFoundNextTo, Note::Error);
            return false;
        }
        if (!WritePrivateProfileStringW(section, key, value, ini.c_str()))
        {
            SetNote(ui::CouldNotUpdateOptiscalerIniCheckThe, Note::Error);
            return false;
        }
        return true;
    }
    // What OptiScaler.ini says right now (read again whenever the launcher looks at the file).
    bool dlss5On_ = false; // [DlssNr] Enabled = true
    bool hudFix_ = false;  // [OptiFG] HUDFix = true
    bool asyncFg_ = false; // [FSRFG] AllowAsync = true
    bool ReadFgState()
    {
        const std::wstring ini = oc::ExeDir() + L"OptiScaler.ini";
        auto isTrue = [&](const wchar_t* sec, const wchar_t* key)
        {
            wchar_t v[32] = {};
            GetPrivateProfileStringW(sec, key, L"auto", v, 32, ini.c_str());
            return _wcsicmp(v, L"true") == 0;
        };
        const bool a = isTrue(L"DlssNr", L"Enabled"), b = isTrue(L"OptiFG", L"HUDFix"), c = isTrue(L"FSRFG", L"AllowAsync");
        const bool changed = a != dlss5On_ || b != hudFix_ || c != asyncFg_;
        dlss5On_ = a;
        hudFix_ = b;
        asyncFg_ = c;
        return changed;
    }
    // OptiScaler's own frame limit: [Framerate] FramerateLimit = the refresh rate of the game's monitor (e.g. 75.000000)
    void ApplyOptiFpsLimit()
    {
        if (!st_.optiFps)
            return;
        const std::wstring ini = oc::ExeDir() + L"OptiScaler.ini";
        if (GetFileAttributesW(ini.c_str()) == INVALID_FILE_ATTRIBUTES)
            return;
        std::wstring device;
        HWND w = (selected_ >= 0 && selected_ < static_cast<int>(games_.size())) ? games_[selected_].hwnd : nullptr;
        MONITORINFOEXW mi{};
        mi.cbSize = sizeof(mi);
        if (w && IsWindow(w) && GetMonitorInfoW(MonitorFromWindow(w, MONITOR_DEFAULTTONEAREST), &mi))
            device = mi.szDevice;
        DEVMODEW dm{};
        dm.dmSize = sizeof(dm);
        if (!EnumDisplaySettingsW(device.empty() ? nullptr : device.c_str(), ENUM_CURRENT_SETTINGS, &dm) || dm.dmDisplayFrequency <= 1)
        {
            dm = DEVMODEW{};
            dm.dmSize = sizeof(dm);
            if (!EnumDisplaySettingsW(nullptr, ENUM_CURRENT_SETTINGS, &dm) || dm.dmDisplayFrequency <= 1)
                return;
        }
        wchar_t v[32];
        swprintf_s(v, L"%.6f", static_cast<double>(dm.dmDisplayFrequency));
        WritePrivateProfileStringW(L"Framerate", L"FramerateLimit", v, ini.c_str());
    }
    // "Run the upscaler on the picture": sharpness override + CAS + native upscaler pass in OptiScaler.ini.
    bool WritePictureIni(bool on)
    {
        const std::wstring iniPath = oc::ExeDir() + L"OptiScaler.ini";
        const wchar_t* v = on ? L"true" : L"false";
        bool ok = WritePrivateProfileStringW(L"Sharpness", L"OverrideSharpness", v, iniPath.c_str()) != 0;
        if (on)
            ok = WritePrivateProfileStringW(L"Sharpness", L"Sharpness", L"0.600000", iniPath.c_str()) != 0 && ok;
        ok = WritePrivateProfileStringW(L"CAS", L"Enabled", v, iniPath.c_str()) != 0 && ok;
        ok = WritePrivateProfileStringW(L"DlssNr", L"NativeUpscalerPass", v, iniPath.c_str()) != 0 && ok;
        return ok;
    }
    bool pictureOn_ = false; // [CAS] Enabled in OptiScaler.ini
    void ReadPictureState()
    {
        wchar_t v[16] = {};
        GetPrivateProfileStringW(L"CAS", L"Enabled", L"false", v, 16, (oc::ExeDir() + L"OptiScaler.ini").c_str());
        pictureOn_ = _wcsicmp(v, L"true") == 0;
        ReadFgState();
    }
    RtssApi rtss_;
    std::wstring limitInfo_; // what the limiter has applied (shown under the Apply button)
    void RaiseWarning(int kind, const std::wstring& text);
    void OpenNextDialog();
    void CloseDialog(bool accepted);
    void CheckLive();
    void PaintLive(gp::Graphics& g, const gp::RectF& view);
    void PaintDialog(gp::Graphics& g, int W, int H);

    HWND hwnd_ = nullptr;
    HINSTANCE inst_ = nullptr;
    float scale_ = 1.0f;
    Fonts f_;

    oc::Settings st_;
    int vsyncMode_ = -1; // the user's VSync choice: -1 auto (default), 2 on / SyncInterval 0, 1 on / SyncInterval 1, 0 off
    // With Arthur frame generation OptiScaler's forced VSync blocks every internal present (heavy frame drops), so
    // there the host paces its frames itself and OptiScaler is left on auto.
    bool ApplyVsyncToOptiScaler()
    {
        // Arthur: both "On" modes leave OptiScaler on auto and let the host pace its frames instead.
        // Classic (2.2.0) mode: VSync is only the host's own setting and OptiScaler is left on auto for every FG.
        const bool classic = st_.presentMode == 3 || st_.presentMode == 5 || st_.presentMode == 6;
        const bool ls = st_.presentMode == 4; // Lossless-style: tearing unless VSync is explicitly On, host-side only
        const bool on = vsyncMode_ == 1 || vsyncMode_ == 2;
        const bool hostOnly = on && (classic || ls || st_.frameGenMode == 1);
        const int eff = (classic || hostOnly) ? -1 : vsyncMode_;
        // Classic = 2.2.0: the host's own VSync toggle, on by default (Auto and both On modes), off only for Off.
        st_.vsync = classic ? vsyncMode_ != 0 : (ls ? on : (vsyncMode_ == 1 || hostOnly)); // host-side VSync (pacing / classic Present)
        Save();
        WritePrivateProfileStringW(L"General", L"VSyncMode", vsyncMode_ < 0 ? L"auto" : (vsyncMode_ == 2 ? L"on0" : (vsyncMode_ == 1 ? L"on1" : L"off")),
                                   oc::SettingsPath().c_str());
        return WriteForceVsync(eff);
    }
    std::vector<GameItem> games_;
    int selected_ = -1;
    float scroll_ = 0.0f;

    std::vector<Hit> hits_;
    int hover_ = kIdNone;
    int pressed_ = kIdNone;
    bool tracking_ = false;
    int capturing_ = -1;

    std::wstring note_;
    Note noteKind_ = Note::Info;

    HANDLE host_ = nullptr;
    DWORD hostPid_ = 0;
    HWND target_ = nullptr;
    std::wstring targetTitle_;
    bool stopping_ = false;
    ULONGLONG stopAt_ = 0;

    oc::LiveStats live_;
    bool liveValid_ = false;
    unsigned long long lastLiveStamp_ = 0;
    int blockCount_ = 0;
    bool capActive_ = false;   // the game currently looks capped
    bool blockActive_ = false; // Present currently waits although nobody asked for VSync
    bool hideWarn_[kWarnCount] = {};  // "don't show again", saved in OptiCaptureHost.ini
    bool shownWarn_[kWarnCount] = {}; // already raised during this capture
    std::wstring warnText_[kWarnCount];
    std::vector<int> warnQueue_;      // waiting for the launcher window to be visible
    int dlgKind_ = -1;                // the warning dialog that is open (-1 none)
    bool dlgDontShow_ = false;
    bool dlgStart_ = false;           // the dialog asks "start anyway?" before a capture starts
    bool startConfirmed_ = false;
};

bool App::Create(HINSTANCE inst)
{
    inst_ = inst;
    oc::LoadSettings(st_);
    {
        // The user's VSync choice lives in OptiCaptureHost.ini ([General] VSyncMode = auto / on / off). Without it
        // (first run of this version) it is taken from OptiScaler's ForceVsync.
        wchar_t vm[16] = {};
        GetPrivateProfileStringW(L"General", L"VSyncMode", L"", vm, 16, oc::SettingsPath().c_str());
        if (_wcsicmp(vm, L"on0") == 0) vsyncMode_ = 2;
        else if (_wcsicmp(vm, L"on1") == 0 || _wcsicmp(vm, L"on") == 0) vsyncMode_ = 1;
        else if (_wcsicmp(vm, L"off") == 0) vsyncMode_ = 0;
        else if (_wcsicmp(vm, L"auto") == 0) vsyncMode_ = -1;
        else vsyncMode_ = ReadForceVsync();
        st_.vsync = (st_.presentMode == 3 || st_.presentMode == 5 || st_.presentMode == 6) ? vsyncMode_ != 0
                    : (st_.presentMode == 4) ? (vsyncMode_ == 1 || vsyncMode_ == 2)
                                             : (vsyncMode_ == 1 || (vsyncMode_ == 2 && st_.frameGenMode == 1));
    }

    WNDCLASSEXW wc{};
    wc.cbSize = sizeof(wc);
    wc.lpfnWndProc = Proc;
    wc.hInstance = inst;
    wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    wc.lpszClassName = kClassName;
    if (!RegisterClassExW(&wc))
        return false;

    const DWORD style = WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX;
    const UINT dpi = GetDpiForSystem();
    scale_ = ScaleFor(dpi, nullptr);

    RECT rc{ 0, 0, static_cast<LONG>(kW * scale_), static_cast<LONG>(kH * scale_) };
    AdjustWindowRectExForDpi(&rc, style, FALSE, 0, dpi);
    const int ww = rc.right - rc.left;
    const int wh = rc.bottom - rc.top;

    RECT work{};
    SystemParametersInfoW(SPI_GETWORKAREA, 0, &work, 0);
    const int x = work.left + ((work.right - work.left) - ww) / 2;
    const int y = work.top + ((work.bottom - work.top) - wh) / 2;

    hwnd_ = CreateWindowExW(0, kClassName, ui::LauncherTitle, style, x, y, ww, wh, nullptr, nullptr, inst,
                            this);
    if (!hwnd_)
        return false;

    const UINT actual = GetDpiForWindow(hwnd_);
    if (actual != 0 && actual != dpi)
        ApplyDpi(actual, nullptr);
    else
        MakeFonts();

    // Dark title bar, matching caption colour, rounded corners (the last two are Windows 11 only and ignored before).
    BOOL dark = TRUE;
    if (FAILED(DwmSetWindowAttribute(hwnd_, 20, &dark, sizeof(dark))))
        DwmSetWindowAttribute(hwnd_, 19, &dark, sizeof(dark));
    const COLORREF caption = RGB(13, 15, 20);
    DwmSetWindowAttribute(hwnd_, 35, &caption, sizeof(caption));
    const DWORD round = 2;
    DwmSetWindowAttribute(hwnd_, 33, &round, sizeof(round));

    if (HICON bigIcon = reinterpret_cast<HICON>(LoadImageW(inst, MAKEINTRESOURCEW(1), IMAGE_ICON, 64, 64, LR_DEFAULTCOLOR)))
        SendMessageW(hwnd_, WM_SETICON, ICON_BIG, reinterpret_cast<LPARAM>(bigIcon));
    if (HICON smallIcon = reinterpret_cast<HICON>(LoadImageW(inst, MAKEINTRESOURCEW(1), IMAGE_ICON, 32, 32, LR_DEFAULTCOLOR)))
        SendMessageW(hwnd_, WM_SETICON, ICON_SMALL, reinterpret_cast<LPARAM>(smallIcon));

    LoadWarnFlags();
    ReadPictureState();
    DeleteFileW(oc::StatusPath().c_str()); // a leftover from a crashed host
    Refresh();
    RegisterShowHotkey();
    SetTimer(hwnd_, kTimerList, 3000, nullptr);
    SetTimer(hwnd_, kTimerHost, 400, nullptr);

    ShowWindow(hwnd_, SW_SHOW);
    UpdateWindow(hwnd_);
    CreatePanel();
    return true;
}

int App::Loop()
{
    MSG msg{};
    while (GetMessageW(&msg, nullptr, 0, 0) > 0)
    {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }
    return static_cast<int>(msg.wParam);
}

LRESULT CALLBACK App::Proc(HWND h, UINT m, WPARAM w, LPARAM l)
{
    App* self = reinterpret_cast<App*>(GetWindowLongPtrW(h, GWLP_USERDATA));
    if (m == WM_NCCREATE)
    {
        auto* cs = reinterpret_cast<CREATESTRUCTW*>(l);
        self = static_cast<App*>(cs->lpCreateParams);
        self->hwnd_ = h;
        SetWindowLongPtrW(h, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(self));
    }
    return self ? self->OnMessage(m, w, l) : DefWindowProcW(h, m, w, l);
}

void App::MakeFonts()
{
    auto mk = [&](const wchar_t* family, float px, int style)
    { return std::make_unique<gp::Font>(family, S(px), style, gp::UnitPixel); };

    const int regular = gp::FontStyleRegular;
    int emphasis = regular;
    const wchar_t* normal = L"Segoe UI";
    const wchar_t* semibold = L"Segoe UI Semibold";
#ifdef OC_UI_ZH_CN
    gp::FontFamily chinese(L"Microsoft YaHei UI");
    if (chinese.GetLastStatus() == gp::Ok)
    {
        normal = semibold = L"Microsoft YaHei UI";
        emphasis = gp::FontStyleBold;
    }
#endif
    f_.title = mk(semibold, 24.0f, emphasis);
    f_.h2 = mk(semibold, 16.5f, emphasis);
    f_.body = mk(normal, 15.0f, regular);
    f_.bodyB = mk(semibold, 15.5f, emphasis);
    f_.sm = mk(normal, 13.5f, regular);
    f_.tiny = mk(normal, 12.0f, regular);
    f_.link = mk(semibold, 14.5f, emphasis | gp::FontStyleUnderline);
    f_.linkN = mk(semibold, 14.5f, emphasis);
    f_.btn = mk(semibold, 16.5f, emphasis);
    f_.chip = mk(semibold, 14.0f, emphasis);
}

void App::ApplyDpi(UINT dpi, const RECT* suggested)
{
    scale_ = ScaleFor(dpi, hwnd_);
    MakeFonts();

    if (suggested)
    {
        SetWindowPos(hwnd_, nullptr, suggested->left, suggested->top, suggested->right - suggested->left,
                     suggested->bottom - suggested->top, SWP_NOZORDER | SWP_NOACTIVATE);
    }
    else
    {
        RECT rc{ 0, 0, static_cast<LONG>(kW * scale_), static_cast<LONG>(kH * scale_) };
        AdjustWindowRectExForDpi(&rc, static_cast<DWORD>(GetWindowLongPtrW(hwnd_, GWL_STYLE)), FALSE, 0, dpi);
        SetWindowPos(hwnd_, nullptr, 0, 0, rc.right - rc.left, rc.bottom - rc.top, SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE);
    }
    ClampScroll();
    Invalidate();
}

// ---------------------------------------------------------------------------------------------------------------
// Data
// ---------------------------------------------------------------------------------------------------------------

void App::Refresh()
{
    const HWND keep = (selected_ >= 0 && selected_ < static_cast<int>(games_.size())) ? games_[selected_].hwnd : nullptr;

    std::vector<GameItem> list;
    EnumCtx ctx{ &list, &games_, GetCurrentProcessId(), hostPid_ };
    EnumWindows(EnumWindowsProc, reinterpret_cast<LPARAM>(&ctx));

    // Borderless / fullscreen-sized windows first (that is what a game usually looks like), the rest in z-order.
    std::stable_sort(list.begin(), list.end(), [](const GameItem& a, const GameItem& b) { return a.fills && !b.fills; });

    games_ = std::move(list);

    selected_ = -1;
    if (keep)
    {
        for (size_t i = 0; i < games_.size(); ++i)
        {
            if (games_[i].hwnd == keep)
            {
                selected_ = static_cast<int>(i);
                break;
            }
        }
    }

    if (selected_ < 0)
        lastOffered_ = -1; // the game is gone: offer its profile again when it is picked next time

    ClampScroll();
    Invalidate();
}

void App::ClampScroll()
{
    const float content = static_cast<float>(games_.size()) * S(62.0f) - S(6.0f);
    const float maxScroll = max(0.0f, content - ListView().Height);
    scroll_ = min(max(scroll_, 0.0f), maxScroll);
}

void App::SetNote(const std::wstring& t, Note k)
{
    note_ = t;
    noteKind_ = k;
    Invalidate();
}

void App::Save()
{
    oc::SaveSettings(st_);
}

UINT* App::KeyRef(int sc)
{
    switch (sc)
    {
    case kScMenuKey: return &st_.menuKey;
    case kScFpsKey: return &st_.fpsKey;
    case kScFpsCycleKey: return &st_.fpsCycleKey;
    case kScFgKey: return &st_.fgKey;
    default: return nullptr;
    }
}

oc::Hotkey* App::HotkeyRef(int sc)
{
    switch (sc)
    {
    case kScStop: return &st_.stop;
    case kScLauncher: return &st_.launcher;
    default: return nullptr;
    }
}

std::wstring App::ChipText(int sc)
{
    if (const oc::Hotkey* h = HotkeyRef(sc))
        return HotkeyText(*h);
    const UINT* k = KeyRef(sc);
    return k ? KeyName(*k) : std::wstring();
}

bool App::RegisterShowHotkey()
{
    UnregisterHotKey(hwnd_, kHotkeyShow);
    if (!RegisterHotKey(hwnd_, kHotkeyShow, st_.launcher.mods | MOD_NOREPEAT, st_.launcher.vk))
    {
        SetNote(ui::TheShowThisWindowShortcutIsAlready, Note::Error);
        return false;
    }
    return true;
}

// ---------------------------------------------------------------------------------------------------------------
// Start / stop
// ---------------------------------------------------------------------------------------------------------------

void App::Start()
{
    if (Running())
    {
        Stop(false);
        return;
    }

    if (selected_ < 0 || selected_ >= static_cast<int>(games_.size()))
    {
        SetNote(ui::PickAGameFromTheListFirst, Note::Error);
        return;
    }

    if (st_.upscale != 0 && !Running())
    {
        WritePictureIni(true); // output scaling always runs the upscaler on the picture
        ReadPictureState();
    }
    ApplyOptiFpsLimit(); // OptiScaler's frame limit = the monitor's refresh rate

    const GameItem game = games_[selected_];
    if (!IsWindow(game.hwnd))
    {
        Refresh();
        SetNote(ui::ThatWindowIsGonePickTheGame, Note::Error);
        return;
    }

    const std::wstring dir = oc::ExeDir();
    const std::wstring host = dir + kHostExe;
    if (GetFileAttributesW(host.c_str()) == INVALID_FILE_ATTRIBUTES)
    {
        SetNote(ui::OpticapturehostExeWasNotFoundNextTo, Note::Error);
        return;
    }

    startConfirmed_ = false;

    Save();
    if (st_.limitAuto)
        ApplyLimiter(); // RTSS profile first, so the limit is already in place when the game runs
    WriteOptiScalerKeys(st_);

    if (IsIconic(game.hwnd))
        ShowWindow(game.hwnd, SW_RESTORE);

    SetRtssOsd(true); // RTSS overlay off for this game while the host runs

    // Output scaling: make the game render small (monitor size / 1.5, 1.7 or 2) before the host starts; the host scales it up.
    keepW_ = keepH_ = keepTries_ = 0;
    keepSince_ = keepNext_ = 0;
    if (st_.upscale != 0)
        ResizeGameWindow(true);

    std::wstring cmd = L"\"" + host + L"\" --hwnd " + std::to_wstring(reinterpret_cast<ULONG_PTR>(game.hwnd)) +
                       L" --parent " + std::to_wstring(GetCurrentProcessId());
    std::vector<wchar_t> buf(cmd.begin(), cmd.end());
    buf.push_back(L'\0');

    STARTUPINFOW si{};
    si.cb = sizeof(si);
    PROCESS_INFORMATION pi{};
    if (!CreateProcessW(host.c_str(), buf.data(), nullptr, nullptr, FALSE, 0, nullptr, dir.c_str(), &si, &pi))
    {
        SetNote(std::format(ui::HostStartFailed, GetLastError()), Note::Error);
        return;
    }
    CloseHandle(pi.hThread);

    host_ = pi.hProcess;
    hostPid_ = pi.dwProcessId;

    // new capture: forget the live data and let every warning be raised once more
    DeleteFileW(oc::StatusPath().c_str());
    liveValid_ = false;
    lastLiveStamp_ = 0;
    blockCount_ = 0;
    capActive_ = false;
    blockActive_ = false;
    for (bool& b : shownWarn_)
        b = false;
    warnQueue_.clear();

    target_ = game.hwnd;
    targetTitle_ = game.title;
    stopping_ = false;
    SetNote(L"");

    // Get out of the way, then hand the foreground back to the game once the overlay is up.
    ShowWindow(hwnd_, SW_MINIMIZE);
    SetTimer(hwnd_, kTimerFront, 1800, nullptr);
    Invalidate();
}

void App::Stop(bool wait)
{
    if (!host_)
        return;

    EnumWindows(CloseOverlayProc, static_cast<LPARAM>(hostPid_));
    stopping_ = true;
    stopAt_ = GetTickCount64() + 4000;
    Invalidate();

    if (wait)
    {
        if (WaitForSingleObject(host_, 2000) != WAIT_OBJECT_0)
            TerminateProcess(host_, 1);
    }
}

void App::PollHost()
{
    if (!host_)
        return;

    const DWORD w = WaitForSingleObject(host_, 0);
    if (w == WAIT_OBJECT_0)
    {
        DWORD code = 0;
        GetExitCodeProcess(host_, &code);
        CloseHandle(host_);
        host_ = nullptr;
        hostPid_ = 0;
        liveValid_ = false;
        capActive_ = false;
        blockActive_ = false;

        const bool asked = stopping_;
        stopping_ = false;

        SetRtssOsd(false);
        RestoreLimiter();
        if (upWnd_)
        {
            if (IsWindow(upWnd_))
                SetWindowPos(upWnd_, nullptr, upRect_.left, upRect_.top, upRect_.right - upRect_.left, upRect_.bottom - upRect_.top,
                             SWP_NOZORDER | SWP_NOACTIVATE);
            upWnd_ = nullptr;
        }

        if (asked || code == 0)
            SetNote(ui::CaptureEnded, Note::Ok);
        else
            SetNote(std::format(ui::HostStopped, code), Note::Error);

        Refresh();
        ShowOnTop();
        SetWindowPos(hwnd_, HWND_NOTOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
        return;
    }

    if (stopping_ && GetTickCount64() > stopAt_)
    {
        TerminateProcess(host_, 1);
        stopAt_ = GetTickCount64() + 4000;
    }

    CheckLive();
}

void App::ShowOnTop()
{
    if (IsIconic(hwnd_))
        ShowWindow(hwnd_, SW_RESTORE);
    else
        ShowWindow(hwnd_, SW_SHOW);

    SetWindowPos(hwnd_, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE);
    ForceForeground(hwnd_);
}

// ---------------------------------------------------------------------------------------------------------------
// Live status and warnings
// ---------------------------------------------------------------------------------------------------------------

namespace
{
std::wstring Num(double v, int decimals)
{
    wchar_t b[48] = {};
    swprintf_s(b, 48, decimals <= 0 ? L"%.0f" : (decimals == 1 ? L"%.1f" : L"%.2f"), v);
    return b;
}

std::wstring SizeText(long width, long height)
{
    return std::format(L"{}x{}", width, height);
}
} // namespace

void App::LoadWarnFlags()
{
    for (int i = 0; i < kWarnCount; ++i)
        hideWarn_[i] = GetPrivateProfileIntW(L"Warnings", kWarnKeys[i], 0, oc::SettingsPath().c_str()) != 0;
}

// Queues a warning dialog. It opens at once when the launcher window is visible; otherwise (the launcher is minimized
// while a game runs) it waits and the taskbar button flashes. Each kind is raised at most once per capture.
void App::RaiseWarning(int kind, const std::wstring& text)
{
    warnText_[kind] = text;
    if (hideWarn_[kind] || shownWarn_[kind])
        return;
    shownWarn_[kind] = true;
    warnQueue_.push_back(kind);

    if (IsIconic(hwnd_) || !IsWindowVisible(hwnd_))
    {
        FLASHWINFO fi{};
        fi.cbSize = sizeof(fi);
        fi.hwnd = hwnd_;
        fi.dwFlags = FLASHW_TRAY | FLASHW_TIMERNOFG;
        fi.uCount = 3;
        FlashWindowEx(&fi);
    }
    else
    {
        OpenNextDialog();
    }
}

void App::OpenNextDialog()
{
    if (dlgKind_ >= 0 || warnQueue_.empty())
        return;
    dlgKind_ = warnQueue_.front();
    warnQueue_.erase(warnQueue_.begin());
    dlgDontShow_ = false;
    dlgStart_ = false;
    Invalidate();
}

void App::ApplyLimiter(bool off)
{
    std::wstring exe;
    HWND wnd = nullptr;
    if (selected_ >= 0 && selected_ < static_cast<int>(games_.size()))
    {
        exe = games_[selected_].exe;
        wnd = games_[selected_].hwnd;
    }
    const int hz = RefreshRateOf(wnd);

    LimiterOut r = DoLimiter(rtss_, st_, exe, hz, off);
    if (r.code == 1 && !IsElevated())
    {
        SetNote(ui::RtssNeedsAdministratorRightsToWriteThe, Note::Info);
        r = RunLimiterElevated(exe, hz, off);
    }

    if (r.code == 2)
    {
        shownWarn_[kWarnRtssMissing] = false;
        RaiseWarning(kWarnRtssMissing,
                     ui::RtssInstallHelp);
    }
    if (!r.info.empty())
        limitInfo_ = r.info;
    SetNote(r.note, r.code == 0 ? Note::Ok : Note::Error);
}

// Force Borderless: takes the frame off the selected game's window and stretches it over its monitor; pressing it again
// brings the frame and the old size back. (Exclusive fullscreen cannot be changed this way.)
void App::ToggleBorderless()
{
    if (selected_ < 0 || selected_ >= static_cast<int>(games_.size()))
    {
        SetNote(ui::PickAGameInTheListFirst2, Note::Error);
        return;
    }
    const HWND w = games_[selected_].hwnd;
    if (!IsWindow(w))
    {
        SetNote(ui::ThatWindowIsGonePressRefresh, Note::Error);
        return;
    }

    const int idx = BorderIndex(w);
    if (idx >= 0)
    {
        const BorderState b = border_[idx];
        bool ok = BorderApplyOff(w, b);
        if (!ok && !IsElevated())
        {
            SetNote(ui::TheGameWindowNeedsAdministratorRightsAllow, Note::Info);
            ok = RunBorderElevated(false, b);
        }
        if (!ok)
        {
            SetNote(ui::CouldNotBringTheWindowFrameBack, Note::Error);
            return;
        }
        border_.erase(border_.begin() + idx);
        SetNote(ui::BorderlessIsOffTheWindowFrameIs, Note::Ok);
        Refresh();
        return;
    }

    BorderState b;
    b.wnd = w;
    b.style = GetWindowLongPtrW(w, GWL_STYLE);
    b.exStyle = GetWindowLongPtrW(w, GWL_EXSTYLE);
    b.wasMaximized = IsZoomed(w) != FALSE;
    WINDOWPLACEMENT wp{};
    wp.length = sizeof(wp);
    if (GetWindowPlacement(w, &wp) && (b.wasMaximized || IsIconic(w)))
        b.rect = wp.rcNormalPosition;
    else
        GetWindowRect(w, &b.rect);

    bool ok = BorderApplyOn(w, b);
    if (!ok && !IsElevated())
    {
        SetNote(ui::TheGameWindowNeedsAdministratorRightsAllow, Note::Info);
        ok = RunBorderElevated(true, b);
    }
    if (!ok)
    {
        SetNote(ui::WindowsDidNotLetTheLauncherChange, Note::Error);
        return;
    }
    border_.push_back(b);
    SetNote(ui::BorderlessIsOnPressTheButtonAgain, Note::Ok);
    Refresh();
}

// The capture is over: the limiter goes back to what the game's RTSS profile had before.
void App::RestoreLimiter()
{
    if (GetPrivateProfileIntW(L"Rtss", L"Has", 0, oc::SettingsPath().c_str()) == 0)
        return;
    int r = DoLimitRestore(rtss_);
    if (r == 1 && !IsElevated())
        r = RunLimitRestoreElevated();
    if (r == 0)
        limitInfo_ = ui::LimiterPutBackAfterTheCapture;
}

void App::SetRtssOsd(bool hide)
{
    if (hide)
    {
        if (!st_.hideOsd || selected_ < 0 || selected_ >= static_cast<int>(games_.size()))
            return;
        osdExe_ = games_[selected_].exe;
    }
    if (osdExe_.empty())
        return;
    int r = DoOsd(rtss_, osdExe_, hide);
    if (r == 1 && !IsElevated())
        r = RunOsdElevated(osdExe_, hide);
    if (!hide || r == 2)
        osdExe_.clear();
}

// Output scaling: the game window is made as small as the chosen level asks (monitor size / 1.5, 1.7 or 2) and put in the
// top left corner of its monitor; the host then scales the picture back up to the whole monitor.
// While the capture runs: if the game changed its resolution (its own settings menu), put the window back on the chosen size
// (Quality, Balanced, Performance or Native) once the new size has been stable for a moment.
void App::KeepGameSize()
{
    if (!Running() || st_.upscale == 0 || !upWnd_ || !IsWindow(upWnd_) || IsIconic(upWnd_))
        return;
    MONITORINFO mi{};
    mi.cbSize = sizeof(mi);
    if (!GetMonitorInfoW(MonitorFromWindow(upWnd_, MONITOR_DEFAULTTONEAREST), &mi))
        return;
    static const double factor[] = { 1.5, 1.7, 2.0, 1.0 };
    const double f = factor[(st_.upscaleLevel >= 0 && st_.upscaleLevel <= 3) ? st_.upscaleLevel : 1];
    const int cw = (static_cast<int>((mi.rcMonitor.right - mi.rcMonitor.left) / f + 0.5)) & ~1;
    const int ch = (static_cast<int>((mi.rcMonitor.bottom - mi.rcMonitor.top) / f + 0.5)) & ~1;
    RECT cr{};
    if (!GetClientRect(upWnd_, &cr))
        return;
    const int w = cr.right - cr.left, h = cr.bottom - cr.top;
    if (std::abs(w - cw) <= 2 && std::abs(h - ch) <= 2)
    {
        keepW_ = keepH_ = 0;
        keepTries_ = 0;
        return;
    }
    const ULONGLONG now = GetTickCount64();
    if (w != keepW_ || h != keepH_)
    {
        keepW_ = w; // a new size: wait until it stays for a moment (the game may still be changing it)
        keepH_ = h;
        keepSince_ = now;
        return;
    }
    if (now - keepSince_ < 1500 || now < keepNext_ || keepTries_ >= 4)
        return;
    ++keepTries_;
    keepNext_ = now + 4000;
    ResizeGameWindow(true);
    SetNote(std::format(ui::GameResolutionRestored, SizeText(w, h), SizeText(cw, ch)), Note::Info);
}

void App::ResizeGameWindow(bool quiet)
{
    if (selected_ < 0 || selected_ >= static_cast<int>(games_.size()))
    {
        if (!quiet)
            SetNote(ui::PickAGameFirst, Note::Info);
        return;
    }
    HWND w = games_[selected_].hwnd;
    if (!IsWindow(w))
    {
        if (!quiet)
            SetNote(ui::ThatWindowIsGonePressRefresh, Note::Error);
        return;
    }
    if (upWnd_ != w)
    {
        // remember where the window was, to put it back when the capture ends
        WINDOWPLACEMENT wp{};
        wp.length = sizeof(wp);
        if ((IsZoomed(w) || IsIconic(w)) && GetWindowPlacement(w, &wp))
            upRect_ = wp.rcNormalPosition;
        else
            GetWindowRect(w, &upRect_);
        upWnd_ = w;
    }
    if (IsZoomed(w) || IsIconic(w))
        ShowWindow(w, SW_RESTORE);
    MONITORINFO mi{};
    mi.cbSize = sizeof(mi);
    if (!GetMonitorInfoW(MonitorFromWindow(w, MONITOR_DEFAULTTONEAREST), &mi))
        return;
    static const double factor[] = { 1.5, 1.7, 2.0, 1.0 };
    const double f = factor[(st_.upscaleLevel >= 0 && st_.upscaleLevel <= 3) ? st_.upscaleLevel : 1];
    const int mw = mi.rcMonitor.right - mi.rcMonitor.left;
    const int mh = mi.rcMonitor.bottom - mi.rcMonitor.top;
    const int cw = (static_cast<int>(mw / f + 0.5)) & ~1;
    const int ch = (static_cast<int>(mh / f + 0.5)) & ~1;
    RECT rc{ 0, 0, cw, ch };
    AdjustWindowRectEx(&rc, static_cast<DWORD>(GetWindowLongPtrW(w, GWL_STYLE)), FALSE, static_cast<DWORD>(GetWindowLongPtrW(w, GWL_EXSTYLE)));
    SetWindowPos(w, nullptr, mi.rcMonitor.left, mi.rcMonitor.top, rc.right - rc.left, rc.bottom - rc.top, SWP_NOZORDER | SWP_NOACTIVATE);
    RECT cr{};
    GetClientRect(w, &cr);
    if (quiet)
        return;
    if (cr.right - cr.left == cw && cr.bottom - cr.top == ch)
        {
        static const wchar_t* const methodN[] = { ui::Off, L"Bilinear", L"FSR 1", L"NIS", L"FSR3/4", L"DLSS" };
        static const wchar_t* const levelN[] = { ui::Quality, ui::Balanced, ui::Performance, ui::Native };
        const std::wstring mode = std::wstring(methodN[(st_.upscale >= 0 && st_.upscale <= 5) ? st_.upscale : 0]) + L" \u00B7 " +
                                  levelN[(st_.upscaleLevel >= 0 && st_.upscaleLevel <= 3) ? st_.upscaleLevel : 0];
        SetNote(std::format(ui::GameResolutionScaled, SizeText(cw, ch), mode, SizeText(mw, mh)), Note::Ok);
    }
    else
        SetNote(std::format(ui::GameResolutionAdjusted, SizeText(cr.right - cr.left, cr.bottom - cr.top), SizeText(cw, ch)), Note::Info);
}

// Folder of the saved profile of a game: <launcher folder>\Game Profiles\<exe name without .exe>
bool App::ProfileFolder(const std::wstring& exe, std::wstring& dir) const
{
    if (exe.empty())
        return false;
    std::wstring name = exe;
    const size_t dot = name.find_last_of(L'.');
    if (dot != std::wstring::npos && dot > 0)
        name.resize(dot);
    for (wchar_t& c : name)
        if (wcschr(L"<>:\"/\\|?*", c) != nullptr)
            c = L'_';
    while (!name.empty() && (name.back() == L' ' || name.back() == L'.'))
        name.pop_back();
    if (name.empty())
        return false;
    dir = oc::ExeDir() + kProfilesDir + L"\\" + name;
    return true;
}

// Button "Save game profile": copies the OptiScaler.ini next to the launcher into the game's own profile folder.
void App::SaveProfile()
{
    if (selected_ < 0 || selected_ >= static_cast<int>(games_.size()))
    {
        SetNote(ui::PickAGameInTheListFirst2, Note::Error);
        return;
    }
    const std::wstring exe = games_[selected_].exe;
    std::wstring dir;
    if (!ProfileFolder(exe, dir))
    {
        SetNote(ui::ThisWindowHasNoProgramNameSo, Note::Error);
        return;
    }
    const std::wstring src = oc::ExeDir() + L"OptiScaler.ini";
    if (GetFileAttributesW(src.c_str()) == INVALID_FILE_ATTRIBUTES)
    {
        SetNote(ui::OptiscalerIniWasNotFoundNextTo, Note::Error);
        return;
    }
    CreateDirectoryW((oc::ExeDir() + kProfilesDir).c_str(), nullptr);
    CreateDirectoryW(dir.c_str(), nullptr);
    if (!CopyFileW(src.c_str(), (dir + L"\\" + SlotFile(profileSlot_)).c_str(), FALSE))
    {
        SetNote(ui::CouldNotSaveTheProfileCheckThe, Note::Error);
        return;
    }
    RememberSlot(exe, profileSlot_);
    SetNote(std::format(ui::ProfileSaved, profileSlot_, exe, std::wstring(kProfilesDir) + L"\\" + dir.substr(dir.find_last_of(L'\\') + 1)), Note::Ok);
}

// Puts the saved OptiScaler.ini of a game back next to the launcher.
void App::ApplyProfile(const std::wstring& exe, int slot)
{
    std::wstring dir;
    if (!ProfileFolder(exe, dir))
        return;
    const std::wstring src = dir + L"\\" + SlotFile(slot);
    if (GetFileAttributesW(src.c_str()) == INVALID_FILE_ATTRIBUTES)
    {
        SetNote(std::format(ui::EmptyProfile, slot, exe), Note::Info);
        return;
    }
    if (!CopyFileW(src.c_str(), (oc::ExeDir() + L"OptiScaler.ini").c_str(), FALSE))
    {
        SetNote(std::format(ui::ProfileApplyFailed, exe), Note::Error);
        return;
    }
    ReadPictureState();
    profileSlot_ = slot;
    RememberSlot(exe, slot);
    SetNote(std::format(ui::ProfileApplied, slot, exe), Note::Ok);
}

// A game was picked in the list: if it has a saved profile, apply it (auto mode) or ask.
void App::OfferProfile()
{
    if (selected_ < 0 || selected_ >= static_cast<int>(games_.size()) || selected_ == lastOffered_)
        return;
    lastOffered_ = selected_;
    const std::wstring exe = games_[selected_].exe;
    int slot = LastSlot(exe);
    profileSlot_ = slot;
    if (!SlotExists(exe, slot))
    {
        slot = 0;
        for (int i = 1; i <= 4 && !slot; ++i)
            if (SlotExists(exe, i))
                slot = i;
        if (!slot)
            return;
        profileSlot_ = slot;
    }
    if (st_.profileAuto)
    {
        ApplyProfile(exe, slot);
        return;
    }
    profileExe_ = exe;
    profileApplySlot_ = slot;
    warnText_[kWarnProfile] = std::format(ui::ConfirmProfileApply, slot, exe);
    dlgKind_ = kWarnProfile;
    dlgProfile_ = true;
    dlgDontShow_ = false;
    dlgStart_ = false;
}

void App::CloseDialog(bool accepted)
{
    const int kind = dlgKind_;
    const bool wasStart = dlgStart_;
    if (dlgProfile_)
    {
        // "Apply the saved profile?": the check box means "do this automatically from now on"
        const bool autoNow = dlgDontShow_;
        dlgKind_ = -1;
        dlgDontShow_ = false;
        dlgProfile_ = false;
        if (accepted)
        {
            if (autoNow)
            {
                st_.profileAuto = 1;
                Save();
            }
            ApplyProfile(profileExe_, profileApplySlot_);
        }
        Invalidate();
        return;
    }
    if (kind >= 0 && dlgDontShow_)
    {
        hideWarn_[kind] = true;
        WritePrivateProfileStringW(L"Warnings", kWarnKeys[kind], L"1", oc::SettingsPath().c_str());
    }
    dlgKind_ = -1;
    dlgDontShow_ = false;
    dlgStart_ = false;
    Invalidate();

    if (wasStart)
    {
        if (accepted)
        {
            startConfirmed_ = true;
            Start();
        }
    }
    else
    {
        OpenNextDialog();
    }
}

// Reads the host's status file (about once a second) and raises the warnings that follow from it.
void App::CheckLive()
{
    liveValid_ = oc::ReadLiveStats(live_) && live_.pid == hostPid_ && GetTickCount64() - live_.stamp < 4000;
    if (!liveValid_)
    {
        capActive_ = false;
        blockActive_ = false;
        blockCount_ = 0;
        if (!IsIconic(hwnd_))
            Invalidate();
        return;
    }

    capActive_ = false;
    blockActive_ = false;
    lastLiveStamp_ = live_.stamp;

    if (!IsIconic(hwnd_))
        Invalidate();
}

// The live panel that replaces the (read-only) game list while a capture runs.
void App::PaintLive(gp::Graphics& g, const gp::RectF& view)
{
    FillRound(g, gp::RectF(view.X - S(2), view.Y - S(2), view.Width + S(4), view.Height + S(4)), S(10), col::Card());

    {
        gp::SolidBrush dot(liveValid_ ? col::Green() : col::Amber());
        g.FillEllipse(&dot, view.X + S(4), view.Y + S(8), S(9), S(9));
    }
    Text(g, liveValid_ ? ui::LiveStatus : ui::LiveStatusWaitingForTheHost, *f_.bodyB,
         gp::RectF(view.X + S(20), view.Y + S(2), view.Width - S(24), S(22)), col::Text());

    float y = view.Y + S(30);
    const float rowH = S(24);
    auto row = [&](const wchar_t* label, const std::wstring& value, const gp::Color& tone)
    {
        Text(g, label, *f_.body, gp::RectF(view.X + S(6), y, S(170), rowH), col::Muted());
        Text(g, value, *f_.bodyB, gp::RectF(view.X + S(180), y, view.Width - S(186), rowH), tone);
        y += rowH;
    };


    static const wchar_t* vsShort[] = { ui::Auto, ui::On0, ui::On1, ui::Off };
    static const wchar_t* focusNames[] = { ui::Game, ui::Overlay, ui::OtherWindow };

    const std::wstring dash = L"–";
    row(ui::Game, targetTitle_.empty() ? dash : targetTitle_, col::Text());
    row(ui::DisplayRefresh, liveValid_ && live_.refreshHz > 0 ? Num(live_.refreshHz, 0) + L" Hz" : dash, col::Text());
    row(ui::GameFramesCapture, liveValid_ ? Num(live_.captureFps, 1) + ui::FPSUnit + (capActive_ ? ui::LooksCapped : L"") : dash,
        capActive_ ? col::Amber() : col::Text());
    row(ui::PresentedByTheHost, liveValid_ ? Num(live_.presentFps, 1) + L" /s" : dash, col::Text());
    row(ui::PresentsCountedByDxgi, liveValid_ ? Num(live_.dxgiFps, 1) + L" /s" : dash, col::Text());
    row(ui::TimeInsidePresent,
        liveValid_ ? std::format(ui::PresentTiming, Num(live_.presentMs, 2), Num(live_.waitMs, 2)) : dash,
        blockActive_ ? col::Amber() : col::Text());
    row(ui::SyncInterval,
        liveValid_ ? std::to_wstring(live_.interval) + L"   · VSync " + vsShort[vsyncMode_ < 0 ? 0 : (vsyncMode_ == 2 ? 1 : (vsyncMode_ == 1 ? 2 : 3))] : dash,
        col::Text());
    row(ui::Focus, liveValid_ ? focusNames[(live_.focus >= 0 && live_.focus <= 2) ? live_.focus : 2] : dash, col::Text());
    row(ui::FrameGeneration, kFgNames[(st_.frameGenMode >= 0 && st_.frameGenMode <= 10) ? st_.frameGenMode : 0], col::Text());

    if (capActive_ || blockActive_)
    {
        std::wstring w = capActive_ ? ui::GameLooksCapped : L"";
        if (blockActive_)
            w += (w.empty() ? L"" : L"  ·  ") + std::wstring(ui::PresentIsWaiting);
        Text(g, L"Check: " + w, *f_.sm, gp::RectF(view.X + S(6), y + S(4), view.Width - S(12), S(20)), col::Amber());
    }
}

// The warning dialog, drawn over the whole window (no separate window: the launcher is plain GDI+).
void App::PaintDialog(gp::Graphics& g, int W, int H)
{
    hits_.clear(); // nothing below the dialog can be clicked

    gp::SolidBrush dim(gp::Color(175, 6, 8, 12));
    g.FillRectangle(&dim, 0, 0, W, H);

    const int k = (dlgKind_ >= 0 && dlgKind_ < kWarnCount) ? dlgKind_ : 0;
    const gp::RectF card = R(130, 224, 676, 330);
    FillRound(g, card, S(16), col::Card());
    StrokeRound(g, card, S(16), col::Alpha(col::Amber(), 150), max(1.0f, S(1.5f)));

    {
        gp::SolidBrush dot(col::Amber());
        const gp::RectF d = R(160, 250, 12, 12);
        g.FillEllipse(&dot, d.X, d.Y, d.Width, d.Height);
    }
    Text(g, kWarnTitles[k], *f_.h2, R(182, 242, 590, 28), col::Amber());
    Text(g, warnText_[k], *f_.body, R(160, 282, 616, 190), col::Text(), 0, true);

    // "Don't show this warning again"
    {
        const gp::RectF box = R(160, 486, 20, 20);
        const bool hot = hover_ == kIdDlgCheck;
        FillRound(g, box, S(5), dlgDontShow_ ? col::Accent() : col::Row());
        StrokeRound(g, box, S(5), hot ? col::Accent2() : col::Line(), max(1.0f, S(1)));
        if (dlgDontShow_)
            Text(g, L"✓", *f_.chip, box, col::White(), 1);
        Text(g, dlgProfile_ ? ui::AlwaysApplySavedProfilesAutomatically : ui::DonTShowThisWarningAgain, *f_.body, R(190, 484, 380, 24), col::Text());
        AddHit(R(160, 480, 360, 32), kIdDlgCheck);
    }

    auto button = [&](const gp::RectF& r, const wchar_t* text, int id, bool primary)
    {
        const bool hot = hover_ == id;
        if (primary)
            FillRound(g, r, S(10), hot ? col::Accent2() : col::Accent());
        else
            FillRound(g, r, S(10), hot ? col::RowHot() : col::Row());
        StrokeRound(g, r, S(10), hot ? col::Accent() : col::Line(), max(1.0f, S(1)));
        Text(g, text, *f_.chip, r, primary ? col::White() : col::Text(), 1);
        AddHit(r, id);
    };

    if (dlgProfile_)
    {
        button(R(494, 512, 130, 34), ui::No, kIdDlgCancel, false);
        button(R(636, 512, 150, 34), ui::Apply, kIdDlgOk, true);
    }
    else if (dlgStart_)
    {
        button(R(494, 512, 130, 34), ui::Cancel, kIdDlgCancel, false);
        button(R(636, 512, 150, 34), ui::StartAnyway, kIdDlgOk, true);
    }
    else
    {
        button(R(656, 512, 130, 34), L"OK", kIdDlgOk, true);
    }
}

// ---------------------------------------------------------------------------------------------------------------
// Input
// ---------------------------------------------------------------------------------------------------------------

int App::HitAt(int x, int y) const
{
    for (auto it = hits_.rbegin(); it != hits_.rend(); ++it)
    {
        if (it->r.Contains(static_cast<float>(x), static_cast<float>(y)))
            return it->id;
    }
    return kIdNone;
}

void App::BeginCapture(int sc)
{
    capturing_ = sc;
    SetNote(L"");
    UnregisterHotKey(hwnd_, kHotkeyShow); // so that its own combination can be assigned too
    Invalidate();
}

LRESULT App::OnCaptureKey(UINT vk)
{
    const int sc = capturing_;

    if (vk == VK_ESCAPE)
    {
        capturing_ = -1;
        RegisterShowHotkey();
        Invalidate();
        return 0;
    }
    if (IsModifierVk(vk))
        return 0; // wait for the real key

    if (oc::Hotkey* target = HotkeyRef(sc))
    {
        // Stop / launcher: a global shortcut, so it needs Ctrl, Shift or Alt (otherwise it would clash with the game).
        UINT mods = 0;
        if (GetKeyState(VK_CONTROL) & 0x8000)
            mods |= MOD_CONTROL;
        if (GetKeyState(VK_SHIFT) & 0x8000)
            mods |= MOD_SHIFT;
        if (GetKeyState(VK_MENU) & 0x8000)
            mods |= MOD_ALT;
        if (mods == 0)
        {
            SetNote(ui::HoldCtrlShiftOrAltTogetherWith, Note::Error);
            return 0;
        }

        oc::Hotkey candidate;
        candidate.mods = mods;
        candidate.vk = vk;
        for (int other = 0; other < kScCount; ++other)
        {
            const oc::Hotkey* h = HotkeyRef(other);
            if (other != sc && h && *h == candidate)
            {
                SetNote(std::format(ui::ShortcutInUse, kShortcutDefs[other].label), Note::Error);
                return 0;
            }
        }
        *target = candidate;
    }
    else
    {
        // OptiScaler's own keys: a single key, no modifiers.
        for (int other = 0; other < kScCount; ++other)
        {
            const UINT* k = KeyRef(other);
            if (other != sc && k && *k == vk)
            {
                SetNote(std::format(ui::ShortcutInUse, kShortcutDefs[other].label), Note::Error);
                return 0;
            }
        }
        if (UINT* k = KeyRef(sc))
            *k = vk;
    }

    capturing_ = -1;
    Save();
    WriteOptiScalerKeys(st_);
    SetNote(ui::SavedToOptiscalerIni, Note::Ok);
    RegisterShowHotkey();
    Invalidate();
    return 0;
}

bool IsGameStillRunning(HWND gameHwnd, const std::wstring& exeName)
{
    if (gameHwnd && IsWindow(gameHwnd))
        return true;
    if (!exeName.empty() && IsProcessRunningByExe(exeName))
        return true;
    return false;
}

bool CloseGameCompletely(HWND gameHwnd, const std::wstring& exeName)
{
    DWORD wndPid = 0;
    if (gameHwnd && IsWindow(gameHwnd))
    {
        GetWindowThreadProcessId(gameHwnd, &wndPid);
        PostMessageW(gameHwnd, WM_CLOSE, 0, 0);
    }

    const ULONGLONG graceStart = GetTickCount64();
    while (GetTickCount64() - graceStart < 2500)
    {
        if (!IsGameStillRunning(gameHwnd, exeName))
            return true;
        Sleep(100);
    }

    if (wndPid != 0 && wndPid != GetCurrentProcessId())
    {
        if (HANDLE pw = OpenProcess(PROCESS_TERMINATE | SYNCHRONIZE, FALSE, wndPid))
        {
            TerminateProcess(pw, 0);
            WaitForSingleObject(pw, 3000);
            CloseHandle(pw);
        }
    }

    HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snap != INVALID_HANDLE_VALUE)
    {
        PROCESSENTRY32W pe{};
        pe.dwSize = sizeof(pe);
        if (Process32FirstW(snap, &pe))
        {
            do
            {
                if (pe.th32ProcessID == GetCurrentProcessId())
                    continue;
                if (!_wcsicmp(pe.szExeFile, L"OptiCaptureLauncher.exe") || !_wcsicmp(pe.szExeFile, L"OptiCaptureHost.exe"))
                    continue;
                if (!exeName.empty() && _wcsicmp(pe.szExeFile, exeName.c_str()) == 0)
                {
                    if (HANDLE p = OpenProcess(PROCESS_TERMINATE | SYNCHRONIZE, FALSE, pe.th32ProcessID))
                    {
                        TerminateProcess(p, 0);
                        WaitForSingleObject(p, 3000);
                        CloseHandle(p);
                    }
                }
            } while (Process32NextW(snap, &pe));
        }
        CloseHandle(snap);
    }

    const ULONGLONG settleStart = GetTickCount64();
    while (GetTickCount64() - settleStart < 2000)
    {
        if (!IsGameStillRunning(gameHwnd, exeName))
            break;
        Sleep(100);
    }
    Sleep(250);
    return !IsGameStillRunning(gameHwnd, exeName);
}

void App::InstallReshadeForSelectedGame()
{
    if (selected_ < 0 || selected_ >= static_cast<int>(games_.size()))
    {
        SetNote(ui::PickAGameFromTheListFirst, Note::Error);
        return;
    }
    const GameItem game = games_[selected_];
    if (game.fullExe.empty())
    {
        SetNote(ui::CouldNotReadTheGameSExecutable, Note::Error);
        return;
    }

    const size_t slash = game.fullExe.find_last_of(L"\\/");
    if (slash == std::wstring::npos)
    {
        SetNote(ui::InvalidGameExecutablePath, Note::Error);
        return;
    }
    const std::wstring gameDir = game.fullExe.substr(0, slash);
    const std::wstring bundleDir = oc::ExeDir() + kReshadeBundleDir;

    const DWORD srcAttr = GetFileAttributesW(bundleDir.c_str());
    if (srcAttr == INVALID_FILE_ATTRIBUTES || !(srcAttr & FILE_ATTRIBUTE_DIRECTORY))
    {
        CreateDirectoryW(bundleDir.c_str(), nullptr);
        SetNote(std::format(ui::BundleFolderCreated, kReshadeBundleDir), Note::Error);
        return;
    }

    WIN32_FIND_DATAW fd{};
    HANDLE hf = FindFirstFileW((bundleDir + L"\\*").c_str(), &fd);
    bool hasAny = false;
    if (hf != INVALID_HANDLE_VALUE)
    {
        do
        {
            if (wcscmp(fd.cFileName, L".") != 0 && wcscmp(fd.cFileName, L"..") != 0)
            {
                hasAny = true;
                break;
            }
        } while (FindNextFileW(hf, &fd));
        FindClose(hf);
    }
    if (!hasAny)
    {
        SetNote(std::format(ui::EmptyBundleFolder, kReshadeBundleDir), Note::Error);
        return;
    }

    KillTimer(hwnd_, kTimerList);
    const bool wasRunning = IsGameStillRunning(game.hwnd, game.exe);
    bool closedOk = true;
    if (wasRunning)
    {
        SetNote(std::format(ui::ClosingGameForReshade, game.exe), Note::Info);
        UpdateWindow(hwnd_);
        closedOk = CloseGameCompletely(game.hwnd, game.exe);
    }

    int copied = 0, failed = 0;
    CopyTree(bundleDir, gameDir, copied, failed);

    bool relaunched = false;
    if (wasRunning && closedOk)
    {
        SHELLEXECUTEINFOW sei{};
        sei.cbSize = sizeof(sei);
        sei.lpVerb = L"open";
        sei.lpFile = game.fullExe.c_str();
        sei.lpDirectory = gameDir.c_str();
        sei.nShow = SW_SHOWNORMAL;
        relaunched = ShellExecuteExW(&sei) != FALSE;
    }

    SetTimer(hwnd_, kTimerList, 3000, nullptr);
    Refresh();
    if (hwnd_ && IsWindow(hwnd_))
    {
        ShowWindow(hwnd_, SW_SHOWNOACTIVATE);
        DockPanel();
    }

    std::wstring msg = std::format(ui::ReshadeCopyHelp, copied, gameDir);
    if (wasRunning)
    {
        if (relaunched)
            msg += L"\n\n" + std::format(ui::ReshadeGameRelaunched, game.exe);
        else if (!closedOk)
            msg += L"\n\n" + std::format(ui::ReshadeGameStillRunning, game.exe);
        else
            msg += L"\n\n" + std::format(ui::ReshadeGameClosed, game.exe);
    }
    if (failed > 0)
        msg += L"\n\n" + std::format(ui::ReshadeCopyFailed, failed);

    MessageBoxW(hwnd_, msg.c_str(), ui::ReshadeInstallation, MB_OK | (failed > 0 ? MB_ICONWARNING : MB_ICONINFORMATION));
    SetNote(relaunched ? std::format(ui::ReshadeCopyAndRestart, copied, game.exe)
                       : std::format(ui::ReshadeCopyComplete, copied, game.exe),
            failed > 0 ? Note::Error : Note::Ok);
}

void App::OnClick(int id)
{
    if (dlgKind_ >= 0)
    {
        if (id == kIdDlgCheck)
            dlgDontShow_ = !dlgDontShow_;
        else if (id == kIdDlgOk)
            CloseDialog(true);
        else if (id == kIdDlgCancel)
            CloseDialog(false);
        return;
    }

    if (id == kIdWarnReset)
    {
        for (int i = 0; i < kWarnCount; ++i)
        {
            hideWarn_[i] = false;
            WritePrivateProfileStringW(L"Warnings", kWarnKeys[i], L"0", oc::SettingsPath().c_str());
        }
        SetNote(ui::HiddenWarningsWillBeShownAgain, Note::Ok);
        return;
    }

    if (id == kIdRefresh)
    {
        Refresh();
    }
    else if (id == kIdVsync)
    {
        // Order in the menu: Auto, On with SyncInterval 0, On with SyncInterval 1, Off.
        static const wchar_t* names[] = {
            ui::AutoDefaultNothingForced, ui::OnForcevsyncTrueSyncinterval0,
            ui::OnForcevsyncTrueSyncinterval1, ui::OffForcevsyncFalse
        };
        static const int modes[] = { -1, 2, 1, 0 };
        HMENU menu = CreatePopupMenu();
        if (menu)
        {
            int cur = 0;
            for (int v = 0; v < 4; ++v)
                if (modes[v] == vsyncMode_) cur = v;
            for (int v = 0; v < 4; ++v)
                AppendMenuW(menu, MF_STRING | (cur == v ? MF_CHECKED : 0), kVsyncMenuBase + v, names[v]);
            POINT pt{};
            GetCursorPos(&pt);
            const UINT cmd = TrackPopupMenu(menu, TPM_RETURNCMD | TPM_NONOTIFY | TPM_RIGHTBUTTON, pt.x, pt.y, 0, hwnd_, nullptr);
            DestroyMenu(menu);
            if (cmd >= kVsyncMenuBase && cmd <= kVsyncMenuBase + 3)
            {
                const int v = static_cast<int>(cmd - kVsyncMenuBase);
                vsyncMode_ = modes[v];
                if (ApplyVsyncToOptiScaler())
                {
                    SetNote(std::format(ui::VsyncChanged, names[v]), Note::Ok);
                }
                else
                {
                    SetNote(ui::CouldNotUpdateOptiscalerIniCheckThe, Note::Error);
                }
            }
        }
    }
    else if (id == kIdStart)
    {
        Start();
    }
    else if (id == kIdReset)
    {
        if (Running())
        {
            SetNote(ui::StopTheHostFirstThenReset, Note::Error);
        }
        else if (MessageBoxW(hwnd_, ui::ResetAllSettingsToTheirDefaultsFrame,
                             ui::ResetToDefault, MB_YESNO | MB_ICONQUESTION) == IDYES)
        {
            st_ = oc::Settings();
            st_.presentDelay = 0;  // reset values: no present delay,
            st_.presentMode = 0;   // Auto present mode,
            st_.upscalerMode = 1;  // AMD FSR 3.1.6
            st_.optiFps = 0;       // "OptiScaler fps = Hz" unchecked,
            vsyncMode_ = 0;        // VSync off
            Save();
            WriteOptiScalerKeys(st_);
            {
                const std::wstring ini = oc::ExeDir() + L"OptiScaler.ini";
                if (GetFileAttributesW(ini.c_str()) != INVALID_FILE_ATTRIBUTES)
                    WritePrivateProfileStringW(L"Framerate", L"FramerateLimit", L"0.000000", ini.c_str());
            }
            ApplyVsyncToOptiScaler();
            RegisterShowHotkey();
            OnClick(kIdFrameGenApply);
            OnClick(kIdUpscalerApply);
            ReadPictureState();
            SetNote(ui::AllSettingsResetToDefault, Note::Ok);
        }
    }
    else if (id == kIdLink)
    {
        ShellExecuteW(hwnd_, L"open", kTelegramUrl, nullptr, nullptr, SW_SHOWNORMAL);
    }
    else if (id == kIdLinkJH)
    {
        ShellExecuteW(hwnd_, L"open", kTelegramJHUrl, nullptr, nullptr, SW_SHOWNORMAL);
    }
    else if (id == kIdLinkApps)
    {
        ShellExecuteW(hwnd_, L"open", kTelegramAppsUrl, nullptr, nullptr, SW_SHOWNORMAL);
    }
    else if (id == kIdLinkGithub)
    {
        ShellExecuteW(hwnd_, L"open", kGithubUrl, nullptr, nullptr, SW_SHOWNORMAL);
    }
    else if (id == kIdLinkArchive)
    {
        ShellExecuteW(hwnd_, L"open", kTelegramArchiveUrl, nullptr, nullptr, SW_SHOWNORMAL);
    }
    else if (id == kIdFrameGenSelect)
    {
        HMENU menu = CreatePopupMenu();
        if (menu)
        {
            for (int k = 0; k < 10; ++k)
                AppendMenuW(menu, MF_STRING | (st_.frameGenMode == kFgMenuOrder[k] ? MF_CHECKED : 0), kFgMenuBase + kFgMenuOrder[k],
                            kFgNames[kFgMenuOrder[k]]);

            POINT pt{};
            GetCursorPos(&pt);
            const UINT cmd = TrackPopupMenu(
                menu, TPM_RETURNCMD | TPM_NONOTIFY | TPM_RIGHTBUTTON,
                pt.x, pt.y, 0, hwnd_, nullptr);
            DestroyMenu(menu);

            if (cmd >= kFgMenuBase && cmd <= kFgMenuBase + 10)
            {
                st_.frameGenMode = static_cast<int>(cmd - kFgMenuBase);
                // VSync and "OptiScaler FPS = Hz" are left as they are: only the quick modes at the top set them
                Save();
                OnClick(kIdUpscalerApply); // the chosen Opti Upscaler is written again with every frame generation choice

                OnClick(kIdFrameGenApply); // choosing writes OptiScaler.ini straight away (no Apply button)
            }
        }
    }
    else if (id == kIdFrameGenApply)
    {
        if (st_.frameGenMode < 0 || st_.frameGenMode > 10 || st_.frameGenMode == 3)
        {
            SetNote(ui::InvalidFrameGenerationSelection, Note::Error);
        }
        else
        {
            // Every frame generation is just two lines of [FrameGen] in OptiScaler.ini (the file itself is not replaced).
            //                                      0        1         2        3       4        5        6         7       8       9        10
            static const wchar_t* const output[11] = { L"XeFG", L"DLSSG", L"nofg", L"XeFG", L"XeFG", L"DLSSG", L"DLSSG", L"DLSSG", L"DLSSG", L"FSRFG", L"FSRFG" };
            static const wchar_t* const nvngx[11] = { L"auto", L"Arturs", L"auto", L"auto", L"auto", L"Combo", L"Nukems", L"FFX", L"None", L"auto", L"auto" };
            const int m = st_.frameGenMode;
            if (PutIni(L"FrameGen", L"FGOutput", output[m]) && PutIni(L"FrameGen", L"FGNvngxReplacement", nvngx[m]))
            {
                WriteOptiScalerKeys(st_);
                ApplyVsyncToOptiScaler();
                Save();
                SetNote(std::format(ui::FrameGenerationChanged, output[m], nvngx[m]), Note::Ok);
            }
        }
        // the ini was just replaced: the chosen upscaler, the multi frame generation count and the picture upscaler go back in
        if (st_.frameGenMode >= 0 && st_.frameGenMode <= 10 && st_.frameGenMode != 3)
            OnClick(kIdUpscalerApply);
        ApplyMfgCount();
        ApplyOptiFpsLimit();
        if (st_.upscale != 0)
            WritePictureIni(true);
        ReadPictureState();
    }
    else if (id == kIdPresentSelect)
    {
        static const wchar_t* names[] = {
            ui::AutoFgOnNoWaitFgOff, ui::AlwaysNoWaitInterval0, ui::AlwaysWaitForVblankInterval1,
            ui::ClassicBehavesLikeVersion220, ui::LosslessStyleLowLatencyTearingAllowed,
            ui::ClassicHalfRefreshExactlyRefresh2Frames,
            ui::ClassicUnlimitedSameNoHalfRefreshLimit
        };
        HMENU menu = CreatePopupMenu();
        if (menu)
        {
            for (int v = 0; v <= 6; ++v)
                AppendMenuW(menu, MF_STRING | (st_.presentMode == v ? MF_CHECKED : 0), kPresentMenuBase + v, names[v]);
            POINT pt{};
            GetCursorPos(&pt);
            const UINT cmd = TrackPopupMenu(menu, TPM_RETURNCMD | TPM_NONOTIFY | TPM_RIGHTBUTTON, pt.x, pt.y, 0, hwnd_, nullptr);
            DestroyMenu(menu);
            if (cmd >= kPresentMenuBase && cmd <= kPresentMenuBase + 6)
            {
                st_.presentMode = static_cast<int>(cmd - kPresentMenuBase);
                Save();
                ApplyVsyncToOptiScaler(); // classic mode keeps VSync host-side only
                SetNote(L"Present mode: " + std::wstring(names[st_.presentMode]) + (Running() ? L". Restart the game to apply." : L"."), Note::Ok);
            }
        }
    }
    else if (id == kIdDelaySelect)
    {
        HMENU menu = CreatePopupMenu();
        if (menu)
        {
            for (int v = 0; v <= 12; ++v)
            {
                std::wstring t = std::to_wstring(v) + L" ms";
                if (v == 0) t += ui::Off2;
                AppendMenuW(menu, MF_STRING | (st_.presentDelay == v ? MF_CHECKED : 0), kDelayMenuBase + v, t.c_str());
            }
            POINT pt{};
            GetCursorPos(&pt);
            const UINT cmd = TrackPopupMenu(menu, TPM_RETURNCMD | TPM_NONOTIFY | TPM_RIGHTBUTTON, pt.x, pt.y, 0, hwnd_, nullptr);
            DestroyMenu(menu);
            if (cmd >= static_cast<UINT>(kDelayMenuBase) && cmd <= static_cast<UINT>(kDelayMenuBase + 12))
            {
                st_.presentDelay = static_cast<int>(cmd - kDelayMenuBase);
                Save();
                SetNote(L"Present delay: " + std::to_wstring(st_.presentDelay) + L" ms" + (Running() ? L" (applied live)." : L"."), Note::Ok);
            }
        }
    }
    else if (id == kIdLimitTarget)
    {
        static const wchar_t* names[] = { ui::Off, ui::GameTheSelectedGameSExe, ui::HostOpticapturehostExe, ui::GlobalEveryProgram };
        HMENU menu = CreatePopupMenu();
        if (menu)
        {
            for (int v = 0; v <= 3; ++v)
                AppendMenuW(menu, MF_STRING | (st_.limitTarget == v ? MF_CHECKED : 0), kLimitTargetMenuBase + v, names[v]);
            POINT pt{};
            GetCursorPos(&pt);
            const UINT cmd = TrackPopupMenu(menu, TPM_RETURNCMD | TPM_NONOTIFY | TPM_RIGHTBUTTON, pt.x, pt.y, 0, hwnd_, nullptr);
            DestroyMenu(menu);
            if (cmd >= static_cast<UINT>(kLimitTargetMenuBase) && cmd <= static_cast<UINT>(kLimitTargetMenuBase + 3))
            {
                st_.limitTarget = static_cast<int>(cmd - kLimitTargetMenuBase);
                Save();
                SetNote(ui::PressApplyToUseTheLimiterSetting, Note::Info);
            }
        }
    }
    else if (id == kIdLimitMode)
    {
        static const wchar_t* names[] = { ui::Async2, ui::FrontEdgeSyncDefault, ui::BackEdgeSync2 };
        HMENU menu = CreatePopupMenu();
        if (menu)
        {
            for (int v = 0; v <= 2; ++v)
                AppendMenuW(menu, MF_STRING | (st_.limitMode == v ? MF_CHECKED : 0), kLimitModeMenuBase + v, names[v]);
            POINT pt{};
            GetCursorPos(&pt);
            const UINT cmd = TrackPopupMenu(menu, TPM_RETURNCMD | TPM_NONOTIFY | TPM_RIGHTBUTTON, pt.x, pt.y, 0, hwnd_, nullptr);
            DestroyMenu(menu);
            if (cmd >= static_cast<UINT>(kLimitModeMenuBase) && cmd <= static_cast<UINT>(kLimitModeMenuBase + 2))
            {
                st_.limitMode = static_cast<int>(cmd - kLimitModeMenuBase);
                Save();
                SetNote(ui::PressApplyToUseTheLimiterSetting, Note::Info);
            }
        }
    }
    else if (id == kIdLimitFps)
    {
        static const int fpsList[] = { 30, 40, 45, 48, 50, 60, 72, 75, 90, 100, 120, 144, 165, 240, 300, 360, 400, 500 };
        HMENU menu = CreatePopupMenu();
        if (menu)
        {
            AppendMenuW(menu, MF_STRING | (st_.limitFps == 0 ? MF_CHECKED : 0), kLimitFpsMenuBase + 0, ui::RefreshRateAuto);
            AppendMenuW(menu, MF_STRING | (st_.limitFps == -2 ? MF_CHECKED : 0), kLimitFpsMenuBase + 1, ui::Text12RefreshRateAuto);
            AppendMenuW(menu, MF_STRING | (st_.limitFps == -3 ? MF_CHECKED : 0), kLimitFpsMenuBase + 2, ui::Text13RefreshRateAuto);
            AppendMenuW(menu, MF_STRING | (st_.limitFps == -4 ? MF_CHECKED : 0), kLimitFpsMenuBase + 3, ui::Text14RefreshRateAuto);
            for (int i = 0; i < static_cast<int>(sizeof(fpsList) / sizeof(fpsList[0])); ++i)
                AppendMenuW(menu, MF_STRING | (st_.limitFps == fpsList[i] ? MF_CHECKED : 0), kLimitFpsMenuBase + 4 + i,
                            (std::to_wstring(fpsList[i]) + L" fps").c_str());
            POINT pt{};
            GetCursorPos(&pt);
            const UINT cmd = TrackPopupMenu(menu, TPM_RETURNCMD | TPM_NONOTIFY | TPM_RIGHTBUTTON, pt.x, pt.y, 0, hwnd_, nullptr);
            DestroyMenu(menu);
            if (cmd >= static_cast<UINT>(kLimitFpsMenuBase) && cmd < static_cast<UINT>(kLimitFpsMenuBase + 4 + static_cast<int>(sizeof(fpsList) / sizeof(fpsList[0]))))
            {
                const int k = static_cast<int>(cmd - kLimitFpsMenuBase);
                st_.limitFps = k == 0 ? 0 : (k <= 3 ? -(k + 1) : fpsList[k - 4]);
                Save();
                SetNote(ui::PressApplyToUseTheLimiterSetting, Note::Info);
            }
        }
    }
    else if (id == kIdD5Toggle || id == kIdDepthToggle)
    {
        const int kind = id == kIdDepthToggle ? 1 : 0;
        if (panelOpen_ && panelKind_ == kind)
        {
            panelOpen_ = false; // the same button again: close
        }
        else
        {
            panelKind_ = kind;
            panelOpen_ = true;
        }
        if (panel_)
        {
            if (panelOpen_)
            {
                ReadFgState();
                lastHave_ = DlssnrPresent();
                DockPanel();
                ShowWindow(panel_, SW_SHOWNOACTIVATE);
                InvalidatePanel();
            }
            else
            {
                ShowWindow(panel_, SW_HIDE);
            }
        }
        Invalidate();
    }
    else if (id == kIdDepthFlat || id == kIdDepthReshade)
    {
        st_.depthSource = id == kIdDepthReshade ? 1 : 0;
        if (st_.depthSource == 1)
        {
            st_.upscale = 5;      // DLSS
            st_.upscaleLevel = 0; // Quality
            WritePictureIni(true);
            ReadPictureState();
        }
        else
        {
            st_.upscale = 0; // Off when Flat is selected
        }
        Save();
        SetNote(st_.depthSource == 1
                    ? ui::DepthReshadeOutputScalingAutomaticallySetTo
                    : ui::DepthFlatAsAlwaysOutputScalingSet, Note::Ok);
        InvalidatePanel();
    }
    else if (id == kIdDepthInstall)
    {
        InstallReshadeForSelectedGame();
        InvalidatePanel();
    }
    else if (id == kIdDepthFolder)
    {
        const std::wstring dir = oc::ExeDir() + kReshadeBundleDir;
        CreateDirectoryW(dir.c_str(), nullptr);
        ShellExecuteW(hwnd_, L"open", dir.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
    }
    else if (id == kIdD5Enable)
    {
        if (PutIni(L"DlssNr", L"Enabled", dlss5On_ ? L"auto" : L"true"))
        {
            ReadFgState();
            SetNote(dlss5On_ ? ui::Dlss5EnabledDlssnrEnabledTrue : ui::Dlss5DisabledDlssnrEnabledAuto, Note::Ok);
        }
        InvalidatePanel();
    }
    else if (id == kIdHudFix)
    {
        if (PutIni(L"OptiFG", L"HUDFix", hudFix_ ? L"auto" : L"true"))
        {
            ReadFgState();
            SetNote(hudFix_ ? ui::HudFixOnOptifgHudfixTrue : ui::HudFixOffOptifgHudfixAuto, Note::Ok);
        }
    }
    else if (id == kIdAsyncFg)
    {
        if (PutIni(L"FSRFG", L"AllowAsync", asyncFg_ ? L"auto" : L"true"))
        {
            ReadFgState();
            SetNote(asyncFg_ ? ui::AsyncOnFsrfgAllowasyncTrue : ui::AsyncOffFsrfgAllowasyncAuto, Note::Ok);
        }
    }
    else if (id == kIdD5Download)
    {
        if (kDlssnrUrl[0] != 0)
            ShellExecuteW(hwnd_, L"open", kDlssnrUrl, nullptr, nullptr, SW_SHOWNORMAL);
        else
            SetNote(ui::TheDownloadLinkForNvngxDlssnrDll, Note::Error);
    }
    else if (id == kIdOptiFps)
    {
        st_.optiFps = st_.optiFps ? 0 : 1;
        Save();
        if (!st_.optiFps)
        {
            // switched off: OptiScaler's own limit goes to 0 (no limit)
            const std::wstring ini = oc::ExeDir() + L"OptiScaler.ini";
            if (GetFileAttributesW(ini.c_str()) != INVALID_FILE_ATTRIBUTES)
                WritePrivateProfileStringW(L"Framerate", L"FramerateLimit", L"0.000000", ini.c_str());
        }
        SetNote(st_.optiFps ? ui::OptiscalerSFrameLimitWillBeSet
                            : ui::OptiscalerSFrameLimitWasSetTo, Note::Ok);
    }
    else if (id == kIdLimitAuto)
    {
        st_.limitAuto = st_.limitAuto ? 0 : 1;
        Save();
        SetNote(st_.limitAuto ? ui::TheLimiterWillBeAppliedEveryTime : ui::TheLimiterIsAppliedOnlyWithThe, Note::Ok);
    }
    else if (id == kIdProfileSave)
    {
        SaveProfile();
    }
    else if (id >= kIdProfSlot0 && id < kIdProfSlot0 + 4)
    {
        profileSlot_ = id - kIdProfSlot0 + 1;
        if (selected_ >= 0 && selected_ < static_cast<int>(games_.size()))
            RememberSlot(games_[selected_].exe, profileSlot_);
        SetNote(L"Profile " + std::to_wstring(profileSlot_) + L" selected. Save stores the current settings in it, Load applies it.", Note::Info);
    }
    else if (id >= kIdMfg0 && id < kIdMfg0 + 5)
    {
        const int n = id - kIdMfg0 + 1;
        if (!MfgMode(st_.frameGenMode))
        {
            SetNote(ui::MultiFrameGenerationCountIsForIntel, Note::Info);
        }
        else
        {
            st_.mfgCount = st_.mfgCount == n ? 0 : n; // pressing the chosen one again goes back to auto
            Save();
            ApplyMfgCount();
            SetNote(st_.mfgCount == 0 ? std::wstring(L"Multi frame generation: auto.")
                                      : L"Multi frame generation: " + std::to_wstring(st_.mfgCount + 1) + L"x.", Note::Ok);
        }
    }
    else if (id >= kIdPreset0 && id < kIdPreset0 + 7)
    {
        if (Running())
            SetNote(ui::StopTheCaptureFirstThenPickA, Note::Info);
        else
            ApplyPreset(id - kIdPreset0);
    }
    else if (id == kIdProfileLoad)
    {
        if (selected_ >= 0 && selected_ < static_cast<int>(games_.size()))
            ApplyProfile(games_[selected_].exe, profileSlot_);
        else
            SetNote(ui::PickAGameInTheListFirst2, Note::Error);
    }
    else if (id == kIdProfileAuto)
    {
        st_.profileAuto = st_.profileAuto ? 0 : 1;
        Save();
        SetNote(st_.profileAuto ? ui::SavedGameProfilesWillBeAppliedAutomatically
                                : ui::YouWillBeAskedBeforeASaved, Note::Ok);
    }
    else if (id == kIdPictureOn || id == kIdPictureOff)
    {
        const bool on = id == kIdPictureOn;
        const bool ok = WritePictureIni(on);
        if (ok)
            SetNote(on ? ui::UpscalerOnThePictureIsOnSharpness
                       : ui::UpscalerOnThePictureIsOffRestart, Note::Ok);
        else
            SetNote(ui::CouldNotUpdateOptiscalerIniCheckThe, Note::Error);
    }
    else if (id == kIdBorderless)
    {
        const bool isOn = selected_ >= 0 && selected_ < static_cast<int>(games_.size()) && BorderIndex(games_[selected_].hwnd) >= 0;
        if (st_.upscale != 0 && !isOn)
            MessageBoxW(hwnd_, ui::BorderlessCannotBeUsedWhileOutputScaling,
                        ui::OutputScalingIsOn, MB_OK | MB_ICONINFORMATION);
        else
            ToggleBorderless();
    }
    else if (id == kIdUpResize)
    {
        ResizeGameWindow();
    }
    else if (id == kIdUpSelect)
    {
        // Input sizes for the monitor of the selected game (or the primary one)
        int mw = GetSystemMetrics(SM_CXSCREEN);
        int mh = GetSystemMetrics(SM_CYSCREEN);
        if (selected_ >= 0 && selected_ < static_cast<int>(games_.size()) && IsWindow(games_[selected_].hwnd))
        {
            MONITORINFO mi{};
            mi.cbSize = sizeof(mi);
            if (GetMonitorInfoW(MonitorFromWindow(games_[selected_].hwnd, MONITOR_DEFAULTTONEAREST), &mi))
            {
                mw = mi.rcMonitor.right - mi.rcMonitor.left;
                mh = mi.rcMonitor.bottom - mi.rcMonitor.top;
            }
        }
        static const wchar_t* lvName[] = { ui::Quality15x, ui::Balanced17x, ui::Performance2x, ui::Native1x };
        static const double lvF[] = { 1.5, 1.7, 2.0, 1.0 };
        static const wchar_t* mName[] = { L"", L"Bilinear", L"FSR 1", L"NIS", L"FSR3/4", L"DLSS" };   // by method number
        // order: FSR3/4 Native first, then FSR3/4 Quality / Balanced / Performance, DLSS the same, then FSR 1, NIS and bilinear, Off last
        struct Entry { int method; int level; };
        static const Entry entries[] = {
            { 4, 3 }, { 4, 0 }, { 4, 1 }, { 4, 2 },
            { 5, 3 }, { 5, 0 }, { 5, 1 }, { 5, 2 },
            { 2, 3 }, { 2, 0 }, { 2, 1 }, { 2, 2 }, { 3, 3 }, { 3, 0 }, { 3, 1 }, { 3, 2 }, { 1, 3 }, { 1, 0 }, { 1, 1 }, { 1, 2 } };
        constexpr int kEntries = static_cast<int>(sizeof(entries) / sizeof(entries[0]));
        HMENU menu = CreatePopupMenu();
        if (menu)
        {
            for (int i = 0; i < kEntries; ++i)
            {
                if (i > 0 && entries[i].method != entries[i - 1].method)
                    AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
                const Entry& e = entries[i];
                const int cw = (static_cast<int>(mw / lvF[e.level] + 0.5)) & ~1;
                const int ch = (static_cast<int>(mh / lvF[e.level] + 0.5)) & ~1;
                std::wstring t = std::wstring(mName[e.method]) + L"  \u00B7  " + lvName[e.level] + L", " + std::to_wstring(cw) + L"x" +
                                 std::to_wstring(ch) + L" \u2192 " + std::to_wstring(mw) + L"x" + std::to_wstring(mh) + L")";
                AppendMenuW(menu, MF_STRING | ((st_.upscale == e.method && st_.upscaleLevel == e.level) ? MF_CHECKED : 0),
                            kUpMenuBase + 1 + i, t.c_str());
            }
            AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
            AppendMenuW(menu, MF_STRING | (st_.upscale == 0 ? MF_CHECKED : 0), kUpMenuBase, ui::Off);
            POINT pt{};
            GetCursorPos(&pt);
            const UINT cmd = TrackPopupMenu(menu, TPM_RETURNCMD | TPM_NONOTIFY | TPM_RIGHTBUTTON, pt.x, pt.y, 0, hwnd_, nullptr);
            DestroyMenu(menu);
            if (cmd == static_cast<UINT>(kUpMenuBase))
            {
                st_.upscale = 0;
                Save();
                SetNote(ui::OutputScalingIsOff, Note::Ok);
            }
            else if (cmd > static_cast<UINT>(kUpMenuBase) && cmd <= static_cast<UINT>(kUpMenuBase + kEntries))
            {
                const Entry& e = entries[static_cast<int>(cmd) - kUpMenuBase - 1];
                st_.upscale = e.method;
                st_.upscaleLevel = e.level;
                if (st_.upscale >= 4)
                    st_.upscalerMode = 4; // Opti Upscaler: FSR 4.1.2 whenever DLSS or FSR3/4 output scaling is on
                Save();
                OnClick(kIdPictureOn); // the upscaler on the picture goes on with it
                if (st_.upscale >= 4)
                    OnClick(kIdUpscalerApply);
                ReadPictureState();
                SetNote(e.level == 3 ? ui::NativeTheGameKeepsItsSizeAnd
                                     : ui::StartMakesTheGameWindowThatSmall,
                        Note::Info);
            }
        }
    }
    else if (id == kIdLimitOff)
    {
        st_.limitAuto = 0; // otherwise START would switch it on again
        Save();
        ApplyLimiter(true);
    }
    else if (id == kIdLimitApply)
    {
        ApplyLimiter();
    }
    else if (id == kIdBoostSelect)
    {
        static const wchar_t* names[] = {
            ui::OffWeakGpus, ui::LightCpuPriorityOnly, ui::BalancedCpuGpuHigh, ui::MaxBestForStrongGpus
        };

        HMENU menu = CreatePopupMenu();
        if (menu)
        {
            for (int v = 3; v >= 0; --v)
                AppendMenuW(menu, MF_STRING | (st_.boost == v ? MF_CHECKED : 0), kBoostMenuBase + v, names[v]);

            POINT pt{};
            GetCursorPos(&pt);
            const UINT cmd = TrackPopupMenu(menu, TPM_RETURNCMD | TPM_NONOTIFY | TPM_RIGHTBUTTON, pt.x, pt.y, 0, hwnd_, nullptr);
            DestroyMenu(menu);

            if (cmd >= kBoostMenuBase && cmd <= kBoostMenuBase + 3)
            {
                st_.boost = static_cast<int>(cmd - kBoostMenuBase);
                Save();
                SetNote(L"Performance: " + std::wstring(names[st_.boost]) + (Running() ? L". Applies the next time you press START." : L"."),
                        Note::Ok);
            }
        }
    }
    else if (id == kIdUpscalerSelect)
    {
        HMENU menu = CreatePopupMenu();
        if (menu)
        {
            AppendMenuW(menu, MF_STRING | (st_.upscalerMode == 0 ? MF_CHECKED : 0),
                kUpscalerMenuBase + 0, L"Intel XeSS");
            AppendMenuW(menu, MF_STRING | (st_.upscalerMode == 1 ? MF_CHECKED : 0),
                kUpscalerMenuBase + 1, L"AMD FSR 3.1.6");
            AppendMenuW(menu, MF_STRING | (st_.upscalerMode == 2 ? MF_CHECKED : 0),
                kUpscalerMenuBase + 2, L"NVIDIA DLSS");
            AppendMenuW(menu, MF_STRING | (st_.upscalerMode == 3 ? MF_CHECKED : 0),
                kUpscalerMenuBase + 3, ui::Dlss37AmdAndGtx16);
            AppendMenuW(menu, MF_STRING | (st_.upscalerMode == 4 ? MF_CHECKED : 0),
                kUpscalerMenuBase + 4, L"AMD FSR 4.1.2");
            AppendMenuW(menu, MF_STRING | (st_.upscalerMode == 5 ? MF_CHECKED : 0),
                kUpscalerMenuBase + 5, L"DittoSR");

            POINT pt{};
            GetCursorPos(&pt);
            const UINT cmd = TrackPopupMenu(
                menu, TPM_RETURNCMD | TPM_NONOTIFY | TPM_RIGHTBUTTON,
                pt.x, pt.y, 0, hwnd_, nullptr);
            DestroyMenu(menu);

            if (cmd >= kUpscalerMenuBase && cmd <= kUpscalerMenuBase + 5)
            {
                st_.upscalerMode = static_cast<int>(cmd - kUpscalerMenuBase);
                Save();

                OnClick(kIdUpscalerApply); // selecting applies it: OptiScaler.ini is written and the folder's files are copied
            }
        }
    }
    else if (id == kIdUpscalerApply)
    {
        if (st_.upscalerMode < 0 || st_.upscalerMode > 5)
        {
            SetNote(ui::InvalidUpscalerSelection, Note::Error);
        }
        else
        {
            static const wchar_t* values[] = {
                L"xess", L"ffx", L"dlss", L"ffx" /* DLSS 3.7 for AMD / GTX 16 runs on the FSR upscaler */, L"ffx", L"ffx" /* DittoSR */
            };
            const std::wstring iniPath = oc::ExeDir() + L"OptiScaler.ini";

            if (WritePrivateProfileStringW(
                    L"Upscalers", L"Dx12Upscaler",
                    values[st_.upscalerMode], iniPath.c_str()))
            {
                // FSR 3.1.6: FSR upscaler index 2 (the comment in OptiScaler.ini about the indexes is out of date). FSR 4.1.2: automatic index, plus sharpness, CAS and the native upscaler pass.
                if (st_.upscalerMode == 3) // DLSS 3.7 (runs on the FSR upscaler): index 1
                {
                    WritePrivateProfileStringW(L"FSR", L"UpscalerIndex", L"1", iniPath.c_str());
                }
                else if (st_.upscalerMode == 1 || st_.upscalerMode == 5) // FSR 3.1.6 and DittoSR: index 2
                {
                    WritePrivateProfileStringW(L"FSR", L"UpscalerIndex", L"2", iniPath.c_str());
                }
                else if (st_.upscalerMode == 4)
                {
                    WritePrivateProfileStringW(L"FSR", L"UpscalerIndex", L"auto", iniPath.c_str());
                    WritePrivateProfileStringW(L"Sharpness", L"OverrideSharpness", L"true", iniPath.c_str());
                    WritePrivateProfileStringW(L"Sharpness", L"Sharpness", L"0.600000", iniPath.c_str());
                    WritePrivateProfileStringW(L"CAS", L"Enabled", L"true", iniPath.c_str());
                    WritePrivateProfileStringW(L"DlssNr", L"NativeUpscalerPass", L"true", iniPath.c_str());
                }

                // FSR 3.1.6, FSR 4.1.2 and DLSS 3.7: also put the ready-made DLL set of that folder into the MK-JH and / or OptiScaler folder (whichever exists).
                if (st_.upscalerMode == 1 || st_.upscalerMode == 3 || st_.upscalerMode == 4 || st_.upscalerMode == 5)
                {
                    const bool ditto = st_.upscalerMode == 5;
                    const bool fsr = st_.upscalerMode == 1 || st_.upscalerMode == 4;
                    const std::vector<std::wstring> names = ditto
                        ? std::vector<std::wstring>{ L"DittoSR", L"Ditto SR", L"DittoSR_", L"Ditto" }
                        : (fsr ? std::vector<std::wstring>{ L"FSR 4", L"FSR4", L"FSR_4", L"FSR-4" }
                               : std::vector<std::wstring>{ L"Dlss 3.7", L"Dlss3.7", L"Dlss_3.7", L"Dlss-3.7" });
                    const std::wstring root = oc::ExeDir();
                    std::wstring from;
                    for (const std::wstring& n : names)
                    {
                        for (const std::wstring& base : { root + L"MK-JH\\", root + L"OptiScaler\\", root })
                        {
                            const std::wstring cand = base + n;
                            const DWORD at = GetFileAttributesW(cand.c_str());
                            if (at != INVALID_FILE_ATTRIBUTES && (at & FILE_ATTRIBUTE_DIRECTORY))
                            {
                                from = cand;
                                break;
                            }
                        }
                        if (!from.empty())
                            break;
                    }
                    const std::wstring label = ditto ? L"DittoSR" : (fsr ? L"FSR 4" : L"Dlss 3.7");
                    if (from.empty())
                    {
                        SetNote(L"OptiScaler.ini updated, but the \"" + label + L"\" folder was not found (next to the launcher, or inside MK-JH / OptiScaler).", Note::Error);
                    }
                    else
                    {
                        // Where the files go: the MK-JH folder and / or the OptiScaler folder, whichever exist (usually one of
                        // them). When neither exists, MK-JH is made.
                        std::vector<std::wstring> targets;
                        for (const wchar_t* d : { L"MK-JH", L"OptiScaler" })
                        {
                            const DWORD at = GetFileAttributesW((root + d).c_str());
                            if (at != INVALID_FILE_ATTRIBUTES && (at & FILE_ATTRIBUTE_DIRECTORY))
                                targets.push_back(d);
                        }
                        if (targets.empty())
                        {
                            CreateDirectoryW((root + L"MK-JH").c_str(), nullptr);
                            targets.push_back(L"MK-JH");
                        }
                        int copied = 0, failed = 0;
                        std::wstring names2;
                        for (const std::wstring& t : targets)
                        {
                            CopyTree(from, root + t, copied, failed);
                            names2 += (names2.empty() ? L"" : L" and ") + t;
                        }
                        if (failed > 0)
                            SetNote(L"Copied " + std::to_wstring(copied) + L" files from " + label + L" to " + names2 + L", but " + std::to_wstring(failed) +
                                        L" could not be replaced (is the game or the host still running?).", Note::Error);
                        else
                            SetNote(L"OptiScaler.ini updated and the " + label + L" files (" + std::to_wstring(copied) +
                                        L") were copied into " + names2 + L". Restart the game to use the change.", Note::Ok);
                    }
                }
                else
                {
                    SetNote(ui::OptiscalerIniUpdatedRestartTheGameTo, Note::Ok);
                }
            }
            else
            {
                SetNote(ui::CouldNotUpdateOptiscalerIniCheckThe, Note::Error);
            }
        }
    }
    else if (id >= kIdShortcut0 && id < kIdShortcut0 + kScCount)
    {
        BeginCapture(id - kIdShortcut0);
    }
    else if (id >= kIdRow0 && id < kIdRow0 + static_cast<int>(games_.size()))
    {
        selected_ = id - kIdRow0;
        SetNote(L"");
        OfferProfile();
    }
    ReadPictureState();
    Invalidate();
}

// ---------------------------------------------------------------------------------------------------------------
// The DLSS 5 window: small, separate, docked to the right side of the main window
// ---------------------------------------------------------------------------------------------------------------

namespace
{
constexpr float kPanelW = 270.0f; // logical units, like the main window (see S / R)
constexpr float kPanelHHave = 446.0f;    // dll found: shorter
constexpr float kPanelHDepth = 845.0f;   // the Depth window
constexpr float kPanelHMissing = 556.0f; // dll missing: warning, download button and a hint more
constexpr float kPanelDrop = 90.0f; // how far below the top of the main window the DLSS 5 window starts
} // namespace

LRESULT CALLBACK App::PanelProc(HWND h, UINT m, WPARAM w, LPARAM l)
{
    App* self = reinterpret_cast<App*>(GetWindowLongPtrW(h, GWLP_USERDATA));
    if (m == WM_NCCREATE)
    {
        auto* cs = reinterpret_cast<CREATESTRUCTW*>(l);
        self = static_cast<App*>(cs->lpCreateParams);
        self->panel_ = h;
        SetWindowLongPtrW(h, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(self));
    }
    return self ? self->OnPanelMessage(m, w, l) : DefWindowProcW(h, m, w, l);
}

bool App::CreatePanel()
{
    WNDCLASSEXW wc{};
    wc.cbSize = sizeof(wc);
    wc.lpfnWndProc = PanelProc;
    wc.hInstance = inst_;
    wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    wc.lpszClassName = kPanelClass;
    if (!RegisterClassExW(&wc) && GetLastError() != ERROR_CLASS_ALREADY_EXISTS)
        return false;

    // An owned popup: it stays above the main window, and hides / shows with it (minimise, restore).
    panel_ = CreateWindowExW(WS_EX_TOOLWINDOW, kPanelClass, ui::DLSS5Title, WS_POPUP, 0, 0, static_cast<int>(S(kPanelW)),
                             static_cast<int>(S(kPanelHMissing)), hwnd_, nullptr, inst_, this);
    if (!panel_)
        return false;

    const DWORD round = 2;
    DwmSetWindowAttribute(panel_, 33, &round, sizeof(round));
    SetTimer(panel_, 1, 1500, nullptr); // looks again whether nvngx_dlssnr.dll is next to the program

    DockPanel(); // stays hidden until the "DLSS 5" button of the main window is pressed
    return true;
}

// Puts the window against the right edge of the main window (left edge when there is no room on the right).
void App::DockPanel()
{
    if (!panel_ || !hwnd_ || IsIconic(hwnd_))
        return;
    RECT r{};
    GetWindowRect(hwnd_, &r);
    const int pw = static_cast<int>(S(kPanelW));
    if (panelKind_ == 0)
        lastHave_ = DlssnrPresent();
    const int ph = static_cast<int>(S(panelKind_ == 1 ? kPanelHDepth : (lastHave_ ? kPanelHHave : kPanelHMissing)));

    MONITORINFO mi{};
    mi.cbSize = sizeof(mi);
    int x = r.right + 2;
    if (GetMonitorInfoW(MonitorFromWindow(hwnd_, MONITOR_DEFAULTTONEAREST), &mi) && x + pw > mi.rcWork.right)
        x = r.left - pw - 2;
    int y = r.top + static_cast<int>(S(kPanelDrop));
    if (GetMonitorInfoW(MonitorFromWindow(hwnd_, MONITOR_DEFAULTTONEAREST), &mi) && y + ph > mi.rcWork.bottom)
        y = (std::max)(static_cast<int>(mi.rcWork.top), static_cast<int>(mi.rcWork.bottom) - ph);
    SetWindowPos(panel_, nullptr, x, y, pw, ph, SWP_NOZORDER | SWP_NOACTIVATE);
}

int App::HitAt2(int x, int y) const
{
    for (auto it = hits2_.rbegin(); it != hits2_.rend(); ++it)
    {
        if (it->r.Contains(static_cast<float>(x), static_cast<float>(y)))
            return it->id;
    }
    return kIdNone;
}

void App::DrawDepthPanel(gp::Graphics& g)
{
    Text(g, ui::Depth, *f_.title, R(18, 14, 234, 34), col::Text());
    Text(g, ui::ForTheHostSOwnOutputScaling, *f_.sm, R(18, 50, 234, 36), col::Muted(), 0, true);
    auto sepAt = [&](float yy)
    {
        gp::Pen sep(col::Alpha(col::Line(), 150), max(1.0f, S(1)));
        g.DrawLine(&sep, S(18), S(yy), S(252), S(yy));
    };
    sepAt(92);

    auto radio = [&](float y, const wchar_t* title, const wchar_t* desc, bool on, int id)
    {
        const gp::RectF ring = R(18, y + 4, 20, 20);
        const bool hot = hover_ == id;
        FillRound(g, ring, S(10), col::Row());
        StrokeRound(g, ring, S(10), on ? col::Accent() : (hot ? col::Accent2() : col::Line()), max(1.0f, S(on ? 2.0f : 1.0f)));
        if (on)
        {
            gp::SolidBrush dot(col::Accent());
            g.FillEllipse(&dot, ring.X + S(5), ring.Y + S(5), S(10), S(10));
        }
        Text(g, title, *f_.bodyB, R(48, y, 204, 24), col::Text());
        Text(g, desc, *f_.sm, R(48, y + 26, 204, 36), col::Muted(), 0, true);
        AddHit(R(18, y - 4, 234, 68), id);
    };
    radio(104, ui::FlatDefault, ui::AsAlwaysNothingChangesWorksInEvery, st_.depthSource == 0, kIdDepthFlat);
    radio(178, L"ReShade", ui::TheGameSRealDepthBufferFound, st_.depthSource == 1, kIdDepthReshade);
    sepAt(250);

    // what the host sees
    std::wstring head, detail;
    gp::Color tone = col::Muted();
    if (st_.depthSource == 0)
    {
        head = ui::NotUsed;
        detail = ui::TheFlatDepthIsUsedAsBefore;
    }
    else if (!Running() || !liveValid_)
    {
        head = ui::WaitingForStart;
        detail = ui::ChooseOutputScalingFsr34Or;
        tone = col::Amber();
    }
    else
    {
        const std::wstring size = std::to_wstring(live_.depthW) + L"\u00D7" + std::to_wstring(live_.depthH);
        switch (live_.depthState)
        {
        case 2:
            head = ui::Receiving + size;
            detail = ui::TheGameSDepthIsInUse;
            tone = col::Green();
            break;
        case 3:
            head = ui::DifferentSize;
            detail = L"Depth " + size + L" is not the size of the captured picture.";
            tone = col::Red();
            break;
        case 4:
            head = ui::CannotOpenIt;
            detail = ui::StartTheGameAndTheHostWith;
            tone = col::Red();
            break;
        case 5:
            head = ui::NoNewPictures;
            detail = ui::TickOpticapturedepthOnInReshadeSMenu;
            tone = col::Amber();
            break;
        case 1:
            head = ui::WaitingForTheAddOn;
            detail = ui::IsReshadeWithOpticapturedepthAddon64RunningIn;
            tone = col::Amber();
            break;
        default:
            head = ui::Waiting;
            detail = ui::OutputScalingMustBeFsr34;
            tone = col::Amber();
            break;
        }
    }
    {
        const gp::RectF box = R(18, 262, 234, 70);
        FillRound(g, box, S(10), col::Row());
        StrokeRound(g, box, S(10), col::Alpha(tone, 150), max(1.0f, S(1)));
        Text(g, head, *f_.bodyB, R(30, 268, 216, 22), tone);
        Text(g, detail, *f_.sm, R(30, 292, 216, 38), col::Muted(), 0, true);
    }

    Text(g, ui::HowToUse, *f_.bodyB, R(18, 342, 234, 22), col::Text());
    {
        const gp::RectF box = R(18, 368, 234, 356);
        FillRound(g, box, S(10), col::Alpha(col::Accent(), 24));
        StrokeRound(g, box, S(10), col::Alpha(col::Accent(), 90), max(1.0f, S(1)));
        Text(g,
             L"1. Install ReShade (with Add-on support) onto your game first.\n"
             L"2. Click \"Install ReShade files\" below: it closes the game completely, copies only the required files from \"ReShade depth\" next to the game exe, and launches the game again.\n"
             L"3. Tick OptiCaptureDepth on in ReShade's menu.\n"
             L"4. Supports DX11/DX12 (GPU) and DX9/DX10/OpenGL/Vulkan (CPU). Single-player only.",
             *f_.sm, R(28, 376, 216, 340), col::Text(), 0, true);
    }
    {
        const bool pick = selected_ >= 0 && selected_ < static_cast<int>(games_.size()) && !Running();
        const bool installed = pick && IsGameReshadeInstalled(games_[selected_].fullExe);
        const gp::RectF instBtn = R(18, 734, 234, 38);
        const bool hotI = hover_ == kIdDepthInstall && pick;
        if (installed)
        {
            FillRound(g, instBtn, S(9), col::Alpha(col::Green(), hotI ? 70 : 42));
            StrokeRound(g, instBtn, S(9), hotI ? col::White() : col::Green(), max(1.0f, S(1.5f)));
            Text(g, ui::Installed, *f_.chip, instBtn, col::Green(), 1);
        }
        else if (pick)
        {
            FillRound(g, instBtn, S(9), hotI ? col::Accent2() : col::Accent());
            StrokeRound(g, instBtn, S(9), hotI ? col::Accent2() : col::Line(), max(1.0f, S(1)));
            Text(g, ui::InstallReshadeFiles, *f_.chip, instBtn, col::White(), 1);
        }
        else
        {
            FillRound(g, instBtn, S(9), col::Row());
            StrokeRound(g, instBtn, S(9), hotI ? col::Accent2() : col::Line(), max(1.0f, S(1)));
            Text(g, ui::InstallReshadeFiles, *f_.chip, instBtn, col::Muted(), 1);
        }
        if (pick)
            AddHit(instBtn, kIdDepthInstall);
    }
    {
        const gp::RectF btn = R(18, 782, 234, 38);
        const bool hot = hover_ == kIdDepthFolder;
        FillRound(g, btn, S(9), hot ? col::RowHot() : col::Row());
        StrokeRound(g, btn, S(9), hot ? col::Accent2() : col::Line(), max(1.0f, S(1)));
        Text(g, ui::OpenReshadeDepthFolder, *f_.chip, btn, col::Text(), 1);
        AddHit(btn, kIdDepthFolder);
    }
}

void App::PaintPanel(HDC dc, int W, int H)
{
    gp::Graphics g(dc);
    g.SetSmoothingMode(gp::SmoothingModeAntiAlias);
    g.SetPixelOffsetMode(gp::PixelOffsetModeHalf);
    g.SetTextRenderingHint(gp::TextRenderingHintClearTypeGridFit);

    // The shared helpers (Text, FillRound, AddHit...) work on hits_ / hover_: borrow them for this window.
    std::vector<Hit> savedHits;
    savedHits.swap(hits_);
    const int savedHover = hover_;
    hover_ = hover2_;

    {
        gp::SolidBrush bg(col::Bg());
        g.FillRectangle(&bg, 0, 0, W, H);
        const gp::RectF glow(0.0f, 0.0f, static_cast<float>(W), S(90));
        gp::LinearGradientBrush gb(glow, gp::Color(70, 108, 140, 255), gp::Color(0, 13, 15, 20), gp::LinearGradientModeVertical);
        g.FillRectangle(&gb, glow);
        gp::Pen border(col::Line(), max(1.0f, S(1)));
        g.DrawRectangle(&border, 0.0f, 0.0f, static_cast<float>(W) - 1.0f, static_cast<float>(H) - 1.0f);
    }

    // header
    if (panelKind_ == 1)
    {
        DrawDepthPanel(g);
        hits2_.swap(hits_);
        hits_.swap(savedHits);
        hover_ = savedHover;
        return;
    }

    Text(g, L"DLSS 5", *f_.title, R(18, 14, 234, 34), col::Text());
    Text(g, ui::OptionalNeedsNvngxDlssnrDll, *f_.sm, R(18, 50, 240, 18), col::Muted());
    auto sepAt = [&](float yy)
    {
        gp::Pen sep(col::Alpha(col::Line(), 150), max(1.0f, S(1)));
        g.DrawLine(&sep, S(18), S(yy), S(252), S(yy));
    };
    sepAt(80);

    // enable / disable: a switch that shows what OptiScaler.ini says ([DlssNr] Enabled = true)
    {
        const bool hot = hover_ == kIdD5Enable;
        Text(g, ui::EnableDlss5, *f_.bodyB, R(18, 96, 180, 26), col::Text());
        const gp::RectF track = R(208, 96, 44, 24);
        FillRound(g, track, S(12), dlss5On_ ? col::Accent() : col::Row());
        StrokeRound(g, track, S(12), dlss5On_ ? col::Accent2() : (hot ? col::Accent() : col::Line()), max(1.0f, S(1.5f)));
        gp::SolidBrush knob(dlss5On_ ? col::White() : col::Muted());
        const float kr = S(8);
        const float kx = dlss5On_ ? track.X + track.Width - S(13) : track.X + S(13);
        g.FillEllipse(&knob, kx - kr, track.Y + track.Height / 2 - kr, kr * 2, kr * 2);
        Text(g, dlss5On_ ? ui::OnDlssnrEnabledTrue : ui::OffDlssnrEnabledAuto, *f_.tiny, R(18, 124, 234, 14),
             dlss5On_ ? col::Green() : col::Muted());
        AddHit(R(18, 90, 234, 52), kIdD5Enable);
    }
    sepAt(150);

    // the file DLSS 5 needs, next to the program
    float y = 162.0f;
    {
        const bool have = DlssnrPresent();
        Text(g, ui::RequiredFile, *f_.bodyB, R(18, y, 234, 22), col::Text());
        y += 28.0f;
        const gp::RectF box = R(18, y, 234, 58);
        FillRound(g, box, S(10), col::Row());
        StrokeRound(g, box, S(10), have ? col::Alpha(col::Green(), 130) : col::Alpha(col::Red(), 150), max(1.0f, S(1)));
        Text(g, kDlssnrFile, *f_.sm, R(30, y + 7, 214, 18), col::Muted());
        Text(g, have ? ui::FoundNextToTheProgram : ui::NotFoundNextToTheProgram, *f_.bodyB, R(30, y + 28, 216, 22),
             have ? col::Green() : col::Red());
        y += 70.0f;
        if (!have)
        {
            Text(g, ui::Dlss5WillNotWorkWithoutIt, *f_.sm, R(18, y, 234, 18), col::Red());
            y += 26.0f;

            const gp::RectF link = R(18, y, 234, 32);
            const bool hot = hover_ == kIdD5Download;
            FillRound(g, link, S(9), hot ? col::Accent2() : col::Accent());
            StrokeRound(g, link, S(9), col::Line(), max(1.0f, S(1)));
            Text(g, ui::DownloadNvngxDlssnrDll, *f_.chip, link, col::White(), 1);
            AddHit(link, kIdD5Download);
            y += 40.0f;
            Text(g, ui::DownloadItAndPlaceItExactlyNext, *f_.sm, R(18, y, 234, 36), col::Muted(), 0, true);
            y += 44.0f;
        }
        Text(g, ui::Dlss5NeedsVersion310916, *f_.sm, R(18, y, 234, 18), col::Muted());
        y += 30.0f;
    }
    sepAt(y);

    // where the DLSS 5 settings are
    {
        Text(g, ui::Dlss5Settings, *f_.bodyB, R(18, y + 12, 234, 22), col::Text());
        const gp::RectF box = R(18, y + 40, 234, 96);
        FillRound(g, box, S(10), col::Alpha(col::Accent(), 24));
        StrokeRound(g, box, S(10), col::Alpha(col::Accent(), 90), max(1.0f, S(1)));
        Text(g, ui::InTheGameOpenTheOptiscalerMenu,
             *f_.sm, R(28, y + 48, 216, 82), col::Text(), 0, true);
    }

    hits2_.swap(hits_);
    hits_.swap(savedHits);
    hover_ = savedHover;
}

LRESULT App::OnPanelMessage(UINT m, WPARAM w, LPARAM l)
{
    switch (m)
    {
    case WM_ERASEBKGND:
        return 1;

    case WM_MOUSEACTIVATE:
        return MA_NOACTIVATE; // clicking here must not take the focus from the game / main window

    case WM_TIMER:
        // look at OptiScaler.ini and at the dll again (they may have been changed by hand)
        if (ReadFgState())
            Invalidate();
        if (panelKind_ == 0 && DlssnrPresent() != lastHave_)
            DockPanel(); // the window gets shorter / taller
        InvalidatePanel();
        return 0;

    case WM_PAINT:
    {
        PAINTSTRUCT ps;
        HDC dc = BeginPaint(panel_, &ps);
        RECT rc{};
        GetClientRect(panel_, &rc);
        const int cw = rc.right - rc.left;
        const int ch = rc.bottom - rc.top;
        if (cw > 0 && ch > 0)
        {
            HDC mem = CreateCompatibleDC(dc);
            HBITMAP bmp = CreateCompatibleBitmap(dc, cw, ch);
            HGDIOBJ old = SelectObject(mem, bmp);
            PaintPanel(mem, cw, ch);
            BitBlt(dc, 0, 0, cw, ch, mem, 0, 0, SRCCOPY);
            SelectObject(mem, old);
            DeleteObject(bmp);
            DeleteDC(mem);
        }
        EndPaint(panel_, &ps);
        return 0;
    }

    case WM_MOUSEMOVE:
    {
        if (!tracking2_)
        {
            TRACKMOUSEEVENT t{};
            t.cbSize = sizeof(t);
            t.dwFlags = TME_LEAVE;
            t.hwndTrack = panel_;
            TrackMouseEvent(&t);
            tracking2_ = true;
        }
        const int h = HitAt2(GET_X_LPARAM(l), GET_Y_LPARAM(l));
        if (h != hover2_)
        {
            hover2_ = h;
            InvalidatePanel();
        }
        return 0;
    }

    case WM_MOUSELEAVE:
        tracking2_ = false;
        if (hover2_ != kIdNone)
        {
            hover2_ = kIdNone;
            InvalidatePanel();
        }
        return 0;

    case WM_SETCURSOR:
        if (LOWORD(l) == HTCLIENT)
        {
            SetCursor(LoadCursorW(nullptr, hover2_ != kIdNone ? IDC_HAND : IDC_ARROW));
            return TRUE;
        }
        break;

    case WM_LBUTTONDOWN:
        pressed2_ = HitAt2(GET_X_LPARAM(l), GET_Y_LPARAM(l));
        SetCapture(panel_);
        return 0;

    case WM_LBUTTONUP:
    {
        ReleaseCapture();
        const int h = HitAt2(GET_X_LPARAM(l), GET_Y_LPARAM(l));
        const int was = pressed2_;
        pressed2_ = kIdNone;
        if (h != kIdNone && h == was)
            OnClick(h);
        InvalidatePanel();
        Invalidate();
        return 0;
    }

    case WM_DESTROY:
        KillTimer(panel_, 1);
        panel_ = nullptr;
        return 0;

    default:
        break;
    }
    return DefWindowProcW(panel_, m, w, l);
}

LRESULT App::OnMessage(UINT m, WPARAM w, LPARAM l)
{
    switch (m)
    {
    case WM_ERASEBKGND:
        return 1;

    case WM_PAINT:
    {
        PAINTSTRUCT ps;
        HDC dc = BeginPaint(hwnd_, &ps);
        RECT rc{};
        GetClientRect(hwnd_, &rc);
        const int cw = rc.right - rc.left;
        const int ch = rc.bottom - rc.top;
        if (cw > 0 && ch > 0)
        {
            HDC mem = CreateCompatibleDC(dc);
            HBITMAP bmp = CreateCompatibleBitmap(dc, cw, ch);
            HGDIOBJ old = SelectObject(mem, bmp);
            Paint(mem, cw, ch);
            BitBlt(dc, 0, 0, cw, ch, mem, 0, 0, SRCCOPY);
            SelectObject(mem, old);
            DeleteObject(bmp);
            DeleteDC(mem);
        }
        EndPaint(hwnd_, &ps);
        return 0;
    }

    case WM_MOUSEMOVE:
    {
        if (!tracking_)
        {
            TRACKMOUSEEVENT t{};
            t.cbSize = sizeof(t);
            t.dwFlags = TME_LEAVE;
            t.hwndTrack = hwnd_;
            TrackMouseEvent(&t);
            tracking_ = true;
        }
        if (upSliding_)
        {
            SetUpSharpFromX(GET_X_LPARAM(l), false);
            Invalidate();
            return 0;
        }
        const int h = HitAt(GET_X_LPARAM(l), GET_Y_LPARAM(l));
        if (h != hover_)
        {
            hover_ = h;
            Invalidate();
        }
        return 0;
    }

    case WM_MOUSELEAVE:
        tracking_ = false;
        if (hover_ != kIdNone)
        {
            hover_ = kIdNone;
            Invalidate();
        }
        return 0;

    case WM_SETCURSOR:
        if (LOWORD(l) == HTCLIENT)
        {
            SetCursor(LoadCursorW(nullptr, hover_ != kIdNone ? IDC_HAND : IDC_ARROW));
            return TRUE;
        }
        break;

    case WM_LBUTTONDOWN:
    {
        const int h = HitAt(GET_X_LPARAM(l), GET_Y_LPARAM(l));
        if (capturing_ >= 0 && h != kIdShortcut0 + capturing_)
        {
            capturing_ = -1;
            RegisterShowHotkey();
        }
        pressed_ = h;
        SetCapture(hwnd_);
        if (h == kIdUpSlider)
        {
            upSliding_ = true;
            SetUpSharpFromX(GET_X_LPARAM(l), false);
        }
        Invalidate();
        return 0;
    }

    case WM_LBUTTONUP:
    {
        ReleaseCapture();
        if (upSliding_)
        {
            upSliding_ = false;
            SetUpSharpFromX(GET_X_LPARAM(l), true);
            pressed_ = kIdNone;
            Invalidate();
            return 0;
        }
        const int h = HitAt(GET_X_LPARAM(l), GET_Y_LPARAM(l));
        const int was = pressed_;
        pressed_ = kIdNone;
        if (h != kIdNone && h == was)
            OnClick(h);
        Invalidate();
        return 0;
    }

    case WM_MOUSEWHEEL:
    {
        POINT p{ GET_X_LPARAM(l), GET_Y_LPARAM(l) };
        ScreenToClient(hwnd_, &p);
        const gp::RectF view = ListView();
        if (view.Contains(static_cast<float>(p.x), static_cast<float>(p.y)))
        {
            scroll_ -= static_cast<float>(GET_WHEEL_DELTA_WPARAM(w)) / 120.0f * S(48.0f);
            ClampScroll();
            Invalidate();
        }
        return 0;
    }

    case WM_KEYDOWN:
    case WM_SYSKEYDOWN:
        if (dlgKind_ >= 0)
        {
            if (m == WM_KEYDOWN && w == VK_RETURN)
                CloseDialog(true);
            else if (m == WM_KEYDOWN && w == VK_ESCAPE)
                CloseDialog(false);
            return 0;
        }
        if (capturing_ >= 0)
            return OnCaptureKey(static_cast<UINT>(w));
        if (m == WM_KEYDOWN && w == VK_F5)
        {
            Refresh();
            return 0;
        }
        break;

    case WM_SYSCHAR:
        if (capturing_ >= 0)
            return 0; // no menu beep while a shortcut is being typed
        break;

    case WM_TIMER:
        if (w == kTimerList)
        {
            if (!Running() && capturing_ < 0)
                Refresh();
        }
        else if (w == kTimerHost)
        {
            PollHost();
            KeepGameSize();
            // a warning that came up while the launcher was minimized: show it as soon as the window is visible again
            if (dlgKind_ < 0 && !warnQueue_.empty() && !IsIconic(hwnd_) && IsWindowVisible(hwnd_))
                OpenNextDialog();
        }
        else if (w == kTimerFront)
        {
            KillTimer(hwnd_, kTimerFront);
            if (Running() && IsWindow(target_))
                ForceForeground(target_);
        }
        return 0;

    case WM_HOTKEY:
        if (w == kHotkeyShow)
        {
            ShowOnTop();
            Refresh();
        }
        return 0;

    case WM_ACTIVATE:
        if (LOWORD(w) == WA_INACTIVE)
        {
            if (capturing_ >= 0)
            {
                capturing_ = -1;
                RegisterShowHotkey();
                Invalidate();
            }
            // Shown on top of the game by the shortcut: go back below the overlay once the user clicks elsewhere.
            SetWindowPos(hwnd_, HWND_NOTOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
        }
        return 0;

    case WM_MOVE:
        DockPanel();
        return 0;

    case WM_DPICHANGED:
        ApplyDpi(HIWORD(w), reinterpret_cast<const RECT*>(l));
        DockPanel();
        InvalidatePanel();
        return 0;

    case WM_CLOSE:
        Stop(true);
        SetRtssOsd(false);
        RestoreLimiter();
        DestroyWindow(hwnd_);
        return 0;

    case WM_DESTROY:
        UnregisterHotKey(hwnd_, kHotkeyShow);
        KillTimer(hwnd_, kTimerList);
        KillTimer(hwnd_, kTimerHost);
        PostQuitMessage(0);
        return 0;

    default:
        break;
    }
    return DefWindowProcW(hwnd_, m, w, l);
}

// ---------------------------------------------------------------------------------------------------------------
// Painting
// ---------------------------------------------------------------------------------------------------------------

void App::Text(gp::Graphics& g, const std::wstring& t, const gp::Font& f, const gp::RectF& r, const gp::Color& c, int align,
               bool wrap)
{
    gp::StringFormat fmt;
    fmt.SetAlignment(align == 0 ? gp::StringAlignmentNear : (align == 1 ? gp::StringAlignmentCenter : gp::StringAlignmentFar));
    fmt.SetLineAlignment(wrap ? gp::StringAlignmentNear : gp::StringAlignmentCenter);
    fmt.SetTrimming(gp::StringTrimmingEllipsisCharacter);
    if (!wrap)
        fmt.SetFormatFlags(gp::StringFormatFlagsNoWrap);

    gp::SolidBrush b(c);
    g.DrawString(t.c_str(), -1, &f, r, &fmt, &b);
}

void App::Card(gp::Graphics& g, const gp::RectF& r)
{
    FillRound(g, r, S(14), col::Card());
    StrokeRound(g, r, S(14), col::Line(), max(1.0f, S(1)));
}

void App::Chip(gp::Graphics& g, const gp::RectF& r, const std::wstring& text, int id, bool capturing)
{
    const bool hot = hover_ == id;
    if (capturing)
    {
        FillRound(g, r, S(8), col::Alpha(col::Accent(), 48));
        StrokeRound(g, r, S(8), col::Accent(), max(1.5f, S(1.5f)));
        Text(g, ui::PressKeys, *f_.chip, r, col::Accent(), 1);
    }
    else
    {
        FillRound(g, r, S(8), hot ? col::RowHot() : col::Row());
        StrokeRound(g, r, S(8), hot ? col::Accent() : col::Line(), max(1.0f, S(1)));
        Text(g, text, *f_.chip, r, col::Text(), 1);
    }
    AddHit(r, id);
}

void App::Button(gp::Graphics& g, const gp::RectF& r, const std::wstring& text, bool disabled, bool danger)
{
    const bool hot = hover_ == kIdStart;
    const bool down = pressed_ == kIdStart && hot;

    if (disabled)
    {
        FillRound(g, r, S(12), col::Row());
        StrokeRound(g, r, S(12), col::Line(), max(1.0f, S(1)));
        Text(g, text, *f_.btn, r, col::Muted(), 1);
    }
    else
    {
        if (danger)
            GradRound(g, r, S(12), col::Red(), gp::Color(255, 255, 140, 92));
        else
            GradRound(g, r, S(12), col::Accent(), col::Accent2());

        if (hot)
            FillRound(g, r, S(12), gp::Color(down ? 40 : 26, down ? 0 : 255, down ? 0 : 255, down ? 0 : 255));
        Text(g, text, *f_.btn, r, col::White(), 1);
    }
    AddHit(r, kIdStart);
}

void App::Paint(HDC dc, int W, int H)
{
    gp::Graphics g(dc);
    g.SetSmoothingMode(gp::SmoothingModeAntiAlias);
    g.SetPixelOffsetMode(gp::PixelOffsetModeHalf);
    g.SetTextRenderingHint(gp::TextRenderingHintClearTypeGridFit);

    hits_.clear();
    const bool running = Running();
    const bool locked = running; // the list is read-only while capturing

    {
        gp::SolidBrush bg(col::Bg());
        g.FillRectangle(&bg, 0, 0, W, H);

        // A soft glow behind the header.
        const gp::RectF glow(0.0f, 0.0f, static_cast<float>(W), S(130));
        gp::LinearGradientBrush gb(glow, gp::Color(70, 108, 140, 255), gp::Color(0, 13, 15, 20), gp::LinearGradientModeVertical);
        g.FillRectangle(&gb, glow);
    }

    // ---- header ----
    if (HICON logo = reinterpret_cast<HICON>(LoadImageW(GetModuleHandleW(nullptr), MAKEINTRESOURCEW(1), IMAGE_ICON, 64, 64, LR_DEFAULTCOLOR))) { HDC dc = g.GetHDC(); DrawIconEx(dc, static_cast<int>(S(24)), static_cast<int>(S(22)), logo, static_cast<int>(S(46)), static_cast<int>(S(46)), 0, nullptr, DI_NORMAL); g.ReleaseHDC(dc); DestroyIcon(logo); }
    Text(g, L"OptiCaptureHost", *f_.title, R(84, 18, 420, 32), col::Text());
    {
        // version badge, right after the title
        const gp::RectF badge = R(288, 25, 58, 20);
        FillRound(g, badge, S(10), col::Alpha(col::Accent(), 40));
        StrokeRound(g, badge, S(10), col::Alpha(col::Accent(), 110), max(1.0f, S(1)));
        Text(g, std::wstring(L"v") + kVersion, *f_.tiny, badge, col::Accent(), 1);
    }
    Text(g, ui::CaptureAnyGameFsr4AndXess, *f_.sm, R(84, 50, 620, 20),
         col::Muted());

    {
        // "DLSS 5" button: opens / closes the small window docked to the right side of this one
        const bool open5 = panelOpen_ && panelKind_ == 0;
        const gp::RectF b = R(792, 30, 120, 30);
        const bool hot = hover_ == kIdD5Toggle;
        FillRound(g, b, S(15), open5 ? col::Alpha(col::Accent(), 90) : (hot ? col::RowHot() : col::Row()));
        StrokeRound(g, b, S(15), open5 ? col::Accent() : (hot ? col::Accent2() : col::Line()), max(1.0f, S(open5 ? 2 : 1)));
        Text(g, open5 ? L"DLSS 5  \u2039" : L"DLSS 5  \u203A", *f_.bodyB, b, col::Text(), 1);
        AddHit(b, kIdD5Toggle);
    }

    {
        // "Depth" button: opens / closes the window with the depth source (flat / ReShade)
        const bool openD = panelOpen_ && panelKind_ == 1;
        const gp::RectF b = R(684, 30, 100, 30);
        const bool hot = hover_ == kIdDepthToggle;
        FillRound(g, b, S(15), openD ? col::Alpha(col::Accent(), 90) : (hot ? col::RowHot() : col::Row()));
        StrokeRound(g, b, S(15), openD ? col::Accent() : (hot ? col::Accent2() : col::Line()), max(1.0f, S(openD ? 2 : 1)));
        Text(g, openD ? ui::Depth2 : ui::Depth3, *f_.bodyB, b, col::Text(), 1);
        AddHit(b, kIdDepthToggle);
    }

    {
        const gp::RectF pill = R(560, 30, 116, 30);
        const gp::Color tone = stopping_ ? col::Amber() : (running ? col::Green() : col::Muted());
        FillRound(g, pill, S(15), col::Alpha(tone, 36));
        StrokeRound(g, pill, S(15), col::Alpha(tone, 120), max(1.0f, S(1)));
        gp::SolidBrush dot(tone);
        g.FillEllipse(&dot, pill.X + S(14), pill.Y + S(10), S(10), S(10));
        Text(g, stopping_ ? ui::StoppingCapture : (running ? ui::Capturing : ui::CaptureReady), *f_.bodyB,
             gp::RectF(pill.X + S(32), pill.Y, S(82), pill.Height), tone);
    }

    // ---- quick modes ----
    {
        static const wchar_t* const names[] = { ui::NoFgMod, ui::X2FgNewGpu, ui::X3FgNewGpu, ui::X4FgNewGpu,
                                                ui::X2FgOldGpu, ui::X3FgOldGpu, ui::X4FgOldGpu };
        for (int k = 0; k < 7; ++k)
        {
            const gp::RectF chip = R(24.0f + k * 127.0f, 72, 122, 26);
            const bool active = PresetActive(k);
            const bool hot = hover_ == kIdPreset0 + k && !locked;
            FillRound(g, chip, S(8), active ? col::Alpha(col::Accent(), 70) : (hot ? col::RowHot() : col::Row()));
            StrokeRound(g, chip, S(8), active ? col::Accent() : (hot ? col::Accent2() : col::Line()), max(1.0f, S(active ? 2 : 1)));
            Text(g, names[k], *f_.chip, chip, locked ? col::Muted() : col::Text(), 1);
            if (!locked)
                AddHit(chip, kIdPreset0 + k);
        }
    }

    // ---- games card ----
    const gp::RectF left = R(24, 100, 500, 766);
    Card(g, left);
    Text(g, ui::ChooseAGame, *f_.h2, R(44, 112, 300, 24), col::Text());
    Text(g, ui::PickTheWindowOfTheGameYou, *f_.sm, R(44, 136, 340, 18), col::Muted());

    {
        // Force Borderless on / off for the selected game
        const bool pick = selected_ >= 0 && selected_ < static_cast<int>(games_.size());
        const bool on = pick && BorderIndex(games_[selected_].hwnd) >= 0;
        const gp::RectF bb = R(310, 108, 94, 26);
        const bool hotB = hover_ == kIdBorderless && pick;
        FillRound(g, bb, S(8), on ? col::Alpha(col::Green(), 40) : (hotB ? col::RowHot() : col::Row()));
        StrokeRound(g, bb, S(8), on ? col::Green() : (hotB ? col::Accent() : col::Line()), max(1.0f, S(1)));
        Text(g, on ? ui::BorderlessOn : ui::BorderlessOff, *f_.chip, bb, pick ? (on ? col::Green() : (st_.upscale != 0 ? col::Muted() : col::Text())) : col::Muted(), 1);
        if (pick)
            AddHit(bb, kIdBorderless);
    }

    {
        const gp::RectF btn = R(414, 114, 90, 30);
        const bool hot = hover_ == kIdRefresh && !locked;
        FillRound(g, btn, S(8), hot ? col::RowHot() : col::Row());
        StrokeRound(g, btn, S(8), hot ? col::Accent() : col::Line(), max(1.0f, S(1)));
        Text(g, ui::Refresh, *f_.chip, btn, locked ? col::Muted() : col::Text(), 1);
        if (!locked)
            AddHit(btn, kIdRefresh);
    }

    {
        const gp::RectF view = ListView();
        const float pitch = S(62);
        const float rowH = S(56);
        const float rowW = S(462);

        g.SetClip(view);
        float y = view.Y - scroll_;
        for (size_t i = 0; i < games_.size(); ++i, y += pitch)
        {
            if (y + rowH < view.Y || y > view.Y + view.Height)
                continue;

            const GameItem& it = games_[i];
            const int id = kIdRow0 + static_cast<int>(i);
            const bool sel = static_cast<int>(i) == selected_;
            const bool hot = hover_ == id && !locked;
            const gp::RectF row(view.X, y, rowW, rowH);

            FillRound(g, row, S(10), sel ? col::RowSel() : (hot ? col::RowHot() : col::Row()));
            if (sel)
                StrokeRound(g, row, S(10), col::Accent(), max(1.5f, S(1.5f)));

            const float ic = S(30);
            const float ix = row.X + S(14);
            const float iy = row.Y + (rowH - ic) / 2.0f;
            if (iy >= view.Y && iy + ic <= view.Y + view.Height)
            {
                HDC hdc = g.GetHDC();
                DrawIconEx(hdc, static_cast<int>(ix), static_cast<int>(iy), it.icon ? it.icon : LoadIconW(nullptr, IDI_APPLICATION),
                           static_cast<int>(ic), static_cast<int>(ic), 0, nullptr, DI_NORMAL);
                g.ReleaseHDC(hdc);
            }

            Text(g, it.title, *f_.bodyB, gp::RectF(row.X + S(56), row.Y + S(8), rowW - S(56) - S(34), S(22)), col::Text());

            std::wstring sub = it.exe.empty() ? ui::ProtectedProcess : it.exe;
            sub += L"  \u00B7  " + std::to_wstring(it.w) + L"\u00D7" + std::to_wstring(it.h);
            if (it.fills)
                sub += ui::Borderless;
            if (it.minimized)
                sub += ui::Minimized;
            Text(g, sub, *f_.tiny, gp::RectF(row.X + S(56), row.Y + S(30), rowW - S(56) - S(34), S(18)), col::Muted());

            if (sel)
            {
                gp::SolidBrush dot(col::Accent());
                g.FillEllipse(&dot, row.X + rowW - S(26), row.Y + (rowH - S(10)) / 2.0f, S(10), S(10));
            }

            if (!locked)
            {
                const float top = max(row.Y, view.Y);
                const float bot = min(row.Y + row.Height, view.Y + view.Height);
                if (bot > top)
                    AddHit(gp::RectF(row.X, top, row.Width, bot - top), id);
            }
        }

        if (games_.empty())
            Text(g, ui::NoWindowsFoundStartTheGameThen, *f_.body, view, col::Muted(), 1);

        // thin scrollbar
        const float content = static_cast<float>(games_.size()) * pitch - S(6);
        if (content > view.Height)
        {
            const float track = view.Height;
            const float thumb = max(S(30), track * view.Height / content);
            const float maxScroll = content - view.Height;
            const float ty = view.Y + (track - thumb) * (maxScroll > 0 ? scroll_ / maxScroll : 0.0f);
            FillRound(g, gp::RectF(view.X + view.Width - S(5), ty, S(4), thumb), S(2), col::Alpha(col::Muted(), 120));
        }
        g.ResetClip();

        if (locked)
            FillRound(g, gp::RectF(view.X - S(2), view.Y - S(2), view.Width + S(4), view.Height + S(4)), S(10),
                      col::Alpha(col::Card(), 150));
    }

    // While a capture runs the game list is read-only anyway: show the live status in its place.
    if (running)
        PaintLive(g, ListView());

    // ---- performance selector (under the game list) ----
    {
        static const wchar_t* boostNames[] = { ui::OffWeakGpus, ui::LightCpuPriority, ui::BalancedCpuGpuHigh, ui::MaxStrongGpus };

        Text(g, ui::Performance, *f_.bodyB, R(44, 612, 130, 24), col::Text());

        const gp::RectF selector = R(176, 608, 328, 32);
        const bool hot = hover_ == kIdBoostSelect;
        FillRound(g, selector, S(8), hot ? col::RowHot() : col::Row());
        StrokeRound(g, selector, S(8), hot ? col::Accent() : col::Line(), max(1.0f, S(1)));
        Text(g, boostNames[(st_.boost >= 0 && st_.boost <= 3) ? st_.boost : 3], *f_.chip, R(186, 614, 308, 22), col::Text());
        AddHit(selector, kIdBoostSelect);
    }

    // ---- present mode selector ----
    {
        static const wchar_t* presentNames[] = { ui::Auto, ui::AlwaysNoWaitInterval0, ui::AlwaysWaitForVblankInterval1, ui::ClassicLike220, ui::LosslessStyleLowLatency, ui::ClassicHalfRefresh, ui::ClassicUnlimited };

        Text(g, ui::PresentMode, *f_.bodyB, R(44, 654, 130, 24), col::Text());

        const gp::RectF selector = R(176, 650, 328, 32);
        const bool hot = hover_ == kIdPresentSelect;
        FillRound(g, selector, S(8), hot ? col::RowHot() : col::Row());
        StrokeRound(g, selector, S(8), hot ? col::Accent() : col::Line(), max(1.0f, S(1)));
        Text(g, presentNames[(st_.presentMode >= 0 && st_.presentMode <= 6) ? st_.presentMode : 0], *f_.chip, R(186, 656, 308, 22), col::Text());
        AddHit(selector, kIdPresentSelect);
    }

    // ---- present delay selector ----
    {
        Text(g, ui::PresentDelay, *f_.bodyB, R(44, 696, 130, 24), col::Text());

        const gp::RectF selector = R(176, 692, 328, 32);
        const bool hot = hover_ == kIdDelaySelect;
        FillRound(g, selector, S(8), hot ? col::RowHot() : col::Row());
        StrokeRound(g, selector, S(8), hot ? col::Accent() : col::Line(), max(1.0f, S(1)));
        std::wstring t = std::to_wstring(st_.presentDelay) + L" ms";
        if (st_.presentDelay == 0) t += ui::Off2;
        Text(g, t, *f_.chip, R(186, 698, 308, 22), col::Text());
        AddHit(selector, kIdDelaySelect);
    }

    // ---- output scaling ----
    {
        static const wchar_t* lvN[] = { ui::Quality, ui::Balanced, ui::Performance, ui::Native };
        Text(g, ui::OutputScaling, *f_.bodyB, R(44, 738, 130, 24), col::Text());

        const gp::RectF selector = R(176, 734, 232, 32);
        const bool hot = hover_ == kIdUpSelect;
        FillRound(g, selector, S(8), hot ? col::RowHot() : col::Row());
        StrokeRound(g, selector, S(8), hot ? col::Accent() : col::Line(), max(1.0f, S(1)));
        std::wstring t = ui::Off;
        if (st_.upscale != 0)
            t = std::wstring(st_.upscale == 2 ? ui::Fsr1 : (st_.upscale == 3 ? ui::Nis : (st_.upscale == 4 ? ui::Fsr34 : (st_.upscale == 5 ? ui::Dlss : ui::Bilinear)))) +
                lvN[(st_.upscaleLevel >= 0 && st_.upscaleLevel <= 3) ? st_.upscaleLevel : 1];
        Text(g, t, *f_.chip, R(186, 740, 212, 22), col::Text());
        AddHit(selector, kIdUpSelect);

        const bool pick = selected_ >= 0 && selected_ < static_cast<int>(games_.size());
        const gp::RectF rb = R(416, 734, 88, 32);
        const bool hotR = hover_ == kIdUpResize && pick;
        FillRound(g, rb, S(8), hotR ? col::RowHot() : col::Row());
        StrokeRound(g, rb, S(8), hotR ? col::Accent() : col::Line(), max(1.0f, S(1)));
        Text(g, ui::Resize, *f_.chip, rb, pick ? col::Text() : col::Muted(), 1);
        if (pick)
            AddHit(rb, kIdUpResize);
    }

    // ---- sharpness slider (FSR 1: RCAS, NIS: sharpness); bilinear has none ----
    {
        const bool has = st_.upscale >= 2;
        Text(g, ui::Sharpness, *f_.bodyB, R(44, 800, 130, 24), has ? col::Text() : col::Muted());
        const gp::RectF track = R(176, 808, 268, 6);
        upSliderRect_ = track;
        FillRound(g, track, S(3), col::Row());
        if (has)
        {
            const float f = static_cast<float>(UpSharpValue()) / 100.0f;
            FillRound(g, gp::RectF(track.X, track.Y, track.Width * f, track.Height), S(3), col::Accent());
            const float kx = track.X + track.Width * f;
            const float kr = S(8);
            gp::SolidBrush knob(hover_ == kIdUpSlider || upSliding_ ? col::Accent2() : col::Accent());
            g.FillEllipse(&knob, kx - kr, track.Y + track.Height / 2 - kr, kr * 2, kr * 2);
            AddHit(R(168, 800, 284, 22), kIdUpSlider);
            Text(g, std::to_wstring(UpSharpValue()) + L"%", *f_.chip, R(456, 800, 48, 22), col::Text(), 2);
        }
        else
        {
            Text(g, st_.upscale == 1 ? ui::NA : ui::Off3, *f_.chip, R(456, 800, 48, 22), col::Muted(), 2);
        }
    }

    // ---- game profiles: 4 slots per game, Save, Load, automatic apply ----
    {
        const bool pick = selected_ >= 0 && selected_ < static_cast<int>(games_.size());
        for (int i = 1; i <= 4; ++i)
        {
            const gp::RectF sl = R(44.0f + (i - 1) * 40.0f, 770, 34, 28);
            const bool cur = profileSlot_ == i;
            const bool has = pick && SlotExists(games_[selected_].exe, i);
            const bool hot = hover_ == kIdProfSlot0 + i - 1 && pick;
            FillRound(g, sl, S(8), cur ? col::Accent() : (hot ? col::RowHot() : col::Row()));
            StrokeRound(g, sl, S(8), has ? col::Green() : (hot ? col::Accent() : col::Line()), max(1.0f, S(has ? 2 : 1)));
            Text(g, std::to_wstring(i), *f_.chip, sl, cur ? col::White() : (pick ? col::Text() : col::Muted()), 1);
            if (pick)
                AddHit(sl, kIdProfSlot0 + i - 1);
        }

        const gp::RectF sb = R(210, 770, 62, 28);
        const bool hotS = hover_ == kIdProfileSave && pick;
        FillRound(g, sb, S(8), hotS ? col::Accent2() : col::Accent());
        StrokeRound(g, sb, S(8), col::Line(), max(1.0f, S(1)));
        Text(g, ui::Save, *f_.chip, sb, pick ? col::White() : col::Muted(), 1);
        if (pick)
            AddHit(sb, kIdProfileSave);

        const gp::RectF lb = R(278, 770, 62, 28);
        const bool hotL = hover_ == kIdProfileLoad && pick;
        FillRound(g, lb, S(8), hotL ? col::RowHot() : col::Row());
        StrokeRound(g, lb, S(8), hotL ? col::Accent() : col::Line(), max(1.0f, S(1)));
        Text(g, ui::Load, *f_.chip, lb, pick ? col::Text() : col::Muted(), 1);
        if (pick)
            AddHit(lb, kIdProfileLoad);

        const gp::RectF box = R(354, 775, 18, 18);
        const bool hotA = hover_ == kIdProfileAuto;
        FillRound(g, box, S(5), st_.profileAuto ? col::Accent() : col::Row());
        StrokeRound(g, box, S(5), hotA ? col::Accent2() : col::Line(), max(1.0f, S(1)));
        if (st_.profileAuto)
            Text(g, L"\u2713", *f_.chip, box, col::White(), 1);
        Text(g, ui::AutoApply, *f_.sm, R(378, 773, 130, 22), col::Text());
        AddHit(R(352, 770, 156, 28), kIdProfileAuto);
    }

    // ---- VSync card ----
    {
        const gp::RectF c = R(540, 100, 372, 92);
        Card(g, c);
        if (hover_ == kIdVsync)
            StrokeRound(g, c, S(14), col::Alpha(col::Accent(), 150), max(1.0f, S(1)));

        Text(g, ui::VsyncLabel, *f_.h2, R(560, 114, 200, 24), col::Text());
        Text(g, ui::VsyncHelp, *f_.sm,
             R(560, 140, 200, 40), col::Muted(), 0, true);

        static const wchar_t* vsNames[] = { ui::Auto, ui::On0, ui::On1, ui::Off };
        const gp::RectF chip = R(790, 131, 104, 30);
        const bool hotChip = hover_ == kIdVsync;
        FillRound(g, chip, S(8), hotChip ? col::RowHot() : col::Row());
        StrokeRound(g, chip, S(8), hotChip ? col::Accent() : col::Line(), max(1.0f, S(1)));
        Text(g, vsNames[vsyncMode_ < 0 ? 0 : (vsyncMode_ == 2 ? 1 : (vsyncMode_ == 1 ? 2 : 3))], *f_.chip, R(802, 136, 84, 22), col::Text());
        AddHit(c, kIdVsync);
    }

    // ---- frame limiter card (RivaTuner, async) ----
    {
        static const wchar_t* targetNames[] = { ui::Off, ui::Game, ui::Host, ui::Global };
        static const wchar_t* modeShort[] = { ui::Async2, ui::FrontEdgeSync2, ui::BackEdgeSync2 };
        const gp::RectF c = R(540, 204, 372, 164);
        Card(g, c);
        Text(g, ui::FrameLimiterRivatuner, *f_.h2, R(560, 214, 340, 24), col::Text());

        const gp::RectF t = R(560, 246, 150, 30);
        const bool hotT = hover_ == kIdLimitTarget;
        FillRound(g, t, S(8), hotT ? col::RowHot() : col::Row());
        StrokeRound(g, t, S(8), hotT ? col::Accent() : col::Line(), max(1.0f, S(1)));
        Text(g, std::wstring(ui::Lock) + targetNames[(st_.limitTarget >= 0 && st_.limitTarget <= 3) ? st_.limitTarget : 0], *f_.chip,
             R(570, 252, 134, 22), col::Text());
        AddHit(t, kIdLimitTarget);

        const gp::RectF mo = R(720, 246, 172, 30);
        const bool hotM = hover_ == kIdLimitMode;
        FillRound(g, mo, S(8), hotM ? col::RowHot() : col::Row());
        StrokeRound(g, mo, S(8), hotM ? col::Accent() : col::Line(), max(1.0f, S(1)));
        Text(g, modeShort[(st_.limitMode >= 0 && st_.limitMode <= 2) ? st_.limitMode : 1], *f_.chip, R(730, 252, 154, 22), col::Text());
        AddHit(mo, kIdLimitMode);

        std::wstring fpsText;
        if (st_.limitFps == 0) fpsText = ui::RefreshRateAuto;
        else if (st_.limitFps == -2) fpsText = ui::Text12RefreshAuto;
        else if (st_.limitFps == -3) fpsText = ui::Text13RefreshAuto;
        else if (st_.limitFps == -4) fpsText = ui::Text14RefreshAuto;
        else fpsText = std::to_wstring(st_.limitFps) + L" fps";
        const gp::RectF f = R(560, 284, 230, 30);
        const bool hotF = hover_ == kIdLimitFps;
        FillRound(g, f, S(8), hotF ? col::RowHot() : col::Row());
        StrokeRound(g, f, S(8), hotF ? col::Accent() : col::Line(), max(1.0f, S(1)));
        Text(g, fpsText, *f_.chip, R(568, 290, 214, 22), col::Text());
        AddHit(f, kIdLimitFps);

        const gp::RectF apply = R(800, 284, 48, 30);
        const bool hotA = hover_ == kIdLimitApply;
        FillRound(g, apply, S(8), hotA ? col::Accent2() : col::Accent());
        StrokeRound(g, apply, S(8), col::Line(), max(1.0f, S(1)));
        Text(g, ui::Apply, *f_.chip, apply, col::White(), 1);
        AddHit(apply, kIdLimitApply);

        const gp::RectF offb = R(852, 284, 40, 30);
        const bool hotO = hover_ == kIdLimitOff;
        FillRound(g, offb, S(8), hotO ? col::RowHot() : col::Row());
        StrokeRound(g, offb, S(8), hotO ? col::Accent() : col::Line(), max(1.0f, S(1)));
        Text(g, ui::Off, *f_.chip, offb, col::Text(), 1);
        AddHit(offb, kIdLimitOff);

        {
            const gp::RectF box = R(560, 324, 18, 18);
            const bool hot = hover_ == kIdLimitAuto;
            FillRound(g, box, S(5), st_.limitAuto ? col::Accent() : col::Row());
            StrokeRound(g, box, S(5), hot ? col::Accent2() : col::Line(), max(1.0f, S(1)));
            if (st_.limitAuto)
                Text(g, L"\u2713", *f_.chip, box, col::White(), 1);
            Text(g, ui::ApplyOnStart, *f_.body, R(586, 322, 120, 22), col::Text());
            AddHit(R(560, 320, 150, 26), kIdLimitAuto);
        }
        {
            // OptiScaler's own limit (OptiScaler.ini [Framerate] FramerateLimit) = the monitor's refresh rate, written at every START
            const gp::RectF box = R(722, 324, 18, 18);
            const bool hot = hover_ == kIdOptiFps;
            FillRound(g, box, S(5), st_.optiFps ? col::Accent() : col::Row());
            StrokeRound(g, box, S(5), hot ? col::Accent2() : col::Line(), max(1.0f, S(1)));
            if (st_.optiFps)
                Text(g, L"\u2713", *f_.chip, box, col::White(), 1);
            Text(g, ui::OptiscalerFpsHz, *f_.body, R(748, 322, 150, 22), col::Text());
            AddHit(R(722, 320, 172, 26), kIdOptiFps);
        }
        Text(g, limitInfo_.empty() ? std::wstring(ui::NeedsRivatunerStatisticsServerAsyncFrontBack) : limitInfo_,
             *f_.tiny, R(560, 348, 332, 14), col::Muted());
    }

    // ---- shortcuts card ----
    {
        const gp::RectF c = R(540, 380, 372, 480);
        Card(g, c);
        Text(g, ui::Shortcuts, *f_.h2, R(560, 576, 200, 24), col::Text());

        const gp::RectF reset = R(812, 576, 80, 24);
        Text(g, ui::ResetAll, hover_ == kIdReset ? *f_.link : *f_.linkN, reset, hover_ == kIdReset ? col::Accent2() : col::Muted(), 2);
        AddHit(reset, kIdReset);

        // brings back the warnings that were hidden with "Don't show this warning again"
        const gp::RectF warnReset = R(640, 576, 170, 24);
        Text(g, ui::ShowWarningsAgain, hover_ == kIdWarnReset ? *f_.link : *f_.linkN, warnReset,
             hover_ == kIdWarnReset ? col::Accent2() : col::Muted(), 2);
        AddHit(warnReset, kIdWarnReset);

        // ---- Frame Generation controls ----
        {


            Text(g, ui::FrameGeneration2, *f_.bodyB,
                R(560, 386, 230, 24), col::Text());

            // multi frame generation: 2x / 3x / 4x (Intel MFG, Artur, Combo, Nvidia MFG)
            {
                const bool mfgOk = MfgMode(st_.frameGenMode) && !locked;
                static const wchar_t* const mfgNames[] = { L"2x", L"3x", L"4x", L"5x", L"6x" };
                for (int i = 0; i < 5; ++i)
                {
                    const gp::RectF c = R(712.0f + i * 36.0f, 388, 32, 20);
                    const bool act = MfgMode(st_.frameGenMode) && st_.mfgCount == i + 1;
                    const bool hot = hover_ == kIdMfg0 + i && mfgOk;
                    FillRound(g, c, S(7), act ? col::Accent() : (hot ? col::RowHot() : col::Row()));
                    StrokeRound(g, c, S(7), act || hot ? col::Accent() : col::Line(), max(1.0f, S(1)));
                    Text(g, mfgNames[i], *f_.tiny, c, act ? col::White() : (mfgOk ? col::Text() : col::Muted()), 1);
                    if (!locked)
                        AddHit(c, kIdMfg0 + i);
                }
            }

            const gp::RectF selector = R(560, 412, 332, 30);
            const bool selectorHot = hover_ == kIdFrameGenSelect && !locked;
            FillRound(g, selector, S(8), selectorHot ? col::RowHot() : col::Row());
            StrokeRound(g, selector, S(8),
                selectorHot ? col::Accent() : col::Line(), max(1.0f, S(1)));
            Text(g, kFgNames[(st_.frameGenMode >= 0 && st_.frameGenMode <= 10)
                ? st_.frameGenMode : 0], *f_.chip,
                R(568, 418, 316, 22), locked ? col::Muted() : col::Text());
            if (!locked)
                AddHit(selector, kIdFrameGenSelect);

            // HUD FIX / ASYNC: tick boxes on the line between Frame Generation and Opti Upscaler; they write (and read)
            // OptiScaler.ini: true when ticked, auto when not
            auto tick = [&](float x, float w, const wchar_t* label, bool on, int id)
            {
                const gp::RectF box = R(x, 451, 18, 18);
                const bool hot = hover_ == id && !locked;
                FillRound(g, box, S(5), on ? col::Accent() : col::Row());
                StrokeRound(g, box, S(5), hot ? col::Accent2() : col::Line(), max(1.0f, S(1)));
                if (on)
                    Text(g, L"\u2713", *f_.chip, box, col::White(), 1);
                Text(g, label, *f_.body, R(x + 26, 449, w - 26, 22), locked ? col::Muted() : col::Text());
                if (!locked)
                    AddHit(R(x - 4, 447, w + 4, 26), id);
            };
            tick(714, 92, ui::HudFix, hudFix_, kIdHudFix);
            tick(812, 80, ui::Async3, asyncFg_, kIdAsyncFg);
        }

        // ---- Upscaler controls ----
        {
            static const wchar_t* upscalerNames[] = {
                L"Intel XeSS",
                L"AMD FSR 3.1.6",
                L"NVIDIA DLSS",
                ui::Dlss37AmdGtx16,
                L"AMD FSR 4.1.2",
                L"DittoSR"
            };

            Text(g, ui::OptiUpscaler, *f_.bodyB,
                R(560, 448, 230, 24), col::Text());

            const gp::RectF selector = R(560, 472, 332, 30);
            const bool selectorHot = hover_ == kIdUpscalerSelect && !locked;
            FillRound(g, selector, S(8), selectorHot ? col::RowHot() : col::Row());
            StrokeRound(g, selector, S(8),
                selectorHot ? col::Accent() : col::Line(), max(1.0f, S(1)));
            Text(g, upscalerNames[(st_.upscalerMode >= 0 && st_.upscalerMode <= 5)
                ? st_.upscalerMode : 0], *f_.chip,
                R(568, 478, 316, 22), locked ? col::Muted() : col::Text());
            if (!locked)
                AddHit(selector, kIdUpscalerSelect);

        }

        // ---- "Run the upscaler on the picture" (sharpening + CAS + native upscaler pass) ----
        {
            Text(g, ui::RunTheUpscalerOnThePicture, *f_.bodyB, R(560, 508, 330, 24), col::Text());

            const gp::RectF onB = R(560, 534, 110, 30);
            const bool hotOn = hover_ == kIdPictureOn && !locked;
            FillRound(g, onB, S(8), hotOn ? col::Accent2() : col::Accent());
            StrokeRound(g, onB, S(8), col::Line(), max(1.0f, S(1)));
            Text(g, ui::Enable, *f_.chip, onB, col::White(), 1);
            if (!locked)
                AddHit(onB, kIdPictureOn);

            const gp::RectF offB = R(678, 534, 110, 30);
            const bool hotOff = hover_ == kIdPictureOff && !locked;
            FillRound(g, offB, S(8), hotOff ? col::RowHot() : col::Row());
            StrokeRound(g, offB, S(8), hotOff ? col::Accent() : col::Line(), max(1.0f, S(1)));
            Text(g, ui::Off, *f_.chip, offB, col::Text(), 1);
            if (!locked)
                AddHit(offB, kIdPictureOff);

            Text(g, pictureOn_ ? ui::On : ui::Off, *f_.bodyB, R(800, 538, 92, 22), pictureOn_ ? col::Green() : col::Muted(), 2);
        }

        float y = 606.0f;
        for (int i = 0; i < kScCount; ++i, y += 36.0f)
        {
            if (i > 0)
            {
                gp::Pen sep(col::Alpha(col::Line(), 150), max(1.0f, S(1)));
                g.DrawLine(&sep, S(560), S(y - 1), S(892), S(y - 1));
            }

            Text(g, kShortcutDefs[i].label, *f_.body, R(560, y + 2, 150, 18), col::Text());
            Text(g, kShortcutDefs[i].hint, *f_.tiny, R(560, y + 20, 176, 14), col::Muted());

            if (i == kScFgKey)
            {
                // ON / OFF badge: this key switches frame generation on and off
                const gp::RectF badge = R(680, y + 4, 52, 18);
                FillRound(g, badge, S(9), col::Alpha(col::Green(), 36));
                StrokeRound(g, badge, S(9), col::Alpha(col::Green(), 130), max(1.0f, S(1)));
                Text(g, ui::OnOff, *f_.tiny, badge, col::Green(), 1);
            }

            Chip(g, R(742, y + 5, 150, 26), ChipText(i), kIdShortcut0 + i, capturing_ == i);
        }

        // Two more shortcuts that always work while the capture host runs (fixed, not changeable here).
        {
            gp::Pen sep(col::Alpha(col::Line(), 150), max(1.0f, S(1)));
            g.DrawLine(&sep, S(560), S(y + 2), S(892), S(y + 2));
            Text(g, L"Ctrl+Shift+F10", *f_.tiny, R(560, y + 6, 100, 14), col::Accent());
            Text(g, ui::FocusOverlayOpenOptiscalerMenu, *f_.tiny, R(668, y + 6, 224, 14), col::Muted());
            Text(g, L"Ctrl+Shift+F11", *f_.tiny, R(560, y + 23, 100, 14), col::Accent());
            Text(g, ui::MouseKeyboardGameOverlay, *f_.tiny, R(668, y + 23, 224, 14), col::Muted());
        }
    }

    // ---- footer ----
    {
        gp::Pen line(col::Line(), max(1.0f, S(1)));
        g.DrawLine(&line, S(24), S(870), S(912), S(870));

        // Links panel: a framed card with four chips (icon, small label, value).
        {
            const gp::RectF panel = R(24, 904, 626, 42);
            Card(g, panel);
            struct LinkChip { const wchar_t* label; const wchar_t* value; float x; float w; int id; bool heart; };
            static const LinkChip chips[] = {
                { ui::Developer, L"MK-JH", 32, 128, kIdLinkJH, false },
                { ui::TelegramChannel, L"MK-JH_APPS", 168, 168, kIdLinkApps, false },
                { ui::Archive, L"MK-JH_ARCHIVE", 344, 168, kIdLinkArchive, false },
                { ui::SourceCode, L"GitHub", 520, 122, kIdLinkGithub, true },
            };
            for (const LinkChip& c : chips)
            {
                const gp::RectF chip = R(c.x, 908, c.w, 34);
                const bool hot = hover_ == c.id;
                const gp::Color tone = col::Accent();
                FillRound(g, chip, S(9), hot ? col::Alpha(tone, 46) : col::Row());
                StrokeRound(g, chip, S(9), hot ? tone : col::Line(), max(1.0f, S(1)));
                if (c.heart)
                    DrawGithubMark(g, R(c.x + 8, 914, 22, 22), hot ? col::Accent2() : col::Text());
                else
                    DrawPlane(g, R(c.x + 8, 914, 22, 22));
                Text(g, c.label, *f_.tiny, R(c.x + 36, 910, c.w - 40, 14), col::Muted());
                Text(g, c.value, hot ? *f_.link : *f_.linkN, R(c.x + 36, 922, c.w - 40, 18), hot ? col::Accent2() : tone);
                AddHit(chip, c.id);
            }
        }

        // status line
        std::wstring status;
        gp::Color tone = col::Muted();
        if (!note_.empty())
        {
            status = note_;
            tone = noteKind_ == Note::Error ? col::Red() : (noteKind_ == Note::Ok ? col::Green() : col::Muted());
        }
        else if (running)
        {
            status = std::format(ui::CapturingStatus, targetTitle_, KeyName(st_.menuKey), HotkeyText(st_.launcher));
        }
        else if (selected_ >= 0 && selected_ < static_cast<int>(games_.size()))
        {
            status = std::format(ui::CaptureReadyTitle, games_[selected_].title);
        }
        else
        {
            status = ui::PickAGameFromTheListTo;
        }
        Text(g, status, *f_.sm, R(24, 876, 640, 28), tone, 0, true);

        const bool noPick = !running && (selected_ < 0 || selected_ >= static_cast<int>(games_.size()));
        Button(g, R(672, 900, 240, 46), running ? (stopping_ ? ui::StoppingCapture2 : ui::StopCapture2) : ui::StartCapture, noPick, running);
    }

    // ---- warning dialog (on top of everything) ----
    if (dlgKind_ >= 0)
        PaintDialog(g, W, H);
}

} // namespace

int WINAPI wWinMain(HINSTANCE inst, HINSTANCE, PWSTR, int)
{
    SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);

    // Helper mode: the launcher started itself elevated only to write the RTSS profile.
    {
        int argc = 0;
        wchar_t** argv = CommandLineToArgvW(GetCommandLineW(), &argc);
        if (argv && argc >= 11 && wcscmp(argv[1], L"--borderless") == 0)
        {
            BorderState b;
            const bool on = wcscmp(argv[2], L"on") == 0;
            b.wnd = reinterpret_cast<HWND>(static_cast<ULONG_PTR>(_wcstoui64(argv[3], nullptr, 10)));
            b.style = static_cast<LONG_PTR>(_wcstoi64(argv[4], nullptr, 10));
            b.exStyle = static_cast<LONG_PTR>(_wcstoi64(argv[5], nullptr, 10));
            b.rect.left = _wtoi(argv[6]);
            b.rect.top = _wtoi(argv[7]);
            b.rect.right = _wtoi(argv[8]);
            b.rect.bottom = _wtoi(argv[9]);
            b.wasMaximized = _wtoi(argv[10]) != 0;
            LocalFree(argv);
            if (!IsWindow(b.wnd))
                return 1;
            return (on ? BorderApplyOn(b.wnd, b) : BorderApplyOff(b.wnd, b)) ? 0 : 1;
        }
        if (argv && argc >= 2 && wcscmp(argv[1], L"--rtss-restore") == 0)
        {
            LocalFree(argv);
            RtssApi api;
            return DoLimitRestore(api);
        }
        if (argv && argc >= 4 && wcscmp(argv[1], L"--rtss-osd") == 0)
        {
            const std::wstring exe = argv[2];
            const bool hide = wcscmp(argv[3], L"hide") == 0;
            LocalFree(argv);
            RtssApi api;
            return DoOsd(api, exe, hide);
        }
        if (argv && argc >= 4 && wcscmp(argv[1], L"--rtss-apply") == 0)
        {
            const std::wstring exe = wcscmp(argv[2], L"-") == 0 ? std::wstring() : std::wstring(argv[2]);
            const int hz = _wtoi(argv[3]);
            const bool offArg = argc >= 5 && wcscmp(argv[4], L"off") == 0;
            LocalFree(argv);

            oc::Settings st;
            oc::LoadSettings(st);
            RtssApi api;
            const LimiterOut r = DoLimiter(api, st, exe, hz > 1 ? hz : 60, offArg);
            WritePrivateProfileStringW(L"Rtss", L"LastInfo", r.info.c_str(), oc::SettingsPath().c_str());
            WritePrivateProfileStringW(L"Rtss", L"LastNote", r.note.c_str(), oc::SettingsPath().c_str());
            return r.code;
        }
        if (argv)
            LocalFree(argv);
    }

    // One launcher at a time: a second start brings the first one forward.
    HANDLE once = CreateMutexW(nullptr, TRUE, L"Local\\OptiCaptureLauncher.SingleInstance");
    if (once && GetLastError() == ERROR_ALREADY_EXISTS)
    {
        if (HWND other = FindWindowW(kClassName, nullptr))
        {
            if (IsIconic(other))
                ShowWindow(other, SW_RESTORE);
            SetForegroundWindow(other);
        }
        CloseHandle(once);
        return 0;
    }

    gp::GdiplusStartupInput input;
    ULONG_PTR token = 0;
    if (gp::GdiplusStartup(&token, &input, nullptr) != gp::Ok)
    {
        MessageBoxW(nullptr, ui::CouldNotStartGdi, L"OptiCaptureHost", MB_OK | MB_ICONERROR);
        return 1;
    }

    int code = 0;
    {
        App app;
        if (app.Create(inst))
            code = app.Loop();
        else
            MessageBoxW(nullptr, ui::CouldNotCreateTheWindow, L"OptiCaptureHost", MB_OK | MB_ICONERROR);
    }

    gp::GdiplusShutdown(token);
    if (once)
        CloseHandle(once);
    return code;
}
