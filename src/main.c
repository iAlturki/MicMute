#include "micmute.h"

AppState g_appState = {0};
HANDLE g_hMutex = NULL;

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow)
{
    // Check for single instance
    g_hMutex = CreateMutexW(NULL, TRUE, L"iAlturki-MicMute-SingleInstance");
    if (GetLastError() == ERROR_ALREADY_EXISTS) {
        MessageBoxW(NULL, L"iAlturki-MicMute is already running!", L"Already Running", MB_OK | MB_ICONINFORMATION);
        if (g_hMutex) {
            CloseHandle(g_hMutex);
        }
        return 0;
    }
    
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
        if (g_hMutex) {
            ReleaseMutex(g_hMutex);
            CloseHandle(g_hMutex);
        }
        return 1;
    }
    
    // Load the muted icon from file
    LoadIconFromFile();
    
    // Initialize audio
    if (!InitializeAudio()) {
        MessageBoxW(NULL, L"Failed to initialize audio", L"Error", MB_OK | MB_ICONERROR);
        if (g_hMutex) {
            ReleaseMutex(g_hMutex);
            CloseHandle(g_hMutex);
        }
        return 1;
    }
    
    // Check actual microphone state and sync with saved state
    BOOL actualMuteState;
    if (g_appState.pEndpointVolume && 
        SUCCEEDED(g_appState.pEndpointVolume->lpVtbl->GetMute(g_appState.pEndpointVolume, &actualMuteState))) {
        g_appState.isMuted = actualMuteState;
        
        // Show overlay immediately if currently muted
        if (actualMuteState) {
            ShowOverlay(TRUE);
        }
        
        // Update tray icon to reflect current state
        UpdateTrayIcon(actualMuteState);
        
        // Save the actual state
        g_appState.settings.lastMuteState = actualMuteState;
    } else {
        // Fallback: restore last saved state if we can't read current state
        if (g_appState.settings.lastMuteState != g_appState.isMuted) {
            ToggleMicrophone();
        }
    }
    
    // Initialize system tray
    if (!InitializeSystemTray(g_appState.hWnd, hInstance)) {
        MessageBoxW(NULL, L"Failed to initialize system tray", L"Error", MB_OK | MB_ICONERROR);
        if (g_hMutex) {
            ReleaseMutex(g_hMutex);
            CloseHandle(g_hMutex);
        }
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
    
    // Save current state before exit
    g_appState.settings.lastMuteState = g_appState.isMuted;
    SaveSettings();
    
    if (g_appState.hMutedIcon) {
        DestroyIcon(g_appState.hMutedIcon);
    }
    
    // Release single instance mutex
    if (g_hMutex) {
        ReleaseMutex(g_hMutex);
        CloseHandle(g_hMutex);
    }
    
    CoUninitialize();
    
    return 0;
}
