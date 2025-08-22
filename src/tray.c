#include "micmute.h"

BOOL InitializeSystemTray(HWND hwnd, HINSTANCE hInstance)
{
    NOTIFYICONDATAW nid = {0};
    nid.cbSize = sizeof(NOTIFYICONDATAW);
    nid.hWnd = hwnd;
    nid.uID = 1;
    nid.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP;
    nid.uCallbackMessage = WM_TRAYICON;
    nid.hIcon = LoadIcon(NULL, IDI_INFORMATION);
    wcscpy_s(nid.szTip, sizeof(nid.szTip)/sizeof(wchar_t), L"iAlturki-MicMute Enhanced");
    
    return Shell_NotifyIconW(NIM_ADD, &nid);
}

void UpdateTrayIcon(BOOL isMuted) 
{
    NOTIFYICONDATAW nid = {0};
    nid.cbSize = sizeof(NOTIFYICONDATAW);
    nid.hWnd = g_appState.hWnd;
    nid.uID = 1;
    nid.uFlags = NIF_ICON | NIF_TIP;
    
    if (isMuted) {
        nid.hIcon = LoadIcon(NULL, IDI_ERROR);
        if (g_appState.settings.volumeLockEnabled) {
            wcscpy_s(nid.szTip, sizeof(nid.szTip)/sizeof(wchar_t), L"iAlturki-MicMute - MUTED - Volume Locked");
        } else {
            wcscpy_s(nid.szTip, sizeof(nid.szTip)/sizeof(wchar_t), L"iAlturki-MicMute - MUTED");
        }
    } else {
        nid.hIcon = LoadIcon(NULL, IDI_INFORMATION);
        if (g_appState.settings.volumeLockEnabled) {
            wcscpy_s(nid.szTip, sizeof(nid.szTip)/sizeof(wchar_t), L"iAlturki-MicMute - ACTIVE - Volume Locked");
        } else {
            wcscpy_s(nid.szTip, sizeof(nid.szTip)/sizeof(wchar_t), L"iAlturki-MicMute - ACTIVE");
        }
    }
    
    Shell_NotifyIconW(NIM_MODIFY, &nid);
}

HMENU CreateOverlaySettingsMenu(void)
{
    HMENU hSubmenu = CreatePopupMenu();
    
    // Size submenu
    HMENU hSizeMenu = CreatePopupMenu();
    AppendMenuW(hSizeMenu, (g_appState.settings.overlaySize == SIZE_SUPER_SMALL) ? MF_STRING | MF_CHECKED : MF_STRING, 
                ID_MENU_SIZE_SUPER_SMALL, L"Super Small (16px)");
    AppendMenuW(hSizeMenu, (g_appState.settings.overlaySize == SIZE_SMALL) ? MF_STRING | MF_CHECKED : MF_STRING, 
                ID_MENU_SIZE_SMALL, L"Small (32px)");
    AppendMenuW(hSizeMenu, (g_appState.settings.overlaySize == SIZE_MEDIUM) ? MF_STRING | MF_CHECKED : MF_STRING, 
                ID_MENU_SIZE_MEDIUM, L"Medium (64px)");
    AppendMenuW(hSizeMenu, (g_appState.settings.overlaySize == SIZE_LARGE) ? MF_STRING | MF_CHECKED : MF_STRING, 
                ID_MENU_SIZE_LARGE, L"Large (96px)");
    
    // Position submenu
    HMENU hPosMenu = CreatePopupMenu();
    AppendMenuW(hPosMenu, (g_appState.settings.overlayPosition == POS_TOP_LEFT) ? MF_STRING | MF_CHECKED : MF_STRING, 
                ID_MENU_POS_TOP_LEFT, L"Top-Left");
    AppendMenuW(hPosMenu, (g_appState.settings.overlayPosition == POS_TOP_CENTER) ? MF_STRING | MF_CHECKED : MF_STRING, 
                ID_MENU_POS_TOP_CENTER, L"Top-Center");
    AppendMenuW(hPosMenu, (g_appState.settings.overlayPosition == POS_TOP_RIGHT) ? MF_STRING | MF_CHECKED : MF_STRING, 
                ID_MENU_POS_TOP_RIGHT, L"Top-Right");
    AppendMenuW(hPosMenu, MF_SEPARATOR, 0, NULL);
    AppendMenuW(hPosMenu, (g_appState.settings.overlayPosition == POS_CENTER_LEFT) ? MF_STRING | MF_CHECKED : MF_STRING, 
                ID_MENU_POS_CENTER_LEFT, L"Center-Left");
    AppendMenuW(hPosMenu, (g_appState.settings.overlayPosition == POS_CENTER_CENTER) ? MF_STRING | MF_CHECKED : MF_STRING, 
                ID_MENU_POS_CENTER_CENTER, L"Center");
    AppendMenuW(hPosMenu, (g_appState.settings.overlayPosition == POS_CENTER_RIGHT) ? MF_STRING | MF_CHECKED : MF_STRING, 
                ID_MENU_POS_CENTER_RIGHT, L"Center-Right");
    AppendMenuW(hPosMenu, MF_SEPARATOR, 0, NULL);
    AppendMenuW(hPosMenu, (g_appState.settings.overlayPosition == POS_BOTTOM_LEFT) ? MF_STRING | MF_CHECKED : MF_STRING, 
                ID_MENU_POS_BOTTOM_LEFT, L"Bottom-Left");
    AppendMenuW(hPosMenu, (g_appState.settings.overlayPosition == POS_BOTTOM_CENTER) ? MF_STRING | MF_CHECKED : MF_STRING, 
                ID_MENU_POS_BOTTOM_CENTER, L"Bottom-Center");
    AppendMenuW(hPosMenu, (g_appState.settings.overlayPosition == POS_BOTTOM_RIGHT) ? MF_STRING | MF_CHECKED : MF_STRING, 
                ID_MENU_POS_BOTTOM_RIGHT, L"Bottom-Right");
    
    // Add to main submenu
    AppendMenuW(hSubmenu, MF_STRING | MF_POPUP, (UINT_PTR)hSizeMenu, L"Size");
    AppendMenuW(hSubmenu, MF_STRING | MF_POPUP, (UINT_PTR)hPosMenu, L"Position");
    AppendMenuW(hSubmenu, MF_SEPARATOR, 0, NULL);
    AppendMenuW(hSubmenu, g_appState.settings.multiMonitorMode ? MF_STRING | MF_CHECKED : MF_STRING, 
                ID_MENU_MULTIMONITOR_TOGGLE, L"All Monitors");
    
    return hSubmenu;
}

HMENU CreateAudioSettingsMenu(void)
{
    HMENU hSubmenu = CreatePopupMenu();
    
    // Audio settings menu
    AppendMenuW(hSubmenu, g_appState.settings.audioFeedbackEnabled ? MF_STRING | MF_CHECKED : MF_STRING, 
                ID_MENU_AUDIO_FEEDBACK_TOGGLE, L"Audio Feedback");
    AppendMenuW(hSubmenu, MF_SEPARATOR, 0, NULL);
    AppendMenuW(hSubmenu, g_appState.settings.reduceVolumeWhenMuted ? MF_STRING | MF_CHECKED : MF_STRING, 
                ID_MENU_REDUCE_VOLUME_TOGGLE, L"Reduce System Volume When Muted");
    
    return hSubmenu;
}

HMENU CreateHotkeySettingsMenu(void)
{
    HMENU hSubmenu = CreatePopupMenu();
    
    // F-key options
    AppendMenuW(hSubmenu, (g_appState.settings.currentHotkey == HOTKEY_F1) ? MF_STRING | MF_CHECKED : MF_STRING, 
                ID_MENU_HOTKEY_F1, L"F1");
    AppendMenuW(hSubmenu, (g_appState.settings.currentHotkey == HOTKEY_F2) ? MF_STRING | MF_CHECKED : MF_STRING, 
                ID_MENU_HOTKEY_F2, L"F2");
    AppendMenuW(hSubmenu, (g_appState.settings.currentHotkey == HOTKEY_F3) ? MF_STRING | MF_CHECKED : MF_STRING, 
                ID_MENU_HOTKEY_F3, L"F3");
    AppendMenuW(hSubmenu, (g_appState.settings.currentHotkey == HOTKEY_F4) ? MF_STRING | MF_CHECKED : MF_STRING, 
                ID_MENU_HOTKEY_F4, L"F4");
    AppendMenuW(hSubmenu, (g_appState.settings.currentHotkey == HOTKEY_F5) ? MF_STRING | MF_CHECKED : MF_STRING, 
                ID_MENU_HOTKEY_F5, L"F5");
    AppendMenuW(hSubmenu, (g_appState.settings.currentHotkey == HOTKEY_F6) ? MF_STRING | MF_CHECKED : MF_STRING, 
                ID_MENU_HOTKEY_F6, L"F6");
    AppendMenuW(hSubmenu, (g_appState.settings.currentHotkey == HOTKEY_F7) ? MF_STRING | MF_CHECKED : MF_STRING, 
                ID_MENU_HOTKEY_F7, L"F7");
    AppendMenuW(hSubmenu, (g_appState.settings.currentHotkey == HOTKEY_F8) ? MF_STRING | MF_CHECKED : MF_STRING, 
                ID_MENU_HOTKEY_F8, L"F8");
    AppendMenuW(hSubmenu, (g_appState.settings.currentHotkey == HOTKEY_F9) ? MF_STRING | MF_CHECKED : MF_STRING, 
                ID_MENU_HOTKEY_F9, L"F9");
    AppendMenuW(hSubmenu, (g_appState.settings.currentHotkey == HOTKEY_F10) ? MF_STRING | MF_CHECKED : MF_STRING, 
                ID_MENU_HOTKEY_F10, L"F10");
    AppendMenuW(hSubmenu, (g_appState.settings.currentHotkey == HOTKEY_F11) ? MF_STRING | MF_CHECKED : MF_STRING, 
                ID_MENU_HOTKEY_F11, L"F11");
    AppendMenuW(hSubmenu, (g_appState.settings.currentHotkey == HOTKEY_F12) ? MF_STRING | MF_CHECKED : MF_STRING, 
                ID_MENU_HOTKEY_F12, L"F12");
    
    AppendMenuW(hSubmenu, MF_SEPARATOR, 0, NULL);
    AppendMenuW(hSubmenu, (g_appState.settings.currentHotkey == HOTKEY_CTRL_M) ? MF_STRING | MF_CHECKED : MF_STRING, 
                ID_MENU_HOTKEY_CTRL_M, L"Ctrl+M");
    
    AppendMenuW(hSubmenu, MF_SEPARATOR, 0, NULL);
    AppendMenuW(hSubmenu, MF_STRING | MF_GRAYED, 
                ID_MENU_HOTKEY_CUSTOM, L"Custom Hotkey COMING SOON");
    
    return hSubmenu;
}

HMENU CreateStartupSettingsMenu(void)
{
    HMENU hSubmenu = CreatePopupMenu();
    
    AppendMenuW(hSubmenu, g_appState.settings.startupEnabled ? MF_STRING | MF_CHECKED : MF_STRING, 
                ID_MENU_STARTUP_ENABLE, L"Enable Startup");
    AppendMenuW(hSubmenu, !g_appState.settings.startupEnabled ? MF_STRING | MF_CHECKED : MF_STRING, 
                ID_MENU_STARTUP_DISABLE, L"Disable Startup");
    
    return hSubmenu;
}

void ShowTrayMenu(HWND hwnd, POINT pt)
{
    SetForegroundWindow(hwnd);
    
    HMENU hMenu = CreatePopupMenu();
    
    // Main toggle option
    if (g_appState.isMuted) {
        AppendMenuW(hMenu, MF_STRING, ID_MENU_TOGGLE, L"Unmute Microphone");
    } else {
        AppendMenuW(hMenu, MF_STRING, ID_MENU_TOGGLE, L"Mute Microphone");
    }
    
    AppendMenuW(hMenu, MF_SEPARATOR, 0, NULL);
    
    // Volume lock option
    if (g_appState.settings.volumeLockEnabled) {
        AppendMenuW(hMenu, MF_STRING | MF_CHECKED, ID_MENU_VOLUME_LOCK_TOGGLE, L"MIC Volume Lock");
    } else {
        AppendMenuW(hMenu, MF_STRING, ID_MENU_VOLUME_LOCK_TOGGLE, L"MIC Volume Lock");
    }
    
    AppendMenuW(hMenu, MF_SEPARATOR, 0, NULL);
    
    // Settings submenus
    HMENU hOverlayMenu = CreateOverlaySettingsMenu();
    HMENU hAudioMenu = CreateAudioSettingsMenu();
    HMENU hHotkeyMenu = CreateHotkeySettingsMenu();
    HMENU hStartupMenu = CreateStartupSettingsMenu();
    
    AppendMenuW(hMenu, MF_STRING | MF_POPUP, (UINT_PTR)hOverlayMenu, L"Overlay Settings");
    AppendMenuW(hMenu, MF_STRING | MF_POPUP, (UINT_PTR)hAudioMenu, L"Audio Settings");
    AppendMenuW(hMenu, MF_STRING | MF_POPUP, (UINT_PTR)hHotkeyMenu, L"Hotkey Settings");
    AppendMenuW(hMenu, MF_STRING | MF_POPUP, (UINT_PTR)hStartupMenu, L"Startup Settings");
    
    AppendMenuW(hMenu, MF_SEPARATOR, 0, NULL);
    AppendMenuW(hMenu, MF_STRING, ID_MENU_ABOUT, L"About");
    AppendMenuW(hMenu, MF_STRING, ID_MENU_EXIT, L"Exit");
    
    TrackPopupMenu(hMenu, TPM_RIGHTBUTTON, pt.x, pt.y, 0, hwnd, NULL);
    DestroyMenu(hMenu);
}

void CleanupSystemTray(void)
{
    NOTIFYICONDATAW nid = {0};
    nid.cbSize = sizeof(NOTIFYICONDATAW);
    nid.hWnd = g_appState.hWnd;
    nid.uID = 1;
    
    Shell_NotifyIconW(NIM_DELETE, &nid);
}
