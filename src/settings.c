#include "micmute.h"

#define REGISTRY_KEY L"SOFTWARE\\iAlturki\\MicMute"
#define STARTUP_KEY L"SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Run"
#define APP_NAME L"iAlturki-MicMute"

void InitializeDefaultSettings(void)
{
    g_appState.settings.overlaySize = SIZE_MEDIUM;
    g_appState.settings.overlayPosition = POS_TOP_RIGHT;
    g_appState.settings.multiMonitorMode = TRUE; // All monitors by default
    g_appState.settings.animationSpeed = ANIM_NORMAL;
    g_appState.settings.autoHideDuration = 2000; // 2 seconds
    g_appState.settings.audioFeedbackEnabled = TRUE;
    g_appState.settings.reduceVolumeWhenMuted = FALSE; // OFF by default
    g_appState.settings.currentHotkey = HOTKEY_F8;
    g_appState.settings.customHotkeyModifiers = MOD_CONTROL | MOD_SHIFT;
    g_appState.settings.customHotkeyVK = VK_F12; // Default custom: Ctrl+Shift+F12
    g_appState.settings.startupEnabled = FALSE;
    g_appState.settings.volumeLockEnabled = FALSE;
    g_appState.settings.lockedVolume = 1.0f;
    g_appState.settings.volumeCheckInterval = 500; // Check every 500ms for more reliability
}

void LoadSettings(void)
{
    HKEY hKey;
    DWORD dwType, dwSize;
    
    if (RegOpenKeyExW(HKEY_CURRENT_USER, REGISTRY_KEY, 0, KEY_READ, &hKey) == ERROR_SUCCESS) {
        dwSize = sizeof(DWORD);
        
        RegQueryValueExW(hKey, L"OverlaySize", NULL, &dwType, (LPBYTE)&g_appState.settings.overlaySize, &dwSize);
        RegQueryValueExW(hKey, L"OverlayPosition", NULL, &dwType, (LPBYTE)&g_appState.settings.overlayPosition, &dwSize);
        RegQueryValueExW(hKey, L"MultiMonitorMode", NULL, &dwType, (LPBYTE)&g_appState.settings.multiMonitorMode, &dwSize);
        RegQueryValueExW(hKey, L"AnimationSpeed", NULL, &dwType, (LPBYTE)&g_appState.settings.animationSpeed, &dwSize);
        RegQueryValueExW(hKey, L"AutoHideDuration", NULL, &dwType, (LPBYTE)&g_appState.settings.autoHideDuration, &dwSize);
        RegQueryValueExW(hKey, L"AudioFeedbackEnabled", NULL, &dwType, (LPBYTE)&g_appState.settings.audioFeedbackEnabled, &dwSize);
        RegQueryValueExW(hKey, L"ReduceVolumeWhenMuted", NULL, &dwType, (LPBYTE)&g_appState.settings.reduceVolumeWhenMuted, &dwSize);
        RegQueryValueExW(hKey, L"CurrentHotkey", NULL, &dwType, (LPBYTE)&g_appState.settings.currentHotkey, &dwSize);
        RegQueryValueExW(hKey, L"CustomHotkeyModifiers", NULL, &dwType, (LPBYTE)&g_appState.settings.customHotkeyModifiers, &dwSize);
        RegQueryValueExW(hKey, L"CustomHotkeyVK", NULL, &dwType, (LPBYTE)&g_appState.settings.customHotkeyVK, &dwSize);
        RegQueryValueExW(hKey, L"VolumeLockEnabled", NULL, &dwType, (LPBYTE)&g_appState.settings.volumeLockEnabled, &dwSize);
        RegQueryValueExW(hKey, L"VolumeCheckInterval", NULL, &dwType, (LPBYTE)&g_appState.settings.volumeCheckInterval, &dwSize);
        
        dwSize = sizeof(float);
        RegQueryValueExW(hKey, L"LockedVolume", NULL, &dwType, (LPBYTE)&g_appState.settings.lockedVolume, &dwSize);
        
        RegCloseKey(hKey);
    }
    
    // Check startup setting separately and sync with our registry
    BOOL actualStartup = IsStartupEnabled();
    g_appState.settings.startupEnabled = actualStartup;
}

void SaveSettings(void)
{
    HKEY hKey;
    DWORD dwDisposition;
    
    if (RegCreateKeyExW(HKEY_CURRENT_USER, REGISTRY_KEY, 0, NULL, 0, KEY_WRITE, NULL, &hKey, &dwDisposition) == ERROR_SUCCESS) {
        RegSetValueExW(hKey, L"OverlaySize", 0, REG_DWORD, (LPBYTE)&g_appState.settings.overlaySize, sizeof(DWORD));
        RegSetValueExW(hKey, L"OverlayPosition", 0, REG_DWORD, (LPBYTE)&g_appState.settings.overlayPosition, sizeof(DWORD));
        RegSetValueExW(hKey, L"MultiMonitorMode", 0, REG_DWORD, (LPBYTE)&g_appState.settings.multiMonitorMode, sizeof(DWORD));
        RegSetValueExW(hKey, L"AnimationSpeed", 0, REG_DWORD, (LPBYTE)&g_appState.settings.animationSpeed, sizeof(DWORD));
        RegSetValueExW(hKey, L"AutoHideDuration", 0, REG_DWORD, (LPBYTE)&g_appState.settings.autoHideDuration, sizeof(DWORD));
        RegSetValueExW(hKey, L"AudioFeedbackEnabled", 0, REG_DWORD, (LPBYTE)&g_appState.settings.audioFeedbackEnabled, sizeof(DWORD));
        RegSetValueExW(hKey, L"ReduceVolumeWhenMuted", 0, REG_DWORD, (LPBYTE)&g_appState.settings.reduceVolumeWhenMuted, sizeof(DWORD));
        RegSetValueExW(hKey, L"CurrentHotkey", 0, REG_DWORD, (LPBYTE)&g_appState.settings.currentHotkey, sizeof(DWORD));
        RegSetValueExW(hKey, L"CustomHotkeyModifiers", 0, REG_DWORD, (LPBYTE)&g_appState.settings.customHotkeyModifiers, sizeof(DWORD));
        RegSetValueExW(hKey, L"CustomHotkeyVK", 0, REG_DWORD, (LPBYTE)&g_appState.settings.customHotkeyVK, sizeof(DWORD));
        RegSetValueExW(hKey, L"VolumeLockEnabled", 0, REG_DWORD, (LPBYTE)&g_appState.settings.volumeLockEnabled, sizeof(DWORD));
        RegSetValueExW(hKey, L"VolumeCheckInterval", 0, REG_DWORD, (LPBYTE)&g_appState.settings.volumeCheckInterval, sizeof(DWORD));
        RegSetValueExW(hKey, L"StartupEnabled", 0, REG_DWORD, (LPBYTE)&g_appState.settings.startupEnabled, sizeof(DWORD));
        RegSetValueExW(hKey, L"LockedVolume", 0, REG_BINARY, (LPBYTE)&g_appState.settings.lockedVolume, sizeof(float));
        
        RegCloseKey(hKey);
    }
}

BOOL IsStartupEnabled(void)
{
    HKEY hKey;
    BOOL enabled = FALSE;
    
    if (RegOpenKeyExW(HKEY_CURRENT_USER, STARTUP_KEY, 0, KEY_READ, &hKey) == ERROR_SUCCESS) {
        DWORD dwType, dwSize = 0;
        if (RegQueryValueExW(hKey, APP_NAME, NULL, &dwType, NULL, &dwSize) == ERROR_SUCCESS) {
            enabled = TRUE;
        }
        RegCloseKey(hKey);
    }
    
    return enabled;
}

void EnableStartup(BOOL enable)
{
    HKEY hKey;
    
    if (RegOpenKeyExW(HKEY_CURRENT_USER, STARTUP_KEY, 0, KEY_WRITE, &hKey) == ERROR_SUCCESS) {
        if (enable) {
            wchar_t exePath[MAX_PATH];
            GetModuleFileNameW(NULL, exePath, MAX_PATH);
            
            // Add the application to startup
            RegSetValueExW(hKey, APP_NAME, 0, REG_SZ, (LPBYTE)exePath, (wcslen(exePath) + 1) * sizeof(wchar_t));
        } else {
            // Remove from startup
            RegDeleteValueW(hKey, APP_NAME);
        }
        RegCloseKey(hKey);
    }
    
    g_appState.settings.startupEnabled = enable;
    SaveSettings();
}
