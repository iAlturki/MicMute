#include "micmute.h"

AppState g_appState = {0};
const DWORD kOverlaySizes[4] = {16, 32, 64, 96};

static HANDLE g_hMutex = NULL;
static UINT g_msgActivate;   // broadcast by a second instance

static LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);

// Re-sync everything that reflects mute state.
void SyncMuteUI(void)
{
    TrayUpdate();
    if (g_appState.isMuted) OverlayShowMuted(TRUE);
    else                    OverlayHide();
    DuckSystemVolume(g_appState.isMuted);
    g_appState.settings.lastMuteState = g_appState.isMuted;
    SaveSettings();
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow)
{
    g_msgActivate = RegisterWindowMessageW(L"iAlturki-MicMute-Activate");

    g_hMutex = CreateMutexW(NULL, TRUE, L"iAlturki-MicMute-SingleInstance");
    if (GetLastError() == ERROR_ALREADY_EXISTS) {
        // Tell the running instance to announce itself instead of erroring out.
        PostMessageW(HWND_BROADCAST, g_msgActivate, 0, 0);
        if (g_hMutex) CloseHandle(g_hMutex);
        return 0;
    }

    // DPI awareness (PerMonitorV2) is declared in the embedded manifest.
    CoInitialize(NULL);
    Render_Init();
    LoadSettings();

    WNDCLASSW wc = {0};
    wc.lpfnWndProc = WindowProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = L"MicMuteWindow";
    wc.hIcon = LoadIconW(hInstance, MAKEINTRESOURCEW(1));
    RegisterClassW(&wc);

    g_appState.hWnd = CreateWindowW(L"MicMuteWindow", APP_NAME, WS_OVERLAPPED,
                                    0, 0, 0, 0, NULL, NULL, hInstance, NULL);
    if (!g_appState.hWnd) {
        MessageBoxW(NULL, L"Failed to create window.", APP_NAME, MB_OK | MB_ICONERROR);
        return 1;
    }

    if (!AudioInit()) {
        MessageBoxW(NULL, L"Audio system unavailable (COM initialization failed).", APP_NAME, MB_OK | MB_ICONERROR);
        return 1;
    }

    // Adopt the device's real state so the UI never lies about a hot mic.
    BOOL actual;
    if (AudioGetMute(&actual)) g_appState.isMuted = actual;
    else                       g_appState.isMuted = g_appState.settings.lastMuteState;
    RestoreLockedVolume();   // enforce a persisted volume lock against drift while closed

    // Never fatal: at logon we may start before Explorer's taskbar exists.
    // Retry on a timer (and on TaskbarCreated) until the icon lands; the
    // hotkey and overlay work the whole time.
    if (!TrayInit(g_appState.hWnd))
        SetTimer(g_appState.hWnd, TRAY_RETRY_TIMER_ID, 2000, NULL);
    SyncMuteUI();
    RegisterCurrentHotkey();

    MSG msg;
    while (GetMessage(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    UnregisterCurrentHotkey();
    TrayCleanup();
    DuckSystemVolume(FALSE);
    AudioCleanup();
    OverlayCleanup();
    g_appState.settings.lastMuteState = g_appState.isMuted;
    SaveSettings();
    Render_Shutdown();
    CoUninitialize();
    if (g_hMutex) { ReleaseMutex(g_hMutex); CloseHandle(g_hMutex); }
    return 0;
}

static void OnCommand(HWND hwnd, UINT id)
{
    if (id >= ID_MENU_POS_FIRST && id <= ID_MENU_POS_LAST) {
        g_appState.settings.overlayPosition = (OverlayPosition)(id - ID_MENU_POS_FIRST);
        SaveSettings();
        OverlayRebuild();
        return;
    }
    if (id >= ID_MENU_SIZE_FIRST && id <= ID_MENU_SIZE_LAST) {
        g_appState.settings.overlaySize = kOverlaySizes[id - ID_MENU_SIZE_FIRST];
        SaveSettings();
        OverlayRebuild();
        return;
    }
    if (id >= ID_MENU_HOTKEY_F1 && id <= ID_MENU_HOTKEY_F12) {
        ChangeHotkey(VK_F1 + (id - ID_MENU_HOTKEY_F1));
        return;
    }

    switch (id) {
        case ID_MENU_TOGGLE:        ToggleMicrophone(); break;
        case ID_MENU_HOTKEY_CTRL_M: ChangeHotkey(HOTKEY_CTRL_M); break;
        case ID_MENU_HOTKEY_CUSTOM: BeginHotkeyCapture(); break;

        case ID_MENU_MULTIMONITOR:
            g_appState.settings.multiMonitorMode = !g_appState.settings.multiMonitorMode;
            SaveSettings();
            OverlayRebuild();
            break;

        case ID_MENU_SOUND_FEEDBACK:
            g_appState.settings.audioFeedbackEnabled = !g_appState.settings.audioFeedbackEnabled;
            SaveSettings();
            break;

        case ID_MENU_DUCK_VOLUME:
            g_appState.settings.reduceVolumeWhenMuted = !g_appState.settings.reduceVolumeWhenMuted;
            DuckSystemVolume(g_appState.isMuted);
            SaveSettings();
            break;

        case ID_MENU_VOLUME_LOCK:
            if (g_appState.settings.volumeLockEnabled) {
                g_appState.settings.volumeLockEnabled = FALSE;
                SaveSettings();
            } else {
                LockCurrentVolume();
            }
            break;

        case ID_MENU_STARTUP:
            EnableStartup(!g_appState.settings.startupEnabled);
            break;

        case ID_MENU_ABOUT: {
            wchar_t text[256];
            wsprintfW(text, L"%s v%s\n\nMicrophone mute utility for Windows.\n\nVisit GitHub page?",
                      APP_NAME, APP_VERSION);
            if (MessageBoxW(hwnd, text, L"About", MB_YESNO | MB_ICONINFORMATION) == IDYES)
                ShellExecuteW(NULL, L"open", APP_URL, NULL, NULL, SW_SHOWNORMAL);
            break;
        }

        case ID_MENU_EXIT:
            DestroyWindow(hwnd);
            break;
    }
}

static LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    if (uMsg == g_msgActivate && g_msgActivate) {
        wchar_t hk[64], text[128];
        FormatHotkeyName(hk, 64, g_appState.settings.currentHotkey,
                         g_appState.settings.customHotkeyModifiers, g_appState.settings.customHotkeyVK);
        wsprintfW(text, L"Already running. Press %s to toggle the microphone.", hk);
        TrayBalloon(APP_NAME, text);
        return 0;
    }
    if (uMsg == TrayTaskbarCreatedMsg()) {
        TrayInit(hwnd);   // Explorer restarted: re-add the icon
        TrayUpdate();
        return 0;
    }

    switch (uMsg) {
        case WM_HOTKEY:
            if (wParam == HOTKEY_ID) ToggleMicrophone();
            return 0;

        case WM_TIMER:
            if (wParam == TRAY_RETRY_TIMER_ID && TrayInit(hwnd)) {
                KillTimer(hwnd, TRAY_RETRY_TIMER_ID);
                TrayUpdate();
            }
            return 0;

        case WM_APP_TRAY:
            switch (LOWORD(lParam)) {
                case WM_LBUTTONDBLCLK: ToggleMicrophone(); break;
                case WM_RBUTTONUP: {
                    POINT pt;
                    GetCursorPos(&pt);
                    TrayShowMenu(hwnd, pt);
                    break;
                }
            }
            return 0;

        case WM_APP_AUDIO: {   // change made outside the app (Teams, mic key, Settings...)
            BOOL muted = (BOOL)wParam;
            AudioGetMute(&muted);   // snapshot may be stale if we toggled meanwhile
            float vol = (float)lParam / 10000.0f;
            if (muted != g_appState.isMuted) {
                g_appState.isMuted = muted;
                SyncMuteUI();
            }
            if (g_appState.settings.volumeLockEnabled &&
                fabsf(vol - g_appState.settings.lockedVolume) > 0.004f)
                RestoreLockedVolume();
            return 0;
        }

        case WM_APP_DEVICE:
            if (wParam) AudioReinitSystemEndpoint();   // default output changed
            else        AudioReinitDevice();           // default mic changed
            return 0;

        case WM_DISPLAYCHANGE:
        case WM_DPICHANGED:
            OverlayRebuild();
            return 0;

        case WM_SETTINGCHANGE:
            if (wParam == SPI_SETWORKAREA)             // taskbar moved/resized
                OverlayRebuild();
            else if (lParam && wcscmp((const wchar_t*)lParam, L"ImmersiveColorSet") == 0)
                TrayRefreshTheme();
            return 0;

        case WM_ENDSESSION:
            if (wParam) {   // process may be killed any moment: undo the duck now
                DuckSystemVolume(FALSE);
                g_appState.settings.lastMuteState = g_appState.isMuted;
                SaveSettings();
            }
            return 0;

        case WM_COMMAND:
            OnCommand(hwnd, LOWORD(wParam));
            return 0;

        case WM_DESTROY:
            PostQuitMessage(0);
            return 0;
    }
    return DefWindowProc(hwnd, uMsg, wParam, lParam);
}
