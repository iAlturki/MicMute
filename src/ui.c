#include "micmute.h"

BOOL CreateMainWindow(HINSTANCE hInstance)
{
    WNDCLASSW wc = {0};
    wc.lpfnWndProc = WindowProc;
    wc.hInstance = hInstance;
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wc.lpszClassName = L"MicMuteWindow";
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    
    if (!RegisterClassW(&wc)) {
        return FALSE;
    }
    
    // Create hidden window for tray functionality
    g_appState.hWnd = CreateWindowW(
        L"MicMuteWindow",
        L"iAlturki-MicMute",
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT,
        300, 200,
        NULL, NULL, hInstance, NULL
    );
    
    if (!g_appState.hWnd) {
        return FALSE;
    }
    
    // Hide the window - we only want system tray
    ShowWindow(g_appState.hWnd, SW_HIDE);
    
    return TRUE;
}

void LoadIconFromFile(void)
{
    wchar_t iconPath[MAX_PATH];
    
    // First try loading from the same directory as the exe
    GetModuleFileNameW(NULL, iconPath, MAX_PATH);
    wchar_t* lastSlash = wcsrchr(iconPath, L'\\');
    if (lastSlash) {
        *(lastSlash + 1) = L'\0';
        wcscat_s(iconPath, MAX_PATH, L"Mic_Muted_icon.ico");
        
        g_appState.hMutedIcon = (HICON)LoadImageW(NULL, iconPath, IMAGE_ICON, 
            g_appState.settings.overlaySize, g_appState.settings.overlaySize, 
            LR_LOADFROMFILE | LR_DEFAULTSIZE);
    }
    
    // If that fails, try current directory
    if (!g_appState.hMutedIcon) {
        g_appState.hMutedIcon = (HICON)LoadImageW(NULL, L"Mic_Muted_icon.ico", IMAGE_ICON,
            g_appState.settings.overlaySize, g_appState.settings.overlaySize, 
            LR_LOADFROMFILE | LR_DEFAULTSIZE);
    }
    
    // If still no icon, try loading the original without sizing
    if (!g_appState.hMutedIcon) {
        GetModuleFileNameW(NULL, iconPath, MAX_PATH);
        lastSlash = wcsrchr(iconPath, L'\\');
        if (lastSlash) {
            *(lastSlash + 1) = L'\0';
            wcscat_s(iconPath, MAX_PATH, L"Mic_Muted_icon.ico");
            
            g_appState.hMutedIcon = (HICON)LoadImageW(NULL, iconPath, IMAGE_ICON, 
                0, 0, LR_LOADFROMFILE | LR_DEFAULTSIZE);
        }
    }
}

void InitializeMultiMonitorOverlay(void)
{
    // Initialize monitor count
    g_appState.numMonitors = GetSystemMetrics(SM_CMONITORS);
    
    if (g_appState.numMonitors > 1) {
        g_appState.hOverlayWindows = (HWND*)calloc(g_appState.numMonitors, sizeof(HWND));
    }
}

void CleanupMultiMonitorOverlay(void)
{
    if (g_appState.hOverlayWindows) {
        for (int i = 0; i < g_appState.numMonitors; i++) {
            if (g_appState.hOverlayWindows[i]) {
                DestroyWindow(g_appState.hOverlayWindows[i]);
            }
        }
        free(g_appState.hOverlayWindows);
        g_appState.hOverlayWindows = NULL;
    }
}

typedef struct {
    int currentIndex;
    RECT* monitors;
} MonitorEnumData;

BOOL CALLBACK MonitorEnumProc(HMONITOR hMonitor, HDC hdcMonitor, LPRECT lprcMonitor, LPARAM dwData)
{
    MonitorEnumData* data = (MonitorEnumData*)dwData;
    if (data->currentIndex < g_appState.numMonitors) {
        data->monitors[data->currentIndex] = *lprcMonitor;
        data->currentIndex++;
    }
    return TRUE;
}

void UpdateOverlayPosition(void)
{
    if (g_appState.settings.multiMonitorMode) {
        // Show on all monitors
        if (!g_appState.hOverlayWindows) return;
        
        RECT monitors[10]; // Support up to 10 monitors
        MonitorEnumData enumData = {0, monitors};
        EnumDisplayMonitors(NULL, NULL, MonitorEnumProc, (LPARAM)&enumData);
        
        for (int i = 0; i < g_appState.numMonitors && i < 10; i++) {
            if (g_appState.hOverlayWindows[i]) {
                RECT rect = monitors[i];
                int screenWidth = rect.right - rect.left;
                int screenHeight = rect.bottom - rect.top;
                int size = g_appState.settings.overlaySize;
                int x = rect.left, y = rect.top;
                
                // Calculate position based on 9-point grid
                switch (g_appState.settings.overlayPosition) {
                    case POS_TOP_LEFT:
                        x = rect.left + 20; y = rect.top + 20;
                        break;
                    case POS_TOP_CENTER:
                        x = rect.left + (screenWidth - size) / 2; y = rect.top + 20;
                        break;
                    case POS_TOP_RIGHT:
                        x = rect.right - size - 20; y = rect.top + 20;
                        break;
                    case POS_CENTER_LEFT:
                        x = rect.left + 20; y = rect.top + (screenHeight - size) / 2;
                        break;
                    case POS_CENTER_CENTER:
                        x = rect.left + (screenWidth - size) / 2; y = rect.top + (screenHeight - size) / 2;
                        break;
                    case POS_CENTER_RIGHT:
                        x = rect.right - size - 20; y = rect.top + (screenHeight - size) / 2;
                        break;
                    case POS_BOTTOM_LEFT:
                        x = rect.left + 20; y = rect.bottom - size - 60;
                        break;
                    case POS_BOTTOM_CENTER:
                        x = rect.left + (screenWidth - size) / 2; y = rect.bottom - size - 60;
                        break;
                    case POS_BOTTOM_RIGHT:
                        x = rect.right - size - 20; y = rect.bottom - size - 60;
                        break;
                }
                
                SetWindowPos(g_appState.hOverlayWindows[i], HWND_TOPMOST, x, y, size, size, SWP_NOACTIVATE);
            }
        }
    } else {
        // Show on primary monitor only
        if (!g_appState.hOverlayWnd) return;
        
        int screenWidth = GetSystemMetrics(SM_CXSCREEN);
        int screenHeight = GetSystemMetrics(SM_CYSCREEN);
        int size = g_appState.settings.overlaySize;
        int x = 0, y = 0;
        
        // Calculate position based on 9-point grid
        switch (g_appState.settings.overlayPosition) {
            case POS_TOP_LEFT:
                x = 20; y = 20;
                break;
            case POS_TOP_CENTER:
                x = (screenWidth - size) / 2; y = 20;
                break;
            case POS_TOP_RIGHT:
                x = screenWidth - size - 20; y = 20;
                break;
            case POS_CENTER_LEFT:
                x = 20; y = (screenHeight - size) / 2;
                break;
            case POS_CENTER_CENTER:
                x = (screenWidth - size) / 2; y = (screenHeight - size) / 2;
                break;
            case POS_CENTER_RIGHT:
                x = screenWidth - size - 20; y = (screenHeight - size) / 2;
                break;
            case POS_BOTTOM_LEFT:
                x = 20; y = screenHeight - size - 60;
                break;
            case POS_BOTTOM_CENTER:
                x = (screenWidth - size) / 2; y = screenHeight - size - 60;
                break;
            case POS_BOTTOM_RIGHT:
                x = screenWidth - size - 20; y = screenHeight - size - 60;
                break;
        }
        
        SetWindowPos(g_appState.hOverlayWnd, HWND_TOPMOST, x, y, size, size, SWP_NOACTIVATE);
    }
}

void ShowOverlay(BOOL show)
{
    if (show && g_appState.isMuted) {
        if (g_appState.settings.multiMonitorMode) {
            // Multi-monitor mode
            if (!g_appState.hOverlayWindows) return;
            
            for (int i = 0; i < g_appState.numMonitors; i++) {
                if (!g_appState.hOverlayWindows[i]) {
                    // Create overlay window for this monitor
                    WNDCLASSW wc = {0};
                    wc.lpfnWndProc = OverlayWndProc;
                    wc.hInstance = GetModuleHandle(NULL);
                    wc.lpszClassName = L"MicMuteOverlay";
                    wc.hbrBackground = NULL;
                    RegisterClassW(&wc);
                    
                    g_appState.hOverlayWindows[i] = CreateWindowExW(
                        WS_EX_LAYERED | WS_EX_TOPMOST | WS_EX_TOOLWINDOW | WS_EX_TRANSPARENT,
                        L"MicMuteOverlay",
                        L"",
                        WS_POPUP,
                        0, 0, g_appState.settings.overlaySize, g_appState.settings.overlaySize,
                        NULL, NULL, GetModuleHandle(NULL), NULL
                    );
                    
                    // Set transparent background
                    SetLayeredWindowAttributes(g_appState.hOverlayWindows[i], RGB(0, 0, 0), 255, LWA_COLORKEY | LWA_ALPHA);
                }
                
                ShowWindow(g_appState.hOverlayWindows[i], SW_SHOW);
                InvalidateRect(g_appState.hOverlayWindows[i], NULL, TRUE);
            }
        } else {
            // Single monitor mode (primary only)
            if (!g_appState.hOverlayWnd) {
                // Create overlay window
                WNDCLASSW wc = {0};
                wc.lpfnWndProc = OverlayWndProc;
                wc.hInstance = GetModuleHandle(NULL);
                wc.lpszClassName = L"MicMuteOverlay";
                wc.hbrBackground = NULL;
                RegisterClassW(&wc);
                
                g_appState.hOverlayWnd = CreateWindowExW(
                    WS_EX_LAYERED | WS_EX_TOPMOST | WS_EX_TOOLWINDOW | WS_EX_TRANSPARENT,
                    L"MicMuteOverlay",
                    L"",
                    WS_POPUP,
                    0, 0, g_appState.settings.overlaySize, g_appState.settings.overlaySize,
                    NULL, NULL, GetModuleHandle(NULL), NULL
                );
                
                // Set transparent background
                SetLayeredWindowAttributes(g_appState.hOverlayWnd, RGB(0, 0, 0), 255, LWA_COLORKEY | LWA_ALPHA);
            }
            
            ShowWindow(g_appState.hOverlayWnd, SW_SHOW);
            InvalidateRect(g_appState.hOverlayWnd, NULL, TRUE);
        }
        
        g_appState.isOverlayVisible = TRUE;
        UpdateOverlayPosition();
    } else {
        // Hide overlay
        if (g_appState.settings.multiMonitorMode && g_appState.hOverlayWindows) {
            for (int i = 0; i < g_appState.numMonitors; i++) {
                if (g_appState.hOverlayWindows[i]) {
                    ShowWindow(g_appState.hOverlayWindows[i], SW_HIDE);
                }
            }
        } else if (g_appState.hOverlayWnd) {
            ShowWindow(g_appState.hOverlayWnd, SW_HIDE);
        }
        
        g_appState.isOverlayVisible = FALSE;
    }
}

LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    switch (uMsg) {
        case WM_CREATE:
            // Set a timer to register hotkey after window is fully created
            SetTimer(hwnd, 9999, 100, NULL); // Register hotkey after 100ms
            break;
            
        case WM_TIMER:
            if (wParam == 9999) {
                // One-time hotkey registration
                KillTimer(hwnd, 9999);
                RegisterCurrentHotkey();
            } else if (wParam == VOLUME_MONITOR_TIMER_ID) {
                MonitorVolumeLevel();
            }
            break;
            
        case WM_HOTKEY:
            if (wParam == HOTKEY_ID) {
                ToggleMicrophone();
            }
            break;
            
        case WM_TRAYICON:
            switch (LOWORD(lParam)) {
                case WM_LBUTTONDBLCLK:
                    ToggleMicrophone();
                    break;
                case WM_RBUTTONUP:
                    {
                        POINT pt;
                        GetCursorPos(&pt);
                        ShowTrayMenu(hwnd, pt);
                    }
                    break;
            }
            break;
            
        case WM_COMMAND:
            switch (LOWORD(wParam)) {
                case ID_MENU_TOGGLE:
                    ToggleMicrophone();
                    break;
                    
                // Overlay size menu
                case ID_MENU_SIZE_SUPER_SMALL:
                    g_appState.settings.overlaySize = SIZE_SUPER_SMALL;
                    LoadIconFromFile(); // Reload icon with new size
                    SaveSettings();
                    // Update overlay immediately if currently muted
                    if (g_appState.isMuted && g_appState.isOverlayVisible) {
                        UpdateOverlayPosition();
                    }
                    break;
                case ID_MENU_SIZE_SMALL:
                    g_appState.settings.overlaySize = SIZE_SMALL;
                    LoadIconFromFile();
                    SaveSettings();
                    if (g_appState.isMuted && g_appState.isOverlayVisible) {
                        UpdateOverlayPosition();
                    }
                    break;
                case ID_MENU_SIZE_MEDIUM:
                    g_appState.settings.overlaySize = SIZE_MEDIUM;
                    LoadIconFromFile();
                    SaveSettings();
                    if (g_appState.isMuted && g_appState.isOverlayVisible) {
                        UpdateOverlayPosition();
                    }
                    break;
                case ID_MENU_SIZE_LARGE:
                    g_appState.settings.overlaySize = SIZE_LARGE;
                    LoadIconFromFile();
                    SaveSettings();
                    if (g_appState.isMuted && g_appState.isOverlayVisible) {
                        UpdateOverlayPosition();
                    }
                    break;
                    
                // Position menu
                case ID_MENU_POS_TOP_LEFT:
                    g_appState.settings.overlayPosition = POS_TOP_LEFT;
                    SaveSettings();
                    if (g_appState.isMuted && g_appState.isOverlayVisible) {
                        UpdateOverlayPosition();
                    }
                    break;
                case ID_MENU_POS_TOP_CENTER:
                    g_appState.settings.overlayPosition = POS_TOP_CENTER;
                    SaveSettings();
                    if (g_appState.isMuted && g_appState.isOverlayVisible) {
                        UpdateOverlayPosition();
                    }
                    break;
                case ID_MENU_POS_TOP_RIGHT:
                    g_appState.settings.overlayPosition = POS_TOP_RIGHT;
                    SaveSettings();
                    if (g_appState.isMuted && g_appState.isOverlayVisible) {
                        UpdateOverlayPosition();
                    }
                    break;
                case ID_MENU_POS_CENTER_LEFT:
                    g_appState.settings.overlayPosition = POS_CENTER_LEFT;
                    SaveSettings();
                    if (g_appState.isMuted && g_appState.isOverlayVisible) {
                        UpdateOverlayPosition();
                    }
                    break;
                case ID_MENU_POS_CENTER_CENTER:
                    g_appState.settings.overlayPosition = POS_CENTER_CENTER;
                    SaveSettings();
                    if (g_appState.isMuted && g_appState.isOverlayVisible) {
                        UpdateOverlayPosition();
                    }
                    break;
                case ID_MENU_POS_CENTER_RIGHT:
                    g_appState.settings.overlayPosition = POS_CENTER_RIGHT;
                    SaveSettings();
                    if (g_appState.isMuted && g_appState.isOverlayVisible) {
                        UpdateOverlayPosition();
                    }
                    break;
                case ID_MENU_POS_BOTTOM_LEFT:
                    g_appState.settings.overlayPosition = POS_BOTTOM_LEFT;
                    SaveSettings();
                    if (g_appState.isMuted && g_appState.isOverlayVisible) {
                        UpdateOverlayPosition();
                    }
                    break;
                case ID_MENU_POS_BOTTOM_CENTER:
                    g_appState.settings.overlayPosition = POS_BOTTOM_CENTER;
                    SaveSettings();
                    if (g_appState.isMuted && g_appState.isOverlayVisible) {
                        UpdateOverlayPosition();
                    }
                    break;
                case ID_MENU_POS_BOTTOM_RIGHT:
                    g_appState.settings.overlayPosition = POS_BOTTOM_RIGHT;
                    SaveSettings();
                    if (g_appState.isMuted && g_appState.isOverlayVisible) {
                        UpdateOverlayPosition();
                    }
                    break;
                    
                // Multi-monitor toggle
                case ID_MENU_MULTIMONITOR_TOGGLE:
                    // Hide current overlays before changing setting
                    ShowOverlay(FALSE);
                    
                    g_appState.settings.multiMonitorMode = !g_appState.settings.multiMonitorMode;
                    SaveSettings();
                    
                    // Show overlays again if muted, respecting new setting
                    if (g_appState.isMuted) {
                        ShowOverlay(TRUE);
                    }
                    break;
                    
                // Reduce volume when muted toggle
                case ID_MENU_REDUCE_VOLUME_TOGGLE:
                    g_appState.settings.reduceVolumeWhenMuted = !g_appState.settings.reduceVolumeWhenMuted;
                    // Apply/remove volume reduction if currently muted
                    if (g_appState.isMuted) {
                        ReduceSystemVolume(g_appState.settings.reduceVolumeWhenMuted);
                    }
                    SaveSettings();
                    break;
                    
                // Audio feedback toggle
                case ID_MENU_AUDIO_FEEDBACK_TOGGLE:
                    g_appState.settings.audioFeedbackEnabled = !g_appState.settings.audioFeedbackEnabled;
                    SaveSettings();
                    break;
                    
                // Hotkey settings - All F keys
                case ID_MENU_HOTKEY_F1:
                    ChangeHotkey(HOTKEY_F1);
                    break;
                case ID_MENU_HOTKEY_F2:
                    ChangeHotkey(HOTKEY_F2);
                    break;
                case ID_MENU_HOTKEY_F3:
                    ChangeHotkey(HOTKEY_F3);
                    break;
                case ID_MENU_HOTKEY_F4:
                    ChangeHotkey(HOTKEY_F4);
                    break;
                case ID_MENU_HOTKEY_F5:
                    ChangeHotkey(HOTKEY_F5);
                    break;
                case ID_MENU_HOTKEY_F6:
                    ChangeHotkey(HOTKEY_F6);
                    break;
                case ID_MENU_HOTKEY_F7:
                    ChangeHotkey(HOTKEY_F7);
                    break;
                case ID_MENU_HOTKEY_F8:
                    ChangeHotkey(HOTKEY_F8);
                    break;
                case ID_MENU_HOTKEY_F9:
                    ChangeHotkey(HOTKEY_F9);
                    break;
                case ID_MENU_HOTKEY_F10:
                    ChangeHotkey(HOTKEY_F10);
                    break;
                case ID_MENU_HOTKEY_F11:
                    ChangeHotkey(HOTKEY_F11);
                    break;
                case ID_MENU_HOTKEY_F12:
                    ChangeHotkey(HOTKEY_F12);
                    break;
                case ID_MENU_HOTKEY_CTRL_M:
                    ChangeHotkey(HOTKEY_CTRL_M);
                    break;
                case ID_MENU_HOTKEY_CUSTOM:
                    // Disabled - Coming soon
                    break;
                    
                // Startup settings
                case ID_MENU_STARTUP_ENABLE:
                    EnableStartup(TRUE);
                    break;
                case ID_MENU_STARTUP_DISABLE:
                    EnableStartup(FALSE);
                    break;
                    
                // Volume lock
                case ID_MENU_VOLUME_LOCK_TOGGLE:
                    if (g_appState.settings.volumeLockEnabled) {
                        g_appState.settings.volumeLockEnabled = FALSE;
                        KillTimer(hwnd, VOLUME_MONITOR_TIMER_ID);
                    } else {
                        LockCurrentVolume();
                    }
                    SaveSettings();
                    break;
                    
                case ID_MENU_ABOUT:
                    {
                        int result = MessageBoxW(hwnd, 
                            L"iAlturki-MicMute v1.3\n\n"
                            L"By iAlturki - Compiled using C\n\n"
                            L"Visit GitHub page?", 
                            L"About iAlturki-MicMute", 
                            MB_YESNO | MB_ICONINFORMATION);
                        
                        if (result == IDYES) {
                            ShellExecuteW(NULL, L"open", L"https://github.com/iAlturki", NULL, NULL, SW_SHOWNORMAL);
                        }
                    }
                    break;
                case ID_MENU_EXIT:
                    PostQuitMessage(0);
                    break;
            }
            break;
            
        case WM_DESTROY:
            PostQuitMessage(0);
            break;
            
        default:
            return DefWindowProc(hwnd, uMsg, wParam, lParam);
    }
    return 0;
}

LRESULT CALLBACK OverlayWndProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    switch (uMsg) {
        case WM_PAINT:
            {
                PAINTSTRUCT ps;
                HDC hdc = BeginPaint(hwnd, &ps);
                
                RECT rect;
                GetClientRect(hwnd, &rect);
                
                // Fill background with transparent black
                HBRUSH blackBrush = CreateSolidBrush(RGB(0, 0, 0));
                FillRect(hdc, &rect, blackBrush);
                DeleteObject(blackBrush);
                
                // Draw the loaded icon if available
                if (g_appState.hMutedIcon) {
                    // Draw icon with proper transparency
                    DrawIconEx(hdc, 0, 0, g_appState.hMutedIcon, 
                              rect.right, rect.bottom, 0, NULL, DI_NORMAL);
                } else {
                    // Fallback: Draw red circle with slash if icon not found
                    HBRUSH redBrush = CreateSolidBrush(RGB(220, 20, 20));
                    HBRUSH oldBrush = SelectObject(hdc, redBrush);
                    HPEN redPen = CreatePen(PS_SOLID, 2, RGB(200, 0, 0));
                    HPEN oldPen = SelectObject(hdc, redPen);
                    
                    Ellipse(hdc, 2, 2, rect.right - 2, rect.bottom - 2);
                    
                    // Draw diagonal slash
                    HPEN whitePen = CreatePen(PS_SOLID, rect.right / 8, RGB(255, 255, 255));
                    SelectObject(hdc, whitePen);
                    MoveToEx(hdc, rect.right / 4, rect.bottom / 4, NULL);
                    LineTo(hdc, 3 * rect.right / 4, 3 * rect.bottom / 4);
                    
                    SelectObject(hdc, oldPen);
                    SelectObject(hdc, oldBrush);
                    DeleteObject(whitePen);
                    DeleteObject(redPen);
                    DeleteObject(redBrush);
                }
                
                EndPaint(hwnd, &ps);
            }
            break;
            
        default:
            return DefWindowProc(hwnd, uMsg, wParam, lParam);
    }
    return 0;
}
