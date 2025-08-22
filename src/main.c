#include "micmute.h"

AppState g_appState = {0};

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow)
{
    // Initialize COM
    CoInitialize(NULL);
    
    // Initialize settings with defaults
    InitializeDefaultSettings();
    LoadSettings();
    
    // Initialize multi-monitor overlay system
    InitializeMultiMonitorOverlay();
    
    // Create main window
    if (!CreateMainWindow(hInstance)) {
        MessageBoxW(NULL, L"Failed to create window", L"Error", MB_OK | MB_ICONERROR);
        return 1;
    }
    
    // Load the muted icon from file
    LoadIconFromFile();
    
    // Initialize audio
    if (!InitializeAudio()) {
        MessageBoxW(NULL, L"Failed to initialize audio", L"Error", MB_OK | MB_ICONERROR);
        return 1;
    }
    
    // Initialize system tray
    if (!InitializeSystemTray(g_appState.hWnd, hInstance)) {
        MessageBoxW(NULL, L"Failed to initialize system tray", L"Error", MB_OK | MB_ICONERROR);
        return 1;
    }
    
    // Hotkey will be registered automatically by WM_CREATE timer
    
    // Start volume monitoring timer if volume lock is enabled
    if (g_appState.settings.volumeLockEnabled) {
        SetTimer(g_appState.hWnd, VOLUME_MONITOR_TIMER_ID, g_appState.settings.volumeCheckInterval, NULL);
    }
    
    // Message loop
    MSG msg;
    while (GetMessage(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
    
    // Cleanup
    UnregisterCurrentHotkey();
    CleanupSystemTray();
    CleanupAudio();
    CleanupMultiMonitorOverlay();
    SaveSettings();
    
    if (g_appState.hMutedIcon) {
        DestroyIcon(g_appState.hMutedIcon);
    }
    
    CoUninitialize();
    
    return 0;
}
