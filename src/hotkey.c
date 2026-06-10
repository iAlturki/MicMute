// hotkey.c - global hotkey registration plus a real "press a combination"
// capture window (dark rounded popup, live preview of held modifiers).
#include "micmute.h"

#define S g_appState.settings

static HWND g_captureWnd;
static BOOL g_finishing;

static void HotkeyParams(DWORD hk, UINT* mods, UINT* vk)
{
    if (hk == HOTKEY_CTRL_M)      { *mods = MOD_CONTROL; *vk = 'M'; }
    else if (hk == HOTKEY_CUSTOM) { *mods = S.customHotkeyModifiers; *vk = S.customHotkeyVK; }
    else                          { *mods = 0; *vk = hk; }
}

void RegisterCurrentHotkey(void)
{
    UINT mods, vk;
    HotkeyParams(S.currentHotkey, &mods, &vk);
    if (!RegisterHotKey(g_appState.hWnd, HOTKEY_ID, mods | MOD_NOREPEAT, vk)) {
        wchar_t hk[64], msg[160];
        FormatHotkeyName(hk, ARRAYSIZE(hk), S.currentHotkey, S.customHotkeyModifiers, S.customHotkeyVK);
        wsprintfW(msg, L"Could not register %s - it may be in use by another app.", hk);
        TrayBalloon(APP_NAME, msg);
    }
}

void UnregisterCurrentHotkey(void)
{
    UnregisterHotKey(g_appState.hWnd, HOTKEY_ID);
}

void ChangeHotkey(DWORD newHotkey)
{
    UnregisterCurrentHotkey();
    S.currentHotkey = newHotkey;
    RegisterCurrentHotkey();
    SaveSettings();
    TrayUpdate();   // tooltip shows the hotkey
}

static void AppendKeyName(wchar_t* buf, size_t cch, UINT vk)
{
    UINT sc = MapVirtualKeyW(vk, MAPVK_VK_TO_VSC);
    switch (vk) {   // extended keys need the extended bit for a correct name
        case VK_LEFT: case VK_RIGHT: case VK_UP: case VK_DOWN:
        case VK_INSERT: case VK_DELETE: case VK_HOME: case VK_END:
        case VK_PRIOR: case VK_NEXT: case VK_DIVIDE: case VK_NUMLOCK:
            sc |= 0x100;
            break;
    }
    wchar_t name[32];
    if (GetKeyNameTextW((LONG)(sc << 16), name, ARRAYSIZE(name)))
        wcscat_s(buf, cch, name);
    else {
        wchar_t fallback[16];
        wsprintfW(fallback, L"0x%02X", vk);
        wcscat_s(buf, cch, fallback);
    }
}

void FormatHotkeyName(wchar_t* buf, size_t cch, DWORD hotkey, UINT mods, UINT vk)
{
    buf[0] = 0;
    UINT m = 0, v = hotkey;
    if (hotkey == HOTKEY_CTRL_M)      { m = MOD_CONTROL; v = 'M'; }
    else if (hotkey == HOTKEY_CUSTOM) { m = mods; v = vk; }
    if (m & MOD_CONTROL) wcscat_s(buf, cch, L"Ctrl+");
    if (m & MOD_ALT)     wcscat_s(buf, cch, L"Alt+");
    if (m & MOD_SHIFT)   wcscat_s(buf, cch, L"Shift+");
    if (m & MOD_WIN)     wcscat_s(buf, cch, L"Win+");
    AppendKeyName(buf, cch, v);
}

// ---- capture window ----

static UINT HeldModifiers(void)
{
    UINT m = 0;
    if (GetKeyState(VK_CONTROL) & 0x8000) m |= MOD_CONTROL;
    if (GetKeyState(VK_MENU)    & 0x8000) m |= MOD_ALT;
    if (GetKeyState(VK_SHIFT)   & 0x8000) m |= MOD_SHIFT;
    if ((GetKeyState(VK_LWIN) | GetKeyState(VK_RWIN)) & 0x8000) m |= MOD_WIN;
    return m;
}

static BOOL IsModifierVk(UINT vk)
{
    return vk == VK_CONTROL || vk == VK_LCONTROL || vk == VK_RCONTROL
        || vk == VK_MENU    || vk == VK_LMENU    || vk == VK_RMENU
        || vk == VK_SHIFT   || vk == VK_LSHIFT   || vk == VK_RSHIFT
        || vk == VK_LWIN    || vk == VK_RWIN;
}

static void FinishCapture(HWND hwnd, BOOL accepted, UINT mods, UINT vk)
{
    if (g_finishing) return;
    g_finishing = TRUE;
    DestroyWindow(hwnd);
    g_captureWnd = NULL;
    if (accepted) {
        S.customHotkeyModifiers = mods;
        S.customHotkeyVK = vk;
        ChangeHotkey(HOTKEY_CUSTOM);
        wchar_t hk[64], msg[128];
        FormatHotkeyName(hk, ARRAYSIZE(hk), HOTKEY_CUSTOM, mods, vk);
        wsprintfW(msg, L"Hotkey set to %s.", hk);
        TrayBalloon(APP_NAME, msg);
    } else {
        RegisterCurrentHotkey();   // was released for the capture
    }
}

static void PaintCapture(HWND hwnd)
{
    PAINTSTRUCT ps;
    HDC dc = BeginPaint(hwnd, &ps);
    RECT rc;
    GetClientRect(hwnd, &rc);

    HBRUSH bg = CreateSolidBrush(RGB(22, 24, 32));
    FillRect(dc, &rc, bg);
    DeleteObject(bg);
    SetBkMode(dc, TRANSPARENT);

    int dpi = GetDpiForWindow(hwnd);
    if (!dpi) dpi = 96;

    wchar_t combo[64] = L"";
    UINT held = HeldModifiers();
    if (held & MOD_CONTROL) wcscat_s(combo, 64, L"Ctrl + ");
    if (held & MOD_ALT)     wcscat_s(combo, 64, L"Alt + ");
    if (held & MOD_SHIFT)   wcscat_s(combo, 64, L"Shift + ");
    if (held & MOD_WIN)     wcscat_s(combo, 64, L"Win + ");
    if (!combo[0]) wcscpy_s(combo, 64, L"...");

    HFONT title = CreateFontW(-MulDiv(11, dpi, 72), 0, 0, 0, FW_SEMIBOLD, 0, 0, 0,
                              DEFAULT_CHARSET, 0, 0, CLEARTYPE_QUALITY, 0, L"Segoe UI");
    HFONT big   = CreateFontW(-MulDiv(19, dpi, 72), 0, 0, 0, FW_BOLD, 0, 0, 0,
                              DEFAULT_CHARSET, 0, 0, CLEARTYPE_QUALITY, 0, L"Segoe UI");
    HFONT hint  = CreateFontW(-MulDiv(9, dpi, 72), 0, 0, 0, FW_NORMAL, 0, 0, 0,
                              DEFAULT_CHARSET, 0, 0, CLEARTYPE_QUALITY, 0, L"Segoe UI");

    RECT r = rc;
    r.top += MulDiv(16, dpi, 96);
    HFONT old = SelectObject(dc, title);
    SetTextColor(dc, RGB(240, 242, 247));
    DrawTextW(dc, L"Set Custom Hotkey", -1, &r, DT_CENTER | DT_TOP | DT_SINGLELINE);

    SelectObject(dc, big);
    SetTextColor(dc, RGB(124, 196, 255));
    DrawTextW(dc, combo, -1, &rc, DT_CENTER | DT_VCENTER | DT_SINGLELINE);

    r = rc;
    r.bottom -= MulDiv(14, dpi, 96);
    SelectObject(dc, hint);
    SetTextColor(dc, RGB(140, 145, 160));
    DrawTextW(dc, L"Hold modifiers and press a key  -  Esc cancels  -  F-keys work alone", -1,
              &r, DT_CENTER | DT_BOTTOM | DT_SINGLELINE);

    SelectObject(dc, old);
    DeleteObject(title);
    DeleteObject(big);
    DeleteObject(hint);
    EndPaint(hwnd, &ps);
}

static LRESULT CALLBACK CaptureProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    switch (uMsg) {
        case WM_KEYDOWN:
        case WM_SYSKEYDOWN: {
            UINT vk = (UINT)wParam;
            if (vk == VK_ESCAPE) { FinishCapture(hwnd, FALSE, 0, 0); return 0; }
            if (IsModifierVk(vk)) { InvalidateRect(hwnd, NULL, TRUE); return 0; }
            UINT mods = HeldModifiers();
            BOOL bareOk = vk >= VK_F1 && vk <= VK_F24;   // F-keys may stand alone
            if (mods || bareOk) FinishCapture(hwnd, TRUE, mods, vk);
            return 0;
        }
        case WM_KEYUP:
        case WM_SYSKEYUP:
            InvalidateRect(hwnd, NULL, TRUE);
            return 0;
        case WM_KILLFOCUS:
            FinishCapture(hwnd, FALSE, 0, 0);
            return 0;
        case WM_PAINT:
            PaintCapture(hwnd);
            return 0;
    }
    return DefWindowProcW(hwnd, uMsg, wParam, lParam);
}

void BeginHotkeyCapture(void)
{
    if (g_captureWnd) {
        SetForegroundWindow(g_captureWnd);
        return;
    }
    static BOOL registered;
    if (!registered) {
        WNDCLASSW wc = {0};
        wc.lpfnWndProc = CaptureProc;
        wc.hInstance = GetModuleHandleW(NULL);
        wc.lpszClassName = L"MicMuteCapture";
        wc.hCursor = LoadCursor(NULL, IDC_ARROW);
        RegisterClassW(&wc);
        registered = TRUE;
    }

    UnregisterCurrentHotkey();   // so the current hotkey can be re-picked
    g_finishing = FALSE;

    UINT dpi = GetDpiForSystem();
    int w = MulDiv(400, dpi, 96), h = MulDiv(150, dpi, 96);
    RECT wa;
    SystemParametersInfoW(SPI_GETWORKAREA, 0, &wa, 0);
    int x = wa.left + (wa.right - wa.left - w) / 2;
    int y = wa.top + (wa.bottom - wa.top - h) / 2;

    g_captureWnd = CreateWindowExW(WS_EX_TOPMOST | WS_EX_TOOLWINDOW, L"MicMuteCapture",
                                   L"", WS_POPUP, x, y, w, h,
                                   NULL, NULL, GetModuleHandleW(NULL), NULL);
    if (!g_captureWnd) {
        RegisterCurrentHotkey();
        return;
    }
    int r = MulDiv(14, dpi, 96);
    SetWindowRgn(g_captureWnd, CreateRoundRectRgn(0, 0, w + 1, h + 1, r, r), TRUE);
    ShowWindow(g_captureWnd, SW_SHOW);
    SetForegroundWindow(g_captureWnd);
    SetFocus(g_captureWnd);
}
