// tray.c - notification icon and menu. Icons are rendered at runtime (red
// badge when muted, theme-aware glyph with green dot when live), the icon
// survives Explorer restarts via TaskbarCreated, and menus are built from
// tables instead of repeated AppendMenu blocks.
#include "micmute.h"

static NOTIFYICONDATAW g_nid;
static HICON g_iconMuted, g_iconLive;
static UINT  g_taskbarCreated;

UINT TrayTaskbarCreatedMsg(void)
{
    return g_taskbarCreated;
}

static BOOL IsLightTaskbar(void)
{
    DWORD v = 0, size = sizeof(v);
    return RegGetValueW(HKEY_CURRENT_USER,
                        L"Software\\Microsoft\\Windows\\CurrentVersion\\Themes\\Personalize",
                        L"SystemUsesLightTheme", RRF_RT_REG_DWORD, NULL, &v, &size) == ERROR_SUCCESS
           && v != 0;
}

static void BuildIcons(void)
{
    int px = max(16, GetSystemMetrics(SM_CXSMICON));
    BOOL light = IsLightTaskbar();
    if (g_iconMuted) DestroyIcon(g_iconMuted);
    if (g_iconLive)  DestroyIcon(g_iconLive);
    g_iconMuted = RenderTrayIcon(px, TRUE, light);
    g_iconLive  = RenderTrayIcon(px, FALSE, light);
}

BOOL TrayInit(HWND hwnd)
{
    if (!g_taskbarCreated)
        g_taskbarCreated = RegisterWindowMessageW(L"TaskbarCreated");

    // The process runs elevated; Explorer (medium IL) must be allowed
    // through UIPI or tray clicks and TaskbarCreated never arrive.
    ChangeWindowMessageFilterEx(hwnd, WM_APP_TRAY, MSGFLT_ALLOW, NULL);
    ChangeWindowMessageFilterEx(hwnd, g_taskbarCreated, MSGFLT_ALLOW, NULL);

    if (!g_iconMuted) BuildIcons();

    ZeroMemory(&g_nid, sizeof(g_nid));
    g_nid.cbSize = sizeof(g_nid);
    g_nid.hWnd = hwnd;
    g_nid.uID = 1;
    g_nid.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP;
    g_nid.uCallbackMessage = WM_APP_TRAY;
    g_nid.hIcon = g_appState.isMuted ? g_iconMuted : g_iconLive;
    wcscpy_s(g_nid.szTip, ARRAYSIZE(g_nid.szTip), APP_NAME);

    // At logon the shell can be busy: NIM_ADD times out but may still have
    // landed. Documented handling is retry, probing with NIM_MODIFY.
    for (int i = 0; i < 8; i++) {
        if (Shell_NotifyIconW(NIM_ADD, &g_nid)) return TRUE;
        if (GetLastError() != ERROR_TIMEOUT) return FALSE;
        if (Shell_NotifyIconW(NIM_MODIFY, &g_nid)) return TRUE;
        Sleep(500);
    }
    return FALSE;
}

void TrayUpdate(void)
{
    wchar_t hk[64];
    FormatHotkeyName(hk, ARRAYSIZE(hk), g_appState.settings.currentHotkey,
                     g_appState.settings.customHotkeyModifiers, g_appState.settings.customHotkeyVK);
    g_nid.uFlags = NIF_ICON | NIF_TIP;
    g_nid.hIcon = g_appState.isMuted ? g_iconMuted : g_iconLive;
    wsprintfW(g_nid.szTip, L"%s - %s (%s)%s", APP_NAME,
              g_appState.isMuted ? L"Muted" : L"Live", hk,
              g_appState.settings.volumeLockEnabled ? L" * volume locked" : L"");
    Shell_NotifyIconW(NIM_MODIFY, &g_nid);
}

void TrayRefreshTheme(void)
{
    BuildIcons();
    TrayUpdate();
}

void TrayBalloon(const wchar_t* title, const wchar_t* text)
{
    g_nid.uFlags = NIF_INFO;
    g_nid.dwInfoFlags = NIIF_INFO;
    wcscpy_s(g_nid.szInfoTitle, ARRAYSIZE(g_nid.szInfoTitle), title);
    wcscpy_s(g_nid.szInfo, ARRAYSIZE(g_nid.szInfo), text);
    Shell_NotifyIconW(NIM_MODIFY, &g_nid);
}

static void AddItem(HMENU m, UINT id, const wchar_t* text, BOOL checked)
{
    AppendMenuW(m, MF_STRING | (checked ? MF_CHECKED : 0), id, text);
}

void TrayShowMenu(HWND hwnd, POINT pt)
{
    g_appState.settings.startupEnabled = IsStartupEnabled();   // task is the truth
    const AppSettings* s = &g_appState.settings;

    static const wchar_t* kSizeNames[] = {L"Tiny (16 px)", L"Small (32 px)", L"Medium (64 px)", L"Large (96 px)"};
    static const wchar_t* kPosNames[POS_COUNT] = {
        L"Top-Left",    L"Top-Center", L"Top-Right",
        L"Center-Left", L"Center",     L"Center-Right",
        L"Bottom-Left", L"Bottom-Center", L"Bottom-Right"};

    HMENU sizeM = CreatePopupMenu();
    for (int i = 0; i < ARRAYSIZE(kOverlaySizes); i++)
        AddItem(sizeM, ID_MENU_SIZE_FIRST + i, kSizeNames[i], s->overlaySize == kOverlaySizes[i]);

    HMENU posM = CreatePopupMenu();
    for (int i = 0; i < POS_COUNT; i++) {
        if (i && i % 3 == 0) AppendMenuW(posM, MF_SEPARATOR, 0, NULL);
        AddItem(posM, ID_MENU_POS_FIRST + i, kPosNames[i], s->overlayPosition == (OverlayPosition)i);
    }

    HMENU overlay = CreatePopupMenu();
    AppendMenuW(overlay, MF_POPUP, (UINT_PTR)sizeM, L"Size");
    AppendMenuW(overlay, MF_POPUP, (UINT_PTR)posM, L"Position");
    AppendMenuW(overlay, MF_SEPARATOR, 0, NULL);
    AddItem(overlay, ID_MENU_MULTIMONITOR, L"Show on All Monitors", s->multiMonitorMode);

    HMENU sound = CreatePopupMenu();
    AddItem(sound, ID_MENU_SOUND_FEEDBACK, L"Sound Feedback", s->audioFeedbackEnabled);
    AddItem(sound, ID_MENU_DUCK_VOLUME, L"Duck System Volume While Muted", s->reduceVolumeWhenMuted);

    HMENU hotkey = CreatePopupMenu();
    wchar_t label[80];
    for (int i = 0; i < 12; i++) {
        wsprintfW(label, L"F%d", i + 1);
        AddItem(hotkey, ID_MENU_HOTKEY_F1 + i, label, s->currentHotkey == (DWORD)(VK_F1 + i));
    }
    AppendMenuW(hotkey, MF_SEPARATOR, 0, NULL);
    AddItem(hotkey, ID_MENU_HOTKEY_CTRL_M, L"Ctrl+M", s->currentHotkey == HOTKEY_CTRL_M);
    if (s->currentHotkey == HOTKEY_CUSTOM) {
        wchar_t hk[48];
        FormatHotkeyName(hk, ARRAYSIZE(hk), HOTKEY_CUSTOM, s->customHotkeyModifiers, s->customHotkeyVK);
        wsprintfW(label, L"Custom: %s", hk);
        AddItem(hotkey, ID_MENU_HOTKEY_CUSTOM, label, TRUE);
    } else {
        AddItem(hotkey, ID_MENU_HOTKEY_CUSTOM, L"Set Custom...", FALSE);
    }

    HMENU menu = CreatePopupMenu();
    AppendMenuW(menu, MF_STRING, ID_MENU_TOGGLE,
                g_appState.isMuted ? L"Unmute Microphone" : L"Mute Microphone");
    SetMenuDefaultItem(menu, ID_MENU_TOGGLE, FALSE);
    AppendMenuW(menu, MF_SEPARATOR, 0, NULL);
    AddItem(menu, ID_MENU_VOLUME_LOCK, L"Lock Mic Volume", s->volumeLockEnabled);
    AppendMenuW(menu, MF_SEPARATOR, 0, NULL);
    AppendMenuW(menu, MF_POPUP, (UINT_PTR)overlay, L"Overlay");
    AppendMenuW(menu, MF_POPUP, (UINT_PTR)sound, L"Sound");
    AppendMenuW(menu, MF_POPUP, (UINT_PTR)hotkey, L"Hotkey");
    AppendMenuW(menu, MF_SEPARATOR, 0, NULL);
    AddItem(menu, ID_MENU_STARTUP, L"Start with Windows", s->startupEnabled);
    AppendMenuW(menu, MF_SEPARATOR, 0, NULL);
    AppendMenuW(menu, MF_STRING, ID_MENU_ABOUT, L"About");
    AppendMenuW(menu, MF_STRING, ID_MENU_EXIT, L"Exit");

    SetForegroundWindow(hwnd);
    TrackPopupMenu(menu, TPM_RIGHTBUTTON, pt.x, pt.y, 0, hwnd, NULL);
    PostMessageW(hwnd, WM_NULL, 0, 0);   // lets the menu dismiss correctly
    DestroyMenu(menu);                   // destroys submenus recursively
}

void TrayCleanup(void)
{
    Shell_NotifyIconW(NIM_DELETE, &g_nid);
    if (g_iconMuted) { DestroyIcon(g_iconMuted); g_iconMuted = NULL; }
    if (g_iconLive)  { DestroyIcon(g_iconLive);  g_iconLive = NULL; }
}
