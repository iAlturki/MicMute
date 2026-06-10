// overlay.c - on-screen mute badge. One layered window per monitor (true
// per-pixel alpha via UpdateLayeredWindow), per-monitor DPI scaling, and a
// small animation engine. Muted timeline: fade/slide in -> badge plate holds
// 3s then melts away leaving the bare glyph -> settles to 70% opacity at the
// 6s mark. Unmute: green confirmation flash that fades away. The timer
// interval always matches the phase, so nothing busy-ticks.
#include "micmute.h"
#include <shellscalingapi.h>

#define MAX_MON 16
#define BG_HOLD_MS   3000   // muted this long -> background melts away
#define BG_MELT_MS   600
#define DIM_WAIT_MS  2400   // melt end -> 6s total, then settle to 70%
#define DIM_FADE_MS  400
#define DIM_ALPHA    179    // 70% of 255

typedef struct {
    HWND hwnd;
    RECT work;          // monitor work area
    UINT dpi;
    BOOL primary;
    BOOL shown;
    HDC memDC;
    HBITMAP dib;
    int badgePx, winPx, slidePx;
    POINT basePos;      // resting top-left of the window
} Overlay;

static Overlay g_ov[MAX_MON];
static int  g_ovCount;
static UINT_PTR g_timer;
static enum { AN_NONE, AN_FADE_IN, AN_WAIT_BG, AN_BG_OUT, AN_WAIT_DIM, AN_DIM, AN_HOLD, AN_FADE_OUT } g_phase;
static DWORD g_t0;
static BOOL g_visible, g_isFlash;

static BOOL Active(const Overlay* o)
{
    return g_appState.settings.multiMonitorMode || o->primary;
}

// Slide direction follows the badge's screen row: top slides down into
// place, bottom slides up, center just fades.
static int SlideDir(void)
{
    int row = g_appState.settings.overlayPosition / 3;
    return row == 0 ? -1 : (row == 2 ? 1 : 0);
}

static BOOL CALLBACK MonEnum(HMONITOR mon, HDC dc, LPRECT rc, LPARAM lp)
{
    if (g_ovCount >= MAX_MON) return FALSE;
    MONITORINFO mi = {sizeof(mi)};
    if (!GetMonitorInfoW(mon, &mi)) return TRUE;
    Overlay* o = &g_ov[g_ovCount++];
    ZeroMemory(o, sizeof(*o));
    o->work = mi.rcWork;
    o->primary = (mi.dwFlags & MONITORINFOF_PRIMARY) != 0;
    UINT dx = 96, dy = 96;
    o->dpi = SUCCEEDED(GetDpiForMonitor(mon, MDT_EFFECTIVE_DPI, &dx, &dy)) ? dx : 96;
    return TRUE;
}

static void EnsureMonitors(void)
{
    if (g_ovCount == 0)
        EnumDisplayMonitors(NULL, NULL, MonEnum, 0);
}

static void Layout(Overlay* o)
{
    int badge = MulDiv(g_appState.settings.overlaySize, o->dpi, 96);
    int margin = max(8, badge * 3 / 8);   // room for the soft shadow
    int em = MulDiv(24, o->dpi, 96);
    o->badgePx = badge;
    o->winPx = badge + margin * 2;
    o->slidePx = max(6, MulDiv(14, o->dpi, 96));

    int col = g_appState.settings.overlayPosition % 3;
    int row = g_appState.settings.overlayPosition / 3;
    int w = o->work.right - o->work.left, h = o->work.bottom - o->work.top;
    int bx = col == 0 ? o->work.left + em
           : col == 1 ? o->work.left + (w - badge) / 2
           :            o->work.right - em - badge;
    int by = row == 0 ? o->work.top + em
           : row == 1 ? o->work.top + (h - badge) / 2
           :            o->work.bottom - em - badge;
    o->basePos.x = bx - margin;
    o->basePos.y = by - margin;
}

static LRESULT CALLBACK OvProc(HWND h, UINT m, WPARAM w, LPARAM l)
{
    if (m == WM_DPICHANGED) {
        // Secondary-monitor scale changes only notify windows on that monitor.
        // Posted (not direct) because the rebuild destroys this very window.
        PostMessageW(g_appState.hWnd, WM_DPICHANGED, 0, 0);
        return 0;
    }
    return DefWindowProcW(h, m, w, l);
}

static void EnsureWindow(Overlay* o)
{
    static BOOL registered;
    if (!registered) {
        WNDCLASSW wc = {0};
        wc.lpfnWndProc = OvProc;
        wc.hInstance = GetModuleHandleW(NULL);
        wc.lpszClassName = L"MicMuteOverlay";
        RegisterClassW(&wc);
        registered = TRUE;
    }
    if (!o->hwnd)
        o->hwnd = CreateWindowExW(
            WS_EX_LAYERED | WS_EX_TOPMOST | WS_EX_TOOLWINDOW | WS_EX_TRANSPARENT | WS_EX_NOACTIVATE,
            L"MicMuteOverlay", L"", WS_POPUP, 0, 0, o->winPx, o->winPx,
            NULL, NULL, GetModuleHandleW(NULL), NULL);
}

static void SetBitmap(Overlay* o, BOOL muted, float bg)
{
    HBITMAP dib = RenderBadge(o->winPx, o->badgePx, muted, bg);
    if (!dib) return;
    if (!o->memDC) o->memDC = CreateCompatibleDC(NULL);
    SelectObject(o->memDC, dib);
    if (o->dib) DeleteObject(o->dib);
    o->dib = dib;
}

static void SetBitmapAll(BOOL muted, float bg)
{
    for (int i = 0; i < g_ovCount; i++)
        if (Active(&g_ov[i]) && g_ov[i].hwnd)
            SetBitmap(&g_ov[i], muted, bg);
}

static void Present(Overlay* o, BYTE alpha, int yOff)
{
    if (!o->hwnd || !o->memDC) return;
    POINT dst = {o->basePos.x, o->basePos.y + yOff};
    POINT src = {0, 0};
    SIZE  sz  = {o->winPx, o->winPx};
    BLENDFUNCTION bf = {AC_SRC_OVER, 0, alpha, AC_SRC_ALPHA};
    UpdateLayeredWindow(o->hwnd, NULL, &dst, &sz, o->memDC, &src, 0, &bf, ULW_ALPHA);
    if (!o->shown) {
        // Show and re-assert top of the topmost band in one call, so the
        // badge surfaces above fullscreen windows already on screen.
        SetWindowPos(o->hwnd, HWND_TOPMOST, 0, 0, 0, 0,
                     SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE | SWP_SHOWWINDOW);
        o->shown = TRUE;
    }
}

// Fullscreen apps that activate later insert above us in the topmost band;
// lift the badge back to the top whenever the foreground window changes.
// Event-driven (WinEvent hook), so nothing ticks while muted-steady.
static HWINEVENTHOOK g_fgHook;

static void TopmostAll(void)
{
    for (int i = 0; i < g_ovCount; i++)
        if (g_ov[i].shown)
            SetWindowPos(g_ov[i].hwnd, HWND_TOPMOST, 0, 0, 0, 0,
                         SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
}

static void CALLBACK FgChanged(HWINEVENTHOOK h, DWORD ev, HWND hwnd,
                               LONG idObject, LONG idChild, DWORD tid, DWORD time)
{
    if (g_visible) TopmostAll();
}

static void HookForeground(BOOL on)
{
    if (on && !g_fgHook)
        g_fgHook = SetWinEventHook(EVENT_SYSTEM_FOREGROUND, EVENT_SYSTEM_FOREGROUND,
                                   NULL, FgChanged, 0, 0, WINEVENT_OUTOFCONTEXT);
    else if (!on && g_fgHook) {
        UnhookWinEvent(g_fgHook);
        g_fgHook = NULL;
    }
}

static void PresentAll(BYTE alpha, float off01)
{
    int dir = SlideDir();
    for (int i = 0; i < g_ovCount; i++)
        if (Active(&g_ov[i]))
            Present(&g_ov[i], alpha, (int)(off01 * dir * g_ov[i].slidePx));
}

static void HideAll(void)
{
    for (int i = 0; i < g_ovCount; i++)
        if (g_ov[i].shown) {
            ShowWindow(g_ov[i].hwnd, SW_HIDE);
            g_ov[i].shown = FALSE;
        }
    g_visible = FALSE;
    g_isFlash = FALSE;
    HookForeground(FALSE);
}

static void StopAnim(void)
{
    if (g_timer) { KillTimer(NULL, g_timer); g_timer = 0; }
    g_phase = AN_NONE;
}

static void CALLBACK AnimTick(HWND hwnd, UINT msg, UINT_PTR id, DWORD now);

static void StartAnim(int phase, UINT interval)
{
    g_phase = phase;
    g_t0 = GetTickCount();
    if (g_timer) KillTimer(NULL, g_timer);
    g_timer = SetTimer(NULL, 0, interval, AnimTick);
}

static void CALLBACK AnimTick(HWND hwnd, UINT msg, UINT_PTR id, DWORD now)
{
    float t;
    switch (g_phase) {
        case AN_FADE_IN:
            t = (now - g_t0) / 180.0f;
            if (t >= 1.0f) { PresentAll(255, 0); StartAnim(AN_WAIT_BG, BG_HOLD_MS); }
            else {
                float e = 1 - (1 - t) * (1 - t) * (1 - t);   // ease-out cubic
                PresentAll((BYTE)(255 * e), 1.0f - e);
            }
            break;
        case AN_WAIT_BG:         // single long tick, then melt the plate away
            StartAnim(AN_BG_OUT, 25);
            break;
        case AN_BG_OUT:          // re-render per frame with shrinking bg alpha
            t = (now - g_t0) / (float)BG_MELT_MS;
            if (t >= 1.0f) {
                SetBitmapAll(TRUE, 0);
                PresentAll(255, 0);
                StartAnim(AN_WAIT_DIM, DIM_WAIT_MS);
            } else {
                float e = t * t * (3 - 2 * t);               // smoothstep
                SetBitmapAll(TRUE, 1.0f - e);
                PresentAll(255, 0);
            }
            break;
        case AN_WAIT_DIM:        // single long tick, then ease down to 70%
            StartAnim(AN_DIM, 15);
            break;
        case AN_DIM:
            t = (now - g_t0) / (float)DIM_FADE_MS;
            if (t >= 1.0f) { PresentAll(DIM_ALPHA, 0); StopAnim(); }
            else {
                float e = 1 - (1 - t) * (1 - t);             // ease-out
                PresentAll((BYTE)(255 - (255 - DIM_ALPHA) * e), 0);
            }
            break;
        case AN_HOLD:            // flash hold elapsed (single long tick)
            StartAnim(AN_FADE_OUT, 15);
            break;
        case AN_FADE_OUT:
            t = (now - g_t0) / 420.0f;
            if (t >= 1.0f) { HideAll(); StopAnim(); }
            else {
                float e = (1 - t) * (1 - t);                 // ease-in
                PresentAll((BYTE)(255 * e), 1.0f - e);
            }
            break;
        default:
            StopAnim();
    }
}

void OverlayShowMuted(BOOL animate)
{
    EnsureMonitors();
    StopAnim();
    for (int i = 0; i < g_ovCount; i++) {
        Overlay* o = &g_ov[i];
        if (!Active(o)) { if (o->shown) { ShowWindow(o->hwnd, SW_HIDE); o->shown = FALSE; } continue; }
        Layout(o);
        EnsureWindow(o);
        SetBitmap(o, TRUE, 1.0f);
    }
    g_visible = TRUE;
    g_isFlash = FALSE;
    HookForeground(TRUE);
    if (animate) { PresentAll(0, 1.0f); StartAnim(AN_FADE_IN, 15); }
    else         { PresentAll(255, 0);  StartAnim(AN_WAIT_BG, BG_HOLD_MS); }
}

// Called on unmute: morph the visible badge into a green "live" flash that
// holds briefly and fades out. If nothing is visible there is nothing to do.
void OverlayHide(void)
{
    if (!g_visible || g_isFlash) return;
    StopAnim();
    SetBitmapAll(FALSE, 1.0f);
    g_isFlash = TRUE;
    PresentAll(255, 0);
    StartAnim(AN_HOLD, 320);
}

static void DestroyOverlay(Overlay* o)
{
    if (o->memDC) { DeleteDC(o->memDC); o->memDC = NULL; }
    if (o->dib)   { DeleteObject(o->dib); o->dib = NULL; }
    if (o->hwnd)  { DestroyWindow(o->hwnd); o->hwnd = NULL; }
    o->shown = FALSE;
}

void OverlayRebuild(void)
{
    StopAnim();
    for (int i = 0; i < g_ovCount; i++) DestroyOverlay(&g_ov[i]);
    g_ovCount = 0;
    g_visible = FALSE;
    g_isFlash = FALSE;
    HookForeground(FALSE);
    if (g_appState.isMuted) OverlayShowMuted(FALSE);
}

void OverlayCleanup(void)
{
    StopAnim();
    HookForeground(FALSE);
    for (int i = 0; i < g_ovCount; i++) DestroyOverlay(&g_ov[i]);
    g_ovCount = 0;
}
